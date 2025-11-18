#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <Audio.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// ---------------- AUDIO -----------------
AudioPlaySdWav playDeckA;
AudioPlaySdWav playDeckB;
AudioMixer4 mixer;
AudioOutputI2S i2s1;

AudioConnection patchCord1(playDeckA, 0, mixer, 0);
AudioConnection patchCord2(playDeckB, 0, mixer, 1);
AudioConnection patchCord3(mixer, 0, i2s1, 0);
AudioConnection patchCord4(mixer, 0, i2s1, 1);

AudioControlSGTL5000 sgtl5000;

// ---------------- IMU -----------------
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire2);
bool imuConnected = false;

// ---------------- FILTERS -----------------
const float ALPHA_ACCEL = 0.22f;
struct Vec3 { float x; float y; float z; };
Vec3 accelFilt = {0,0,0};

// ---------------- GLOBALS -----------------
const unsigned long IMU_POLL_MS = 12;
const unsigned long GESTURE_COOLDOWN_MS = 500;
unsigned long lastGestureTime = 0;

float volumeLevel = 0.4;      
float pitchShift  = 1.0;      
bool mixerMode = false;       
bool isPlaying = false;

const char* playlist[] = {"song1.wav", "song2.wav", "song3.wav", "song4.wav"};
int currentSongA = 0;  
int currentSongB = 1;  

// ---------------- SETUP -----------------
void setup() {
  Serial.begin(115200);
  AudioMemory(40);
  sgtl5000.enable();
  sgtl5000.volume(volumeLevel);
  mixer.gain(0, 0.5);
  mixer.gain(1, 0.5);

  imuConnected = bno.begin();
  if (imuConnected) bno.setExtCrystalUse(true);

  if (!SD.begin(BUILTIN_SDCARD)) while (1);

  Serial.println("=== Teensy DJ Dual-Deck (Mixer + Crossfade) ===");
}

// ---------------- IIR FILTER -----------------
inline float iirf(float prev, float sample, float alpha) {
  return alpha * sample + (1.0f - alpha) * prev;
}
inline void updateFilters(const sensors_event_t &lin) {
  accelFilt.x = iirf(accelFilt.x, lin.acceleration.x, ALPHA_ACCEL);
  accelFilt.y = iirf(accelFilt.y, lin.acceleration.y, ALPHA_ACCEL);
  accelFilt.z = iirf(accelFilt.z, lin.acceleration.z, ALPHA_ACCEL);
}

// ---------------- DEBOUNCE -----------------
bool readyForGesture() { return (millis() - lastGestureTime) >= GESTURE_COOLDOWN_MS; }
void confirmGesture() { lastGestureTime = millis(); }

// ---------------- PLAYBACK HELPERS -----------------
void playCurrentSongA() {
  playDeckA.play(playlist[currentSongA]);
  delay(10);
  isPlaying = true;
  Serial.printf("Deck A playing: %s\n", playlist[currentSongA]);
}

void playNextDeckB() {
  currentSongB = (currentSongA + 1) % (sizeof(playlist)/sizeof(playlist[0]));
  playDeckB.play(playlist[currentSongB]);
  delay(10);
  Serial.printf("Deck B playing: %s\n", playlist[currentSongB]);
}

// ---------------- LINEAR Y CROSSFADE -----------------
void handleLinearGestures() {
  if (!readyForGesture() || !mixerMode) return;
  float y = accelFilt.y;
  const float THRESH = 6.0;

  if (y > THRESH) {
    mixer.gain(0, 0.3); mixer.gain(1, 0.8);
    Serial.println("Linear Y+ → Focus Deck B");
    confirmGesture();
  } else if (y < -THRESH) {
    mixer.gain(0, 0.8); mixer.gain(1, 0.3);
    Serial.println("Linear Y- → Focus Deck A");
    confirmGesture();
  }
}

// ---------------- MAIN LOOP -----------------
void loop() {
  unsigned long now = millis();

  // --- RPi gestures ---
  if (Serial.available()) {
    int gesture = Serial.read();
    switch (gesture) {
      case 1: if (!isPlaying) playCurrentSongA(); break;      // open palm → start single song
      case 2: if (isPlaying) { playDeckA.stop(); isPlaying=false; Serial.println("Deck A stopped"); } break; // closed fist → stop
      case 5: // thumbs up → mixer
        if (!mixerMode) {
          mixerMode = true;
          Serial.println("🎚 Mixer Mode ON");
          if (isPlaying) playNextDeckB();
          mixer.gain(0,0.5); mixer.gain(1,0.5);
        } else {
          mixerMode = false;
          Serial.println("🎚 Mixer Mode OFF");
          playDeckB.stop();
        }
        break;
    }
  }

  // --- IMU gestures ---
  if (imuConnected && mixerMode && (now - lastGestureTime > IMU_POLL_MS)) {
    sensors_event_t lin;
    bno.getEvent(&lin, Adafruit_BNO055::VECTOR_LINEARACCEL);
    updateFilters(lin);
    handleLinearGestures();
  }
}

