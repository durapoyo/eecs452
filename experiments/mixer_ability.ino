#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// Two simultaneous WAV players
AudioPlaySdWav playDeckA;
AudioPlaySdWav playDeckB;
AudioMixer4 mixer;
AudioOutputI2S i2s1;

AudioConnection patchCord1(playDeckA, 0, mixer, 0);
AudioConnection patchCord2(playDeckB, 0, mixer, 1);
AudioConnection patchCord3(mixer, 0, i2s1, 0);
AudioConnection patchCord4(mixer, 0, i2s1, 1);

AudioControlSGTL5000 sgtl5000;

#define SDCARD_CS_PIN BUILTIN_SDCARD

void setup() {
  Serial.begin(9600);
  AudioMemory(20);
  sgtl5000.enable();
  sgtl5000.volume(0.8);
  mixer.gain(0, 0.8);  // deck A volume
  mixer.gain(1, 0.8);  // deck B volume

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("SD failed!");
    while (1);
  }

  Serial.println("=== Teensy DJ Dual Deck ===");
  Serial.println("Commands: a=start A, b=start B, 1–2 adjust mix");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'a') {
      playDeckA.play("stars.wav");
      delay(10);
      Serial.println("Deck A started");
    } else if (c == 'b') {
      playDeckB.play("song.wav");
      delay(10);
      Serial.println("Deck B started");
    } else if (c == '1') {
      mixer.gain(0, 1.0); mixer.gain(1, 0.3);
      Serial.println("Focus: Deck A");
    } else if (c == '2') {
      mixer.gain(0, 0.3); mixer.gain(1, 1.0);
      Serial.println("Focus: Deck B");
    }
  }
}
