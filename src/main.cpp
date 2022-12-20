#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include <ESP8266WebServer.h>
#include <addons/RTDBHelper.h>
#include <Firebase_ESP_Client.h>
#include "LittleFS.h"
#include <Adafruit_NeoPixel.h>

// GLOBAL VARIABLE
#define PIN_OUT D5
#define POOLING_WIFI 5000

String getMac = WiFi.macAddress();
String GEN_ID_BY_MAC = String(getMac);
String ID_DEVICE;
String TYPE_DEVICE = "COLOR";
bool STATUS_PIN = false;
int NUM_LED = 26;
String DATABASE_URL = "esp8266-device-db-default-rtdb.firebaseio.com";

// JSON DOCUMENT
DynamicJsonDocument bufferBodyPaserModeAP(8192);
DynamicJsonDocument bufferResponseModeAP(8192);
DynamicJsonDocument COLOR_STATE(255);

// FUNCTION PROTOTYPE - TASK
float_t checkRam();
void setupWifiModeAP();
void initColorDefault();
void poolingCheckWifi();
void checkFirebaseInit();
void firebaseFollowData();
void firebaseFollowNumLed();
void checkWifiConnection();
void setupWifiModeStation();
void setupWebserverModeAP();
void setupWebserverModeStation();
void initColorValue(FirebaseJson &ctx);

// FUNCTION PROTOTYPE - COMPONENT
void setUpPinMode();
void maybeSwitchMode();

class EepromMiru
{

public:
  String DATABASE_NODE = "";

  EepromMiru(int size)
  {
    this->size = size;
    this->updateDatabaseNode(this->readUserID(), this->readNodeID());
  }

  void resetAll()
  {
    EEPROM.begin(this->size);
    for (unsigned int i = 0; i < this->size; i++)
    {
      EEPROM.write(i, NULL);
    }
    EEPROM.end();
    this->updateDatabaseNode("", "");
  }

  String readRaw()
  {
    EEPROM.begin(this->size);
    String temp;
    for (unsigned int i = 0; i < this->size; i++)
    {
      uint8_t num = EEPROM.read(i);
      temp = String(temp + (char)num);
    }
    EEPROM.end();
    return temp;
  }

  bool saveSSID(String ssid)
  {
    return this->checkWrite(this->addr_ssid, ssid, this->ssid);
  }
  bool savePassword(String password)
  {
    return this->checkWrite(this->addr_password, password, this->password);
  }
  bool saveUserID(String user_id)
  {
    this->updateDatabaseNode(user_id, this->readNodeID());
    return this->checkWrite(this->addr_userID, user_id, this->userID);
  }
  bool saveNodeID(String node_id)
  {
    this->updateDatabaseNode(this->readUserID(), node_id);
    return this->checkWrite(this->addr_NodeID, node_id, this->NodeID);
  }

  String readSSID()
  {
    return this->checkRead(this->addr_ssid, this->ssid);
  }
  String readPassword()
  {
    return this->checkRead(this->addr_password, this->password);
  }
  String readNodeID()
  {
    return this->checkRead(this->addr_NodeID, this->NodeID);
  }
  String readUserID()
  {
    return this->checkRead(this->addr_userID, this->userID);
  }

private:
  int size = 1024;
  String ssid = "";
  int addr_ssid = 0;
  String password = "";
  int addr_password = 50;
  String userID = "";
  int addr_userID = 100;
  String NodeID = "";
  int addr_NodeID = 150;

  String checkRead(int addr, String &cacheValue)
  {
    if (cacheValue.length() > 0)
    {
      return cacheValue;
    }
    else
    {
      String temp = this->readKey(addr);
      if (temp != cacheValue)
      {
        cacheValue = temp;
      }
      return temp;
    }
  }

  String readKey(int addr)
  {
    EEPROM.begin(this->size);
    String temp = "";
    for (unsigned int i = addr; i < addr + 50; i++)
    {
      uint8_t character = EEPROM.read(i);
      if (character != NULL)
      {
        temp = String(temp + (char)character);
      }
      else
      {
        break;
      }
    }
    EEPROM.end();
    return temp;
  }

  bool writeKey(int addr, String tmp)
  {
    int len_str = tmp.length();
    if (len_str > 50)
    {
      return false;
    }
    else
    {
      EEPROM.begin(this->size);
      int start = 0;
      for (unsigned int i = addr; i < addr + 50; i++)
      {
        if (i < (addr + len_str))
        {
          EEPROM.write(i, (uint8_t)tmp[start]);
          start++;
        }
        else
        {
          EEPROM.write(i, NULL);
        }
      }
      EEPROM.end();
      return true;
    }
  }

  bool checkWrite(int addr, String tmp, String &cached)
  {
    bool check = this->writeKey(addr, tmp);
    if (check)
    {
      cached = tmp;
    }
    return check;
  }

  void updateDatabaseNode(String uid, String nid)
  {
    this->DATABASE_NODE = String("/user-" + uid + "/nodes/" + nid);
  }
};

// [********* INSTANCE *********]

// => EEPROM
EepromMiru eeprom(200);
// => SERVER
ESP8266WebServer server_mode_ap(80);
ESP8266WebServer server_mode_station(80);
// => TASKSHEDULE
Scheduler runner;
// => FIREBASE
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
// => PIXEL LEDs RGB
Adafruit_NeoPixel pixels(NUM_LED, PIN_OUT, NEO_GRB + NEO_KHZ800);

// TASK
Task miruSetupWifiAPMode(TASK_IMMEDIATE, TASK_ONCE, &setupWifiModeAP, &runner);
Task miruSetupWebServerAPMode(TASK_IMMEDIATE, TASK_FOREVER, &setupWebserverModeAP, &runner);
Task miruSetupWifiStationMode(TASK_IMMEDIATE, TASK_ONCE, &setupWifiModeStation, &runner);
Task miruSetupWebServerStationMode(TASK_IMMEDIATE, TASK_FOREVER, &setupWebserverModeStation, &runner);
Task miruCheckWifiConnection(TASK_SECOND, TASK_FOREVER, &checkWifiConnection, &runner);
Task miruFirebaseCheck(TASK_IMMEDIATE, TASK_ONCE, &checkFirebaseInit, &runner);
Task miruFirebaseFollowData(300, TASK_FOREVER, &firebaseFollowData, &runner);
Task miruFirebaseFollowNumLed(5000, TASK_FOREVER, &firebaseFollowNumLed, &runner);
Task miruPoolingCheckWifi(POOLING_WIFI, TASK_FOREVER, &poolingCheckWifi, &runner);

// WIFI MODE - AP
String mode_ap_ssid = "esp-8266-";
String mode_ap_pass = "44448888";
String mode_ap_ip = "192.168.1.120";
String mode_ap_gateway = "192.168.1.1";
String mode_ap_subnet = "255.255.255.0";
String pathFile = "/";

// WIFI MODE - STATION

void setup()
{
  Serial.begin(115200);
  Serial.println();
  LittleFS.begin();

  Serial.println("[RUN]");
  config.database_url = DATABASE_URL;
  config.signer.test_mode = true;
  GEN_ID_BY_MAC.replace(":", "");
  ID_DEVICE = String("device-" + GEN_ID_BY_MAC + "-1");
  Firebase.begin(&config, &auth);
  initColorDefault();
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.clear(); // Set all pixel colors to 'off'

  setUpPinMode();
  runner.startNow();
  maybeSwitchMode();
}

void loop()
{
  // put your main code here, to run repeatedly:
  runner.execute();
}

void checkFirebaseInit()
{

  // [Check] - NodeID is exist
  String devicePath = String("/devices/" + ID_DEVICE);

  Firebase.RTDB.getJSON(&fbdo, eeprom.DATABASE_NODE + devicePath);

  FirebaseJson node = fbdo.jsonObject();

  String value = String(devicePath + "/value");
  String num = String(devicePath + "/num");
  String type = String(devicePath + "/type");
  String pin = String(devicePath + "/pin");

  FirebaseJson JsonFixNode = node;

  if(!JsonFixNode.isMember("value/r")) {
    JsonFixNode.set("value/r", 0);
  }
  if(!JsonFixNode.isMember("value/g")) {
    JsonFixNode.set("value/g", 0);
  }
  if(!JsonFixNode.isMember("value/b")) {
    JsonFixNode.set("value/b", 0);
  }
  if(!JsonFixNode.isMember("value/contrast")) {
    JsonFixNode.set("value/contrast", 0);
  }

  if (!JsonFixNode.isMember(type))
  {
    JsonFixNode.add("type", TYPE_DEVICE);
  }
  if (!JsonFixNode.isMember(num))
  {
    JsonFixNode.add("num", NUM_LED);
  }
  if (!JsonFixNode.isMember(pin))
  {
    JsonFixNode.add("pin", PIN_OUT);
  }
  Firebase.RTDB.updateNode(&fbdo, eeprom.DATABASE_NODE + devicePath, &JsonFixNode);
  // JsonFixNode.clear();
  Serial.println("Enabled Task!");
  miruFirebaseFollowData.enableIfNot();

}

void initColorValue(FirebaseJson &ctx) {
  ctx.add("r", 0);
  ctx.add("g", 0);
  ctx.add("b", 0);
  ctx.add("contrast", 0);
}

void initColorDefault() {
  COLOR_STATE["r"] = 0;
  COLOR_STATE["g"] = 0;
  COLOR_STATE["b"] = 0;
  COLOR_STATE["contrast"] = 0;
}

void firebaseFollowNumLed() {
  String pathValue = String(eeprom.DATABASE_NODE + "/devices/" + ID_DEVICE + "/num");
  Firebase.RTDB.getJSON(&fbdo, pathValue);
  int TEMP_NUM_LED = fbdo.to<int>();
  if (fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_integer && NUM_LED != TEMP_NUM_LED)
  { 
    pixels.updateLength(TEMP_NUM_LED);
    NUM_LED = TEMP_NUM_LED;
  }
}

void firebaseFollowData()
{
  String pathValue = String(eeprom.DATABASE_NODE + "/devices/" + ID_DEVICE + "/value");
  Firebase.RTDB.getJSON(&fbdo, pathValue);
  if (fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_json)
  {
    bool isChanged = false;
    bool init = false;
    FirebaseJson color = fbdo.jsonObject();
    // read color here
    if(miruFirebaseFollowData.isFirstIteration()) {
      init = true;
      miruFirebaseFollowNumLed.enableIfNot();
    }
    String temp;
    color.toString(temp);
    DynamicJsonDocument COLOR_TEMP(255);
    deserializeJson(COLOR_TEMP, temp);
    if(COLOR_STATE["r"] != COLOR_TEMP["r"] || init) {
      COLOR_STATE["r"] = COLOR_TEMP["r"];
      isChanged = true;
    }
    if(COLOR_STATE["b"] != COLOR_TEMP["b"] || init) {
      COLOR_STATE["b"] = COLOR_TEMP["b"];
      isChanged = true;
    }
    if(COLOR_STATE["g"] != COLOR_TEMP["g"] || init) {
      COLOR_STATE["g"] = COLOR_TEMP["g"];
      isChanged = true;
    }
    if(COLOR_STATE["contrast"] != COLOR_TEMP["contrast"] || init) {
      COLOR_STATE["contrast"] = COLOR_TEMP["contrast"];
      isChanged = true;
    }
    COLOR_TEMP.clear();
    if(isChanged) {
      Serial.println("[COLOR - CHANGED]");
      // change color here
      pixels.clear(); // Set all pixel colors to 'off'
      for(int i = 0; i < NUM_LED; i++) { // For each pixel...
        // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
        // Here we're using a moderately bright green color:
        pixels.setPixelColor(i, pixels.Color(COLOR_STATE["r"], COLOR_STATE["g"], COLOR_STATE["b"]));
      }
      pixels.setBrightness(COLOR_STATE["contrast"]);
      pixels.show();   // Send the updated pixel colors to the hardware.
      // Serial.println(String("rgb(" + String(COLOR_STATE["r"]) + ", " + String(COLOR_STATE["g"]) + ", " + String(COLOR_STATE["b"]) + ")"));
    }
  }
}

void changeColor() {

}

void poolingCheckWifi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    miruCheckWifiConnection.enableIfNot();
  }
}

void setupWifiModeStation()
{
  // ACTIVE MODE AP
  String ssid = eeprom.readSSID();
  String password = eeprom.readPassword();
  WiFi.begin(ssid, password);
  miruCheckWifiConnection.enable();
}

void setupWifiModeAP()
{
  // ACTIVE MODE AP
  WiFi.softAP(String(mode_ap_ssid + GEN_ID_BY_MAC), mode_ap_pass);
  miruSetupWebServerAPMode.enable();
}

void checkWifiConnection()
{
  // enable check wifi connection
  if (WiFi.status() == WL_CONNECTED)
  {
    miruSetupWebServerStationMode.enableIfNot();
    miruFirebaseCheck.enableIfNot();
    miruPoolingCheckWifi.enableIfNot();
    miruCheckWifiConnection.cancel();
  }
}

void setUpPinMode()
{
  pinMode(PIN_OUT, OUTPUT);
  digitalWrite(PIN_OUT, STATUS_PIN ? HIGH : LOW);
}

void setupWebserverModeStation()
{
  if (miruSetupWebServerStationMode.isFirstIteration())
  {
    // [GET] - ROUTE: '/' => Render UI interface
    server_mode_station.serveStatic("/", LittleFS, "/index.minify.html");
    server_mode_station.begin();
  }
  server_mode_station.handleClient();
}

void setupWebserverModeAP()
{
  if (miruSetupWebServerAPMode.isFirstIteration())
  {
    // [GET] - ROUTE: '/' => Render UI interface
    server_mode_ap.serveStatic("/", LittleFS, "/index.minify.html");
    // [GET] - ROUTE: '/reset-config' => Reset config WIFI
    server_mode_ap.on("/scan-network", HTTP_GET, []()
                      {
      String responseTemp;
      int8_t lenNetwork = WiFi.scanNetworks();
      JsonArray networks = bufferResponseModeAP.createNestedArray("networks");
      for (size_t i = 0; i < lenNetwork; i++)
      {
        String name = WiFi.SSID(i);
        int32_t quality = WiFi.RSSI(i);
        DynamicJsonDocument temp(200);
        JsonObject network = temp.to<JsonObject>();
        network["name"] = name;
        network["quality"] = quality;
        networks.add(network);
        temp.clear();
      }
      serializeJson(bufferResponseModeAP, responseTemp);
      server_mode_ap.send(200, "application/json", responseTemp);
      bufferResponseModeAP.clear(); });
    // [GET] - ROUTE: '/is-config' => Check WIFI is configuration
    server_mode_ap.on("/is-config", HTTP_GET, []()
                      {
      String ssid = eeprom.readSSID();
      String password = eeprom.readPassword();
      if(ssid.length() > 0 && password.length() > 0) {
        String response_str;
        DynamicJsonDocument response_json(200);
        JsonObject payload = response_json.to<JsonObject>();
        payload["ssid"] = ssid;
        payload["password"] = password;
        payload["ip-station"] = WiFi.localIP();
        payload["status-station"] = WiFi.status() == WL_CONNECTED ? true : false;
        payload["quality-station"] = WiFi.RSSI();
        payload["message"] = "WIFI HAS BEEN CONFIG";
        serializeJson(payload, response_str);
        server_mode_ap.send(200, "application/json", response_str);
        response_json.clear();
      }else {
        server_mode_ap.send(200, "application/json", "{\"message\":\"WIFI NOT YET CONFIG\"}");
      } });
    // [POST] - ROUTE: '/reset-config-wifi' => Reset config WIFI
    server_mode_ap.on("/reset-config-wifi", HTTP_POST, []()
                      {
      String ssid = eeprom.readSSID();
      String password = eeprom.readPassword();
      if(ssid.length() > 0 && password.length() > 0) {
        eeprom.saveSSID("");
        eeprom.savePassword("");
      }
      server_mode_ap.send(200, "application/json", "{\"message\":\"RESET CONFIG SUCCESSFULLY\"}");
      WiFi.disconnect(); });
    // [POST] - ROUTE: '/config-wifi' => Goto config WIFI save below EEPROM
    server_mode_ap.on("/config-wifi", HTTP_POST, []()
                      {
      if(server_mode_ap.hasArg("plain")) {
        deserializeJson(bufferBodyPaserModeAP, server_mode_ap.arg("plain"));
        bufferBodyPaserModeAP.shrinkToFit();
        JsonObject body = bufferBodyPaserModeAP.as<JsonObject>();
        String ssid = body["ssid"];
        String password = body["password"];
        eeprom.saveSSID(ssid);
        eeprom.savePassword(password);
        // Serial.println("[Save config EEPROM]");
        server_mode_ap.send(200, "application/json", "{\"message\":\"CONFIGURATION WIFI SUCCESSFULLY\"}");
        WiFi.begin(ssid, password);
      }else {
        server_mode_ap.send(403, "application/json", "{\"message\":\"NOT FOUND PAYLOAD\"}");
      }
      bufferBodyPaserModeAP.clear(); });
    // [POST] - ROUTE: '/link-app' => Goto config firebase save below EEPROM
    server_mode_ap.on("/link-app", HTTP_POST, []()
                      {
      if(server_mode_ap.hasArg("plain")) {
        if(eeprom.DATABASE_NODE) {
          Firebase.RTDB.deleteNode(&fbdo, eeprom.DATABASE_NODE);
        }
        deserializeJson(bufferBodyPaserModeAP, server_mode_ap.arg("plain"));
        bufferBodyPaserModeAP.shrinkToFit();
        JsonObject body = bufferBodyPaserModeAP.as<JsonObject>();
        bool checkIDUser = body["idUser"].isNull();
        bool checkIDNode = body["idNode"].isNull();
        String idUser = body["idUser"];
        String idNode = body["idNode"];
        if(!checkIDUser && !checkIDNode) {
          eeprom.saveNodeID(idNode);
          eeprom.saveUserID(idUser);
          server_mode_ap.send(200, "application/json", "{\"message\":\"LINK APP HAS BEEN SUCCESSFULLY\"}");
          miruFirebaseCheck.restart();
        }else {
          if(checkIDUser) {
            server_mode_ap.send(200, "application/json", "{\"message\":\"ID USER IS NULL\"}");
          }else {
            server_mode_ap.send(200, "application/json", "{\"message\":\"ID NODE IS NULL\"}");
          }
        }
      }else {
        server_mode_ap.send(403, "application/json", "{\"message\":\"NOT FOUND PAYLOAD\"}");
      }
      bufferBodyPaserModeAP.clear(); });
    // [GET] - ROUTE: '/is-link-app' => Check configuration link-app
    server_mode_ap.on("/is-link-app", HTTP_GET, []()
                      {
      String nodeID = eeprom.readNodeID();
      String userID = eeprom.readUserID();
      if(nodeID.length() && userID.length()) {
        String response_str;
        DynamicJsonDocument response_json(200);
        JsonObject payload = response_json.to<JsonObject>();
        payload["nodeID"] = nodeID;
        payload["userID"] = userID;
        payload["message"] = "CONFIG IS FOUND";
        serializeJson(payload, response_str);
        server_mode_ap.send(200, "application/json", response_str);
        response_json.clear();
      }else {
        server_mode_ap.send(403, "application/json", "{\"message\":\"CONFIG NOT FOUND\"}");
      } });

    // START WEBSERVER
    server_mode_ap.begin();
  }
  server_mode_ap.handleClient();
}

void turnOffModeAP()
{
  server_mode_ap.close();
  miruSetupWebServerAPMode.cancel();
  WiFi.softAPdisconnect(true);
}

float_t checkRam()
{
  uint32_t ramSize = ESP.getFreeHeap();
  return ((float_t)ramSize / (float_t)80000) * (float_t)100;
}

void maybeSwitchMode()
{
  WiFi.mode(WIFI_AP_STA);
  // turn on network(mode - STATION)
  miruSetupWifiStationMode.enable();
  // turn on network(mode - AP)
  miruSetupWifiAPMode.enable();
}
