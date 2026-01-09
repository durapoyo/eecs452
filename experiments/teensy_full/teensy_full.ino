
/*
   Teensy 4.1 Gesture-Controlled DJ System
   ---------------------------------------
   1 = open palm (play)
   2 = closed fist (stop)
   3 = peace sign (equalizer mode)
   4 = point up (skip song)
   5 = two fingers (dual-deck mixer)
   6 = thumbs down (tempo mode ↑/↓)
*/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <Audio.h>
#include "AudioFilterEqualizer_I16.h"
#include <TeensyVariablePlayback.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// =====================================================
// ---------------- AUDIO SYSTEM -----------------------
// =====================================================
AudioPlaySdWav        playSdWav1;     // normal player
AudioPlaySdResmp      playSdTempo;    // variable-speed player (case 6)
AudioFilterEqualizer  eq1;
AudioOutputI2S        i2s1;
AudioMixer4           mixer;
AudioPlaySdWav        playDeckA;
AudioPlaySdWav        playDeckB;

// Patch cords
AudioConnection patchCord1(playSdWav1, 0, eq1, 0);
AudioConnection patchCord2(eq1, 0, i2s1, 0);
AudioConnection patchCord3(playSdWav1, 1, i2s1, 1);
AudioConnection patchCord4(playDeckA, 0, mixer, 0);
AudioConnection patchCord5(playDeckB, 0, mixer, 1);
AudioConnection patchCord6(mixer, 0, i2s1, 0);
AudioConnection patchCord7(mixer, 0, i2s1, 1);
AudioConnection patchCord8(playSdTempo, 0, i2s1, 0);
AudioConnection patchCord9(playSdTempo, 0, i2s1, 1);

AudioControlSGTL5000  sgtl5000;

// =====================================================
// ---------------- CONSTANTS & GLOBALS ----------------
// =====================================================
#define SDCARD_CS_PIN BUILTIN_SDCARD
#define EQ_TAPS       200
const int   GRAIN_MEMORY = 12800;
const float VOLUME_STEP  = 0.05;

bool isPlaying  = false;
bool eqMode     = false;
bool mixerMode  = false;
bool tempoMode  = false;
float volumeLevel = 0.5;
float pitchShift  = 1.0;
float echoLevel   = 1.0;
float playbackRate = 1.0;

// --- Equalizer parameters ---
float32_t fBand[]   = {200.0, 1200.0, 4000.0, 22050.0};
float32_t dbBand[]  = {0.0, 0.0, 0.0, 0.0};
float32_t scaleCoeff = 16384.0f;
int16_t   eqCoeffs[EQ_TAPS];

// Playlist
const char *songs[] = {"song1.wav", "song2.wav", "song3.wav"};
const int NUM_SONGS = sizeof(songs) / sizeof(songs[0]);
int currentSong = 0;

// IMU
Adafruit_BNO055 bno(55, 0x28, &Wire2);
bool imuConnected = false;
const uint16_t BNO055_DELAY = 100;

// =====================================================
// ---------------- FUNCTION DECLARATIONS --------------
// =====================================================
void buildEQ();
void playCurrentSong();
void nextSong();
void prevSong();
void adjustBass(float dB);
void adjustMid(float dB);
void adjustTreble(float dB);
int  linearGestures(sensors_event_t* linearData);
void gyroGestures(sensors_event_t* gyroData);
void tempoGestures(sensors_event_t* gyroData);

// =====================================================
// ---------------- SETUP ------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("=== Teensy 4.1 Gesture DJ Controller ===");

  // --- IMU ---
  imuConnected = bno.begin();
  Serial.println(imuConnected ? "BNO055 connected." : "No BNO055 found.");

  // --- Audio ---
  AudioMemory(80);
  sgtl5000.enable();
  sgtl5000.volume(volumeLevel);
  mixer.gain(0, 0.8);
  mixer.gain(1, 0.8);

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("SD Card init FAILED!");
    while (1);
  }

  buildEQ();

  playSdTempo.enableInterpolation(true);
  playSdTempo.setPlaybackRate(playbackRate);

  Serial.println("Ready for gestures!");
}

// =====================================================
// ---------------- PLAYBACK / EQ -----------------------
void buildEQ() {
  uint16_t err = eq1.equalizerNew(4, fBand, dbBand, EQ_TAPS, eqCoeffs, 60.0f, scaleCoeff);
  if (err) Serial.printf("EQ error %d\n", err);
  else Serial.printf("EQ → Bass=%+.1fdB Mid=%+.1fdB Treble=%+.1fdB\n", dbBand[0], dbBand[1], dbBand[2]);
}
void playCurrentSong() {
  Serial.printf("Now playing: %s\n", songs[currentSong]);
  if (playSdWav1.play(songs[currentSong])) { delay(20); isPlaying = true; }
}
void nextSong() { playSdWav1.stop(); currentSong = (currentSong + 1) % NUM_SONGS; playCurrentSong(); }
void prevSong() { playSdWav1.stop(); currentSong = (currentSong - 1 + NUM_SONGS) % NUM_SONGS; playCurrentSong(); }

// =====================================================
// ---------------- EQ HELPERS --------------------------
void adjustBass(float dB)   { dbBand[0] = constrain(dbBand[0] + dB, -12, 12); buildEQ(); }
void adjustMid(float dB)    { dbBand[1] = constrain(dbBand[1] + dB, -12, 12); buildEQ(); }
void adjustTreble(float dB) { dbBand[2] = constrain(dbBand[2] + dB, -12, 12); buildEQ(); }

// =====================================================
// ---------------- GESTURE CONTROL ---------------------
int linearGestures(sensors_event_t* lin) {
  float threshold = 6;
  float x = lin->acceleration.x, z = lin->acceleration.z;
  if (eqMode) {
    if (x >  threshold) { adjustTreble(+3); Serial.println("Treble ++"); return 1; }
    if (x < -threshold) { adjustTreble(-3); Serial.println("Treble --"); return 1; }
  } else {
    if (x >  threshold) { echoLevel = min(echoLevel + 1, 4); Serial.println("Echo ++"); return 1; }
    if (x < -threshold) { echoLevel = max(echoLevel - 1, 0); Serial.println("Echo --"); return 1; }
  }
  if (z >  threshold) { nextSong(); Serial.println("Next song"); return 1; }
  if (z < -threshold) { prevSong(); Serial.println("Prev song"); return 1; }
  return 0;
}

void gyroGestures(sensors_event_t* gyro) {
  float roll = gyro->gyro.x, pitch = gyro->gyro.y;
  const float bigPush = 7.0;
  if (eqMode) {
    if (roll  >  bigPush) { adjustBass(+3); delay(300); }
    if (roll  < -bigPush) { adjustBass(-3); delay(300); }
    if (pitch >  bigPush) { adjustMid(+3);  delay(300); }
    if (pitch < -bigPush) { adjustMid(-3);  delay(300); }
  } else {
    if (roll  >  bigPush) { volumeLevel = min(volumeLevel + VOLUME_STEP, 1.0); sgtl5000.volume(volumeLevel); delay(300); }
    if (roll  < -bigPush) { volumeLevel = max(volumeLevel - VOLUME_STEP, 0.0); sgtl5000.volume(volumeLevel); delay(300); }
  }
}

// --- Case 6 tempo gestures ---
void tempoGestures(sensors_event_t* gyro) {
  float roll = gyro->gyro.x;
  const float bigPush = 7.0;
  if (roll > bigPush) {
    playbackRate += 0.1; if (playbackRate > 2.0) playbackRate = 2.0;
    playSdTempo.setPlaybackRate(playbackRate);
    Serial.printf("Tempo ++ %.2fx\n", playbackRate);
    delay(300);
  }
  if (roll < -bigPush) {
    playbackRate -= 0.1; if (playbackRate < 0.5) playbackRate = 0.5;
    playSdTempo.setPlaybackRate(playbackRate);
    Serial.printf("Tempo -- %.2fx\n", playbackRate);
    delay(300);
  }
}

// =====================================================
// ---------------- MAIN LOOP ---------------------------
void loop() {
  if (Serial.available()) {
    int gesture = Serial.read();
    Serial.printf("Gesture → %d\n", gesture);

    switch (gesture) {
      case 1: if (!isPlaying) playCurrentSong(); break;
      case 2: playSdWav1.stop(); isPlaying = false; Serial.println("Stopped."); break;

      case 3: eqMode = !eqMode; Serial.println(eqMode ? "🎛 EQ Mode ON" : "🎛 EQ Mode OFF"); break;
      case 4: nextSong(); break;

      // --- Case 5: Dual-Deck Mixer ---
      case 5:
        mixerMode = !mixerMode;
        if (mixerMode) {
          Serial.println("🎚 Dual-Deck Mixer ON");
          playDeckA.play("song1.wav"); delay(10);
          playDeckB.play("song2.wav"); delay(10);
          mixer.gain(0, 0.8); mixer.gain(1, 0.8);
        } else { playDeckA.stop(); playDeckB.stop(); Serial.println("🎚 Mixer OFF"); }
        break;

      // --- Case 6: Tempo Mode ---
      case 6:
        tempoMode = !tempoMode;
        if (tempoMode) {
          Serial.println("⏩ Tempo Mode ON");
          playSdTempo.playWav("song5.wav");
          delay(10);
          playSdTempo.setPlaybackRate(playbackRate);
        } else {
          Serial.println("⏩ Tempo Mode OFF");
          playSdTempo.stop();
        }
        break;

      default: break;
    }
  }

  // --- Mixer cross-fade (gyro roll) ---
  if (mixerMode && imuConnected) {
    sensors_event_t gyro; bno.getEvent(&gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);
    float roll = gyro.gyro.x;
    if      (roll >  4.0) { mixer.gain(0, 0.3); mixer.gain(1, 1.0); Serial.println("Focus → Deck B"); }
    else if (roll < -4.0) { mixer.gain(0, 1.0); mixer.gain(1, 0.3); Serial.println("Focus → Deck A"); }
  }

  // --- Tempo control gestures (case 6 active) ---
  if (tempoMode && imuConnected) {
    sensors_event_t gyro; bno.getEvent(&gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);
    tempoGestures(&gyro);
  }

  // --- Normal gesture control ---
  if (imuConnected && isPlaying && !mixerMode && !tempoMode) {
    sensors_event_t lin, gyro;
    bno.getEvent(&lin, Adafruit_BNO055::VECTOR_LINEARACCEL);
    bno.getEvent(&gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);
    if (fabs(gyro.gyro.x) < 1.5 && fabs(gyro.gyro.y) < 1.5)
         if (linearGestures(&lin)) delay(400);
    else gyroGestures(&gyro);
  }

  delay(BNO055_DELAY);
}


