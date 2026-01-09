#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// === AUDIO OBJECTS ===
AudioPlaySdWav           playSdWav1;
AudioEffectGranular      granularLeft;
AudioEffectGranular      granularRight;
AudioOutputI2S           i2s1;

// Patch: WAV L→granularLeft→L-out, WAV R→granularRight→R-out
AudioConnection          patchCord1(playSdWav1, 0, granularLeft, 0);
AudioConnection          patchCord2(playSdWav1, 1, granularRight, 0);
AudioConnection          patchCord3(granularLeft, 0, i2s1, 0);
AudioConnection          patchCord4(granularRight, 0, i2s1, 1);

AudioControlSGTL5000     sgtl5000_1;

#define SDCARD_CS_PIN BUILTIN_SDCARD
#define GRAIN_MEMORY 12800

// === GLOBAL VARIABLES ===
int16_t granularMemoryL[GRAIN_MEMORY];
int16_t granularMemoryR[GRAIN_MEMORY];

float volumeLevel = 0.6;
float pitchShift = 1.0;   // shared pitch for both channels

// === SETUP ===
void setup() {
  Serial.begin(9600);
  while (!Serial); // Wait for serial monitor
  
  Serial.println("=== Stereo Pitch Shifter ===");
  Serial.println("Commands:");
  Serial.println("  p : Play 'sound2.wav'");
  Serial.println("  s : Stop playback");
  Serial.println("  + : Increase pitch");
  Serial.println("  - : Decrease pitch");
  Serial.println("------------------------------------");

  AudioMemory(30);
  sgtl5000_1.enable();
  sgtl5000_1.volume(volumeLevel);

  granularLeft.begin(granularMemoryL, GRAIN_MEMORY);
  granularRight.begin(granularMemoryR, GRAIN_MEMORY);

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("Unable to access SD card!");
    while (1) delay(500);
  }

  Serial.println("Ready! Type 'p' to start playback.");
}

// === LOOP ===
void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd == 'p') {
      if (!playSdWav1.isPlaying()) {
        Serial.println("Playing 'song2.wav'...");
        playSdWav1.play("song3.wav");
        delay(10);
        granularLeft.beginPitchShift(20);
        granularRight.beginPitchShift(20);
        granularLeft.setSpeed(pitchShift);
        granularRight.setSpeed(pitchShift);
      } else {
        Serial.println("Already playing...");
      }
    }

    else if (cmd == 's') {
      playSdWav1.stop();
      granularLeft.stop();
      granularRight.stop();
      Serial.println("Stopped playback.");
    }

    else if (cmd == '+') {
      pitchShift += 0.1;
      if (pitchShift > 2.5) pitchShift = 2.5;
      granularLeft.setSpeed(pitchShift);
      granularRight.setSpeed(pitchShift);
      Serial.printf("Pitch up → %.2f\n", pitchShift);
    }

    else if (cmd == '-') {
      pitchShift -= 0.1;
      if (pitchShift < 0.30) pitchShift = 0.30;
      granularLeft.setSpeed(pitchShift);
      granularRight.setSpeed(pitchShift);
      Serial.printf("Pitch down → %.2f\n", pitchShift);
    }
  }

  delay(50);
}
