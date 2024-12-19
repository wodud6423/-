//#include <Arduino_BuiltIn.h>

/*
  WiFiEsp test: ClientTest
  http://www.kccistc.net/
  작성일 : 2022.12.19
  작성자 : IoT 임베디드 KSH
*/
#define DEBUG
//#define DEBUG_WIFI

#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h> // MsTimer1 is servomoter!
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <DHT.h>

#define AP_SSID "iot0"
#define AP_PASS "iot00000"
#define SERVER_NAME "10.10.141.74"
#define SERVER_PORT 5000
#define LOGID "YJY_ARD1"
#define PASSWD "PASSWD"

#define Voltage 5 

#define CDS_PIN A0
#define BUTTON_PIN 2
#define LED_LAMP_PIN 3
#define DHTPIN 4
#define SERVO_PIN 5
#define WIFIRX 8  //6:RX-->ESP8266 TX
#define WIFITX 7  //7:TX -->ESP8266 RX
#define MOTOR_PIN 6 // Moter Pin
#define LED_BUILTIN_PIN 13 


#define CMD_SIZE 50
#define ARR_CNT 5
#define DHTTYPE DHT11
bool timerIsrFlag = false;
boolean lastButton = LOW;     // 버튼의 이전 상태 저장
boolean currentButton = LOW;    // 버튼의 현재 상태 저장
boolean ledOn = false;      // LED의 현재 상태 (on/off)
boolean cdsFlag = false;
boolean service = true;
char sendId[10] = "YJY_SQL";
char sendId2[10] = "YJY_BT";
char sendBuf[CMD_SIZE];
char sendBuf2[CMD_SIZE];
char lcdLine1[17] = "Smart IoT By YJY";
char lcdLine2[17] = "WiFi Connecting!";

int cds = 0;
unsigned int secCount;
unsigned int myservoTime = 0; //For Detach Parameter

// 전역 변수 - 서비스 메세지 파싱 부분
char * pArray_par[ARR_CNT] = {0};

char getSensorId[10];
int sensorTime;
float temp = 0.0;
float humi = 0.0;
bool updateTimeFlag = false; //Timer 
// RTC Clock 
typedef struct {
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
} DATETIME;
DATETIME dateTime = {0, 0, 0, 12, 0, 0};
DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;
float powerout(int motorSpeed);
void modifySendId(const char* sendBuf, const char* newSendId, char* sendBuf2);
void setup() {
  lcd.init();
  lcd.backlight();
  lcdDisplay(0, 0, lcdLine1);
  lcdDisplay(0, 1, lcdLine2);
  pinMode(CDS_PIN, INPUT);    // 조도 핀을 입력으로 설정 (생략 가능)
  pinMode(BUTTON_PIN, INPUT);    // 버튼 핀을 입력으로 설정 (생략 가능)
  pinMode(LED_LAMP_PIN, OUTPUT);    // LED 핀을 출력으로 설정
  pinMode(LED_BUILTIN_PIN, OUTPUT); //D13
  pinMode(MOTOR_PIN,OUTPUT);
  
#ifdef DEBUG
  Serial.begin(115200); //DEBUG
#endif
  wifi_Setup();

  MsTimer2::set(1000, timerIsr); // 1000ms period
  MsTimer2::start();

  myservo.attach(SERVO_PIN);
  myservo.write(0);
  myservoTime = secCount;
  dht.begin();
}

void loop() {
  char * src;
  char * state;
  int motorSpeed;
  if (client.available()) {
    socketEvent();
  }

  if (timerIsrFlag) //1초에 한번씩 실행
  {
    timerIsrFlag = false;
    if (!(secCount % 2)) //20초에 한번씩 실행
    {
      // cds = map(analogRead(CDS_PIN), 0, 1023, 0, 100); //
      humi = dht.readHumidity();
      temp = dht.readTemperature(); 
      if (!service){
        if ((cds >= 70) && cdsFlag)
        {
          cdsFlag = false;
          sprintf(sendBuf, "[%s]CDS@%d\n", sendId, cds);
          client.write(sendBuf, strlen(sendBuf));
          client.flush();
          //        digitalWrite(LED_BUILTIN_PIN, HIGH);     // LED 상태 변경
        } else if ((cds < 70) && !cdsFlag)
        {
          cdsFlag = true;
          sprintf(sendBuf, "[%s]CDS@%d\n", sendId, cds);
          client.write(sendBuf, strlen(sendBuf));
          client.flush();
          //        digitalWrite(LED_BUILTIN_PIN, LOW);     // LED 상태 변경
        }
      }
    // Our Project Code
    // 4단계로 나눠서 구현을 한다
    // 1단계 : 매우 불쾌 2단계 : 불쾌  3단계 : 적당 4단계 쾌적 
    // 1단계 : 30도 이상 / 65%~100%의 습도 / 조도량 95 dltkd
    // 2단계 : 27도 에서 30 도 사이 / 45% ~ 65%사이 습도 
    // 3단계 : 24도 에서 27도 사이 / 30% ~ 45% 사이 습도
    // 4단계 : 18도 에서 24도 사이 / 30% 미만의 습도
      else{  //GETTIME
        pArray_par[1] = "SERVICE"; 
        if((30 < temp )||((65 < humi)&&(humi <= 100))){
          motorSpeed = 250;
          analogWrite(MOTOR_PIN,motorSpeed); // 매우 불쾌
          state = "VeryUncomfortable";
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)temp, (int)humi);
        }
        else if(((26 < temp)&&(temp <= 30))||((45 < humi)&&(humi <= 65))){
          motorSpeed = 150;
          analogWrite(MOTOR_PIN,motorSpeed); // 불쾌
          state = "Uncomfortable";
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)temp, (int)humi);
        }
        else if(((22 < temp)&&(temp <= 26))||((30 < humi)&&(humi <= 45))){
          motorSpeed = 80;
          analogWrite(MOTOR_PIN,motorSpeed); // 적당
          state = "Moderate";
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)temp, (int)humi);
        }
        else if(((18 < temp)&&(temp <= 22))||(humi <= 30)){
          motorSpeed = 50;
          analogWrite(MOTOR_PIN,motorSpeed); // 쾌적
          state = "Comfortable"; 
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)50, (int)50);
        }
/*
        if(((18 < temp)&&(temp <= 22))||(humi <= 30)){
          motorSpeed = 50;
          analogWrite(MOTOR_PIN,motorSpeed); // 쾌적
          state = "Comfortable"; 
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)50, (int)50);
        }
        else if(((22 < temp)&&(temp <= 26))||((30 < humi)&&(humi <= 45))){
          motorSpeed = 80;
          analogWrite(MOTOR_PIN,motorSpeed); // 적당
          state = "Moderate";
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)temp, (int)humi);
        }

        if(((18 < temp)&&(temp <= 22))||(humi <= 30)){
          motorSpeed = 50;
          analogWrite(MOTOR_PIN,motorSpeed); // 쾌적
          state = "Comfortable"; 
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)50, (int)50);
        }
        else if(((26 < temp)&&(temp <= 30))||((45 < humi)&&(humi <= 65))){
          motorSpeed = 150;
          analogWrite(MOTOR_PIN,motorSpeed); // 불쾌
          state = "Uncomfortable";
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)temp, (int)humi);
        }
        else if((30 < temp )||((65 < humi)&&(humi <= 100))){
          motorSpeed = 250;
          analogWrite(MOTOR_PIN,motorSpeed); // 매우 불쾌
          state = "VeryUncomfortable";
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)temp, (int)humi);
        }*/
        else{
          motorSpeed = 0;
          analogWrite(MOTOR_PIN,motorSpeed); // 이외의 상태는 모터 작동 X
          state = "Error";
          sprintf(sendBuf, "[%s]%s@%s@%s@%d@%d\n", sendId, pArray_par[1], LOGID, state, (int)temp, (int)humi);
        }
      modifySendId(sendBuf,sendId2,sendBuf2);
      client.write(sendBuf, strlen(sendBuf));
      client.flush();
      client.write(sendBuf2, strlen(sendBuf2));
      client.flush();
    }

  }
#ifdef DEBUG
            // Serial.print("Cds: ");
            // Serial.print(cds);
            // Serial.print(" Humidity: ");
            // Serial.print(humi);
            // Serial.print(" Temperature: ");
            // Serial.println((int)temp);
      
#endif
      sprintf(lcdLine2,"T:%2d H:%2d",(int)temp,(int)humi );
      lcdDisplay(0, 1, lcdLine2);
      
      if (!client.connected()) {
        lcdDisplay(0, 1, "Server Down");
        server_Connect();
      }
    if (myservo.attached() && myservoTime + 2 == secCount)
    {
      myservo.detach();
    }
    if (sensorTime != 0 && !(secCount % sensorTime ))
    {
      sprintf(sendBuf, "[%s]SENSOR@%d@%d@%d\r\n", getSensorId, cds, (int)temp, (int)humi);
            char tempStr[5];
            char humiStr[5];
            dtostrf(humi, 4, 1, humiStr);  //50.0 4:전체자리수,1:소수이하 자리수
            dtostrf(temp, 4, 1, tempStr);  //25.1
            sprintf(sendBuf,"[%s]SENSOR@%d@%s@%s\r\n",getSensorId,cds,tempStr,humiStr);
      
      client.write(sendBuf, strlen(sendBuf));
      client.flush();
    }
    sprintf(lcdLine1, "%02d.%02d  %02d:%02d:%02d", dateTime.month, dateTime.day, dateTime.hour, dateTime.min, dateTime.sec );
    lcdDisplay(0, 0, lcdLine1);
    if (updateTimeFlag)
    {
      client.print("[GETTIME]\n"); //If you send Message, you must send \n after Message!!!
      updateTimeFlag = false;
    }
  }

  currentButton = debounce(lastButton);   // 디바운싱된 버튼 상태 읽기
  if (lastButton == LOW && currentButton == HIGH)  // 버튼을 누르면...
  { 
    if (service) service = false;
    else service = true;
    ledOn = !ledOn;       // LED 상태 값 반전
    digitalWrite(LED_BUILTIN_PIN, ledOn);     // LED 상태 변경
    //    sprintf(sendBuf,"[%s]BUTTON@%s\n",sendId,ledOn?"ON":"OFF");
    // sprintf(sendBuf, "[HM_CON]GAS%s\n", ledOn ? "ON" : "OFF");
    // client.write(sendBuf, strlen(sendBuf));
    // client.flush();
  }
  lastButton = currentButton;     // 이전 버튼 상태를 현재 버튼 상태로 설정

}
float powerout(int motorSpeed){
      float dutyCycle = motorSpeed / 255.0; // Calculate duty cycle (0-1 range)
      float estimatedCurrent = dutyCycle * 5.0; // Estimate current (assuming linear relation)
      float power = 5.0 * estimatedCurrent; // Calculate power in watts
    return power;
}
void modifySendId(const char* sendBuf, const char* newSendId, char* sendBuf2) {
    // 원본 sendBuf에서 "["와 "]" 부분을 찾아서 그 위치를 계산
    const char* start = strchr(sendBuf, '[');
    const char* end = strchr(sendBuf, ']');
    
    if (start && end && end > start) {
        // "[newSendId]" 부분을 새로 생성
        int prefixLength = start - sendBuf + 1; // "[" 포함
        int suffixLength = strlen(end);         // "]" 포함 이후 부분
        
        // 앞부분 복사
        strncpy(sendBuf2, sendBuf, prefixLength);
        sendBuf2[prefixLength] = '\0'; // Null-terminate

        // 새로운 sendId 추가
        strcat(sendBuf2, newSendId);

        // 뒷부분 복사
        strcat(sendBuf2, end);

    } else {
        // "[sendId]" 부분이 없으면 원본 문자열을 그대로 복사
        strcpy(sendBuf2, sendBuf);
    }
}
void socketEvent()
{
  char * state;
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;
  sendBuf[0] = '\0';
  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  client.flush();
#ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
#endif
  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL)
  {
    pArray[i] =  pToken;
    pArray_par[i] = pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  //[KSH_ARD]LED@ON : pArray[0] = "KSH_ARD", pArray[1] = "LED", pArray[2] = "ON"
  if ((strlen(pArray[1]) + strlen(pArray[2])) < 16)
  {
    sprintf(lcdLine2, "%s %s", pArray[1], pArray[2]);
    lcdDisplay(0, 1, lcdLine2);
  }
  if (!strncmp(pArray[1], " New", 4)) // New Connected
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    strcpy(lcdLine2, "Server Connected");
    lcdDisplay(0, 1, lcdLine2);
    updateTimeFlag = true;
    return ;
  }
  else if (!strncmp(pArray[1], " Alr", 4)) //Already logged
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    client.stop();
    server_Connect();
    return ;
  }
    else if (!strcmp(pArray[1], "LED")) {
      if (!strcmp(pArray[2], "ON")) {
        digitalWrite(LED_BUILTIN_PIN, HIGH);
      }
      else if (!strcmp(pArray[2], "OFF")) {
        digitalWrite(LED_BUILTIN_PIN, LOW);
      }
      sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
    } else if (!strcmp(pArray[1], "LAMP")) {
      if (!strcmp(pArray[2], "ON")) {
        digitalWrite(LED_LAMP_PIN, HIGH);
      }
      else if (!strcmp(pArray[2], "OFF")) {
        digitalWrite(LED_LAMP_PIN, LOW);
      }
      sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
    } else if (!strcmp(pArray[1], "GETSTATE")) {
      strcpy(sendId, pArray[0]);
      if (!strcmp(pArray[2], "LED")) {
        sprintf(sendBuf, "[%s]LED@%s\n", pArray[0], digitalRead(LED_BUILTIN_PIN) ? "ON" : "OFF");
      }
    }
    else if (!strcmp(pArray[1], "SERVO"))
    {
      myservo.attach(SERVO_PIN);
      myservoTime = secCount;
      if (!strcmp(pArray[2], "ON"))
        myservo.write(180);
      else
        myservo.write(0);
      sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);

    }
    else if (!strncmp(pArray[1], "GETSENSOR", 9)) {
      if (pArray[2] != NULL) {
        sensorTime = atoi(pArray[2]);
        strcpy(getSensorId, pArray[0]); // I send Only respnse GETSENSOR Host!
        return;
      } else {
        sensorTime = 0;
        sprintf(sendBuf, "[%s]%s@%d@%d@%d\n", pArray[0], pArray[1], cds, (int)temp, (int)humi);
      }
    }
    else if(!strcmp(pArray[0],"GETTIME")) {  //GETTIME`
      dateTime.year = (pArray[1][0]-0x30) * 10 + pArray[1][1]-0x30 ;
      dateTime.month =  (pArray[1][3]-0x30) * 10 + pArray[1][4]-0x30 ;
      dateTime.day =  (pArray[1][6]-0x30) * 10 + pArray[1][7]-0x30 ;
      dateTime.hour = (pArray[1][9]-0x30) * 10 + pArray[1][10]-0x30 ;
      dateTime.min =  (pArray[1][12]-0x30) * 10 + pArray[1][13]-0x30 ;
      dateTime.sec =  (pArray[1][15]-0x30) * 10 + pArray[1][16]-0x30 ;
    }
    client.write(sendBuf, strlen(sendBuf));
    client.flush();
  #ifdef DEBUG
    Serial.print(", send : ");
    Serial.print(sendBuf);
  #endif
}

void timerIsr()
{
  timerIsrFlag = true;
  secCount++;
  clock_calc(&dateTime);
}
void clock_calc(DATETIME *dateTime)
{
  int ret = 0;
  dateTime->sec++;          // increment second

  if(dateTime->sec >= 60)                              // if second = 60, second = 0
  { 
      dateTime->sec = 0;
      dateTime->min++; 
             
      if(dateTime->min >= 60)                          // if minute = 60, minute = 0
      { 
          dateTime->min = 0;
          dateTime->hour++;                               // increment hour
          if(dateTime->hour == 24) 
          {
            dateTime->hour = 0;
            updateTimeFlag = true;
          }
       }
    }
}

void wifi_Setup() {
  wifiSerial.begin(38400);
  wifi_Init();
  server_Connect();
}
void wifi_Init()
{
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG_WIFI
      Serial.println("WiFi shield not present");
#endif
    }
    else
      break;
  } while (1);

#ifdef DEBUG_WIFI
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
#endif
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG_WIFI
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
#endif
  }
  sprintf(lcdLine1, "ID:%s", LOGID);
  lcdDisplay(0, 0, lcdLine1);
  sprintf(lcdLine2, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  lcdDisplay(0, 1, lcdLine2);

#ifdef DEBUG_WIFI
  Serial.println("You're connected to the network");
  printWifiStatus();
#endif
}
int server_Connect()
{
#ifdef DEBUG_WIFI
  Serial.println("Starting connection to server...");
#endif

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI
    Serial.println("Connect to server");
#endif
    client.print("["LOGID":"PASSWD"]");
  }
  else
  {
#ifdef DEBUG_WIFI
    Serial.println("server connection failure");
#endif
  }
}
void printWifiStatus()
{
  // print the SSID of the network you're attached to

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
void lcdDisplay(int x, int y, char * str)
{
  int len = 16 - strlen(str);
  lcd.setCursor(x, y);
  lcd.print(str);
  for (int i = len; i > 0; i--)
    lcd.write(' ');
}
boolean debounce(boolean last)
{
  boolean current = digitalRead(BUTTON_PIN);  // 버튼 상태 읽기
  if (last != current)      // 이전 상태와 현재 상태가 다르면...
  {
    delay(5);         // 5ms 대기
    current = digitalRead(BUTTON_PIN);  // 버튼 상태 다시 읽기
  }
  return current;       // 버튼의 현재 상태 반환
}
