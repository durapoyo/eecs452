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

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);

  // Audio setup
  AudioMemory(8);
  sgtl5000.enable();
  sgtl5000.volume(0.8);

  // SD card setup
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD Card init FAILED!");
    while (1);
  }
  Serial.println("Setup complete. Waiting for gestures...");
}

// ---------- MAIN LOOP ----------
void loop() {
  // Check if a gesture byte is available
  if (Serial.available()) {
    int gesture = Serial.read();

    // Debug print
    Serial.print("Received gesture: ");
    Serial.println(gesture);

    // Play / Stop logic
    switch (gesture) {
      case 1:  // Open Palm → Play
        if (!isPlaying) {
          playSdWav1.play("test.wav");  // Make sure test.wav is on SD card
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
}




