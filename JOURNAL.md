# Developer Journal: The Gesture Link Journey
*A log of late nights, burnt sensors, git merge conflicts, and small victories.*

---

### **Day 1: April 10, 2026 — The Inception**
So I've been chewing on this idea for a couple of weeks now. There are tons of individual gadgets out there—like smart glasses that read out signs, or sign language gloves that output text—but almost none of them focus on *two-way* communication between two differently-abled people. Like, how does a visually impaired person talk back to a speech-impaired person without a third party in the middle? 

The plan is to build **Gesture Link** as a fully offline, wearable system:
- **Visora**: Smart glasses (ESP32-S3 Sense) that handle object and colour detection and read things out.
- **Glove A**: The gesture-to-speech glove (ESP32-C6 + flex sensors + IMU) for the speech-impaired user.
- **Glove B**: The speech-to-Braille glove (ESP32 + I2S Mic + vibration motors) for the visually impaired user.

Ordered a bunch of parts today: ESP32s, flex sensors, a 6-axis IMU, INMP441 mic, ST7735 TFT, DFPlayer Mini, and 12 coin-type vibration motors. RIP my bank account.

---

### **Day 8: April 18, 2026 — Unboxing & Basic Blinks**
Boxes arrived today! Spent my Saturday night setting up VS Code and PlatformIO. Tested basic code on the ESP32-S3 Sense and ESP32-C6. 

Had my first hardware headache: the ESP32-S3 board layout is tiny, and soldering the headers without bridging the adjacent camera connection pads was incredibly stressful. I made so many dumb soldering mistakes, bridged pins, and had to desolder and redo everything. This was just the start of a constant soldering and setup battle that lasted the entire two months. Between troubleshooting loose joints, fixing shorted lines, and replacing components I accidentally burnt out, I easily put in over 35 hours just on soldering and hardware debugging throughout the project. Got a basic camera web server running to make sure the camera module wasn't DOA. It works!

---

### **Day 15: April 25, 2026 — Glove A: Flex Sensors & The DFPlayer Nightmare**
Spent my entire weekend trying to build the Glove A prototype. I literally stitched four flex sensors to a cheap gardening glove using conductive thread, which was super annoying to sew, and then wired them to the ESP32-C6.

The analog readings were all over the place. They drift depending on how warm the room is, how tight the glove is worn, and how much I bend my fingers. Had to write a quick calibration script in the setup to save 'straight' and 'bent' values to calculate a clean percentage.

And then the DFPlayer Mini. What an absolute nightmare of a chip. It wouldn't play any sound for hours. Turns out:
1. The SD card has to be formatted to FAT32.
2. The folders must be named exactly `/mp3/` and the tracks have to be padded like `0001.mp3`, `0002.mp3`.
3. I needed a 1k resistor in series with the RX line to stop the weird buzzing noise.
Finally got it to say "Hello" when I bent my index finger. So satisfying.

---

### **Day 22: May 2, 2026 — Glove A: Adding the IMU & OLED**
Flex sensors alone only tell me if my fingers are bent, not where my hand is pointing. If I want to separate "Hello" (flat hand) from "Help" (hand tilted up), I need orientation.

Wired up the MPU6050 IMU to the I2C pins. Now the ESP32 combines finger flex and hand tilt angles. Set up a simple state machine: index/middle bent + flat hand is "Hello" (track 0001.mp3), all bent + tilted hand is "Help" (track 0002.mp3).

Threw in a 0.96" OLED screen to show the live readings. It looks like some cyberpunk glove now, pretty cool.

---

### **Day 31: May 11, 2026 — Glove B: The Tactile Braille Matrix**
Started working on Glove B (Speech-to-Braille). The plan is to get voice inputs and turn them into haptic vibrations on the hand. Bought 12 flat vibration motors to act as the Braille cell dots.

But wait—the ESP32 GPIO pins can't output enough current for multiple motors. They draw like 70mA each and the ESP32 only gives 20mA. If I ran them directly, the board would probably fry.
Ended up staying up all night soldering a transistor driver array with 12 2N2222 transistors and diodes on a protoboard. Looks like a total rat's nest but it does the job.

Then mapped the letters (a-z) to binary codes (like 'a' -> `100000`) so the right motors vibrate when a letter comes in.

---

### **Day 38: May 18, 2026 — Glove B: I2S Microphone Audio Nightmare**
Configuring the I2S microphone (INMP441) on the ESP32 took me two full days. I2S is such a pain compared to normal SPI or I2C because you have to deal with DMA buffers. Kept getting static and buffer errors. Finally got it configured to 16kHz mono.

Since I wanted the speech recognition to run offline on a local machine, I set up the ESP32 to stream the raw audio over WebSockets to a local Python script running Whisper STT, which sends the text back.

---

### **Day 45: May 25, 2026 — Visora: YOLOv8x & The 100MB Git Wall**
Working on Visora (the smart glasses) today. Got the ESP32-S3 Sense camera streaming JPEG frames. On the PC, I wrote a Python script using OpenCV and YOLOv8 to detect objects in real time.

I used the heavy `yolov8x.pt` model to make sure it's accurate. Works great, but when I tried to push to GitHub, the push got blocked. GitHub has a 100MB limit and the model is 137MB.
Had to set up Git LFS, track `*.pt` files, and re-push. Lesson learned.

---

### **Day 52: June 1, 2026 — Building the Website Simulator**
Wanted to make a nice landing page website so people can actually try this out without having to build all the hardware themselves. Spent a lot of time writing CSS animations for scroll reveals, glowing orbs, and interactive web simulators.
- For Glove A: you click play, watch virtual fingers bend, and hear the audio.
- For Glove B: you talk into the mic, it does speech-to-text, and flashes the virtual motors on the screen in Braille patterns.

Used AI to help fix some CSS issues and write the simulation logic.

---

### **Day 56: June 5, 2026 — Web Speech API Network Errors & Brave Browser**
Tested the simulator on my friend's PC and immediately got hit with a "Speech recognition error: network" error. Thought my code was broken, but it turned out Brave blocks it by default because it sends voice data to Google.
*Fix:* Added a quick warning note to the site telling users to enable Google speech recognition in their settings or use Chrome/Safari.

---

### **Day 57: June 6, 2026 — Adding WHO Stats & Growth Graph**
Added some WHO data to show why this project matters (2.2B vision impaired, 1.5B hearing impaired, 430M with speech difficulties). Built an animated SVG graph to show the growth. The animations work when you scroll down to them.

---

### **Day 63: June 12, 2026 — Final Cleanup and Reflection**
Today was mostly cleaning up the code, README, and writing the PlatformIO setup guide.

Honestly, looking back at the last 2 months, I've easily sunk about 120 hours into this whole thing. Between wasting like 35+ hours just desoldering my mistakes, trying to get the ESP32 code to actually recognize hand gestures, fighting the audio buffer noise, and coding the landing page, it's been a massive time sink.

But seeing it all come together—where a gesture from Glove A plays out loud, and speaking into Glove B triggers the physical vibration pins in Braille format—makes all those sleepless nights and burnt fingers completely worth it.
