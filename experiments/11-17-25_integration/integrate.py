import time
import cv2
import mediapipe as mp
import serial
from pathlib import Path
from typing import Optional
from picamera2 import Picamera2
from gui import run,send_gesture_to_gui


# ==========================
# Serial to Teensy
# ==========================
try:
   teensy = serial.Serial('/dev/ttyACM0', 115200, timeout=0, write_timeout=0)
   print("✅ Connected to Teensy at /dev/ttyACM1")
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
           gesture_code =6
       else:
           gesture_code = 0   # No actionable gesture
   else:
       _latest_gesture = None
       gesture_code = 0


   # ✅ Only send when gesture CHANGES (prevents USB overload)
   if gesture_code != last_sent_gesture and teensy:
       try:
           teensy.write(bytes([gesture_code]))
           send_gesture_to_gui(gesture_code)
           last_sent_gesture = gesture_code
           print("Sent gesture:", gesture_code)
       except Exception as e:
           print("Serial write failed:", e)


   processing_frame = False




# ==========================
# Main Program
# ==========================
def main():
  
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
               if frame_counter % 1 == 0 and not processing_frame:
                   processing_frame = True
                   timestamp_ms = int(time.time() * 1000)
                   mp_image = Image(
                       image_format=mp.ImageFormat.SRGB,
                       data=frame_rgb
                   )
                   recognizer.recognize_async(mp_image, timestamp_ms)


               # Display current gesture for debugging
               if _latest_gesture:
                   cv2.putText(
                       frame_flipped, _latest_gesture, (10, 30),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2
                   )


               cv2.imshow("Gesture Live", frame_flipped)
               if cv2.waitKey(1) & 0xFF == ord("q"):
                   break


       finally:
           cv2.destroyAllWindows()
           picam2.close()




if __name__ == '__main__':
   main()












