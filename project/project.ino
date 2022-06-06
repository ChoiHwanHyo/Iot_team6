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
char sTOPIC_NAME[] = "smartfish/accept";
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
int watering_day = 7; // 물을 가는 주기 설정
int current_watering_day = 0; // 물을 가는 주기를 체크하기 위해 얼마나 기간이 되었는지를 체크
int fish_food_count = 3;
int fish_food_time[3][3] = {{12, 21, 0}, {13, 0, 0}, {17, 0, 0}};
String temp_state = "normal";

// 서버 시간 관련 변수
const char* ntpServer = "pool.ntp.org"; // ntp서버를 설정합니다.
const long gmtOffset_sec = 3600 * 9; // 3600
const int daylightOffset_sec = 0; // 3600

// 웹페이지 타임아웃 관련 변수
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 10000;
String header;
// Feeder 변수
#define MIN_SIGNAL 800 // 최소 pwm값
#define MAX_SIGNAL 1650 // 최대 pwm값

Servo motorA; // 서보모터를 사용하는 객체 설정
int motorA_pin = 3; // 서보모터가 사용할 핀의 번호 설정
float weight = 1.0;
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
const int inpump_pin = 12;
const int outpump_pin = 14;
const int waterlevel_pin = 33;  // ANALOG PIN NEEDED
int MAX_WATER = 2800;
float waterLevel;



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
    JSONVar myObj = JSON.parse(rcvdPayload);
  }
}

// 설정한 주제로 publish합니다.
void publish() {
  // publish할 payload입니다.
  // 기준온도, 물을 가는 주기, 현재 온도에 따른 상태, 물을 갈아야하는지의 여부를 전송합니다.
  sprintf(payload, "{\"state\":{\"reported\":{\"temp\":%f, \"watering_day\":%d, \"current_watering_day\":%d, \"temp_state\":\"%s\"}}}",
          temp, watering_day, current_watering_day, temp_state);

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

void set_DefaultData() {
  Serial.println("\n\nStart set defaultdata\n\n");
  sprintf(payload, "{}");
  // AWS에서 저장된 설정을 불러옵니다.
  char pTOPIC_NAME2[] = "$aws/things/smart_fishbowl/shadow/get";
  while (1) {
    // publish에 성공할 경우 메세지를 publish하고, 반복문을 탈출합니다.
    if (fish.publish(pTOPIC_NAME2, payload) == 0) {
      Serial.print("Getting data to AWS with using Publish Message:");
      Serial.println(payload);

      if (msgReceived == 1) {
        msgReceived = 0;
        Serial.print("Received Message:");
        Serial.println(rcvdPayload);
        // 받은 값들 parse해서 저장하기
        Serial.println("print state, desired, temp, watering_day, current_watering_day");
        JSONVar myObj = JSON.parse(rcvdPayload);
        JSONVar state = myObj["state"];
        JSONVar desired = state["desired"];

        // 마지막으로 설정한 기준 온도값을 받아옵니다.
        JSONVar desired_temp = desired["temp"];
        Serial.print("get temp data!! temp is ");
        temp = (int) desired_temp;
        Serial.println(temp);

        // 마지막으로 설정한 수환 주기를 받아옵니다.
        JSONVar desired_watering_day = desired["watering_day"];
        Serial.print("get watering_day data!! watering_day is ");
        watering_day = (int) desired_watering_day;
        Serial.println(watering_day);

        // 어항의 물이 얼마나 경과되었는지를 받아옵니다.
        JSONVar desired_current_watering_day = desired["current_watering_day"];
        Serial.print("get current_watering_day data!! current_watering_day is ");
        current_watering_day = (int) desired_current_watering_day;
        Serial.println(current_watering_day);

      } else {
        delay(2000);
        continue;
      }

      break;
    }
    // 실패했을 경우 처음으로 돌아가 다시 요청합니다.
    else {
      Serial.println("Publish failed try again");
      // continue;
    }
  }
}

void set_Functions() {
  // 스마트 어항에 필요한 기능들을 설정합니다.

  // 먹이를 주는 기능
  set_Feeder();

  // 온도를 측정하는 기능
  set_WaterTemp();

  // 수환 기능
  set_WaterPump();

  // 설정된 기본값을 AWS IoT에서 불러옵니다.
  set_DefaultData();
}

// 현재상태를 체크하고 상태값을 변경합니다.
void check_state() {
  struct tm timeinfo; // 시간 관련 객체 생성
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  // 수온을 측정하고 설정된 기준값+-5도에 어긋날 경우 필요한 행동을 취합니다.
  //  check_waterTemp();
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

  // 물을 환수하는 주기일 경우, 물을 환수합니다.
  //  check_wateringDay(timeinfo);
  Serial.println("Start check wateringDay");
  if (current_watering_day == watering_day) {
    Serial.println("watering!!");
    watering();
  }
  else { // test
    current_watering_day++;
  }
  Serial.println("End check wateringDay");

  // 물이 너무 탁해졌을 경우, 물을 환수합니다.
  //  check_waterTurbidity();
  Serial.println("Start check WaterTurbidity");
  turbidityValue = analogRead(turbidity_pin);// read the input on analog pin 0:
  Serial.println(turbidityValue); // print out the value you read:

  if (turbidityValue < 1500) { // 탁한 기준
    // 워터펌프 작동
    Serial.println("watering!!");
    watering();
  }
  Serial.println("End check WaterTurbidity");


  // 먹이를 주는 시간일 경우 최대 3번 먹이를 줍니다.
  //  check_feeding(timeinfo);
  Serial.println("Start check feeding\n\n");
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

  Serial.println("End check feeding\n\n");


  //  Serial.println("Start check waterLevel");
  //  waterLevel = analogRead(waterlevel_pin);
  //  Serial.println(waterLevel);
  //  Serial.println("End check waterLevel");
  // 센서들을 통해 얻은 수온이나, 탁도, 서버의 시간을 통해
  // 기능의 상태를 변경합니다.

}



// 먹이를 주는 함수
void feeding(int n) {
  Serial.println("Start feeding!\n\n");

  for (int i = 0; i < N_times; i++)
  {
    Serial.println("do it!\n\n");
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

  Serial.println("End feeding\n\n");
}

// 환수하는 함수
void watering() {
  Serial.println("Start watering");
  // 워터펌프를 작동하여 물을 환수합니다.
  // 물이 뺀다.
  digitalWrite(outpump_pin, HIGH); // START WATER OUT
  while (1) {
    Serial.println("watering!!\n");
    waterLevel = analogRead(waterlevel_pin);
    Serial.println(waterLevel);
    // 물을 채운다.
    if (waterLevel < 800) {
      Serial.println("did!");
      digitalWrite(outpump_pin, LOW); // END WATER OUT
      digitalWrite(inpump_pin, HIGH); // START WATER IN
    }

    // 물이 어느정도 채워지면 종료한다.
    if (waterLevel > MAX_WATER) {
      Serial.println("end!!");
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
  if (client)
  { // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = ""; // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();
      if (client.available()) { // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c); // print it out the serial monitor
        header += c;
        if (c == '\n') { // if the byte is a newline character

          if (currentLine.length() == 0) {

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();


            // 조건 처리 부분
            // publish 부분
            if (header.indexOf("GET /smartfishbowl/publish") >= 0 ) {
              Serial.println("smartfishbowl/update");
              publish();
            }

            if (header.indexOf("GET /smartfishbowl/update?") >= 0 ) {

              // temp처리부분
              Serial.println("header.indexOf_temp: " + String(header.indexOf("temp=")));
              int tempindex = header.indexOf("temp=") + 5;
              int header_temp = 0; // 새로운 값을 입력하기 위해 temp 초기화
              while (header[tempindex] >= '0' && header[tempindex] <= '9') {
                header_temp = header_temp * 10 + header[tempindex++] - '0';
              }
              Serial.println("Print temp after while: " +  String(header_temp));

              // watering_day 처리부분

              Serial.println("header.indexOf_watering_day: " + String(header.indexOf("watering_day=")));
              int waterindex = header.indexOf("watering_day=") + 13;
              int header_watering = 0; // 새로운 값을 입력하기 위해 watering 초기화
              while (header[waterindex] >= '0' && header[waterindex] <= '9') {
                header_watering = header_watering * 10 + header[waterindex++] - '0';
              }
              Serial.println("Print watering_day after while: " +  String(header_watering));

              // fish_food_time1 처리부분

              Serial.println("header.indexOf_fish_food_time1: " + String(header.indexOf("fish_food1=")));
              int timeindex1 = header.indexOf("fish_food1=") + 11;
              int header_hour1 = 0; // 새로운 값을 입력하기 위해 time 초기화
              int header_min1 = 0;
              while (header[timeindex1] >= '0' && header[timeindex1] <= '9') {
                header_hour1 = header_hour1 * 10 + header[timeindex1++] - '0';
              }
              // :는 3개 건너뛴다
              timeindex1 += 3;

              while (header[timeindex1] >= '0' && header[timeindex1] <= '9') {
                header_min1 = header_min1 * 10 + header[timeindex1++] - '0';
              }

              Serial.println("Print time after while: " +  String(header_hour1) + ":" + String(header_min1));

              // fish_food_time2 처리부분

              Serial.println("header.indexOf_fish_food_time2: " + String(header.indexOf("fish_food2=")));
              int timeindex2 = header.indexOf("fish_food2=") + 11;
              int header_hour2 = 0; // 새로운 값을 입력하기 위해 time 초기화
              int header_min2 = 0;
              while (header[timeindex2] >= '0' && header[timeindex2] <= '9') {
                header_hour2 = header_hour2 * 10 + header[timeindex2++] - '0';
              }
              // :는 3개 건너뛴다
              timeindex2 += 3;

              while (header[timeindex2] >= '0' && header[timeindex2] <= '9') {
                header_min2 = header_min2 * 10 + header[timeindex2++] - '0';
              }

              Serial.println("Print time after while: " +  String(header_hour2) + ":" + String(header_min2));

              // fish_food_time3 처리부분

              Serial.println("header.indexOf_fish_food_time3: " + String(header.indexOf("fish_food3=")));
              int timeindex3 = header.indexOf("fish_food3=") + 11;
              int header_hour3 = 0; // 새로운 값을 입력하기 위해 time 초기화
              int header_min3 = 0;
              while (header[timeindex3] >= '0' && header[timeindex3] <= '9') {
                header_hour3 = header_hour3 * 10 + header[timeindex3++] - '0';
              }
              // :는 3개 건너뛴다
              timeindex3 += 3;

              while (header[timeindex3] >= '0' && header[timeindex3] <= '9') {
                header_min3 = header_min3 * 10 + header[timeindex3++] - '0';
              }

              Serial.println("Print time after while: " +  String(header_hour3) + ":" + String(header_min3));



              // 조건에 맞을 경우 저장 아니면 저장 x
              if (header_temp) {
                temp = header_temp;
              }
              if (header_watering) {
                watering_day = header_watering;
              }
              if (header_hour1 && header_min1)
              {
                fish_food_time[0][0] = header_hour1;
                fish_food_time[0][1] = header_min1;
              }
              if (header_hour2 && header_min2)
              {
                fish_food_time[1][0] = header_hour2;
                fish_food_time[1][1] = header_min2;
              }
              if (header_hour3 && header_min3)
              {
                fish_food_time[2][0] = header_hour3;
                fish_food_time[2][1] = header_min3;
              }
              Serial.println("End of Get /smartFishBowlDatas");
            }


            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50;border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");


            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");
            // Display current state, and ON/OFF buttons for GPIO 16
            client.println("<p>Smart FishBowl</p>");
            //form action check
            client.println("<form action = \"/smartfishbowl/update\"id = \"form\">");
            client.println("  <p>Temp:&nbsp" + String(temp) + "<input type=\"text\" name=\"temp\"><p>");
            client.println("  <p>Watering_day:&nbsp" + String(watering_day) + "<input type=\"text\" name=\"watering_day\"><p>");
            client.println("  <p>Fish_food_time 1:&nbsp" + String(fish_food_time[0][0]) + "&nbsp:&nbsp" + String(fish_food_time[0][1])
                           + "<input type=\"text\" name=\"fish_food1\"><p>");
            client.println("  <p>Fish_food_time 2:&nbsp" + String(fish_food_time[1][0]) + "&nbsp:&nbsp" + String(fish_food_time[1][1])
                           + "<input type=\"text\" name=\"fish_food2\"><p>");
            client.println("  <p>Fish_food_time 3:&nbsp" + String(fish_food_time[2][0]) + "&nbsp:&nbsp" + String(fish_food_time[2][1])
                           + "<input type=\"text\" name=\"fish_food3\"><p>");
            client.println("  <input type=\"submit\" value=\"enter\"/>");
            client.println("</form>");

            //            printLocalTime(client);
            //            printCurrentState(client);
            // publish button
            // client.println("<p><a href=\"/smartfishbowl/publish\"><button class=\"button\">PUBLISH</a></p>");


            client.println("</body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } //** if (currentLine.length() == 0) {
          else
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } //** if (c == '\n') {
        else if (c != '\r')
        { // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      } //* if (client.available()) {
    } //** while
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  } //** if (client) {
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
  //   일정시간마다 현재 상태를 체크하여 상태값을 변동시키고 publlish합니다.
  if ((millis() - checkMil) > flagMil) {
    Serial.println("check state and publish");
    check_state();
    Serial.println("publish!!");
    publish();
    checkMil = millis();
  }

  WiFiClient client = server.available();
  show_WebPage(client);


}
