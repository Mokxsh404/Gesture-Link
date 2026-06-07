"""
Dual TTS/STT server used by VISORA.

This script exposes two TCP services:
- TTS: convert incoming text to PCM and stream to ESP32
- STT: receive PCM audio from ESP32 and transcribe using Whisper

Configure language mappings below. Requires `ffmpeg` on PATH for MP3→PCM conversion.
"""

import socket
import subprocess
import os
import time
import threading
import numpy as np
from gtts import gTTS
from faster_whisper import WhisperModel
from googletrans import Translator

# paths and ports
FFMPEG = "ffmpeg"  # assuming it's in path
HOST = "0.0.0.0"
TTS_PORT = 5005
STT_PORT = 5000
CHUNK_SIZE = 1024   
DELAY_BETWEEN_CHUNKS = 0.01
SAMPLE_RATE = 16000

# language config - maps our internal names to gtts/whisper codes
LANGUAGE_MAP = {
    "ENGLISH": {"gtts": "en", "gtts_tld": "co.uk", "whisper": "en", "translate": "en"},
    "HINDI": {"gtts": "hi", "gtts_tld": "co.in", "whisper": "hi", "translate": "hi"},
    "PUNJABI": {"gtts": "pa", "gtts_tld": "co.in", "whisper": "pa", "translate": "pa"},
    "TAMIL": {"gtts": "ta", "gtts_tld": "co.in", "whisper": "ta", "translate": "ta"},
    "MARATHI": {"gtts": "mr", "gtts_tld": "co.in", "whisper": "mr", "translate": "mr"},
}

current_language = "ENGLISH"
language_lock = threading.Lock()

translator = Translator()

def get_current_language():
    with language_lock:
        return current_language

def set_current_language(lang):
    global current_language
    with language_lock:
        if lang in LANGUAGE_MAP:
            current_language = lang
            print(f"[LANG] Language changed to: {lang}")
            return True
        else:
            print(f"[LANG] Unknown language: {lang}")
            return False

def translate_text(text, target_lang):
    """translate text from english to whatever language is selected"""
    if target_lang == "ENGLISH":
        return text
    
    lang_code = LANGUAGE_MAP[target_lang]["translate"]
    
    # retry a few times in case of network issues
    for attempt in range(3):
        try:
            t = Translator()
            result = t.translate(text, src='en', dest=lang_code)
            translated = result.text
            print(f"[TRANSLATE] '{text}' -> '{translated}' ({target_lang})")
            return translated
        except Exception as e:
            print(f"[TRANSLATE] Attempt {attempt+1} failed: {e}")
            time.sleep(0.5)
    
    print(f"[TRANSLATE] All attempts failed, using original text")
    return text

def text_to_pcm(text):
    mp3_file = "tts.mp3"
    pcm_file = "tts.pcm"
    
    lang = get_current_language()
    lang_settings = LANGUAGE_MAP.get(lang, LANGUAGE_MAP["ENGLISH"])
    
    translated_text = translate_text(text, lang)
    
    print(f"[TTS] Generating audio in {lang} ({lang_settings['gtts']})")
    
    gTTS(text=translated_text, lang=lang_settings["gtts"], tld=lang_settings["gtts_tld"]).save(mp3_file)
    
    # convert mp3 to raw pcm that the esp32 speaker can play
    subprocess.run([
        FFMPEG,
        "-y",
        "-i", mp3_file,
        "-f", "s16le",
        "-ac", "1",
        "-ar", "22050",
        pcm_file
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    with open(pcm_file, "rb") as f:
        data = f.read()
    
    os.remove(mp3_file)
    os.remove(pcm_file)
    
    return data

def tts_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, TTS_PORT))
        s.listen(1)
        print(f"[TTS] Server listening on port {TTS_PORT}")
        
        while True:
            conn, addr = s.accept()
            print(f"[TTS] Connected: {addr}")
            
            with conn:
                try:
                    data = b""
                    while not data.endswith(b"\n"):
                        chunk = conn.recv(1024)
                        if not chunk:
                            break
                        data += chunk
                    
                    text = data.decode().strip()
                    if not text:
                        print("[TTS] Empty request")
                        continue
                    
                    print(f"[TTS] Request: {text}")
                    
                    # language change command
                    if text.startswith("LANG:"):
                        lang_name = text[5:].strip()
                        set_current_language(lang_name)
                        conn.sendall((0).to_bytes(4, "little"))
                        continue
                    
                    pcm = text_to_pcm(text)
                    
                    # send length first so esp32 knows how much to expect
                    conn.sendall(len(pcm).to_bytes(4, "little"))
                    
                    for i in range(0, len(pcm), CHUNK_SIZE):
                        try:
                            conn.sendall(pcm[i:i+CHUNK_SIZE])
                            time.sleep(DELAY_BETWEEN_CHUNKS)
                        except (ConnectionResetError, BrokenPipeError):
                            print("[TTS] ESP32 closed connection early")
                            break
                    
                    print("[TTS] Audio sent successfully")
                    
                except Exception as e:
                    print(f"[TTS] Error: {e}")

def stt_server():
    model = WhisperModel("base", device="cuda", compute_type="float16")
    print("[STT] Whisper model loaded")
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((HOST, STT_PORT))
    sock.listen(1)
    print(f"[STT] Server listening on port {STT_PORT}")
    
    while True:
        print("[STT] Waiting for ESP32...")
        conn, addr = sock.accept()
        print(f"[STT] Connected: {addr}")
        
        pcm_buffer = np.empty(0, dtype=np.int16)
        
        try:
            while True:
                data = conn.recv(CHUNK_SIZE * 2)
                if not data:
                    print("[STT] Connection closed")
                    break
                
                samples = np.frombuffer(data, dtype=np.int16)
                pcm_buffer = np.concatenate((pcm_buffer, samples))
                
                # process each second of audio
                if len(pcm_buffer) >= SAMPLE_RATE:
                    audio_f = pcm_buffer.astype(np.float32) / 32768.0
                    
                    lang = get_current_language()
                    lang_settings = LANGUAGE_MAP.get(lang, LANGUAGE_MAP["ENGLISH"])
                    whisper_lang = lang_settings["whisper"]
                    
                    print(f"[STT] Transcribing in {lang} ({whisper_lang})")
                    
                    segments, _ = model.transcribe(
                        audio_f, 
                        vad_filter=True,
                        language=whisper_lang
                    )
                    
                    for seg in segments:
                        text = seg.text.strip()
                        if text:
                            print(f"[STT] Transcript ({lang}): {text}")
                            try:
                                conn.sendall((text + "\n").encode("utf-8"))
                            except:
                                print("[STT] Failed to send transcript")
                                break
                    
                    # keep some audio context for next chunk
                    pcm_buffer = pcm_buffer[SAMPLE_RATE // 2:]
                    
        except KeyboardInterrupt:
            print("[STT] Interrupted")
            break
        except Exception as e:
            print(f"[STT] Error: {e}")
        finally:
            conn.close()

if __name__ == "__main__":
    print("=" * 50)
    print("DUAL SERVER: TTS + STT (Multi-Language Support)")
    print("=" * 50)
    print(f"Supported languages: {', '.join(LANGUAGE_MAP.keys())}")
    print(f"Default language: {current_language}")
    print("=" * 50)
    
    tts_thread = threading.Thread(target=tts_server, daemon=True)
    stt_thread = threading.Thread(target=stt_server, daemon=True)
    
    tts_thread.start()
    stt_thread.start()
    
    print("\nBoth servers running. Press Ctrl+C to exit.")
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nShutting down servers...")
