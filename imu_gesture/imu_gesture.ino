#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

enum GestureMode { IDLE, LINEAR_ACTIVE, GYRO_ACTIVE };
GestureMode currentMode = IDLE;

unsigned long gestureStartTime = 0;
const unsigned long gestureDuration = 500; // lockout time in ms


AudioPlaySdWav           playSdWav1;
AudioEffectGranular      granular1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, granular1, 0);
AudioConnection          patchCord2(granular1, 0, i2s1, 0);
AudioConnection          patchCord3(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;


// SD card pins for Teensy 4.1 built-in slot
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used

float volumeLevel = 0.5;  // Range: 0.0 to 1.0
float pitchShift = 1.0;   
const int GRAIN_MEMORY = 12800;
int16_t granularMemory[GRAIN_MEMORY]; 
/*
Code to classify linear gestures from the IMU based on acceleration values.
*/

/* Set the delay between fresh samples */
uint16_t BNO055_SAMPLERATE_DELAY_MS = 100;


// Check I2C device address and correct line below (by default address is 0x29 or 0x28)
//                                   id, address
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire2);


void setup(void)
{
  Serial.begin(115200);


  while (!Serial) delay(10);  // wait for serial port to open!


  Serial.println("Orientation Sensor Test"); Serial.println("");


  /* Initialise the sensor */
  if (!bno.begin())
  {
    /* There was a problem detecting the BNO055 ... check your connections */
    Serial.println("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }

  AudioMemory(8);
  sgtl5000_1.enable();
  sgtl5000_1.volume(volumeLevel);

  granular1.begin(granularMemory, GRAIN_MEMORY);
  granular1.setSpeed(pitchShift); 

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("Unable to access the SD card!");
    while (1) delay(500);
  }

  playSdWav1.play("test.wav");
  delay(10);
  granular1.beginPitchShift(20);


  delay(1000);
}


double smooth(double newValue, double oldValue, double alpha = 0.7) {
  return alpha * oldValue + (1 - alpha) * newValue;
}

int linearGestures(sensors_event_t* linearData, unsigned long currentTime) 
{

  // gen vals
  float threshold = 6; // m/s^2

  float x_accel = linearData->acceleration.x;
  float y_accel = linearData->acceleration.y;
  float z_accel = linearData->acceleration.z;
  int xa = 0;
  int ya = 0;
  int za = 0;

  // classify movement
  if (x_accel > threshold) xa = 1;
  if (x_accel < -threshold) xa = -1;
  if (y_accel > threshold) ya = 1;
  if (y_accel < -threshold) ya = -1;
  if (z_accel > threshold) za = 1;
  if (z_accel < -threshold) za = -1;

  // "decision tree" if u can even call it that, to detect multiple motions
  if(xa != 0 && ya != 0){
    if(fabs(x_accel) > fabs(y_accel)) ya = 0;
    else  xa = 0;
  }
  if(xa != 0 && za != 0){
    if(fabs(x_accel) > fabs(z_accel)) za = 0;
    else  xa = 0;
  }
  if(za != 0 && ya != 0){
    if(fabs(z_accel) > fabs(y_accel)) ya = 0;
    else  za = 0;
  }

  // print stuff (gonna make it a switch case bc this is embarrasing of me to have 10000 if else)
  if(xa > 0){
    Serial.println("x accel positive");
    return 1;
  }
  else if(xa < 0){
    Serial.println("x accel negative");
    return 1;
  }
  else if(ya > 0){
    Serial.println("y accel positive");
    return 1;
  }
  else if(ya < 0){
    Serial.println("y accel negative");
    return 1;
  }
  else if(za > 0){
    Serial.println("z accel positive");
    return 1;
  }
  else if(za < 0){
    Serial.println("z accel negative");
    return 1;
  }
  // bros got no motion
  else{
    return 0;
  }
}

void gyroGestures(sensors_event_t* gyroData, unsigned long currentTime) {
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
      // Serial.println("🎵 Increase TEMPO 🎵");

      volumeLevel += 0.05;
      if (volumeLevel > 1.0) volumeLevel = 1.0;
      sgtl5000_1.volume(volumeLevel);
      Serial.print("Volume up: ");
      Serial.println(volumeLevel, 2);
      
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
      // Serial.println("🎵 Decrease TEMPO 🎵");

      volumeLevel -= 0.05;
      if (volumeLevel < 0.0) volumeLevel = 0.0;
      sgtl5000_1.volume(volumeLevel);
      Serial.print("Volume down: ");
      Serial.println(volumeLevel, 2);

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
      //Serial.println("🎵 Increase VOLUME 🎵");

      pitchShift += 1;
      if (pitchShift > 4.0) pitchShift = 4.0; 
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch up: ");
      Serial.println(pitchShift, 2);

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
      //Serial.println("🎵 Decrease VOLUME 🎵");

      pitchShift -= 1;
      if (pitchShift < 0.25) pitchShift = 0.25; 
      granular1.setSpeed(pitchShift);
      Serial.print("Pitch down: ");
      Serial.println(pitchShift, 2);

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


void handleGestures(sensors_event_t* lin, sensors_event_t* gyro, unsigned long now) {

  switch (currentMode) {

    case IDLE: {
      // --- Detect whether motion is linear or rotational first ---
      float rollRate = gyro->gyro.x;
      float pitchRate = gyro->gyro.y;
      float yawRate = gyro->gyro.z;
      float gyroMag = sqrt(rollRate*rollRate + pitchRate*pitchRate + yawRate*yawRate);

      float accelMag = sqrt(
        lin->acceleration.x * lin->acceleration.x +
        lin->acceleration.y * lin->acceleration.y +
        lin->acceleration.z * lin->acceleration.z);

      // thresholds to choose gesture type
      const float gyroThreshold = 3.5;   // rad/s
      const float accelThreshold = 4.0;  // m/s^2

      if (gyroMag > gyroThreshold) {
        currentMode = GYRO_ACTIVE;
        gestureStartTime = now;
        Serial.println("[STATE] Gyro gesture started");
      } 
      else if (accelMag > accelThreshold) {
        currentMode = LINEAR_ACTIVE;
        gestureStartTime = now;
        Serial.println("[STATE] Linear gesture started");
      }
      break;
    }

    case LINEAR_ACTIVE: {
      linearGestures(lin, now);

      // after 500 ms, return to idle
      if (now - gestureStartTime > gestureDuration) {
        currentMode = IDLE;
        Serial.println("[STATE] Back to IDLE");
      }
      break;
    }

    case GYRO_ACTIVE: {
      gyroGestures(gyro, now);

      // after 500 ms, return to idle
      if (now - gestureStartTime > gestureDuration) {
        currentMode = IDLE;
        Serial.println("[STATE] Back to IDLE");
      }
      break;
    }
  }
}



// main loop (kinda copied it  from the original adafruit code)
void loop(void) {
  unsigned long now = millis();

  // lin accel
  sensors_event_t lin;
  sensors_event_t gyro;
  bno.getEvent(&lin, Adafruit_BNO055::VECTOR_LINEARACCEL);
  bno.getEvent(&gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);

  handleGestures(&lin, &gyro, now);

  delay(BNO055_SAMPLERATE_DELAY_MS);
}
