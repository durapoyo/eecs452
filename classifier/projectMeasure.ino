#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>


/*

*/

/* Set the delay between fresh samples */
uint16_t BNO055_SAMPLERATE_DELAY_MS = 100;


// Check I2C device address and correct line below (by default address is 0x29 or 0x28)
//                                   id, address
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);


float s = 1;   // speed threshold parameter
float a = 1;   // linear motion threshold parameter
// NOTE: imu tracks in meters, so s is in m/s and a is in m
// NOTE: both are set to 1 just because i wasn't sure what to start with in testing


struct PrevState
{
  double lastVx = 0, lastVy = 0, lastVz = 0;  // last velocity
  double lastX = 0, lastY = 0, lastZ = 0;     // last position
  unsigned long lastTime = 0;
  int typeShit = 0;
};  // struct to keep track of previous state's motion

PrevState prev;


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


void checkLinearMotion(sensors_event_t* linearData, unsigned long currentTime)
{

  float s = 10;   // speed threshold parameter in m/s
  
  // base case
  if(prev.lastTime == 0)
  {
    prev.lastTime = currentTime;
    return;
  }

  

  double dt = (currentTime - prev.lastTime) / 1000.0; // in seconds

  // v1 = v0 + a * t
  // calc current velo
  double vx = prev.lastVx + linearData->acceleration.x * dt;
  double vy = prev.lastVy + linearData->acceleration.y * dt;
  double vz = prev.lastVz + linearData->acceleration.z * dt;


  // if after 1000 ms, reset all values for opportunity for new motions
  if(prev.typeShit > 1000){
    prev.typeShit = 0;
    prev.lastVx = 0;
    prev.lastVy = 0;
    prev.lastVz = 0;
    vx = 0;
    vy = 0;
    vz = 0;
  }

  // d = v * t
  // calc current displacement
  double dx = vx * dt;
  double dy = vy * dt;
  double dz = vz * dt;

  // update struct
  prev.lastX += dx;
  prev.lastY += dy;
  prev.lastZ += dz;

  // s = sqrt(vx ^ 2 + vy ^ 2 + vz ^ 2) 
  // calc speed
  // double speed = sqrt(vx * vx + vy * vy + vz * vz);


  // If moving quickly in x direction, print adjustment in pitch
  if(fabs(vx) > s){
    if(vx > 0)  Serial.println("Moving in positive x to increase pitch. Velocity: ");
    else  Serial.println("Moving in negative x to decrease pitch. Velocity: ");

    Serial.println(vx);
  }

  // If moving quickly in y direction, print adjustment in tempo
  if(fabs(vy) > s){
    if(vy > 0)  Serial.println("Moving in positive y to increase tempo. Velocity: ");
    else  Serial.println("Moving in negative y to decrease tempo. Velocity: ");

    Serial.println(vy);
  }

  // If moving quickly in z direction, print adjustment in volume
  if(fabs(vz) > s){
    if(vz > 0)  Serial.println("Moving in positive z to increase volume. Velocity: ");
    else  Serial.println("Moving in negative z to decrease volume. Velocity: ");

    Serial.println(vz);
  }

  // update struct
  prev.lastVx = vx;
  prev.lastVy = vy;
  prev.lastVz = vz;
  prev.lastTime = currentTime;

  prev.typeShit++;
}


// main loop (kinda copied it  from the original adafruit code)
void loop(void) {
  unsigned long now = millis();

  // lin accel
  sensors_event_t linearData;
  bno.getEvent(&linearData, Adafruit_BNO055::VECTOR_LINEARACCEL);
  checkLinearMotion(&linearData, now);

  delay(BNO055_SAMPLERATE_DELAY_MS);
}

