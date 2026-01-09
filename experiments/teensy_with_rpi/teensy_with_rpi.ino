#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

#include <Audio.h>
#include <SPI.h>
#include <SD.h>

// ---------- AUDIO OBJECTS ----------
AudioPlaySdWav       playSdWav1;
AudioOutputI2S       i2s1;
AudioConnection      patchCord1(playSdWav1, 0, i2s1, 0);
AudioConnection      patchCord2(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000 sgtl5000;

// ---------- VARIABLES ----------
bool isPlaying = false;
float volumeLevel = 0.5;

// ---------- IMU ----------
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire2);
bool imuConnected = false;
const uint16_t BNO055_DELAY = 100;

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Teensy Gesture + Optional IMU Volume Controller");

  // Initialize IMU
  if (bno.begin()) {
    imuConnected = true;
    Serial.println("BNO055 IMU detected.");
  } else {
    imuConnected = false;
    Serial.println("No BNO055 IMU detected, volume control disabled.");
  }

  // Audio init
  AudioMemory(8);
  sgtl5000.enable();
  sgtl5000.volume(volumeLevel);

  // SD card init
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD Card init FAILED!");
    while (1);
  }

  Serial.println("Setup complete. Waiting for gestures...");
}

// ---------- IMU VOLUME CONTROL ----------
void controlVolume() {
  if (!imuConnected) return;  // skip if no IMU

  sensors_event_t lin;
  bno.getEvent(&lin, Adafruit_BNO055::VECTOR_LINEARACCEL);
  float y = lin.acceleration.y;

  if (y > 5.0) {
    volumeLevel += 0.05;
    if (volumeLevel > 1.0) volumeLevel = 1.0;
    sgtl5000.volume(volumeLevel);
    Serial.print("Volume UP: ");
    Serial.println(volumeLevel, 2);
    delay(300);
  } else if (y < -5.0) {
    volumeLevel -= 0.05;
    if (volumeLevel < 0.0) volumeLevel = 0.0;
    sgtl5000.volume(volumeLevel);
    Serial.print("Volume DOWN: ");
    Serial.println(volumeLevel, 2);
    delay(300);
  }
}

// ---------- MAIN LOOP ----------
void loop() {
  // --- Gesture control ---
  if (Serial.available()) {
    int gesture = Serial.read();
    Serial.print("Received gesture: ");
    Serial.println(gesture);

    switch (gesture) {
      case 1:  // Open Palm → Play
        if (!isPlaying) {
          playSdWav1.play("test.wav"); // Ensure test.wav exists on SD
          isPlaying = true;
          Serial.println("Music PLAY");
        }
        break;

      case 2:  // Closed Fist → Stop
        if (isPlaying) {
          playSdWav1.stop();
          isPlaying = false;
          Serial.println("Music STOP");
        }
        break;

      default:
        // Ignore other values
        break;
    }
  }

  // --- IMU Volume control ---
  if (isPlaying && imuConnected) {
    controlVolume();
  }

  delay(BNO055_DELAY);
}



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

// ----------------- HELPER FUNCTIONS -----------------
int linearGestures(sensors_event_t* linearData, unsigned long currentTime) {
  float threshold = 6.0; // m/s²
  float x = linearData->acceleration.x;
  float y = linearData->acceleration.y;
  float z = linearData->acceleration.z;

  int xa = (x > threshold) ? 1 : (x < -threshold ? -1 : 0);
  int ya = (y > threshold) ? 1 : (y < -threshold ? -1 : 0);
  int za = (z > threshold) ? 1 : (z < -threshold ? -1 : 0);

  if (xa != 0 && ya != 0) (fabs(x) > fabs(y)) ? ya = 0 : xa = 0;
  if (xa != 0 && za != 0) (fabs(x) > fabs(z)) ? za = 0 : xa = 0;
  if (ya != 0 && za != 0) (fabs(y) > fabs(z)) ? za = 0 : ya = 0;

  // Volume control with y-axis
  if (ya > 0) {
    volumeLevel += 0.05;
    if (volumeLevel > 1.0) volumeLevel = 1.0;
    sgtl5000.volume(volumeLevel);
    Serial.print("Volume up: ");
    Serial.println(volumeLevel, 2);
  } else if (ya < 0) {
    volumeLevel -= 0.05;
    if (volumeLevel < 0.0) volumeLevel = 0.0;
    sgtl5000.volume(volumeLevel);
    Serial.print("Volume down: ");
    Serial.println(volumeLevel, 2);
  }

  return (xa || ya || za);
}

void gyroGestures(sensors_event_t* gyroData, unsigned long currentTime) {
  static String rollState = "IDLE";
  static unsigned long lastGestureTime = 0;

  float rollRate  = gyroData->gyro.x;
  float pitchRate = gyroData->gyro.y;
  float yawRate   = gyroData->gyro.z;

  const float smallPrepThresh = 4.5;
  const float bigPushBase = 7.0;
  const float stopThresh = 0.3;
  const unsigned long debounce = 800;
  const unsigned long comboWindow = 600;

  if (rollState == "IDLE") {
    if (pitchRate < -smallPrepThresh) {
      rollState = "PREP_DOWN";
      lastGestureTime = currentTime;
    } else if (pitchRate > smallPrepThresh) {
      rollState = "PREP_UP";
      lastGestureTime = currentTime;
    }
  } else if (rollState == "PREP_DOWN") {
    if (pitchRate > bigPushBase && (currentTime - lastGestureTime < comboWindow)) {
      pitchShift += 1;
      if (pitchShift > 4.0) pitchShift = 4.0;
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch up: ");
      Serial.println(pitchShift, 2);
      rollState = "IDLE";
    }
  } else if (rollState == "PREP_UP") {
    if (pitchRate < -bigPushBase && (currentTime - lastGestureTime < comboWindow)) {
      pitchShift -= 1;
      if (pitchShift < 0.25) pitchShift = 0.25;
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch down: ");
      Serial.println(pitchShift, 2);
      rollState = "IDLE";
    }
  }

  // Reset
  if (fabs(pitchRate) < stopThresh && (currentTime - lastGestureTime > debounce)) {
    if (rollState != "IDLE") rollState = "IDLE";
  }
}

void handleGestures(sensors_event_t* lin, sensors_event_t* gyro, unsigned long now) {
  switch (currentMode) {
    case IDLE: {
      float rollRate  = gyro->gyro.x;
      float pitchRate = gyro->gyro.y;
      float yawRate   = gyro->gyro.z;
      float gyroMag   = sqrt(rollRate*rollRate + pitchRate*pitchRate + yawRate*yawRate);

      float accelMag = sqrt(
        lin->acceleration.x * lin->acceleration.x +
        lin->acceleration.y * lin->acceleration.y +
        lin->acceleration.z * lin->acceleration.z);

      const float gyroThreshold = 3.5;
      const float accelThreshold = 4.0;

      if (gyroMag > gyroThreshold) {
        currentMode = GYRO_ACTIVE;
        gestureStartTime = now;
        Serial.println("[STATE] Gyro gesture started");
      } else if (accelMag > accelThreshold) {
        currentMode = LINEAR_ACTIVE;
        gestureStartTime = now;
        Serial.println("[STATE] Linear gesture started");
      }
      break;
    }

    case LINEAR_ACTIVE:
      linearGestures(lin, now);
      if (now - gestureStartTime > gestureDuration) {
        currentMode = IDLE;
        Serial.println("[STATE] Back to IDLE");
      }
      break;

    case GYRO_ACTIVE:
      gyroGestures(gyro, now);
      if (now - gestureStartTime > gestureDuration) {
        currentMode = IDLE;
        Serial.println("[STATE] Back to IDLE");
      }
      break;
  }
}

// ----------------- MAIN LOOP -----------------
void loop() {
  unsigned long now = millis();

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
        break;

      case 2: // Closed fist → Stop
        if (isPlaying) {
          playSdWav1.stop();
          isPlaying = false;
          Serial.println("Music STOP");
        }
        break;

      default:
        break;
    }
  }

  // --- 2. IMU gesture control (if connected) ---
  if (imuConnected && isPlaying) {
    sensors_event_t lin, gyro;
    bno.getEvent(&lin, Adafruit_BNO055::VECTOR_LINEARACCEL);
    bno.getEvent(&gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);
    handleGestures(&lin, &gyro, now);
  }

  delay(BNO055_DELAY);
}


