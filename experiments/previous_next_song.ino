#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioPlaySdWav playWav1;
AudioOutputI2S i2s1;
AudioConnection patchCord1(playWav1, 0, i2s1, 0);
AudioConnection patchCord2(playWav1, 1, i2s1, 1);
AudioControlSGTL5000 sgtl5000;

#define SDCARD_CS_PIN BUILTIN_SDCARD

const char *songs[] = {
  "song1.wav",
  "song2.wav",
  "song3.wav"
};
const int NUM_SONGS = sizeof(songs) / sizeof(songs[0]);
int currentSong = 0;

void playCurrentSong();
void nextSong();
void prevSong();

void setup() {
  Serial.begin(9600);
  AudioMemory(40);
  sgtl5000.enable();
  sgtl5000.volume(0.6);

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("SD card not found!");
    while (1);
  }

  delay(200); 

  Serial.println("=== Teensy Playlist Player ===");
  Serial.println("Commands:");
  Serial.println("  p = play first song");
  Serial.println("  n = next song");
  Serial.println("  b = previous song");
  Serial.println("-------------------------------");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd == 'p') {
      if (!playWav1.isPlaying()) {
        playCurrentSong();
      } else {
        Serial.println("Already playing!");
      }
    }
    else if (cmd == 'n') {
      nextSong();
    }
    else if (cmd == 'b') {
      prevSong();
    }
  }

  if (playWav1.isPlaying() == false && currentSong >= 0) {
    // Uncomment if you want automatic looping through songs
    // nextSong();
  }
}

void playCurrentSong() {
  Serial.print("Now playing: ");
  Serial.println(songs[currentSong]);
  bool ok = playWav1.play(songs[currentSong]);
  delay(20);
  if (ok)
    Serial.println("Playback started!");
  else
    Serial.println("Failed to open file!");
}

void nextSong() {
  playWav1.stop();
  currentSong = (currentSong + 1) % NUM_SONGS;
  playCurrentSong();
}

void prevSong() {
  playWav1.stop();
  currentSong = (currentSong - 1 + NUM_SONGS) % NUM_SONGS;
  playCurrentSong();
}
