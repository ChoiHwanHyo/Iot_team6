// AWS.IOT관련 라이브러리
#include <AWS_IOT.h>
#include <WiFi.h>
#include <Arduino_JSON.h>
#include <WiFi.h>
#include "time.h"

#include <ESP32_Servo.h>
#define MIN_SIGNAL 800
#define MAX_SIGNAL 1650

//int delayTime;

// AWS.IOT 변수 선언 및 핀 번호 선언
AWS_IOT fish;
const char* ssid = "";
// 사용할 와이파이의 비밀번호입니다.
const char* password = "";
char HOST_ADDRESS[] = "";
char CLIENT_ID[] = "ESP32_Smart_FishBowl";
char sTOPIC_NAME[] = "smartfishbowl/subscribe";
char pTOPIC_NAME[] = "$aws/things/smart_fishbowl/shadow/update";
int status = WL_IDLE_STATUS;
int msgCount = 0, msgReceived = 0;
char payload[512];
char rcvdPayload[512];
unsigned long preMil = 0;
unsigned long checkMil = 0;
const long intMil = 5000;
const long flagMil = 10000; // 10초

int fish_hour[3];
int fish_min[3];
WiFiServer server(80);

String header;


// 기본적인 LED의 상태는 off 상태 입니다.
String output16State = "off";
String output17State = "off";

// 스마트 어항 변수들
int temp = 0; // recent temp
int watering_day = 14; // time
int fish_food_amount = 0; // amount/once
int fish_food_count = 3; // 먹이를 주는 횟수
String fish_food_time[3] = {"11:22", "11:35", "11:40"}; // 먹이를 주는 시간 fish_food_count에 따라 크기가 달라짐 최대 10번
int fishbowl_oxygen = 0; // amount/min
// state는 -1, 1이면 비정상, 0이면 정상이다.
int temp_state = 0; // -1: 온도가 낮음, 0: 정상온도, 1: 온도가 높음
int watering_day_state = 0; // 0: 물을 갈지 않아도 된다, 1: 물을 갈아야한다.
int fish_food_amount_state = 0; // 하루마다 먹이를 준 횟수를 체크
int fishbowl_oxygen_state = 0; // 산소가 얼마나 있는지를 체크 정상:0, 비정상:1
// 현재 상태 체크
int now_temp = 0;
int now_watering_day = 0;
int now_fish_food_count = 0;
int now_oxygen = 0;


// 서보 모터 확인
int motorA_pin = 3;

float weight = 3.0;
const float percount = 0.14;//1번 회전 당 weight(단위: g)

int N_times = weight / percount;//돌아야 하는 서보모터의 회전. (왔다갔다하는 수)

Servo motorA;


// Current time
// 현재시간을 체크합니다.
unsigned long currentTime = millis();
// Previous time
// 이전시간을 체크합니다.
unsigned long previousTime = 0;

// Define timeout time in milliseconds (example: 2000ms = 2s)
// 타임아웃을 2초로 설정합니다.
const long timeoutTime = 10000;

// 사용할 ntp서버와 사용할 offset을 설정합니다.
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 9; // 3600
const int daylightOffset_sec = 0; // 3600

// CallBack 함수
void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad)
{
  strncpy(rcvdPayload, payLoad, payloadLen);
  rcvdPayload[payloadLen] = 0;
  msgReceived = 1;
}


boolean start() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if (fish.connect(HOST_ADDRESS, CLIENT_ID) == 0) {
    Serial.println("Connected to AWS");
    delay(1000);
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

  Serial.println("set food time!!");
  // default값 또는 웹서버에서 받은 시간을 파싱한다.
  for (int j = 0; j < 3; j++)
  {
    fish_hour[j] = ((fish_food_time[j][0] - '0') * 10) + fish_food_time[j][1] - '0';
    fish_min[j] = ((fish_food_time[j][3] - '0') * 10) + fish_food_time[j][4] - '0';

    Serial.println("fish_hour[" + String(j) + "]: " + String(fish_hour[j]));
    Serial.println("fish_min[" + String(j) + "]: " + String(fish_min[j]));

  }

  Serial.println("start server!");
  server.begin();

}

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

void publish() {
  //  sprintf(payload, "{\"temp\":%d, \"watering_day\":%d}", temp, watering_day);
  // 현재 state를 aws iot에 전송하고, 이상값이 발견 될 경우, 메일을 lambda를 통해 보냅니다.
  // 보낼 데이터: 적정온도, 수환 주기, 먹이 시간 및 횟수, 산소량
  // 온도, 수환주기, 먹이 횟수, 산소량 상태 를 보낸다.
  // 보낼 경우 값은 low, high, normal이 있으며, low는 낮음 high는 높거나 초과상태를 뜻한다.
  static int a = 0;
  if (a == 0) {
    sprintf(payload, "{\"state\":{\"reported\":{\"temp\":%d, \"watering_day\":%d}}}", temp, watering_day);
    a++;
  }
  else {
    sprintf(payload, "{\"state\":{\"reported\":{\"temp\":%d}}}", 150);
    a--;
  }
  if (fish.publish(pTOPIC_NAME, payload) == 0) {
    Serial.print("Publish Message:");
    Serial.println(payload);
  }
  else
    Serial.println("Publish failed");
}

void check_state() {
  struct tm timeinfo;
  int nowtime = -1;

  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }

  // 현재 온도를 체크하여 비교합니다.
  // 온도를 체크합니다.
  if (now_temp >= temp + 2) { // 온도가 너무 높다
    Serial.println("temp is too hot");
    temp_change(-1); // 온도 내림
  }
  else if ( now_temp <= temp - 2) { // 온도가 너무 낮다
    Serial.println("temp is too cold");
    temp_change(1); // 온도 올림
  }
  else { // 온도가 정상이다
    temp_change(0); // 온도를 변화시키지 않음
  }

  // 물을 가는 날인지 확인합니다.
  if (now_watering_day < watering_day) {
    Serial.println("It is not time to change water");
    watering();
  }
  else {
    Serial.println("Need to change water today");
  }


  //  먹이를 줬는지를 확인합니다.
  for (int i = 0; i < fish_food_count; i++) {
    // 현재 시간이 먹이를 주는 시간이고,
    // 먹이를 한번 주었다면, 같은 시간대라도 먹이를 다시 주지 않습니다.
    // nowtime: 추후 현재시간을 받아 체크로 바꿈

    if (timeinfo.tm_hour == fish_hour[i] and timeinfo.tm_min == fish_min[i] and i == now_fish_food_count) {
      feeding();
      now_fish_food_count++;
      Serial.print("now_fish_food_count: ");
      Serial.println(now_fish_food_count);
    }
  }

  //  feeding();


  // 하루가 지났다면 먹이를 주는 횟수를 초기화합니다.
  // nowtime: 추후 현재시간을 받아 체크로 바꿈
  // 0: 0시0분 정각을 뜻함
  if (timeinfo.tm_hour == 0 and timeinfo.tm_min) {
    now_fish_food_count == 0;
  }

  // 산소가 제대로 전달되는 지를 확이납니다.
  if (now_oxygen > fishbowl_oxygen) {
    oxygen_change(-1);
  }
  else if (now_oxygen < fishbowl_oxygen) {
    oxygen_change(1);
  }
  else {
    oxygen_change(0);
  }
}

void temp_change(int i) {
  // 온도를 변화시킨다.
}

void watering() {
  // 물을 간다.
}

void feeding() {
  // 먹이를 준다.
  Serial.println("start feeding");
  motorA.writeMicroseconds(MAX_SIGNAL);
  delay(600);

  motorA.writeMicroseconds(MIN_SIGNAL);
  delay(600);

  motorA.writeMicroseconds(MAX_SIGNAL);
  delay(600);

  motorA.detach();
  Serial.println("Job Finished.");
}

void oxygen_change(int i) {
  // 산소량을 변화시킨다.
}

void printLocalTime(WiFiClient client)
{

  //client.println("<p>GPIO 16 - State " + output16State + "</p>");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    client.println("Failed to obtain time");
    return;
  }
  client.println("<script>var totalTime=" + String(timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec) +
                 "; setInterval(function(){totalTime++; document.getElementById('timer').innerHTML='NowTime: '+Math.floor(totalTime/3600) + ':' + Math.floor(totalTime%3600/60) + ':' + totalTime%3600%60;}, 1000);</script>");
  client.println(&timeinfo, "<h2 id='timer'>NowTime: %H:%M:%S</h2>");
  client.println("Year: " + String(timeinfo.tm_year + 1900) + ", Month: " + String(timeinfo.tm_mon + 1));


}

void printCurrentState(WiFiClient client)
{
  client.println("now state in smartfishbowl\n");
  client.println("temp: " + String(temp));
  client.println("watering_day: " + String(watering_day));
  client.println("fish_food_amount: " + String(fish_food_amount));
  client.println("fishbowl_oxygen: " + String(fishbowl_oxygen));

}

// setup설정
void setup() {
  // 시리얼 모니터 설정
  Serial.begin(115200);

  Serial.println("Feeder status : ONLINE");
  motorA.attach(motorA_pin);

  start();
}


// 클라이언트를 확인해 웹페이지를 표시합니다.
void loop()
{

  // 설정한 초마다 자동으로 현재상태를 publish합니다
  if ((millis() - checkMil) > flagMil) {
    Serial.println("Before check current state");
    check_state();

    Serial.println("\npublish auto");
    checkMil = millis();
    //    sprintf(payload, "{\"temp\":%d, \"watering_day\":%d}", temp, watering_day);
    //    if (fish.publish(pTOPIC_NAME, payload) == 0) {
    //      Serial.println("Publish in auto");
    //      Serial.print("Publish Message:");
    //      Serial.println(payload);
    //    }
    //    else
    //      Serial.println("Publish failed");
    publish();

    // 위의 과정을 통해 데이터가 왔다면
    // 메세지를 확인하고 실행합니다.
    checkMessage();

    // 마지막으로 현재 상태를 체크하고
    // 작업이 필요하면 수행합니다.
    Serial.println("After check current state");
    check_state();
  }

  WiFiClient client = server.available(); // Listen for incoming clients
  if (client)
  { // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = ""; // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      checkMessage();
      currentTime = millis();
      if (client.available()) { // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c); // print it out the serial monitor
        header += c;
        if (c == '\n') { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();


            // 조건 처리 부분
            // publish 부분
            if (header.indexOf("GET /smartfishbowl/publish") >= 0 ) {
              Serial.println("smartfishbowl/publish");
              publish();
            }
            if (header.indexOf("GET /smartFishBowlDatas?") >= 0 ) {
              int index = 29;
              Serial.println("Get /smartFishBowlDatas");
              temp = 0; // 새로운 값을 입력하기 위해 temp 초기화
              while (header[index] >= '0' && header[index] <= '9') {
                temp = temp * 10 + header[index++] - '0';
              }
              Serial.println("Print temp after while: " +  String(temp));
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
            client.println("<form action = \"/smartFishBowlDatas\"id = \"form\">");
            client.println("  <p>Temp: <input type=\"text\" name=\"temp\"><p>");
            client.println("  <p>Watering_day: <input type=\"text\" name=\"watering_day\"><p>");
            client.println("  <p>Fish_food_amount: <input type=\"text\" name=\"fish_food\"><p>");
            client.println("  <p>Fishbowl_oxygen: <input type=\"text\" name=\"fishbowl_oxygen\"><p>");
            client.println("  <input type=\"submit\" value=\"enter\"/>");
            client.println("</form>");

            //            printLocalTime(client);
            //            printCurrentState(client);
            client.println("<p><a href=\"/smartfishbowl/publish\"><button class=\"button\">PUBLISH</a></p>");


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
} //** loop() {
