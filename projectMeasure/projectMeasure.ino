#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>


/*
Code to classify linear gestures from the IMU based on acceleration values.
*/

/* Set the delay between fresh samples */
uint16_t BNO055_SAMPLERATE_DELAY_MS = 100;


// Check I2C device address and correct line below (by default address is 0x29 or 0x28)
//                                   id, address
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);


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


  delay(1000);
}

int linearGestures(sensors_event_t* linearData, unsigned long currentTime) 
{

  // gen vals
  float threshold = 3; // m/s^2

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


// main loop (kinda copied it  from the original adafruit code)
void loop(void) {
  unsigned long now = millis();

  // lin accel
  sensors_event_t lin;
  bno.getEvent(&lin, Adafruit_BNO055::VECTOR_LINEARACCEL);

  // if gesture detected, wait
  if(linearGestures(&lin, now) != 0){
    delay(2000);
  }

  delay(BNO055_SAMPLERATE_DELAY_MS);
}
