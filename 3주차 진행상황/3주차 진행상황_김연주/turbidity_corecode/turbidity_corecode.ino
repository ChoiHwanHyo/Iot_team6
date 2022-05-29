#include <WiFi.h>
#include <WiFiClient.h>

int turbidity_pin = 36;

void setup() {
  Serial.begin(9600); //Baud rate: 9600
}
void loop() {
  int sensorValue = analogRead(turbidity_pin);// read the input on analog pin 0:
  Serial.println(sensorValue); // print out the value you read:
  delay(500);
}
