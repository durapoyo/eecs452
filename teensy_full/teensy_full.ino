#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

#include <Audio.h>
#include <SPI.h>
#include <SD.h>

// ----------------- AUDIO SYSTEM -----------------
AudioPlaySdWav       playSdWav1;
AudioEffectGranular  granular1;
AudioOutputI2S       i2s1;
AudioConnection      patchCord1(playSdWav1, 0, granular1, 0);
AudioConnection      patchCord2(granular1, 0, i2s1, 0);
AudioConnection      patchCord3(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000 sgtl5000;

#define SDCARD_CS_PIN BUILTIN_SDCARD


// ----------------- GLOBAL VARIABLES -----------------
bool isPlaying = false;
float volumeLevel = 0.5;
float pitchShift  = 1.0;
float echoLevel = 1.0;
float reverbLevel = 1.0;
float baseLevel = 1.0;
float midLevel = 1.0;
float trebleLevel = 1.0;
const int GRAIN_MEMORY = 12800;
int16_t granularMemory[GRAIN_MEMORY];


// ----------------- IMU -----------------
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire2);
bool imuConnected = false;
const uint16_t BNO055_DELAY = 100;

// Gesture state machine
enum GestureMode { IDLE, LINEAR_ACTIVE, GYRO_ACTIVE };
GestureMode currentMode = IDLE;

unsigned long gestureStartTime = 0;
const unsigned long gestureDuration = 500; // ms lockout


// ----------------- SETUP -----------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Teensy Audio + IMU Gesture Controller");

  // --- IMU INIT ---
  if (bno.begin()) {
    imuConnected = true;
    Serial.println("BNO055 IMU detected.");
  } else {
    imuConnected = false;
    Serial.println("No BNO055 detected. IMU disabled.");
  }

  // --- AUDIO INIT ---
  AudioMemory(8);
  sgtl5000.enable();
  sgtl5000.volume(volumeLevel);
  granular1.begin(granularMemory, GRAIN_MEMORY);
  granular1.setSpeed(pitchShift);

  // --- SD CARD INIT ---
  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("SD Card init FAILED!");
    while (1);
  }

  Serial.println("Setup complete. Waiting for RPi gestures...");
}

double smooth(double newValue, double oldValue, double alpha = 0.7) {
  return alpha * oldValue + (1 - alpha) * newValue;
}


int linearGestures(sensors_event_t* linearData, int caseNum, unsigned long currentTime)
{
  // gen vals
  float threshold = 6; // m/s^2

  float x_accel = linearData->acceleration.x;
  float y_accel = linearData->acceleration.y;
  float z_accel = linearData->acceleration.z;
  int xa = 0;
  int ya = 0;
  int za = 0;

  // classify movement
  if (x_accel > threshold) xa = 1;
  if (x_accel < -threshold) xa = -1;
  if (y_accel > threshold) ya = 1;
  if (y_accel < -threshold) ya = -1;
  if (z_accel > threshold) za = 1;
  if (z_accel < -threshold) za = -1;

  // "decision tree" if u can even call it that, to detect multiple motions
  if(xa != 0 && ya != 0)
  {
    if(fabs(x_accel) > fabs(y_accel)) ya = 0;
    else  xa = 0;
  }
  if(xa != 0 && za != 0)
  {
    if(fabs(x_accel) > fabs(z_accel)) za = 0;
    else  xa = 0;
  }
  if(za != 0 && ya != 0)
  {
    if(fabs(z_accel) > fabs(y_accel)) ya = 0;
    else  za = 0;
  }

  // make sure not equalizer
  if(caseNum != 3)
  {
      if(xa > 0)
    {
      // pitchShift -= 1;
      //   if (pitchShift < 0.25) pitchShift = 0.25;
      //   granular1.setSpeed(pitchShift);
      //   Serial.print("Pitch down: ");
      //   Serial.println(pitchShift, 2);
      echoLevel++;
      if(echoLevel > 4) echoLevel = 4;  print("Max echo level reached. ");
      Serial.print("Echo level: ");
      // granular1.setEcho(echoLevel);
      Serial.println(echoLevel);
      return 1;
    }
    else if(xa < 0)
    {
      echoLevel--;
      if(echoLevel < 0) echoLevel = 0;  print("Min echo level reached. ");
      Serial.print("Echo level: ");
      // granular1.setEcho(echoLevel);
      Serial.println(echoLevel);
      return 1;
    }
  } // when not equalizing

  else
  {
    if(xa > 0)
    {
      trebleLevel++;
      if(trebleLevel > 4) trebleLevel = 4;  print("Max treble level reached. ");
      Serial.print("Treble level: ");
      // granular1.setEcho(echoLevel);
      Serial.println(trebleLevel);
      return 1;
    }
    else if(xa < 0)
    {
      trebleLevel--;
      if(trebleLevel < 0) trebleLevel = 0;  print("Min treble level reached. ");
      Serial.print("Treble level: ");
      // granular1.setEcho(echoLevel);
      Serial.println(trebleLevel);
      return 1;
    }


  } // hwen equalizing
 
  else if(ya > 0)
  {
    reverbLevel++;
    if(reverbLevel > 4) reverbLevel = 4;  print("Max reverb level reached. ");
    Serial.print("Reverb level: ");
    // granular1.setReverb(reverbLevel);
    Serial.println(reverbLevel);
    return 1;
  }
  else if(ya < 0)
  {
    reverbLevel--;
    if(reverbLevel < 0) reverbLevel = 0;  print("Min reverb level reached. ");
    Serial.print("Reverb level: ");
    // granular1.setReverb(reverbLevel);
    Serial.println(reverbLevel);
    return 1;
  }
  else if(za > 0)
  {
    Serial.println("z accel positive");
    return 1;
  }
  else if(za < 0)
  {
    Serial.println("z accel negative");
    return 1;
  }
  // bros got no motion
  else  return 0;
}

void gyroGestures(sensors_event_t* gyroData, unsigned long currentTime) {
  static String rollState = "IDLE";
  static unsigned long lastGestureTime = 0;
  float rollRate = gyroData->gyro.x;  // rad/s
  float pitchRate = gyroData->gyro.y;
  float yawRate = gyroData->gyro.z;

  // --- User preference ---
  const bool rightHanded = true;  // change to false for left-handed users
  
  // --- Base parameters ---
  const float smallPrepThresh = 4.5;    // ambik lajak
  const float bigPushBase     = 7.0;    // base strong push
  const float stopThresh      = 0.3;
  const unsigned long debounce = 800;
  const unsigned long comboWindow = 600;

  // --- Adjust thresholds based on handedness ---
  float bigPushRight = bigPushBase;        // strong rightward flick
  float bigPushLeft  = bigPushBase;

  if (rightHanded) {
    bigPushLeft *= 0.75;   // easier to detect weaker left flick
  } else {
    bigPushRight *= 0.75;  // easier for left-handed users' weaker right flick
  }

  // --- State Machine ---
  if (rollState == "IDLE") {
    if (rollRate < -smallPrepThresh) {
      rollState = "PREP_LEFT";
      lastGestureTime = currentTime;
      //Serial.println("Prep LEFT");
    }
    else if (rollRate > smallPrepThresh) {
      rollState = "PREP_RIGHT";
      lastGestureTime = currentTime;
      //Serial.println("Prep RIGHT");
    }
    if (pitchRate < -smallPrepThresh) {
      rollState = "PREP_DOWN";
      lastGestureTime = currentTime;
      //Serial.println("Prep DOWN");
    }
    else if (pitchRate > smallPrepThresh) {
      rollState = "PREP_UP";
      lastGestureTime = currentTime;
      //Serial.println("Prep UP");
    }
  }

  else if (rollState == "PREP_LEFT") {
    // wait for follow-up strong RIGHT roll
    if (rollRate > bigPushRight && (currentTime - lastGestureTime < comboWindow)) {
      // Serial.println("🎵 Increase TEMPO 🎵");
      volumeLevel += 0.05;
      if (volumeLevel > 1.0) volumeLevel = 1.0;
      sgtl5000_1.volume(volumeLevel);
      Serial.print("Volume up: ");
      Serial.println(volumeLevel, 2);
     
      rollState = "IDLE";
      lastGestureTime = currentTime;
    }
    else if (currentTime - lastGestureTime > comboWindow) {
      rollState = "IDLE";
    }
  }

  else if (rollState == "PREP_RIGHT") {
    // wait for follow-up strong LEFT roll
    if (rollRate < -bigPushLeft && (currentTime - lastGestureTime < comboWindow)) {
      // Serial.println("🎵 Decrease TEMPO 🎵");
      volumeLevel -= 0.05;
      if (volumeLevel < 0.0) volumeLevel = 0.0;
      sgtl5000_1.volume(volumeLevel);
      Serial.print("Volume down: ");
      Serial.println(volumeLevel, 2);
      rollState = "IDLE";
      lastGestureTime = currentTime;
    }
    else if (currentTime - lastGestureTime > comboWindow) {
      rollState = "IDLE";
    }
  }
 
  else if (rollState == "PREP_DOWN") {
    // wait for follow-up strong RIGHT roll
    if (pitchRate > bigPushRight && (currentTime - lastGestureTime < comboWindow)) {
      //Serial.println("🎵 Increase VOLUME 🎵");
      pitchShift += 1;
      if (pitchShift > 4.0) pitchShift = 4.0;
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch up: ");
      Serial.println(pitchShift, 2);

      rollState = "IDLE";
      lastGestureTime = currentTime;
    }
    else if (currentTime - lastGestureTime > comboWindow) {
      rollState = "IDLE";
    }
  }

  else if (rollState == "PREP_UP") {
    // wait for follow-up strong LEFT roll
    if (pitchRate < -bigPushLeft && (currentTime - lastGestureTime < comboWindow)) {
      //Serial.println("🎵 Decrease VOLUME 🎵");
      pitchShift -= 1;
      if (pitchShift < 0.25) pitchShift = 0.25;
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch down: ");
      Serial.println(pitchShift, 2);

      rollState = "IDLE";
      lastGestureTime = currentTime;
    }
    else if (currentTime - lastGestureTime > comboWindow) {
      rollState = "IDLE";
    }
  }

  // --- Deadband reset ---
  if (fabs(rollRate) < stopThresh && (currentTime - lastGestureTime > debounce)) {
    if (rollState != "IDLE") rollState = "IDLE";
  }

}

// ----------------- MAIN LOOP -----------------
void loop() {
  unsigned long now = millis();
 
  int caseNum = 0;
  
  // --- 1. RPi gesture input (open palm / closed fist) ---
  if (Serial.available()) {
    int gesture = Serial.read();
    Serial.print("Received gesture: ");
    Serial.println(gesture);

    switch (gesture) {
      case 1: // Open palm → Play
        if (!isPlaying) {
          playSdWav1.play("test.wav");
          granular1.beginPitchShift(20);
          isPlaying = true;
          Serial.println("Music PLAY");
        }
        caseNum = 1;
        break;

      case 2: // Closed fist → Stop
        if (isPlaying) {
          playSdWav1.stop();
          isPlaying = false;
          Serial.println("Music STOP");
        }
        caseNum = 2;
        break;

      case 3: // peace sign to equalizer
        // equalizer shit
        caseNum = 3;
        break;
      
      case 4: // skip when pointing up  qq
        Serial.println("Skipping song.");
        caseNum = 4;
        break;

      case 5: // play 2 songs at once
        caseNum = 5;
        break;
      
      default:
        break;
    }
  }
  
  // --- 2. IMU gesture control (if connected) ---
  if (imuConnected && isPlaying) {
    
  // lin accel
  sensors_event_t lin;
  sensors_event_t gyro;
  bno.getEvent(&lin, Adafruit_BNO055::VECTOR_LINEARACCEL);
  bno.getEvent(&gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);

  // if gesture detected, wait
  float rollRate = gyro.gyro.x;
  float pitchRate = gyro.gyro.y;
  float yawRate = gyro.gyro.z;

  float s = 1.5;
    
  if (fabs(rollRate) < s && fabs(pitchRate) < s && fabs(yawRate) < s) {
    if(linearGestures(&lin, caseNum, now) != 0) delay(500);
  }
  else {
    gyroGestures(&gyro, now);
  }
  }

  delay(BNO055_DELAY);
}
