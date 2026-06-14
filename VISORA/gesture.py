"""
Gesture-to-ESP32 helper script.

Reads the laptop camera, predicts sign-language letters using a trained
RandomForest model, and forwards recognized letters to the ESP32 gesture
server over TCP. Configure `ESP32_IP` and `CAM_IP` before running.
"""

import cv2
import mediapipe as mp
import numpy as np
import pickle
import time
import socket

# update this to match whatever IP the ESP32 prints on boot
ESP32_IP = "XX.XX.XX.XX"
ESP32_PORT = 8095  # gesture server port defined in main.cpp

# using laptop camera instead of ESP32-CAM for gesture detection
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
        client.connect((ESP32_IP, ESP32_PORT))
        client.settimeout(0.1)
        print(f"Connected to ESP32 at {ESP32_IP}:{ESP32_PORT}")
        return True
    except Exception as e:
        print(f"Connection failed: {e}")
        print("Make sure you're on the Gesture To Speech screen on ESP32")
        client = None
        return False

def send_to_esp32(current_letter, sentence):
    global client, last_reconnect_attempt
    
    if client is None:
        current_time = time.time()
        if current_time - last_reconnect_attempt >= RECONNECT_DELAY:
            last_reconnect_attempt = current_time
            connect_to_esp32()
        return
    
    try:
        message = f"GESTURE:{current_letter if current_letter else ''}|{sentence}\n"
        client.sendall(message.encode())
        print(f"Sent: Letter={current_letter}, Sentence={sentence[:30]}...")
    except Exception as e:
        print("Send failed, reconnecting...")
        client = None

print("Loading RandomForest model...")
try:
    rf_model = pickle.load(open('model_rf_static.p', 'rb'))['model']
    print("Model loaded successfully!")
except FileNotFoundError:
    print("ERROR: model_rf_static.p not found!")
    print("Please ensure the model file is in the same directory.")
    exit(1)

mp_hands = mp.solutions.hands
mp_drawing = mp.solutions.drawing_utils
mp_drawing_styles = mp.solutions.drawing_styles
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=1,
    min_detection_confidence=0.3
)

print(f"\nOpening laptop camera...")
cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("Failed to open laptop camera. Exiting.")
    exit(1)

print("Camera connected!")

current_letter = None
letter_start_time = None
sentence = ''
last_send_time = 0
SEND_INTERVAL = 0.3  # send to ESP32 every 300ms

HOLD_TIME_LETTER = 3   # hold for 3 seconds to add letter
HOLD_TIME_SPACE = 10   # hold 'S' for 10 seconds to add a space

print("\n" + "=" * 50)
print("GESTURE TO SPEECH - Sign Language A-Z")
print("=" * 50)
print("Hold a letter for 3 seconds to add it to sentence")
print("Hold 'S' for 10 seconds to add a SPACE")
print("Press 'Q' to quit, 'C' to clear sentence")
print("=" * 50 + "\n")

connect_to_esp32()

while True:
    ret, frame = cap.read()
    if not ret:
        continue

    frame = cv2.flip(frame, 1)  # mirror so it feels natural

    H, W, _ = frame.shape
    frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = hands.process(frame_rgb)

    predicted_character = None

    if results.multi_hand_landmarks:
        hand_landmarks = results.multi_hand_landmarks[0]

        mp_drawing.draw_landmarks(
            frame,
            hand_landmarks,
            mp_hands.HAND_CONNECTIONS,
            mp_drawing_styles.get_default_hand_landmarks_style(),
            mp_drawing_styles.get_default_hand_connections_style()
        )

        x_ = [lm.x for lm in hand_landmarks.landmark]
        y_ = [lm.y for lm in hand_landmarks.landmark]

        data_aux = []
        for lm in hand_landmarks.landmark:
            data_aux.append(lm.x - min(x_))
            data_aux.append(lm.y - min(y_))

        prediction = rf_model.predict([np.array(data_aux)])
        predicted_character = prediction[0]

        x1, y1 = int(min(x_) * W) - 10, int(min(y_) * H) - 10
        x2, y2 = int(max(x_) * W) + 10, int(max(y_) * H) + 10

        cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 3)
        cv2.putText(frame, predicted_character, (x1, y1 - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.5, (0, 255, 0), 3)
    else:
        current_letter = None
        letter_start_time = None

    current_time = time.time()

    if predicted_character:
        if predicted_character == current_letter:
            elapsed = current_time - letter_start_time
            hold_time = HOLD_TIME_SPACE if predicted_character == 'S' else HOLD_TIME_LETTER
            
            # progress bar showing how long they've held
            progress = min(elapsed / hold_time, 1.0)
            bar_width = int(200 * progress)
            cv2.rectangle(frame, (W - 220, 80), (W - 20, 100), (100, 100, 100), -1)
            cv2.rectangle(frame, (W - 220, 80), (W - 220 + bar_width, 100), (0, 255, 0), -1)
            cv2.putText(frame, f"Hold: {elapsed:.1f}s / {hold_time}s", (W - 220, 75),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            
            if predicted_character == 'S' and elapsed >= HOLD_TIME_SPACE:
                sentence += ' '
                letter_start_time = current_time + 1000  # prevent double-adding
            elif predicted_character != 'S' and elapsed >= HOLD_TIME_LETTER:
                sentence += predicted_character
                letter_start_time = current_time + 1000
        else:
            current_letter = predicted_character
            letter_start_time = current_time
    else:
        current_letter = None
        letter_start_time = None

    if current_time - last_send_time >= SEND_INTERVAL:
        send_to_esp32(predicted_character, sentence)
        last_send_time = current_time

    cv2.rectangle(frame, (0, 0), (W, 30), (40, 40, 40), -1)
    cv2.putText(frame, "Input: Laptop Camera", (5, 20),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (55, 213, 197), 1)

    cv2.rectangle(frame, (W - 100, 35), (W - 10, 75), (0, 0, 0), -1)
    cv2.putText(frame, "Current:", (W - 95, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
    if predicted_character:
        cv2.putText(frame, predicted_character, (W - 70, 72), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
    else:
        cv2.putText(frame, "-", (W - 60, 72), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (128, 128, 128), 2)

    # sentence bar at the bottom
    cv2.rectangle(frame, (0, H - 60), (W, H), (0, 0, 0), -1)
    cv2.putText(frame, "Sentence:", (10, H - 40), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)
    
    display_sentence = sentence if len(sentence) <= 40 else "..." + sentence[-37:]
    cv2.putText(frame, display_sentence, (10, H - 15), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

    cv2.imshow('Gesture To Speech - Sign Language', frame)

    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break
    elif key == ord('c'):
        sentence = ''
        print("Sentence cleared!")

cap.release()
cv2.destroyAllWindows()
if client:
    client.close()
    print("Connection closed")

print(f"\nFinal sentence: {sentence}")
print("Gesture detection stopped")
