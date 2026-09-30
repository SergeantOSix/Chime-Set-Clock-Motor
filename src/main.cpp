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

StepperDriver stepper(5, 6, 7, 15);

struct STEPPERVAR
{
  int64_t nextStep = 10000000;
  int32_t stepTime = 14648;
  int64_t stepCount = 0;
}stepvar;


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
  stepper.setDirection(-1);
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
  //jsonDoc["name"] = rigSettings.rigName;
  //jsonDoc["PWM"] = ESC.pulseWidth;
  //jsonDoc["status"] = misc.lastStatus;
  //jsonDoc["testType"] = TestToString(rigSettings.testType);
  String Send;
  serializeJson(jsonDoc, Send);
  ws.textAll(Send);
}

//A funtion for populating the main webpage
void HTML_WIFISendOnLoad(){
  DynamicJsonDocument jsonDoc(1024);
  jsonDoc["ssid"] = clockSettings.ssid;
  jsonDoc["password"] = clockSettings.passward;
  String Send;
  serializeJson(jsonDoc, Send);
  ws.textAll(Send);
}

//This use to be for just the webpage but the MQTT server comunicates the same way and therefor gets to be its own function
void jsonDealings(String command_){
  DynamicJsonDocument jsonDoc(1024);
  DeserializationError error = deserializeJson(jsonDoc, command_);
  Serial.println(command_);
  if(jsonDoc.containsKey("command")){
    String command = jsonDoc["command"];
    if((command == "Quiet")){
      //Command Code Here-
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

  // This guy is going to be for updating the sensor value
  // At some point I am going to want to swap this out for MQTT but I think I'm going to stick with websokets untill we get a server up and running
  ws.onEvent(OnWebsockedEvent);
  server.addHandler(&ws);

  server.begin();

  Serial.println("WIFI Set Up\n");
}

#pragma endregion

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

  MarkAppValid();

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
    Serial.printf("Progress: %u%%\r\n", (progress * 100) / total);
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

void setup() {

  Serial.begin(115200);

  //Setup Database and Settings
  SetupSettingsAndDatabase();

  //Setup Wifi
  SetupWiFi();

  SetupOTA();

  //Setup Stepper
  SetupStepper();
  
  MarkAppValid();
}

//Non blocking main loop any functions held within this loop must also be non blocking
void loop() {
  int64_t currentUs = esp_timer_get_time();

  //Before all else, Make sure to tick the stepper
  if((stepvar.nextStep - currentUs) <= 0){
    stepvar.nextStep += stepvar.stepTime;
    stepper.step();
    stepvar.stepCount++;
  }
  else{
    //Upgrades people! Upgrades!!! ＼(｀0´)／
    ArduinoOTA.handle();
  }
}