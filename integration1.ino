#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

#include <Audio.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// =====================
// Audio System
// =====================
AudioPlaySdWav           playSdWav1;
AudioEffectGranular      granular1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, granular1, 0);
AudioConnection          patchCord2(granular1, 0, i2s1, 0);
AudioConnection          patchCord3(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;

// Teensy 4.1 built-in SD
#define SDCARD_CS_PIN BUILTIN_SDCARD

float volumeLevel = 0.5;
float pitchShift  = 1.0;
const int GRAIN_MEMORY = 12800;
int16_t granularMemory[GRAIN_MEMORY];

uint16_t BNO055_SAMPLERATE_DELAY_MS = 100;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire2);

bool isPlaying = false;

// =====================
// Setup
// =====================
void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Teensy IMU-Audio Controller");

  if (!bno.begin()) {
    Serial.println("No BNO055 detected!");
    while (1);
  }

  AudioMemory(8);
  sgtl5000_1.enable();
  sgtl5000_1.volume(volumeLevel);
  granular1.begin(granularMemory, GRAIN_MEMORY);
  granular1.setSpeed(pitchShift);

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("SD init failed!");
    while (1);
  }

  Serial.println("Setup complete. Waiting for RPi gestures...");
}

// =====================
// IMU-based controls
// =====================
void controlVolume(sensors_event_t* linearData) {
  float y_accel = linearData->acceleration.y;

  if (y_accel > 5) {
    volumeLevel += 0.05;
    if (volumeLevel > 1.0) volumeLevel = 1.0;
    sgtl5000_1.volume(volumeLevel);
    Serial.print("Volume up: ");
    Serial.println(volumeLevel, 2);
    delay(300);
  } else if (y_accel < -5) {
    volumeLevel -= 0.05;
    if (volumeLevel < 0.0) volumeLevel = 0.0;
    sgtl5000_1.volume(volumeLevel);
    Serial.print("Volume down: ");
    Serial.println(volumeLevel, 2);
    delay(300);
  }
}

void controlPitch(sensors_event_t* gyroData) {
  float pitchRate = gyroData->gyro.y;

  if (pitchRate > 5) {
    pitchShift += 0.5;
    if (pitchShift > 4.0) pitchShift = 4.0;
    granular1.setSpeed(pitchShift);
    Serial.print("Pitch up: ");
    Serial.println(pitchShift, 2);
    delay(300);
  } else if (pitchRate < -5) {
    pitchShift -= 0.5;
    if (pitchShift < 0.25) pitchShift = 0.25;
    granular1.setSpeed(pitchShift);
    Serial.print("Pitch down: ");
    Serial.println(pitchShift, 2);
    delay(300);
  }
}

// =====================
// Main Loop
// =====================
void loop() {
  unsigned long now = millis();

  // --- 1. Listen for RPi gesture commands ---
  if (Serial.available()) {
    int gesture = Serial.read();

    switch (gesture) {
      case 1:  // Open palm → Play
        if (!isPlaying) {
          playSdWav1.play("test.wav");
          granular1.beginPitchShift(20);
          isPlaying = true;
          Serial.println("Music playing (Open palm)");
        }
        break;

      case 2:  // Closed fist → Pause
        if (isPlaying) {
          playSdWav1.stop();
          isPlaying = false;
          Serial.println("Music paused (Closed fist)");
        }
        break;
    }
  }

  // --- 2. IMU-based control for volume and pitch ---
  sensors_event_t lin, gyro;
  bno.getEvent(&lin, Adafruit_BNO055::VECTOR_LINEARACCEL);
  bno.getEvent(&gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);

  if (isPlaying) {
    controlVolume(&lin);
    controlPitch(&gyro);
  }

  delay(BNO055_SAMPLERATE_DELAY_MS);
}
