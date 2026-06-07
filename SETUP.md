# Setup and Running Instructions

Follow these steps to set up the Python/PC side of the project (VISORA) and to prepare model files.

1. Create a virtual environment (recommended):

```bash
python -m venv .venv
source .venv/Scripts/activate    # Windows: .venv\Scripts\activate
```

2. Install Python dependencies:

```bash
pip install --upgrade pip
pip install -r requirements.txt
```

Notes:
- `ffmpeg` is required by `VISORA/server.py` for MP3 → PCM conversion. Install via your OS package manager or from https://ffmpeg.org.
- For GPU acceleration with `torch`, install a CUDA-compatible build following PyTorch's install guide.

3. Git LFS and model handling

The repository tracks a large YOLO model via Git LFS at `VISORA/yolov8x.pt`.

```bash
# install git-lfs (once)
git lfs install
git lfs track "VISORA/yolov8x.pt"
git add .gitattributes
```

If you prefer not to store the model in Git, download the model manually and place it at `VISORA/yolov8x.pt`.

4. Run the VISORA components

Start the dual server (STT + TTS):

```bash
python VISORA/server.py
```

Run object detection (ensure `VISORA/yolov8x.pt` is present):

```bash
python VISORA/object_detection.py
```

5. PlatformIO / Microcontroller

Use PlatformIO to build and upload firmware for ESP32 devices. Open each device folder and run:

```bash
# from project root
platformio run -e seeed_xiao_esp32c6 -d GLOVE-A
platformio run -e esp32dev -d GLOVE-B
platformio run -e xiao_esp32s3_sense -d VISORA
```

6. Troubleshooting

- If Python modules fail to import, verify you're using the virtualenv and that `pip install -r requirements.txt` completed without errors.
- If `ffmpeg` is not found, ensure it's on your PATH.
- If object_detection cannot connect to the camera, verify `CAM_IP` and `CAM_PORT` in `VISORA/object_detection.py`.
