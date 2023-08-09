// Libraries
#include <time.h>
#include <PubSubClient.h> 
#include <WiFi.h>
#include <ESP32Servo.h>
#include "DHTesp.h"
#include <ArduinoJson.h>
#include <RTClib.h> 

// Defnitions 
#define BUZZER 18  
#define DHT_PIN 15
#define LIGHT_SENSOR 36
#define SERVO 25

// Object Declarations 
WiFiClient espClient; //wifi client
PubSubClient mqttClient(espClient);
DHTesp dhtSensor;
Servo window;
StaticJsonDocument<200> doc;
RTC_DS1307 RTC;
DateTime now;
hw_timer_t *MediTimer = NULL;

//variables
int bTime = 500;
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
int daysUpdatedTime =0;
int bCounter = 0;
int remainingDays =0; 
int starthour=0;
int startminute=0;

//default values
int windowAngle = 0;
float shadeCF = 0.75;
int shadeAngle = 30;//offset angle
float temperature = 0.0;
float humidity = 0.0;
float lightIntensity = 0.0;

//buzzer data
bool bStatus=false;
int bFreq;
int bDelay;
bool bCts=false;
//scheduler data
bool sStatus=true;
int sNoDays;
bool sShedOn1=false;
int sShedTimeH1=0;
int sShedTimeM1=0;
bool sShedOn2=false;
int sShedTimeH2=0;
int sShedTimeM2=0;
bool sShedOn3=false;
int sShedTimeH3=0;
int sShedTimeM3=0;

bool sShed[3]={sShedOn1,sShedOn2,sShedOn3};
int sShedTime[6]={ sShedTimeH1,sShedTimeM1,sShedTimeH2,sShedTimeM2,sShedTimeH3,sShedTimeM3};

//functions
void setupWiFI();
void setupMqtt();
void connectToBroker();
void updateData();
void print_time(void);
void updatTime(void);
void update_time(void);
void buzzer(bool bCts, int bFreq, int bDelay);
bool medicineTime(int hour, int minute);
void setWindowAngle(float light_intensity );



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello, ESP32!");
  setupWiFi();
  setupMqtt();
  dhtSensor.setup(DHT_PIN,DHTesp::DHT22);//temperature sensor initiate
  window.attach(SERVO);//servo initiate
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  RTC.begin();//RTC initiate
  MediTimer = timerBegin(0, 80, true); 


  now = RTC.now();
  starthour = now.hour();//get the starting hour
  startminute = now.minute();//get the starting minute
  Serial.println(starthour);
  Serial.println(startminute);

}

void loop() {
  // put your main code here, to run repeatedly:
  now = RTC.now();

  if (!mqttClient.connected()){
    connectToBroker();
  }

  mqttClient.loop();//get the any publish value from mqtt

  updateData();//call the function for getting the sensor readings
  String jsonString;
  serializeJson(doc, jsonString);//jason to string 
  Serial.println(jsonString);
  mqttClient.publish("mediData",jsonString.c_str());// Publish the combined data to the MQTT broker
  delay(1000);

  if (remainingDays!=0){
    for(int i=0;i<6;i+=2){
      if (sShed[i/2] && medicineTime(sShedTime[i],sShedTime[i+1])){
        buzzer(bCts,bFreq,bDelay);
      }
    }
  }
  setWindowAngle(lightIntensity);
  updateTime();
}


void setupWiFi() {
  WiFi.begin("Wokwi-GUEST", "");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
}


void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org",1883);//hostname
  mqttClient.setCallback(receiveCallback); //for receiving data(subscribe)
}

//function to connect with mqtt and subscribe channels
void connectToBroker(){
  while (!mqttClient.connected()){
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("ESP32-190005E")){
      Serial.print("Connected");
      mqttClient.subscribe("mediData 1");
      mqttClient.subscribe("mediData 2");
    
    }else{
      Serial.print("Failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

//function to publish data from esp32
void updateData( ) {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  float temperature = data.temperature;
  float humidity = data.humidity;
  float lightIntensity = (float)analogRead(LIGHT_SENSOR)/4096;
  doc["temp"]= String(data.temperature,2);
  doc["hum"]= String(data.humidity,2);
  doc["LDR"]= String(lightIntensity, 2);
  doc["days"] = remainingDays;
}


void receiveCallback(char* topic, byte* payload, unsigned int length){
  char payloadCharArr[length];
  for (int i=0; i< length; i++){//convert the data and save them
    Serial.print((char)payload[i]);
    payloadCharArr[i]=(char)payload[i];
  }
  //create a json object 200 characters
  StaticJsonDocument<200> jsonDocument;
  // Deserialize the JSON string into the JSON object
  DeserializationError error = deserializeJson(jsonDocument, payloadCharArr);
  if (strcmp(topic,"mediData 1")==0){
    bStatus=jsonDocument["bStatus"];
    bFreq=jsonDocument["bFreq"];
    bDelay=jsonDocument["bDelay"];
    bCts=jsonDocument["bCts"];
    sStatus=jsonDocument["sStatus"];
    sNoDays=jsonDocument["sNoDays"];
    shadeAngle=jsonDocument["shadeAngle"];
    shadeCF=jsonDocument["shadeCF"];
  }else{
    sShed[0]=jsonDocument["sShedOn1"];
    sShedTime[0]=jsonDocument["sShedTimeH1"];
    sShedTime[1]=jsonDocument["sShedTimeM1"];
    sShed[1]=jsonDocument["sShedOn2"];
    sShedTime[2]=jsonDocument["sShedTimeH2"];
    sShedTime[3]=jsonDocument["sShedTimeM2"];
    sShed[2]=jsonDocument["sShedOn3"];
    sShedTime[4]=jsonDocument["sShedTimeH3"];
    sShedTime[5]=jsonDocument["sShedTimeM3"];

  }
  remainingDays = sNoDays;
 
  // Serial.println(bStatus);
  // Serial.println(bFreq);
  // Serial.println(bDelay);
  // Serial.println(bCts);
  // Serial.println(sStatus);
  //Serial.println(sNoDays);
  // Serial.println(sShed[0]);
  // Serial.println(sShedTime[0]);
  // Serial.println(sShedTime[1]);
  // Serial.println(sShed[1]);
  // Serial.println(sShedTime[2]);
  // Serial.println(sShedTime[3]);
  // Serial.println(sShed[2]);
  // Serial.println(sShedTime[4]);
  // Serial.println(sShedTime[5]);
  // Serial.println(shadeAngle);
  // Serial.println(shadeCF);
  Serial.println("");
  Serial.println("Msg received");
  Serial.println("=======================================================");

}

//function to control servo
void setWindowAngle(float light_intensity ) {

  int windowAngle =  shadeAngle + (180 - shadeAngle) * light_intensity * shadeCF;
  window.write(windowAngle);
}

//function to check the remaining days in the scheduler
void updateTime(void) {
  
  if (millis() - daysUpdatedTime > 65000){//accept the update if the gap>=65 sec
      if (now.hour() == starthour && now.minute() == startminute){
        if (remainingDays > 0){
          Serial.println(String(daysUpdatedTime));
          remainingDays--;
          daysUpdatedTime = millis();
        }
      }
    }
}

//function to buzzer activating
void buzzer(bool bCts, int bFreq, int bDelay) {
  if (bStatus){// only buzzer switch On
    Serial.println("buzzer is ON");
    if (!bCts) {
      for (int i = 0; i <= bDelay ; i += bTime) {
        tone (BUZZER, bFreq, bTime);
        noTone (BUZZER);
        delay(bTime);
      }
    } else {
      tone(BUZZER, bFreq, bDelay);

    }
  }
  else{Serial.println("buzzer is OFF, waiting...");//alarm time but buzzer off
  }
}

//function to checking the alarm time has come
bool medicineTime(int hour, int minute) {
  bool takeMedicine = false;
  Serial.println("H : " + String(hour) + " M : " + String(minute));

  if (now.minute() == minute && now.hour() == hour) {//compare set time and real time
    takeMedicine = true;
  }
  return takeMedicine;
}

//-----------------------------------------------------------------------------
// function to automatically update the current time
/*void update_time(void) {
  timeNow = millis() / 1000; // number of seconds passed since timeLast; // number of seconds passed fr
  seconds = timeNow;
  // if a minute has passed
  if (seconds >= 60) {
    timeLast += 60;
    minutes += 1;
  }

  // if an hour has passed
  if (minutes == 60) {
    minutes = 0;
    hours += 1;
  }

  //if a day has passed
  if (hours == 24) {

    hours = 0;
    days += 1;
    Serial.println(days);
    if(sNoDays<0){
      sNoDays = 0;
    }
    else{
      sNoDays--;
    }
    day_has_passed = true;
  }
}*/
