"""
Object detection script for VISORA.

Connects to an ESP32-CAM stream, runs a YOLOv8 model locally, and forwards
detected object positions to the ESP32 over TCP.

Note: Update `ESP32_IP` and `CAM_IP` to your device addresses, or place
`yolov8x.pt` in the `VISORA/` folder and use Git LFS (recommended).
"""

import cv2
import socket
import torch
from ultralytics import YOLO
import time

# ESP32 IP - check serial monitor on boot
ESP32_IP = "XX.XX.XX.XX"
ESP32_PORT = 8010  # must match OBJECT_DETECTION_PORT in main.cpp

sock = None

device = "cuda" if torch.cuda.is_available() else "cpu"
print(f"Using device: {device}")

model = YOLO("yolov8x.pt")
model.to(device)

CAM_IP = "XX.XX.XX.XX"
CAM_PORT = 81
CAM_URL = f"http://{CAM_IP}:{CAM_PORT}/stream"
cap = cv2.VideoCapture(CAM_URL)

while not cap.isOpened():
    print(f"Cannot open stream at {CAM_URL}. Retrying in 2s...")
    time.sleep(2)
    cap = cv2.VideoCapture(CAM_URL)

print("Camera stream connected!")
print(f"Stream URL: {CAM_URL}")

while True:
    ret, frame = cap.read()
    if not ret:
        continue

    h, w, _ = frame.shape
    results = model(frame, device=device, verbose=False)[0]

    detected_objects = []

    for box in results.boxes:
        x1, y1, x2, y2 = map(int, box.xyxy[0])
        cls = int(box.cls[0])
        conf = int(float(box.conf[0]) * 100)
        label = model.names[cls]

        cx = (x1 + x2) / 2
        cy = (y1 + y2) / 2

        horiz = "Left" if cx < w / 3 else "Right" if cx > 2 * w / 3 else "Center"
        vert = "Top" if cy < h / 3 else "Bottom" if cy > 2 * h / 3 else "Middle"

        position = f"{vert} {horiz} - {label}"
        detected_objects.append(position)

        # color coding by position
        color = (0, 255, 0)
        if "Left" in horiz:
            color = (255, 100, 0)
        elif "Right" in horiz:
            color = (0, 100, 255)
        
        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
        
        label_text = f"{label} {conf}%"
        position_text = f"{vert} {horiz}"
        
        cv2.rectangle(frame, (x1, y1 - 35), (x1 + 150, y1), (0, 0, 0), -1)
        cv2.putText(frame, label_text, (x1 + 2, y1 - 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
        cv2.putText(frame, position_text, (x1 + 2, y1 - 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)

    cv2.rectangle(frame, (0, 0), (w, 25), (40, 40, 40), -1)
    cv2.putText(frame, f"Stream: {CAM_URL}", (5, 17),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (55, 213, 197), 1)
    
    cv2.putText(frame, f"Objects: {len(detected_objects)}", (w - 100, 17),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

    # try to maintain connection to ESP32, reconnect quietly if lost
    if sock is None:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1)
            sock.connect((ESP32_IP, ESP32_PORT))
            sock.settimeout(0.1)
            print(f"Connected to ESP32 at {ESP32_IP}:{ESP32_PORT}")
        except Exception as e:
            sock = None

    if sock:
        try:
            message = ",".join(detected_objects) if detected_objects else "None"
            sock.sendall((message + "\n").encode())
            print("Sent:", message[:60] + "..." if len(message) > 60 else message)
        except Exception as e:
            print("Failed to send, reconnecting...")
            try:
                sock.close()
            except:
                pass
            sock = None

    cv2.imshow("ESP32-CAM YOLO Detection", frame)

    if cv2.waitKey(1) & 0xFF == 27:
        break

cap.release()
cv2.destroyAllWindows()
if sock:
    sock.close()
