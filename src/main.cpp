#include <Arduino.h>
#include "StepperDriver.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Littlefs.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include "esp_ota_ops.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
IPAddress localIP(1, 2, 3, 4);

StepperDriver stepper(4, 5, 6, 7);

struct STEPPERVAR
{
  int64_t nextStep = 10000000;
  int32_t stepTime = 4883;
  int64_t doubleTime = 2500; //Any smaller of a step time and the the clock face will desync from the expected time
  double doubleTimeRate = 0;
  int64_t stepCount = 0;
}stepvar;

#define HOURS_PER_CYCLE 12

struct CLOCKO
{
  int64_t hours = 0;
  int64_t minutes = 0;
  int64_t seconds = 0;
  int64_t milliseconds = 0;
  int64_t microseconds = 0;
  double rate = 1;
};

struct TIMEE
{
  int64_t nextCalculation = 0;
  int64_t calculationTime = 10000;
  int64_t currentUs = 0;
  int64_t clockFaceUs = 0;
  int64_t timeOffset = 0;
  int64_t elapsedUs = 0;
  int64_t dayUs = 0;
  int64_t usPerCycle = 0;
  double faceRate = 1;
  CLOCKO clock;
  CLOCKO uSPer;
}timeO;

void SetTime(CLOCKO tt);
DynamicJsonDocument BuildTimeJason(CLOCKO foop);
int64_t CalculateUs(CLOCKO tt);
CLOCKO CalculateClock(int64_t tt);

struct WEBDATA
{
  String lastStatus = "";
  int64_t nextPageUpdate = 10000000;
  int64_t pageUpdateTime = 1000000;
}wpd;


#pragma region Settings and Database

#define FILESYSTEM LittleFS

Preferences prefs;

struct SETTINGS{
  String ssid = "";
  String passward = "";
} clockSettings;

//Function for loading the settings out of prefrences
void LoadSettings(){
  prefs.begin("Settings");
  clockSettings.passward = prefs.getString("wifip", clockSettings.passward);
  clockSettings.ssid = prefs.getString("wifin", clockSettings.ssid);
  prefs.end();
  Serial.println("\nSettings Loaded from Prefrences\n");
}

//function for saving the settings out of prefrences
void SaveSettings(){
  prefs.begin("Settings", false);
  prefs.putString("wifip", clockSettings.passward);
  prefs.putString("wifin", clockSettings.ssid);
  prefs.end();
  Serial.println("\nSettings saved to Prefrences\n");
}

//This guy is for opening the spiffs partiton and putting the text files into memory 
const char* SpiffsReadTextToChar(String path){

  String tempText;

  //open the file specified at the path and return nothing if spiffs fails
  File file = FILESYSTEM.open(path.c_str());
  if(!file){
    Serial.println("Failed to open file " + path);
    return "";
  }
  
  //once open, read the file as a string and ureturn it as a const char
  Serial.println("Reading " + path);
  tempText = file.readString();
  file.close();

  return tempText.c_str();
}

//Function for setting up the settings and database
//I know it's small but it might expand
//...Might
void SetupSettingsAndDatabase(){
  Serial.println("Setting Up Database\n");

  delay(10);
  LoadSettings();

  if(!FILESYSTEM.begin(true)){
    while(true){
      Serial.println("Not able to mount LittleFS. Reboot ESP");
      delay(1000);
    }
  }
  
  Serial.println("Database Loaded\n");
}

#pragma endregion


//Function for setting up the stepper motor
void SetupStepper(){
  stepper.enable();
  stepper.setStepMode(1);
  stepper.setDirection(1);
}

#pragma region Wifi and Server
//Function for setting up the WiFi connecion
//This guy is for connecting to Wifi
//Not good code so I will need to rebuild the function to not use delays
bool Connect_WIFI() {//----------------------------------------------------------------------------------------------------------
  WiFi.disconnect(true);
  
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  delay(100);

  byte connectionCounnt = 0;

  bool notConnected = true;

  //connection attempting loop
  while(notConnected){

    //connect to wifi
    WiFi.begin(clockSettings.ssid, clockSettings.passward);
    delay(900);

    //check if connected
    notConnected = WiFi.status() != WL_CONNECTED;
    delay(100);

    //act on not being connected for 5 attempts
    Serial.print(WiFi.status());
    connectionCounnt ++;
    if(connectionCounnt == 5){
      WiFi.disconnect(true);
      break;
    }

  }

  //Echo Connection Status
  if(notConnected){
    Serial.println("\nWiFi not connected");
  }
  else{
    Serial.println("\nWiFi connected");
    Serial.print("IP Adress: ");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.RSSI());
  }
  
  return !notConnected;
}

//This funciton sets up the ESP into Acces Point Mode for wifi configuraiton
bool WiFiAPMode(){
  // Set up WiFi Access Point (self-hosting mode)
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ClockBrain", "0987654321");
  WiFi.softAPConfig(localIP, localIP, IPAddress(255, 255, 255, 0));

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  delay(100);

  //Check if the esp32 is acting as an Acces Point
  boolean isAP = WiFi.softAPIP().toString().length() > 0;
  if(isAP){
    Serial.println("ESP32 is now a WiFi Access Point");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
  }
  else{
    Serial.println("ESP32 failed to start a WiFi Access Point");
  }

  return isAP;
}

//A funtion for populating the main webpage
void HTML_HomeSendOnLoad(){
  DynamicJsonDocument jsonDoc(1024);
  jsonDoc["timeClock"] = BuildTimeJason(timeO.clock);
  //the clock face doesn't have a CLOCKO type and I need rate to be sent with the clock face
  CLOCKO ttt = CalculateClock(timeO.clockFaceUs);
  ttt.rate = timeO.faceRate;
  jsonDoc["faceClock"] = BuildTimeJason(ttt);
  char SendBuf[512];
  serializeJson(jsonDoc, SendBuf, sizeof(SendBuf));
  Serial.println(SendBuf);
  ws.textAll(SendBuf);
}

//A funtion for populating the main webpage
void HTML_WIFISendOnLoad(){
  DynamicJsonDocument jsonDoc(1024);
  jsonDoc["ssid"] = clockSettings.ssid;
  jsonDoc["password"] = clockSettings.passward;
  char SendBuf[512];
  serializeJson(jsonDoc, SendBuf, sizeof(SendBuf));
  ws.textAll(SendBuf);
}

CLOCKO ClockDecoder(DynamicJsonDocument jsonTime){
  CLOCKO tmptime;
  if(jsonTime.containsKey("hours")){
    int64_t temp = jsonTime["hours"];
    //Hard Code for 12 hour time. I know its bad but it works for now
    temp = temp -1;
    if(temp > 11) temp = 11;
    else if(temp < 0) temp = 0;
    tmptime.hours = temp;
  }
  if(jsonTime.containsKey("minutes")){
    int64_t temp = jsonTime["minutes"];
    tmptime.minutes = temp;
  }
  if(jsonTime.containsKey("seconds")){
    int64_t temp = jsonTime["seconds"];
    tmptime.seconds = temp;
  }
  if(jsonTime.containsKey("milliseconds")){
    int64_t temp = jsonTime["milliseconds"];
    tmptime.milliseconds = temp;
  }
  if(jsonTime.containsKey("microseconds")){
    int64_t temp = jsonTime["microseconds"];
    tmptime.microseconds = temp;
  }
  return tmptime;
}

//This use to be for just the webpage but the MQTT server comunicates the same way and therefor gets to be its own function
void jsonDealings(String command_){
  DynamicJsonDocument jsonDoc(1024);
  DeserializationError error = deserializeJson(jsonDoc, command_);
  //Serial.println(command_);
  if(jsonDoc.containsKey("command")){
    String command = jsonDoc["command"];
    if((command == "setTime")){
      if(jsonDoc.containsKey("currentTime")){
        DynamicJsonDocument t1m3(256);
        String wapwap = jsonDoc["currentTime"];
        //Serial.println(wapwap);
        DeserializationError error = deserializeJson(t1m3, wapwap);
        if(error){
          Serial.println(F("Failed to deserialize currentTime"));
          return;
        }
        SetTime(ClockDecoder(t1m3));
      }
    }
    if((command == "setClockFace")){
      if(jsonDoc.containsKey("currentTime")){
        DynamicJsonDocument t1m3(256);
        String wapwap = jsonDoc["currentTime"];
        //Serial.println(wapwap);
        DeserializationError error = deserializeJson(t1m3, wapwap);
        if(error){
          Serial.println(F("Failed to deserialize currentTime"));
          return;
        }
        timeO.clockFaceUs = CalculateUs(ClockDecoder(t1m3));
      }
    }
  }
  else if(jsonDoc.containsKey("page")){
    String page = jsonDoc["page"];
    if(page == "home"){
      HTML_HomeSendOnLoad();
    }
    if(page == "wifi"){
      HTML_WIFISendOnLoad();
    }
  }
  else if(jsonDoc.containsKey("wifi")){
    if(jsonDoc.containsKey("ssid")){
      String temp = jsonDoc["ssid"];
      clockSettings.ssid = temp;
    }
    if(jsonDoc.containsKey("password")){
      String temp = jsonDoc["password"];
      clockSettings.passward = temp;
    }
    SaveSettings();
    //Try to connect WiFi
    Serial.println("Restarting ESP");
    delay(500);
    esp_restart();
  }
}

//Function for checking websocket events and parsing the Json that is recived from the webcite
void OnWebsockedEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
  if (type == WS_EVT_CONNECT){
    Serial.println("WebSocket Client Connected");
  }
  else if (type == WS_EVT_DATA){
    //OnWebsockedMessage(arg, data, len);
    String receivedJson = String((char*)data).substring(0,len);
    Serial.println(receivedJson);
    jsonDealings(receivedJson);
  }
  
}

//Function for setting up the wifi conneciton
void SetupWiFi(){
  Serial.println("Setting Up WIFI\n");
  
  bool apmode = false;
  if(!Connect_WIFI()){
    apmode = WiFiAPMode();//Needs error handeling but oh well
  }

  if(!apmode){
    server.on("/", HTTP_GET, [](AsyncWebServerRequest*request){
      request->send_P(200,"text/html", SpiffsReadTextToChar("/first.html"));
    });
  }
  else{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest*request){
      request->send_P(302, "text/html", SpiffsReadTextToChar("/wifi.html"));
    });

    server.onNotFound([](AsyncWebServerRequest *request) {
      Serial.printf("onNotFound requested: %s\n", request->url().c_str());
      
      if (request->method() == HTTP_GET) {
        // Redirect to login page
        request->redirect("/");
      }
    });
  }
  //first load of the page redirects you to the /Main page
  
  //new meathod for loading the HTML. works by streaming the file from spiffs indtead of loading it into ram first
  server.serveStatic("/home", FILESYSTEM, "/home.html").setCacheControl("max-age=600");
  server.serveStatic("/wifi", FILESYSTEM, "/wifi.html").setCacheControl("max-age=600");
  server.serveStatic("/styles.css", FILESYSTEM, "/styles.css").setCacheControl("max-age=600");
  server.serveStatic("/common.js", FILESYSTEM, "/common.js").setCacheControl("max-age=600");

  // This guy is going to be for updating the sensor value
  // At some point I am going to want to swap this out for MQTT but I think I'm going to stick with websokets untill we get a server up and running
  ws.onEvent(OnWebsockedEvent);
  server.addHandler(&ws);

  server.begin();

  Serial.println("WIFI Set Up\n");
}

#pragma endregion

#pragma region OTA
void MarkAppValid(){
  //Rollback feature incase anything fails
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
    Serial.println("App marked as valid.");
  } else {
    Serial.println("App was already valid or not in rollback state.");
  }
}

//Function for setting up the OTA service (I would rather this thing sit on the shelf)
void SetupOTA(){

  ArduinoOTA.setHostname("ESP32-S3-OTA");
  ArduinoOTA.setPort(3232);

  Serial.print("ota adress: ");
  Serial.println(ArduinoOTA.getHostname());

  ArduinoOTA.onStart([]() {
  Serial.println("Start updating...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nUpdate finished.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}

#pragma endregion

#pragma region Time

DynamicJsonDocument BuildTimeJason(CLOCKO foop){
  DynamicJsonDocument jsonDoc(1024);
  jsonDoc["hour"] = foop.hours + 1;
  jsonDoc["minutes"] = foop.minutes;
  jsonDoc["seconds"] = foop.seconds;
  jsonDoc["milliseconds"] = foop.milliseconds;
  jsonDoc["microseconds"] = foop.microseconds;
  jsonDoc["rate"] = foop.rate;
  return jsonDoc;
}

//Function for going from 
int64_t CalculateUs(CLOCKO tt){
  int64_t currentTimeinUs = 0;
  currentTimeinUs += tt.hours * timeO.uSPer.hours;
  currentTimeinUs += tt.minutes * timeO.uSPer.minutes;
  currentTimeinUs += tt.seconds * timeO.uSPer.seconds;
  currentTimeinUs += tt.milliseconds * timeO.uSPer.milliseconds;
  currentTimeinUs += tt.microseconds * timeO.uSPer.microseconds;
  return currentTimeinUs;
}

CLOCKO CalculateClock(int64_t tt){
  CLOCKO boof;
  int64_t remainder = tt % timeO.usPerCycle;;

  boof.hours = remainder / timeO.uSPer.hours;
  remainder = remainder % timeO.uSPer.hours;

  boof.minutes = remainder / timeO.uSPer.minutes;
  remainder = remainder % timeO.uSPer.minutes;

  boof.seconds = remainder / timeO.uSPer.seconds;
  remainder = remainder % timeO.uSPer.seconds;

  boof.milliseconds = remainder / timeO.uSPer.milliseconds;
  remainder = remainder % timeO.uSPer.milliseconds;

  boof.microseconds = remainder / timeO.uSPer.microseconds;
  remainder = remainder % timeO.uSPer.microseconds;

  return boof;
}

//This function should calculate and set the offset in order
void SetTime(CLOCKO tt){
  timeO.timeOffset = CalculateUs(tt) - timeO.currentUs;
}

//This function is to prevent the esp to think that the clock is desynced even when
//because these numbers are looping, the max they can be appart is one half of the max value
int64_t wrappedDifference(int64_t a, int64_t b, int64_t max) {
  int64_t diff = a - b;

  if(abs(diff) > (max / 2)){
    // Handle wrap-around
    if (diff > 0) {
      diff = diff - max;      // Positive: make it negative
    } else {
      diff = -max - diff;     // Negative: subtract max from already negative value
    }
  }
  
  return diff;
}

#pragma endregion

#pragma region Setup and main loop
void setup() {

  Serial.begin(115200);

  MarkAppValid();

  //Setup Database and Settings
  SetupSettingsAndDatabase();

  //Setup Wifi
  SetupWiFi();

  SetupOTA();

  //Setup Stepper
  SetupStepper();
  
  //OTA App mark
  MarkAppValid();

  //Clock Calculation (hours per cyle * minutes per hour * seconds per minute * ms per second * us per ms)
  timeO.usPerCycle = int64_t(HOURS_PER_CYCLE * 60 * 60 * 1000) * int64_t(1000);
  timeO.uSPer.hours = int64_t(60 * 60 * 1000) * int64_t(1000);
  timeO.uSPer.minutes = 60 * 1000 * 1000;
  timeO.uSPer.seconds = 1000 * 1000;
  timeO.uSPer.milliseconds = 1000;
  timeO.uSPer.microseconds = 1;

  //precalculation of the double time rate
  stepvar.doubleTimeRate = double(stepvar.stepTime) / double(stepvar.doubleTime);
}

//Non blocking main loop any functions held within this loop must also be non blocking
void loop() {
  timeO.currentUs = esp_timer_get_time();

  //Before all else, Make sure to tick the stepper
  if((stepvar.nextStep - timeO.currentUs) <= 0){
    int64_t diff = wrappedDifference(timeO.clockFaceUs, timeO.dayUs, timeO.usPerCycle);

    //Debug Code
    // static int i = 1;
    // if (i <= 20){
    //   Serial.println(diff);
    //   i++;
    // }
    // else{
    //   i = 1;
    // }

    //This is a check that should super speed the clock face till it is close enough to the set time
    if (abs(diff) <= 100000){
      //Step forward at regular rate
      stepvar.nextStep += stepvar.stepTime;
      timeO.clockFaceUs += stepvar.stepTime;
      timeO.faceRate = 1;
      //not a garentee step anymore
      stepvar.stepCount++;
      stepper.step();
    }
    else{
      //make the clock take the shortest path to match the real time
      if(diff < 0){
        //Step forward at double rate
        stepvar.nextStep += stepvar.doubleTime;
        timeO.clockFaceUs += stepvar.stepTime;
        timeO.faceRate = stepvar.doubleTimeRate;
        stepvar.stepCount++;
        stepper.step();
      }
      else{
        //Hold the step
        stepvar.nextStep += stepvar.doubleTime;
        timeO.faceRate = 0;
      }
    }
    
  }
  else{
    //Upgrades people! Upgrades!!! ＼(｀0´)／
    ArduinoOTA.handle();

    //Clock Calculations
    if((timeO.nextCalculation - timeO.currentUs) <= 0){
      timeO.nextCalculation += timeO.calculationTime;
      // Calculate elapsed time since start
      timeO.elapsedUs = timeO.currentUs + timeO.timeOffset;
      // Calculate remainder within 24 hours
    
      timeO.dayUs = timeO.elapsedUs % timeO.usPerCycle;
      int64_t remainder = timeO.dayUs;

      timeO.clock.hours = remainder / timeO.uSPer.hours;
      remainder = remainder % timeO.uSPer.hours;

      timeO.clock.minutes = remainder / timeO.uSPer.minutes;
      remainder = remainder % timeO.uSPer.minutes;

      timeO.clock.seconds = remainder / timeO.uSPer.seconds;
      remainder = remainder % timeO.uSPer.seconds;

      timeO.clock.milliseconds = remainder / timeO.uSPer.milliseconds;
      remainder = remainder % timeO.uSPer.milliseconds;

      timeO.clock.microseconds = remainder / timeO.uSPer.microseconds;
      remainder = remainder % timeO.uSPer.microseconds;
    }

    //page update call Currently just sends the clock every second
    if((wpd.nextPageUpdate - timeO.currentUs) <= 0){
      wpd.nextPageUpdate += wpd.pageUpdateTime;
      DynamicJsonDocument jsonDoc(1024);
      jsonDoc["timeClock"] = BuildTimeJason(timeO.clock);
      //the clock face doesn't have a CLOCKO type and I need rate to be sent with the clock face
      CLOCKO ttt = CalculateClock(timeO.clockFaceUs);
      ttt.rate = timeO.faceRate;
      jsonDoc["faceClock"] = BuildTimeJason(ttt);
      char SendBuf[512];
      serializeJson(jsonDoc, SendBuf, sizeof(SendBuf));
      //Serial.println(SendBuf);
      ws.textAll(SendBuf);
    }
  }
}

#pragma endregion