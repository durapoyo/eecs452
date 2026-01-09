import time
import cv2
import mediapipe as mp
from pathlib import Path
from typing import Optional
from picamera2 import Picamera2




current_directory = Path(__file__).parent
MODEL_PATH = current_directory / 'gesture_recognizer.task'


BaseOptions = mp.tasks.BaseOptions
GestureRecognizer = mp.tasks.vision.GestureRecognizer
GestureRecognizerOptions = mp.tasks.vision.GestureRecognizerOptions
RunningMode = mp.tasks.vision.RunningMode
Image = mp.Image


_latest_gesture: Optional[str] = None


def print_result(result, output_image, timestamp_ms: int):
   """Callback for async gesture recognition."""
   global _latest_gesture
   if result and result.gestures:
       gesture = result.gestures[0][0]
       # handedness = result.handedness[0][0].category_name
       _latest_gesture = f" {gesture.category_name} ({gesture.score:.2f})"
   else:
       _latest_gesture = None


def main():
   options = GestureRecognizerOptions(
       base_options=BaseOptions(model_asset_path=MODEL_PATH),
       running_mode=RunningMode.LIVE_STREAM,
       result_callback=print_result,
   )
  
   with GestureRecognizer.create_from_options(options) as recognizer:
       #Picamera2
       picam2 = Picamera2()
       picam2.configure(picam2.create_preview_configuration(
           main={"format": "RGB888", "size": (320, 240)}
       ))
       picam2.start()
       frame_counter =0


       try:
           while True:
               frame = picam2.capture_array()
               # frame_flipped = cv2.flip(frame, 1) 
               frame_counter += 1
               # Async gesture recognition
               if frame_counter % 2:
                   timestamp_ms = int(time.time() * 1000)
                   mp_image = Image(image_format=mp.ImageFormat.SRGB, data=frame)
                   recognizer.recognize_async(mp_image, timestamp_ms)
                   frame_flipped = cv2.flip(frame, 1)
               
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
