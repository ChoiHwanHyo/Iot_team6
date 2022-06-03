#include <AWS_IOT.h>
#include <WiFi.h>
#include <Arduino_JSON.h>
#include "time.h"
#include <ESP32_Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>

AWS_IOT fish;
// 사용할 와이파이의 아이디입니다.
const char* ssid = "";
// 사용할 와이파이의 비밀번호입니다.
const char* password = "";
// 접속할 AWS의 주소입니다.
char HOST_ADDRESS[] = "";
// AWS에 접속할 클라이언트의 이름입니다.
char CLIENT_ID[] = "ESP32_Smart_FishBowl";
// subscribe할 topic입니다.
char sTOPIC_NAME[] = "$aws/things/smart_fishbowl/shadow/update/delta";
// publish할 topic입니다.
char pTOPIC_NAME[] = "$aws/things/smart_fishbowl/shadow/update";
// 구독한 주제를 통해 메시지가 오면 읽습니다.
int msgReceived = 0;
// publish할 payload의 공간입니다.
char payload[512];
// subscribe topic에서온 payload를 저장합니다.
char rcvdPayload[512];
// 서버는 80포트를 사용합니다.
WiFiServer server(80);
// publish가 일어난 시간을 체크합니다.
unsigned long checkMil = 0;
// publish할 시간주기를 정합니다.
const long flagMil = 10000; // 10초

// 스마트 어항의 기능 구현을 위한 상태 변수
float temp = 12.0;
int watering_day = 14; // 물을 가는 주기 설정
int current_watering_day = 0; // 물을 가는 주기를 체크하기 위해 얼마나 기간이 되었는지를 체크
int fish_food_count = 3;
int fish_food_time[3][3] = {{9, 0, 0}, {13, 0, 0}, {17, 0, 0}};
String temp_state = "normal";

// 서버 시간 관련 변수
const char* ntpServer = "pool.ntp.org"; // ntp서버를 설정합니다.
const long gmtOffset_sec = 3600 * 9; // 3600
const int daylightOffset_sec = 0; // 3600

// 웹페이지 타임아웃 관련 변수
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 10000;

// Feeder 변수
#define MIN_SIGNAL 800 // 최소 pwm값
#define MAX_SIGNAL 1650 // 최대 pwm값

Servo motorA; // 서보모터를 사용하는 객체 설정
int motorA_pin = 3; // 서보모터가 사용할 핀의 번호 설정
float weight = 3.0;
const float percount = 0.14;//1번 회전 당 weight(단위: g)
int N_times = weight / percount;//돌아야 하는 서보모터의 회전. (왔다갔다하는 수)

// 수온측정 변수
#define SENSOR_PIN  21 // ESP32 pin GPIO21 connected to DS18B20 sensor's DQ pin
OneWire oneWire(SENSOR_PIN);
DallasTemperature DS18B20(&oneWire);
float tempC = 0.0;


// 수환기능 변수
// 탁도 변수
int turbidity_pin = 36;
int turbidityValue;
// 환수기능 변수
const int inpump_pin = 22;
const int outpump_pin = 13;
const int waterlevel_pin = 26;  // ANALOG PIN NEEDED
int MAX_WATER = 2800;
int waterLevel;



void connecting_Wifi() {
  // wifi에 접속 중임을 출력합니다.
  Serial.print("Connecting to ");
  // 접속하 wifi의 이름을 표시합니다.
  Serial.println(ssid);

  //  WiFi.mode(WIFI_STA);

  // wifi에 접속을 시도합니다.
  WiFi.begin(ssid, password);

  // wifi 접속에 성공할 때까지 .을 5초마다 한번씩 출력합니다.
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // 로컬 IP와 주소를 출력합니다.
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void accessing_AWS() {
  // 설정한 호스트와 클라이언트id로 접속을 시도합니다.
  if (fish.connect(HOST_ADDRESS, CLIENT_ID) == 0) {
    // AWS에 접속이 완료된 시점입니다.
    Serial.println("Connected to AWS");
    delay(1000);

    // AWS에 접속을  하고 구독을 성공했는지를 확인합니다.
    if (0 == fish.subscribe(sTOPIC_NAME, mySubCallBackHandler)) {
      // AWS 접속 성공 및 구독 성공
      Serial.println("Subscribe Successfull");
    }
    else {
      // 구독 실패
      Serial.println("Subscribe Failed, Check the Thing Name and Certificates");
      while (1);
    }
  }
  else {
    // AWS 접속 실패
    Serial.println("AWS connection failed, Check the HOST Address");
    while (1);
  }
}

// CallBack 함수
void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad)
{
  strncpy(rcvdPayload, payLoad, payloadLen);
  rcvdPayload[payloadLen] = 0;
  msgReceived = 1;
}

// 메시지가 왔는지를 체크하는 함수입니다.
void checkMessage() {
  if (msgReceived == 1) {
    msgReceived = 0;
    Serial.print("Received Message:");
    Serial.println(rcvdPayload);
    //    Parse JSON
    //    JSONVar myObj = JSON.parse(rcvdPayload);
    //    JSONVar state = myObj["state"];
    //    String temp = (const char*) state["temp"];
    //    Serial.print("LED will be ");
    //    Serial.println((const char*) state["temp"]);

  }
}

// 설정한 주제로 publish합니다.
void publish() {
  // publish할 payload입니다.
  // 기준온도, 물을 가는 주기, 현재 온도에 따른 상태, 물을 갈아야하는지의 여부를 전송합니다.
  sprintf(payload, "{\"state\":{\"reported\":{\"temp\":%f, \"watering_day\":%d, \"temp_state\":\"%s\", \"watering_state\":%d}}}",
          temp, watering_day, temp_state, watering_state);
  //  static int a = 0;
  //  if (a == 0) {
  //    sprintf(payload, "{\"state\":{\"reported\":{\"temp\":%f, \"watering_day\":%d, \"current_watering_day\":%d, \"temp_state\":\"%s\"}}}",
  //            temp, 17, current_watering_day, "normal");
  //    a++;
  //  }
  //  else if (a == 1) {
  //    sprintf(payload, "{\"state\":{\"reported\":{\"temp\":%f, \"watering_day\":%d, \"current_watering_day\":%d, \"temp_state\":\"%s\"}}}",
  //            temp, 5, current_watering_day, "high");
  //    a++;
  //  }
  //  else if (a == 2) {
  //    sprintf(payload, "{\"state\":{\"reported\":{\"temp\":%f, \"watering_day\":%d, \"current_watering_day\":%d, \"temp_state\":\"%s\"}}}",
  //            temp, 2, current_watering_day, "low");
  //    a = 0;
  //  }


  // AWS에 작성한 payload를 전송합니다.
  if (fish.publish(pTOPIC_NAME, payload) == 0) {
    Serial.print("Publish Message:");
    Serial.println(payload);
  }
  else
    Serial.println("Publish failed");
}

void set_Feeder() {
  Serial.println("Feeder status : ONLINE");
  motorA.attach(motorA_pin);
}

void set_WaterTemp() {
  Serial.println("Set water temp");
  DS18B20.begin();
}

void set_WaterPump() {
  Serial.println("Set water pump");
  pinMode(inpump_pin, OUTPUT);
  pinMode(outpump_pin, OUTPUT);
  digitalWrite(inpump_pin, LOW);
  digitalWrite(outpump_pin, LOW);

}

void set_Functions() {
  // 스마트 어항에 필요한 기능들을 설정합니다.

  // 먹이를 주는 기능
  set_Feeder();

  // 온도를 측정하는 기능
  set_WaterTemp();

  // 수환 기능
  set_WaterPump();
}

// 현재상태를 체크하고 상태값을 변경합니다.
void check_state() {
  struct tm timeinfo; // 시간 관련 객체 생성

  // 수온을 측정하고 설정된 기준값+-5도에 어긋날 경우 필요한 행동을 취합니다.
  check_waterTemp();

  // 물을 환수하는 주기일 경우, 물을 환수합니다.
  check_wateringDay(timeinfo);

  // 물이 너무 탁해졌을 경우, 물을 환수합니다.
  check_waterTurbidity();

  // 먹이를 주는 시간일 경우 최대 3번 먹이를 줍니다.
  check_feeding(timeinfo);

  // 센서들을 통해 얻은 수온이나, 탁도, 서버의 시간을 통해
  // 기능의 상태를 변경합니다.

}

void check_waterTemp() {
  DS18B20.requestTemperatures();       // send the command to get temperatures
  tempC = DS18B20.getTempCByIndex(0);  // read temperature in °C
  //  tempF = tempC * 9 / 5 + 32; // convert °C to °F

  Serial.print("Temperature: ");
  Serial.print(tempC);    // print the temperature in °C
  Serial.println("°C");


  if ((temp - tempC) > 8.0) {
    temp_state = "high";
  }
  else if ((temp - tempC) < 8.0) {
    temp_state = "low";
  }
  else {
    temp_state = "normal";
  }

  Serial.println("End waterTemp");

}

// 물을 환수하는 날인 경우 환수를 실행합니다.
void check_wateringDay(struct tm timeinfo) {
  Serial.println("Start check wateringDay");
  if (current_watering_day == watering_day) {
    watering();
  }
  else { // test
    current_watering_day++;
  }
  Serial.println("End check wateringDay");
}

// 수질을 체크하여 탁하다면 환수를 합니다.
void check_waterTurbidity() {
  Serial.println("Start check WaterTurbidity");
  turbidityValue = analogRead(turbidity_pin);// read the input on analog pin 0:
  Serial.println(turbidityValue); // print out the value you read:

  if (turbidityValue < 1500) { // 탁한 기준
    // 워터펌프 작동
    Serial.println("watering!!");
    //    watering();
  }
  Serial.println("End check WaterTurbidity");

  //  delay(500);
}

// 먹이를 주는지를 체크합니다.
void check_feeding(struct tm timeinfo) {
  Serial.println("Start check feeding");
  // timeinfo를 통해 시간과 분을 받고
  // fish_food_time[3][3]
  // i = 주는 순서 0, 1, 2 아침, 점심, 저녁이라고 생각하면 된다.
  // j = 시간, 분, 먹이를 주었는지를 체크 0안주었고, 1이면 준것이다.

  for (int i = 0; i < 3; i++) {
    if (timeinfo.tm_hour == fish_food_time[i][0] and timeinfo.tm_min == fish_food_time[i][1]
        and fish_food_time[i][2] == 0 and fish_food_count > 0) {
      feeding(i);
    }
  }

  Serial.println("End check feeding");

}

// 먹이를 주는 함수
void feeding(int n) {
  Serial.println("Start feeding!");

  for (int i = 0; i < N_times; i++)
  {
    motorA.writeMicroseconds(MAX_SIGNAL);
    delay(600);

    motorA.writeMicroseconds(MIN_SIGNAL);
    delay(600);
  }
  motorA.writeMicroseconds(MAX_SIGNAL);
  delay(600);

  // 먹이를 주었다면 남은 기회를 차감합니다.
  fish_food_count--;
  fish_food_time[n][2] = 1;

  Serial.println("End feeding");
}

// 환수하는 함수
void watering() {
  Serial.println("Start watering");
  // 워터펌프를 작동하여 물을 환수합니다.
  digitalWrite(outpump_pin, HIGH); // START WATER OUT
  while (1) {
    Serial.println("watering!!\n");
    waterLevel = analogRead(waterlevel_pin);
    // Serial.println(waterLevel);
    if (waterLevel < 10) {
      digitalWrite(outpump_pin, LOW); // END WATER OUT
      digitalWrite(inpump_pin, HIGH); // START WATER IN
    }
    if (waterLevel > MAX_WATER) {
      digitalWrite(inpump_pin, LOW); // END WATER IN
      break;
    }
    delay(1000);
  }

  // 어항의 물을 갈았다면, 대기 날짜를 초기화 합니다.
  current_watering_day = 0;
  Serial.println("End Watering");
}

// 웹페이지를 보여주는 함수
void show_WebPage(WiFiClient client) {

}

void setup() {
  // 시리얼 모니터 설정
  Serial.begin(115200);

  // WIFI에 접속을 시도
  connecting_Wifi();

  // 시간을 설정
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // AWS에 접속을 시도
  accessing_AWS();

  // 스마트 어항에 필요한 기능 설정
  set_Functions();

  // 위 3가지 함수가 모두 정상적으로 작동한다면, 서버를 시작함
  Serial.println("start server!");
  server.begin();

  checkMil = millis();
}

void loop() {
  // 일정시간마다 현재 상태를 체크하여 상태값을 변동시키고 publlish합니다.
  if ((millis() - checkMil) > flagMil) {
    Serial.println("check state and publish");
    check_state();
    Serial.println("publish!!");
    publish();
    checkMil = millis();
  }

  //  WiFiClient client = server.available();
  //  show_WebPage(client);


}
