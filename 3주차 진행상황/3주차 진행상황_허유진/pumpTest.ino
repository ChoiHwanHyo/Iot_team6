// WATERLEVEL SENSOR값에 따라 PUMP 구동
const int inpump_pin = 22;
const int outpump_pin = 13;
const int waterlevel_pin = 26;  // ANALOG PIN NEEDED 

int MAX_WATER = 2800
  

void setup() {
    Serial.begin(115200);
    pinMode(inpump_pin, OUTPUT);
    pinMode(outpump_pin, OUTPUT); 
    digitalWrite(inpump_pin, LOW);
    digitalWrite(outpump_pin, LOW);
}

void controlWater(){
  digitalWrite(outpump_pin, HIGH); // START WATER OUT
  while (1) {
    waterLevel = analogRead(waterlevel_pin);
    // Serial.println(waterLevel);
    if (waterLevel < 10){
      digitalWrite(outpump_pin, LOW); // END WATER OUT
      digitalWrite(inpump_pin, HIGH); // START WATER IN
    } 
    if (waterLevel > MAX_WATER){
      digitalWrite(inpump_pin, LOW); // END WATER IN
      break;
    }
    delay(1000);
  }
}


void loop() {

  // if (time == set_time) 시간조건시
  controlWater();

  // NO FUNCTION CODE: controlWater 함수 사용안할시 아래코드사용
      // waterLevel = analogRead(waterlevel_pin);
      // if (time == set_time){
      //   digitalWrite(outpump_pin, HIGH); // WATER OUT
      // }
      // if (waterLevel < 10){
      //   digitalWrite(outpump_pin, LOW);
      //   digitalWrite(inpump_pin, HIGH); // WATER IN
      // }
      // if (waterLevel > 2800){
      //   digitalWrite(inpump_pin, LOW);
      // }
}
