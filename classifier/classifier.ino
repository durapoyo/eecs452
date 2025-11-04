/*
  This is the working .ino file for the main logic of the Teensy.
*/


#include "AudioControl.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

uint16_t BNO055_SAMPLERATE_DELAY_MS = 10; // 100 Hz target

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);


void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("BNO055 Angular Acceleration Test (Teensy 4.1)");

  // Teensy-specific I2C tuning
  Wire.setClock(400000); // 400 kHz stable for BNO055
  if (!bno.begin()) {
    Serial.println("No BNO055 detected! Check wiring/I2C address.");
    while (1);
  }

  delay(1000);
  bno.setExtCrystalUse(true);
}

// Moving average filter
double smooth(double newValue, double oldValue, double alpha = 0.7) {
  return alpha * oldValue + (1 - alpha) * newValue;
}

void checkAngularAcceleration(sensors_event_t* gyroData, unsigned long currentTime) {
  static double lastRollRate = 0.0;
  static double lastPitchRate = 0.0;
  static double lastYawRate = 0.0;
  static unsigned long lastTime = 0;

  if (lastTime == 0) {
    lastTime = currentTime;
    lastRollRate = gyroData->gyro.x;
    lastPitchRate = gyroData->gyro.y;
    lastYawRate = gyroData->gyro.z;
    return;
  }

  double deltaTime = (currentTime - lastTime) / 1000.0;
  if (deltaTime <= 0.0) return;  // avoid divide-by-zero

  // Compute angular accelerations
  double rollAccel  = (gyroData->gyro.x - lastRollRate) / deltaTime;
  double pitchAccel = (gyroData->gyro.y - lastPitchRate) / deltaTime;
  double yawAccel   = (gyroData->gyro.z - lastYawRate) / deltaTime;

  // Smooth out high-frequency jitter
  static double sRoll = 0, sPitch = 0, sYaw = 0;
  sRoll = smooth(rollAccel, sRoll);
  sPitch = smooth(pitchAccel, sPitch);
  sYaw = smooth(yawAccel, sYaw);

  // Adjusted threshold for realistic motion
  double threshold = 100; // rad/s²

  if (sRoll > threshold)
    Serial.printf("Rolling RIGHT (%.2f rad/s²)\n", sRoll);
  else if (sRoll < -threshold)
    Serial.printf("Rolling LEFT (%.2f rad/s²)\n", sRoll);

  if (sPitch > threshold)
    Serial.printf("Pitching UP (%.2f rad/s²)\n", sPitch);
  else if (sPitch < -threshold)
    Serial.printf("Pitching DOWN (%.2f rad/s²)\n", sPitch);

  if (sYaw > threshold)
    Serial.printf("Yaw accelerating RIGHT (%.2f rad/s²)\n", sYaw);
  else if (sYaw < -threshold)
    Serial.printf("Yaw accelerating LEFT (%.2f rad/s²)\n", sYaw);

  lastRollRate = gyroData->gyro.x;
  lastPitchRate = gyroData->gyro.y;
  lastYawRate = gyroData->gyro.z;
  lastTime = currentTime;
}

void classifyRotation(sensors_event_t* gyroData, unsigned long currentTime) {
  static String rollState = "IDLE";
  static unsigned long lastTransition = 0;

  float rollRate = gyroData->gyro.x;   // rad/s
  float pitchRate = gyroData->gyro.y;
  float yawRate = gyroData->gyro.z;

  // Hysteresis thresholds
  const float startThresh = 3.0; // rad/s, start gesture
  const float stopThresh = 0.8;  // rad/s, stop gesture
  const unsigned long debounce = 500; // ms

  // --- Roll gesture detection ---
  if (rollState == "IDLE") {
    if (rollRate > startThresh) {
      rollState = "ROLL_RIGHT";
      lastTransition = currentTime;
      Serial.println("Roll Right");
    } else if (rollRate < -startThresh) {
      rollState = "ROLL_LEFT";
      lastTransition = currentTime;
      Serial.println("Roll Left");
    }
  } else if (rollState == "ROLL_RIGHT") {
    if (fabs(rollRate) < stopThresh && currentTime - lastTransition > debounce) {
      rollState = "IDLE";
    }
  } else if (rollState == "ROLL_LEFT") {
    if (fabs(rollRate) < stopThresh && currentTime - lastTransition > debounce) {
      rollState = "IDLE";
    }
  }

}

void classifyGesture(sensors_event_t* gyroData, unsigned long currentTime) {
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
      Serial.println("🎵 Increase TEMPO 🎵");
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
      Serial.println("🎵 Decrease TEMPO 🎵");
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
      Serial.println("🎵 Increase VOLUME 🎵");
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
      Serial.println("🎵 Decrease VOLUME 🎵");
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



void loop(void) {
  sensors_event_t gyroData;
  bno.getEvent(&gyroData, Adafruit_BNO055::VECTOR_GYROSCOPE);

  unsigned long now = millis();
  // classifyRotation(&gyroData, now);
  classifyGesture(&gyroData, now);
  delay(BNO055_SAMPLERATE_DELAY_MS);
}
