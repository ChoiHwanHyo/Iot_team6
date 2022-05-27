#include <ESP32_Servo.h> 
#define MIN_SIGNAL 800
#define MAX_SIGNAL 1650

int motorA_pin = 3;

float weight = 3.0;
const float percount = 0.14;//1번 회전 당 weight(단위: g)

int N_times = weight / percount;//돌아야 하는 서보모터의 회전. (왔다갔다하는 수)

Servo motorA;

void setup() {
  Serial.begin(9600); // open the serial port
  Serial.println("Feeder status : ONLINE");
  motorA.attach(motorA_pin);

  for(int i = 0; i < N_times; i++)
  {
     motorA.writeMicroseconds(MAX_SIGNAL);
     delay(600);
    
     motorA.writeMicroseconds(MIN_SIGNAL);
     delay(600);
  }
  motorA.writeMicroseconds(MAX_SIGNAL);
  delay(600);
   motorA.detach();
   Serial.println("Job Finished.");
}

void loop() {
}
