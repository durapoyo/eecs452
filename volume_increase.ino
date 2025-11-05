#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioPlaySdWav           playSdWav1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, i2s1, 0);
AudioConnection          patchCord2(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;

// SD card pins for Teensy 4.1 built-in slot
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used

float volumeLevel = 0.5;  // Range: 0.0 to 1.0

void setup() {
  Serial.begin(9600);
  while (!Serial) ; 
  Serial.println("=== Teensy Audio Player ===");
  Serial.println("Commands:");
  Serial.println("  + : Increase volume");
  Serial.println("  - : Decrease volume");
  Serial.println("  p : Play audio file");
  Serial.println("---------------------------");

  AudioMemory(8);
  sgtl5000_1.enable();
  sgtl5000_1.volume(volumeLevel);

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("Unable to access the SD card!");
    while (1) delay(500);
  }

  Serial.println("Type 'p' to play sound2.wav");
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '+') {
      volumeLevel += 0.05;
      if (volumeLevel > 1.0) volumeLevel = 1.0;
      sgtl5000_1.volume(volumeLevel);
      Serial.print("Volume up: ");
      Serial.println(volumeLevel, 2);
    }
    else if (command == '-') {
      volumeLevel -= 0.05;
      if (volumeLevel < 0.0) volumeLevel = 0.0;
      sgtl5000_1.volume(volumeLevel);
      Serial.print("Volume down: ");
      Serial.println(volumeLevel, 2);
    }
    else if (command == 'p') {
      if (!playSdWav1.isPlaying()) {
        Serial.println("Playing sound2.wav...");
        playSdWav1.play("sound2.wav");
        delay(10); 
      } else {
        Serial.println("Already playing...");
      }
    }
  }

  if (!playSdWav1.isPlaying()) {
    delay(100);
  }
}
