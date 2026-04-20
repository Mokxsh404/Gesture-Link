import socket
import wave
import os
from faster_whisper import WhisperModel
import numpy as np
from datetime import datetime


class AudioSTTServer:
    def __init__(self, host='0.0.0.0', port=8080, model_size='base', use_gpu=True):
        self.host = host
        self.port = port
        self.server_socket = None
        
        import torch
        cuda_available = torch.cuda.is_available()
        
        if use_gpu and cuda_available:
            device = "cuda"
            compute_type = "float16"
            print(f"GPU detected: {torch.cuda.get_device_name(0)}")
            print(f"GPU Memory: {torch.cuda.get_device_properties(0).total_memory / 1e9:.2f} GB")
        else:
            device = "cpu"
            compute_type = "int8"
            if use_gpu and not cuda_available:
                print("CUDA not available, falling back to CPU")
            else:
                print("Using CPU")
        
        print(f"Loading Faster Whisper model '{model_size}' on {device.upper()}...")
        
        self.model = WhisperModel(
            model_size, 
            device=device, 
            compute_type=compute_type,
            num_workers=4
        )
        print(f"Model loaded successfully on {device.upper()}!\n")
        
        # these need to match ESP32 settings
        self.sample_rate = 16000
        self.channels = 1
        self.sample_width = 2  # 16-bit audio
        
    def start(self):
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)
        
        print(f"TCP Server started on {self.host}:{self.port}")
        print(f"Waiting for ESP32 connections...\n")
        
        try:
            while True:
                client_socket, address = self.server_socket.accept()
                print(f"Connected: {address}")
                self.handle_client(client_socket, address)
        except KeyboardInterrupt:
            print("\n\nServer shutting down...")
        finally:
            if self.server_socket:
                self.server_socket.close()
    
    def handle_client(self, client_socket, address):
        audio_data = bytearray()
        
        try:
            print(f"Receiving audio from {address}...")
            client_socket.settimeout(5.0)
            
            while True:
                try:
                    chunk = client_socket.recv(8192)
                    if not chunk:
                        break
                    audio_data.extend(chunk)
                except socket.timeout:
                    break
            
            if len(audio_data) == 0:
                print(f"No audio data received from {address}")
                client_socket.sendall(b"ERROR: No audio data\n")
                client_socket.close()
                return
            
            print(f"Received {len(audio_data)} bytes ({len(audio_data)/32000:.2f}s of audio)")
            
            # save as wav so whisper can process it
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            temp_file = f"temp_audio_{timestamp}.wav"
            
            self.save_wav(audio_data, temp_file)
            print(f"Saved to: {temp_file}")
            
            print(f"Transcribing...")
            text = self.transcribe_audio(temp_file)
            
            if text:
                print(f"Transcription: '{text}'")
                response = f"{text}\n"
                client_socket.sendall(response.encode('utf-8'))
                print(f"Sent response to {address}\n")
            else:
                print(f"No speech detected")
                client_socket.sendall(b"ERROR: No speech detected\n")
            
            try:
                os.remove(temp_file)
            except:
                pass
                
        except Exception as e:
            print(f"Error handling client {address}: {e}")
            try:
                client_socket.sendall(f"ERROR: {str(e)}\n".encode('utf-8'))
            except:
                pass
        finally:
            client_socket.close()
            print(f"Disconnected: {address}\n")
    
    def save_wav(self, audio_data, filename):
        with wave.open(filename, 'wb') as wav_file:
            wav_file.setnchannels(self.channels)
            wav_file.setsampwidth(self.sample_width)
            wav_file.setframerate(self.sample_rate)
            wav_file.writeframes(audio_data)
    
    def transcribe_audio(self, audio_file):
        try:
            segments, info = self.model.transcribe(
                audio_file, 
                beam_size=1,
                best_of=1,
                language="en",
                vad_filter=True,
                vad_parameters=dict( 
                    min_silence_duration_ms=200,
                    threshold=0.4
                ),
                condition_on_previous_text=False,
                temperature=0.0,
                compression_ratio_threshold=2.4,
                no_speech_threshold=0.6
            )
            
            transcription = " ".join([segment.text.strip() for segment in segments])
            transcription = transcription.strip()
            
            return transcription if transcription else None
            
        except Exception as e:
            print(f"Transcription error: {e}")
            return None


if __name__ == "__main__":
    HOST = '0.0.0.0'
    PORT = 8080
    MODEL_SIZE = 'base'
    USE_GPU = True
    
    print("=" * 60)
    print("SPEECH-TO-BRAILLE TCP SERVER")
    print("=" * 60)
    print(f"Host: {HOST}")
    print(f"Port: {PORT}")
    print(f"Model: {MODEL_SIZE}")
    print(f"GPU Enabled: {USE_GPU}")
    print("=" * 60 + "\n") 
    
    server = AudioSTTServer(host=HOST, port=PORT, model_size=MODEL_SIZE, use_gpu=USE_GPU)
    server.start()