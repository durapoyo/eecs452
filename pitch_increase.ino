#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioPlaySdWav           playSdWav1;
AudioEffectGranular      granular1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, granular1, 0);
AudioConnection          patchCord2(granular1, 0, i2s1, 0);
AudioConnection          patchCord3(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;

#define SDCARD_CS_PIN    BUILTIN_SDCARD

float volumeLevel = 0.5;
float pitchShift = 1.0;   
const int GRAIN_MEMORY = 12800;
int16_t granularMemory[GRAIN_MEMORY]; 

void setup() {
  Serial.begin(9600);
  while (!Serial) ; 
  Serial.println("=== Teensy Real-Time Pitch Shifter ===");
  Serial.println("Commands:");
  Serial.println("  p : Play WAV (sound2.wav)");
  Serial.println("  + : Increase pitch");
  Serial.println("  - : Decrease pitch");
  Serial.println("  s : Stop playback");
  Serial.println("----------------------------");

  AudioMemory(20);
  sgtl5000_1.enable();
  sgtl5000_1.volume(volumeLevel);

  granular1.begin(granularMemory, GRAIN_MEMORY);
  granular1.setSpeed(pitchShift); 

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("Unable to access SD card!");
    while (1) delay(500);
  }

  Serial.println("Ready! Type 'p' to play.");
}


void loop() {
  if (Serial.available()) {
    char command = Serial.read();

    if (command == 'p') {
      if (!playSdWav1.isPlaying()) {
        Serial.println("Playing sound2.wav...");
        playSdWav1.play("sound2.wav");
        delay(10);
        granular1.beginPitchShift(20);
      } else {
        Serial.println("Already playing...");
      }
    }
    else if (command == 's') {
      playSdWav1.stop();
      granular1.stop();
      Serial.println("Stopped.");
    }
    else if (command == '+') {
      pitchShift += 0.1;
      if (pitchShift > 4.0) pitchShift = 4.0; 
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch up: ");
      Serial.println(pitchShift, 2);
    }
    else if (command == '-') {
      pitchShift -= 0.1;
      if (pitchShift < 0.25) pitchShift = 0.25;
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch down: ");
      Serial.println(pitchShift, 2);
    }
  }

  delay(50);
}
