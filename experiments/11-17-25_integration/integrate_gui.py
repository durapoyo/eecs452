import time
import cv2
import mediapipe as mp
import serial
import threading
import queue
import json
import sys
from pathlib import Path
from typing import Optional
from picamera2 import Picamera2

# ==========================
# GUI Import
# ==========================
# Add python_gui to path
sys.path.append(str(Path(__file__).parent / "python_gui"))
from app import GestureAudioApp

# ==========================
# Shared Queue & Hardware Interface
# ==========================
gui_queue = queue.Queue()

class QueueHardware:
    def __init__(self):
        self.state = {
            "volume": 0.4,
            "pitch_a": 1.0,
            "pitch_b": 1.0,
            "deck_a_playing": False,
            "deck_b_playing": False,
            "mixer_mode": False,
            "echo_enabled": False,
            "eq_bands": [0.5, 0.5, 0.5],
            "song_a": "song1.wav",
            "song_b": "song2.wav"
        }
    
    def update(self):
        try:
            while True:
                msg_type, data = gui_queue.get_nowait()
                if msg_type == "STATE":
                    self.state["volume"] = data.get("vol", self.state["volume"])
                    self.state["pitch_a"] = data.get("pitchA", self.state["pitch_a"])
                    self.state["pitch_b"] = data.get("pitchB", self.state["pitch_b"])
                    self.state["deck_a_playing"] = bool(data.get("deckA", self.state["deck_a_playing"]))
                    self.state["deck_b_playing"] = bool(data.get("deckB", self.state["deck_b_playing"]))
                    self.state["mixer_mode"] = bool(data.get("mix", self.state["mixer_mode"]))
                    self.state["echo_enabled"] = bool(data.get("echo", self.state["echo_enabled"]))
        except queue.Empty:
            pass

    def get_state(self):
        return self.state

# ==========================
# Serial to Teensy
# ==========================
try:
   # Note: integrate.py used /dev/ttyACM0, but print said /dev/ttyACM1. I'll stick to ACM0 but user should verify.
   teensy = serial.Serial('/dev/ttyACM0', 115200, timeout=0.1, write_timeout=0)
   print("✅ Connected to Teensy at /dev/ttyACM0")
except Exception as e:
   print("❌ Could not open serial port:", e)
   teensy = None

last_sent_gesture = None  # Only send when gesture changes

# ==========================
# MediaPipe Setup
# ==========================
current_directory = Path(__file__).parent
MODEL_PATH = current_directory / 'gesture_recognizer.task'
BaseOptions = mp.tasks.BaseOptions
GestureRecognizer = mp.tasks.vision.GestureRecognizer
GestureRecognizerOptions = mp.tasks.vision.GestureRecognizerOptions
RunningMode = mp.tasks.vision.RunningMode
Image = mp.Image

_latest_gesture: Optional[str] = None
processing_frame = False

# ==========================
# Callback from MediaPipe
# ==========================
def print_result(result, output_image, timestamp_ms: int):
   global _latest_gesture, processing_frame, last_sent_gesture

   gesture_code = 0  # default: send nothing (music state unchanged)

   if result and result.gestures:
       gesture = result.gestures[0][0].category_name
       _latest_gesture = gesture

       # ✅ Translate gestures to Teensy command codes
       if gesture == "Open_Palm":
           gesture_code = 1   # PLAY
       elif gesture == "Closed_Fist":
           gesture_code = 2   # PAUSE
       elif gesture == "Victory":
           gesture_code = 3
       elif gesture == "Pointing_Up":
           gesture_code = 4
       elif gesture == "Thumb_Up":
           gesture_code = 5
       elif gesture == "Thumb_Down":
           gesture_code = 6
       else:
           gesture_code = 0   # No actionable gesture
   else:
       _latest_gesture = None
       gesture_code = 0

   # ✅ Only send when gesture CHANGES (prevents USB overload)
   if gesture_code != last_sent_gesture and teensy:
       try:
           teensy.write(bytes([gesture_code]))
           # send_gesture_to_gui(gesture_code) # Replaced by queue if needed, but we rely on state feedback
           last_sent_gesture = gesture_code
           print("Sent gesture:", gesture_code)
       except Exception as e:
           print("Serial write failed:", e)

   processing_frame = False

# ==========================
# Serial Read Loop (New)
# ==========================
def run_serial_read():
    while True:
        if teensy and teensy.is_open:
            try:
                # Use a blocking read with timeout or check in_waiting to avoid busy loop
                if teensy.in_waiting > 0:
                    line = teensy.readline().decode('utf-8').strip()
                    if line.startswith("STATE:"):
                        json_str = line[6:]
                        try:
                            state = json.loads(json_str)
                            gui_queue.put(("STATE", state))
                        except json.JSONDecodeError:
                            pass
            except Exception as e:
                print(f"Serial read error: {e}")
                time.sleep(1)
        time.sleep(0.01)

# ==========================
# Camera Loop (Originally main)
# ==========================
def run_camera_loop():
   global processing_frame

   # Gesture Recognizer setup
   options = GestureRecognizerOptions(
       base_options=BaseOptions(model_asset_path=MODEL_PATH),
       running_mode=RunningMode.LIVE_STREAM,
       result_callback=print_result,
   )
  
   with GestureRecognizer.create_from_options(options) as recognizer:

       # Camera Setup
       picam2 = Picamera2()
       picam2.configure(
           picam2.create_preview_configuration(
               main={"format": "RGB888", "size": (320, 240)}
           )
       )
       picam2.start()

       frame_counter = 0

       try:
           while True:
              
               frame = picam2.capture_array()
               frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
               frame_flipped = cv2.flip(frame, 1)

               frame_counter += 1

               # ✅ Process frames asynchronously
               if frame_counter % 2 == 0 and not processing_frame: # Changed to % 2 for performance
                   processing_frame = True
                   timestamp_ms = int(time.time() * 1000)
                   mp_image = Image(
                       image_format=mp.ImageFormat.SRGB,
                       data=frame_rgb
                   )
                   recognizer.recognize_async(mp_image, timestamp_ms)

               # Display current gesture for debugging
               # Note: cv2.imshow in a thread might be problematic on some systems, 
               # but we'll keep it as requested. If it crashes, user can disable.
               if _latest_gesture:
                   cv2.putText(
                       frame_flipped, _latest_gesture, (10, 30),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2
                   )

               cv2.imshow("Gesture Live", frame_flipped)
               if cv2.waitKey(1) & 0xFF == ord("q"):
                   break
               
               time.sleep(0.01)

       except Exception as e:
           print("Camera loop error:", e)
       finally:
           # cv2.destroyAllWindows()
           picam2.close()

# ==========================
# Main Execution
# ==========================
if __name__ == '__main__':
    # 1. Start Serial Reader Thread
    serial_thread = threading.Thread(target=run_serial_read, daemon=True)
    serial_thread.start()

    # 2. Start Camera Thread
    camera_thread = threading.Thread(target=run_camera_loop, daemon=True)
    camera_thread.start()

    # 3. Start GUI
    print("Starting GUI...")
    hardware = QueueHardware()
    app = GestureAudioApp(hardware_interface=hardware)
    app.mainloop()
