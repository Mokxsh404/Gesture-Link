"""
Colour recognition helper for VISORA.

Connects to an ESP32 TCP server and sends detected colour names and RGB
values. Configure `ESP32_IP` and `CAM_IP` to your local network addresses.
"""

import cv2
import mediapipe as mp
import socket
import numpy as np
import webcolors

# ESP32 config - check serial monitor on boot for the actual IP
ESP32_IP = "XX.XX.XX.XX"
PORT = 8090  # must match COLOUR_SERVER_PORT in main.cpp

CAM_IP = "XX.XX.XX.XX"
CAM_PORT = 81
CAM_URL = f"http://{CAM_IP}:{CAM_PORT}/stream"

client = None
last_reconnect_attempt = 0
RECONNECT_DELAY = 2

def connect_to_esp32():
    global client
    try:
        if client:
            client.close()
    except:
        pass
    
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.settimeout(2)
    try:
        client.connect((ESP32_IP, PORT))
        client.settimeout(0.1)
        print(f"Connected to ESP32 at {ESP32_IP}:{PORT}")
        return True
    except Exception as e:
        print(f"Connection failed: {e}")
        print("Make sure you're on the Colour Recognition screen on ESP32")
        client = None
        return False

def send_wifi(message):
    global client, last_reconnect_attempt
    import time
    
    if client is None:
        current_time = time.time()
        if current_time - last_reconnect_attempt >= RECONNECT_DELAY:
            last_reconnect_attempt = current_time
            connect_to_esp32()
        return
    
    try:
        client.sendall((message + "\n").encode())
        print("Sent:", message[:50] + "..." if len(message) > 50 else message)
    except Exception as e:
        print("Send failed, reconnecting...", e)
        client = None

mpHands = mp.solutions.hands
hands = mpHands.Hands(min_detection_confidence=0.7, min_tracking_confidence=0.7)
mpDraw = mp.solutions.drawing_utils

def get_color_name(b, g, r):
    """
    Map an RGB value to the nearest CSS3 color name.
    Uses euclidean distance to find the closest match.
    """
    try:
        color_name = webcolors.rgb_to_name((r, g, b))
        return color_name.replace("-", " ").replace("_", " ").title()
    except ValueError:
        min_distance = float('inf')
        closest_name = "Unknown"
        
        css3_colors = webcolors.names("css3")
        
        for name in css3_colors:
            try:
                color_rgb = webcolors.name_to_rgb(name)
                
                distance = ((r - color_rgb.red) ** 2 + 
                           (g - color_rgb.green) ** 2 + 
                           (b - color_rgb.blue) ** 2) ** 0.5
                
                if distance < min_distance:
                    min_distance = distance
                    closest_name = name
            except:
                continue
        
        return closest_name.replace("-", " ").replace("_", " ").title()


if __name__ == "__main__":
    print("=" * 50)
    print("Colour Recognition for VISORA")
    print("=" * 50)
    print(f"ESP32 TCP Server: {ESP32_IP}:{PORT}")
    print(f"Camera Stream: {CAM_URL}")
    print("=" * 50)
    
    print("\nConnecting to ESP32 TCP server...")
    if not connect_to_esp32():
        print("\nWARNING: Could not connect to ESP32 TCP server.")
        print("Color data will not be sent to ESP32.")
        print("Make sure the ESP32 is on the Colour Recognition screen.")
        input("Press Enter to continue with camera only, or Ctrl+C to exit...")
    
    print("\nOpening camera stream...")
    cap = cv2.VideoCapture(CAM_URL)
    
    if not cap.isOpened():
        print("ERROR: Cannot open ESP32-CAM stream")
        print("Please check if the camera is running and the IP is correct")
        if client:
            client.close()
        exit(1)
    
    print("ESP32-CAM stream opened successfully!")
    
    # smoothing state
    prev_sample_x = None
    prev_sample_y = None
    alpha_pos = 0.3
    prev_b, prev_g, prev_r = None, None, None
    alpha_color = 0.3
    
    print("\nStarting color detection... Press ESC to exit")

    while True:
        success, img = cap.read()
        if not success:
            continue

        h, w, c = img.shape
        imgRGB = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        results = hands.process(imgRGB)

        hand_detected = False
        if results.multi_hand_landmarks:
            for handLms in results.multi_hand_landmarks:
                hand_detected = True
                mpDraw.draw_landmarks(img, handLms, mpHands.HAND_CONNECTIONS)

                x_tip = int(handLms.landmark[8].x * w)
                y_tip = int(handLms.landmark[8].y * h)
                x_joint = int(handLms.landmark[7].x * w)
                y_joint = int(handLms.landmark[7].y * h)

                # sample point slightly beyond the fingertip
                dx = x_tip - x_joint
                dy = y_tip - y_joint
                sample_distance = 20
                sample_x = min(max(0, x_tip + int(dx * sample_distance)), w - 1)
                sample_y = min(max(0, y_tip + int(dy * sample_distance)), h - 1)

                # smooth position to reduce jitter
                if prev_sample_x is None:
                    smoothed_x = sample_x
                    smoothed_y = sample_y
                else:
                    smoothed_x = int(prev_sample_x * (1 - alpha_pos) + sample_x * alpha_pos)
                    smoothed_y = int(prev_sample_y * (1 - alpha_pos) + sample_y * alpha_pos)
                prev_sample_x, prev_sample_y = smoothed_x, smoothed_y

                # sample a small region for more stable color readings
                x1 = max(smoothed_x - 3, 0)
                y1 = max(smoothed_y - 3, 0)
                x2 = min(smoothed_x + 3, w - 1)
                y2 = min(smoothed_y + 3, h - 1)
                
                region = img[y1:y2+1, x1:x2+1]
                if region.size > 0:
                    b, g, r = [int(v) for v in region.mean(axis=(0, 1))]
                else:
                    b, g, r = 0, 0, 0

                if prev_b is None:
                    smoothed_b, smoothed_g, smoothed_r = b, g, r
                else:
                    smoothed_b = int(prev_b * (1 - alpha_color) + b * alpha_color)
                    smoothed_g = int(prev_g * (1 - alpha_color) + g * alpha_color)
                    smoothed_r = int(prev_r * (1 - alpha_color) + r * alpha_color)
                prev_b, prev_g, prev_r = smoothed_b, smoothed_g, smoothed_r

                color_name = get_color_name(smoothed_b, smoothed_g, smoothed_r)

                send_wifi(
                    f"Color: {color_name} | RGB({smoothed_r},{smoothed_g},{smoothed_b}) | "
                    f"Ray: ({x_joint},{y_joint}) -> ({x_tip},{y_tip})"
                )

                cv2.line(img, (x_joint, y_joint), (x_tip, y_tip), (255, 255, 255), 2)
                cv2.circle(img, (x_tip, y_tip), 10, (255, 255, 255), cv2.FILLED)
                cv2.circle(img, (smoothed_x, smoothed_y), 5, (0, 0, 0), 2)
                
                # color preview box in top left
                cv2.rectangle(img, (0, 0), (280, 80), (smoothed_b, smoothed_g, smoothed_r), -1)
                cv2.rectangle(img, (0, 0), (280, 80), (0, 0, 0), 2)
                
                brightness = (smoothed_r * 299 + smoothed_g * 587 + smoothed_b * 114) / 1000
                text_color = (0, 0, 0) if brightness > 128 else (255, 255, 255)
                
                cv2.putText(img, f"{color_name}", (10, 35), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.9, text_color, 2)
                cv2.putText(img, f"RGB({smoothed_r},{smoothed_g},{smoothed_b})", (10, 65), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.6, text_color, 1)

        cv2.rectangle(img, (0, h - 25), (w, h), (40, 40, 40), -1)
        cv2.putText(img, f"Stream: {CAM_URL}", (5, h - 8),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (55, 213, 197), 1)
        
        if not hand_detected:
            cv2.putText(img, "No hand detected", (w - 140, h - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
        else:
            cv2.putText(img, "Hand detected", (w - 120, h - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

        cv2.imshow("Color Detection (Wi-Fi)", img)
        if cv2.waitKey(1) & 0xFF == 27:
            break

    cap.release()
    cv2.destroyAllWindows()
    if client:
        client.close()
        print("Connection closed")

    print("Color detection stopped")
