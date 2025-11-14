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
float pitchShift = 1.0;
const int GRAIN_MEMORY = 12800;
int16_t granularMemory[GRAIN_MEMORY];

// ----------------- IMU -----------------
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire2);
bool imuConnected = false;
const uint16_t BNO055_DELAY = 100;

enum GestureMode { IDLE, LINEAR_ACTIVE, GYRO_ACTIVE };
GestureMode currentMode = IDLE;

unsigned long gestureStartTime = 0;
const unsigned long gestureDuration = 500; // ms lockout

// ----------------- SETUP -----------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Teensy Audio + IMU Gesture Controller");

  if (bno.begin()) {
    imuConnected = true;
    Serial.println("BNO055 IMU detected.");
  } else {
    imuConnected = false;
    Serial.println("No BNO055 detected. IMU disabled.");
  }

  AudioMemory(8);
  sgtl5000.enable();
  sgtl5000.volume(volumeLevel);
  granular1.begin(granularMemory, GRAIN_MEMORY);
  granular1.setSpeed(pitchShift);

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("SD Card init FAILED!");
    while (1);
  }

  Serial.println("Setup complete. Waiting for RPi gestures...");
}

double smooth(double newValue, double oldValue, double alpha = 0.7) {
  return alpha * oldValue + (1 - alpha) * newValue;
}

// ----------------- LINEAR GESTURES -----------------
int linearGestures(sensors_event_t* linearData, unsigned long currentTime) {
  float threshold = 6;
  float x_accel = linearData->acceleration.x;
  float y_accel = linearData->acceleration.y;
  float z_accel = linearData->acceleration.z;

  int xa = 0, ya = 0, za = 0;

  if (x_accel > threshold) xa = 1;
  if (x_accel < -threshold) xa = -1;
  if (y_accel > threshold) ya = 1;
  if (y_accel < -threshold) ya = -1;
  if (z_accel > threshold) za = 1;
  if (z_accel < -threshold) za = -1;

  if (xa != 0 && ya != 0)
    (fabs(x_accel) > fabs(y_accel)) ? ya = 0 : xa = 0;

  if (xa != 0 && za != 0)
    (fabs(x_accel) > fabs(z_accel)) ? za = 0 : xa = 0;

  if (za != 0 && ya != 0)
    (fabs(z_accel) > fabs(y_accel)) ? ya = 0 : za = 0;

  if (xa > 0) { Serial.println("x accel positive"); return 1; }
  if (xa < 0) { Serial.println("x accel negative"); return 1; }
  if (ya > 0) { Serial.println("y accel positive"); return 1; }
  if (ya < 0) { Serial.println("y accel negative"); return 1; }
  if (za > 0) { Serial.println("z accel positive"); return 1; }
  if (za < 0) { Serial.println("z accel negative"); return 1; }

  return 0;
}

// ----------------- GYRO GESTURES -----------------
void gyroGestures(sensors_event_t* gyroData, unsigned long currentTime) {
  static String rollState = "IDLE";
  static unsigned long lastGestureTime = 0;

  float rollRate = gyroData->gyro.x;
  float pitchRate = gyroData->gyro.y;
  float yawRate = gyroData->gyro.z;

  const bool rightHanded = true;
  const float smallPrepThresh = 4.5;
  const float bigPushBase = 7.0;
  const float stopThresh = 0.3;
  const unsigned long debounce = 800;
  const unsigned long comboWindow = 600;

  float bigPushRight = bigPushBase;
  float bigPushLeft = bigPushBase;

  if (rightHanded) bigPushLeft *= 0.75;
  else bigPushRight *= 0.75;

  if (rollState == "IDLE") {
    if (rollRate < -smallPrepThresh) {
      rollState = "PREP_LEFT"; 
      lastGestureTime = currentTime;
    } else if (rollRate > smallPrepThresh) {
      rollState = "PREP_RIGHT"; 
      lastGestureTime = currentTime;
    } else if (pitchRate < -smallPrepThresh) {
      rollState = "PREP_DOWN"; 
      lastGestureTime = currentTime;
    } else if (pitchRate > smallPrepThresh) {
      rollState = "PREP_UP"; 
      lastGestureTime = currentTime;
    }
  }

  else if (rollState == "PREP_LEFT") {
    if (rollRate > bigPushRight && (currentTime - lastGestureTime < comboWindow)) {
      volumeLevel = min(1.0f, volumeLevel + 0.05);
      sgtl5000.volume(volumeLevel);
      Serial.print("Volume up: "); Serial.println(volumeLevel);
      rollState = "IDLE";
      lastGestureTime = currentTime;
    } else if (currentTime - lastGestureTime > comboWindow)
      rollState = "IDLE";
  }

  else if (rollState == "PREP_RIGHT") {
    if (rollRate < -bigPushLeft && (currentTime - lastGestureTime < comboWindow)) {
      volumeLevel = max(0.0f, volumeLevel - 0.05);
      sgtl5000.volume(volumeLevel);
      Serial.print("Volume down: "); Serial.println(volumeLevel);
      rollState = "IDLE";
      lastGestureTime = currentTime;
    } else if (currentTime - lastGestureTime > comboWindow)
      rollState = "IDLE";
  }

  else if (rollState == "PREP_DOWN") {
    if (pitchRate > bigPushRight && (currentTime - lastGestureTime < comboWindow)) {
      pitchShift = min(4.0f, pitchShift + 1.0f);
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch up: "); Serial.println(pitchShift);
      rollState = "IDLE";
      lastGestureTime = currentTime;
    } else if (currentTime - lastGestureTime > comboWindow)
      rollState = "IDLE";
  }

  else if (rollState == "PREP_UP") {
    if (pitchRate < -bigPushLeft && (currentTime - lastGestureTime < comboWindow)) {
      pitchShift = max(0.25f, pitchShift - 1.0f);
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch down: "); Serial.println(pitchShift);
      rollState = "IDLE";
      lastGestureTime = currentTime;
    } else if (currentTime - lastGestureTime > comboWindow)
      rollState = "IDLE";
  }

  if (fabs(rollRate) < stopThresh && (currentTime - lastGestureTime > debounce))
    rollState = "IDLE";
}

// ----------------- MAIN LOOP -----------------
void loop() {
  unsigned long now = millis();

  // --- RPi gestures ---
  if (Serial.available()) {
    int gesture = Serial.read();
    Serial.print("Received gesture: ");
    Serial.println(gesture);

    switch (gesture) {
      case 1:
        if (!isPlaying) {
          playSdWav1.play("stars.wav");
          granular1.beginPitchShift(20);
          isPlaying = true;
          Serial.println("Music PLAY");
        }
        break;
      case 2:
        if (isPlaying) {
          playSdWav1.stop();
          isPlaying = false;
          Serial.println("Music STOP");
        }
        break;
      case 3:
        // equalizer
        break;
      case 4:
        // skip song
        break;
      case 5:
        // play two songs
        break;
    }
  }

  // --- IMU Gestures ---
  if (imuConnected && isPlaying) {
    sensors_event_t lin, gyro;
    bno.getEvent(&lin, Adafruit_BNO055::VECTOR_LINEARACCEL);
    bno.getEvent(&gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);

    float rollRate = gyro.gyro.x;
    float pitchRate = gyro.gyro.y;
    float yawRate = gyro.gyro.z;
    float s = 1.5;

    if (fabs(rollRate) < s && fabs(pitchRate) < s && fabs(yawRate) < s) {
      if (linearGestures(&lin, now) != 0) delay(500);
    } else {
      gyroGestures(&gyro, now);
    }
  }

  delay(BNO055_DELAY);
}
