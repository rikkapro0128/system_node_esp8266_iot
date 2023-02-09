#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include <ESP8266WebServer.h>
#include <addons/RTDBHelper.h>
#include <Firebase_ESP_Client.h>
#include "LittleFS.h"

// GLOBAL VARIABLE
#define PIN_OUT D2
#define POOLING_WIFI 5000

#define _DEBUG_

const char *CHost = "plant.io";
String getMac = WiFi.macAddress();
String GEN_ID_BY_MAC = String(getMac);
String ID_DEVICE;
String TYPE_DEVICE = "LOGIC";
bool STATUS_PIN = false;
String DATABASE_URL = "";
uint32_t ramSize;
float_t flashSize = 80000;
float_t percent = 100;

// JSON DOCUMENT
DynamicJsonDocument bufferBodyPaserModeAP(8192);
DynamicJsonDocument bufferResponseModeAP(8192);

// FUNCTION PROTOTYPE - TASK
void checkRam();
void checkWifi();
void checkRequestComing();
void firebaseFollowData();
void checkFirebaseInit();
void setupWifiModeStation();
void setupWebserverModeAP();

// FUNCTION PROTOTYPE - COMPONENT
#ifdef _DEBUG_
void viewEEPROM();
#endif
void scanListNetwork();
void setUpPinMode();
void maybeSwitchMode();
void checkLinkAppication();
void linkAppication();
void addConfiguration();
void resetConfiguration();
void checkConfiguration();

class EepromMiru
{

public:
  String DATABASE_NODE = "";
  String databaseUrl = "";
  bool canAccess = false;

  EepromMiru(int size, String url_database = "")
  {
    this->size = size;
    this->databaseUrl = url_database;
  }

  void begin()
  {
    this->DATABASE_NODE = this->readDatabaseUrlNode();
    this->checkAccess();
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
    bool state = this->checkWrite(this->addr_userID, user_id, this->userID);
    this->updateDatabaseNode(user_id, this->readNodeID());
    return state;
  }
  bool saveNodeID(String node_id)
  {
    bool state = this->checkWrite(this->addr_NodeID, node_id, this->NodeID);
    this->updateDatabaseNode(this->readUserID(), node_id);
    return state;
  }
  bool saveDatabaseUrl(String url)
  {
    this->maxCharacter = 120;
    bool state = this->checkWrite(this->addr_databaseUrl, url, this->databaseUrl);
    this->maxCharacter = 0;
    return state;
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
  String readDatabaseUrl()
  {
    this->maxCharacter = 120;
    String temp = this->checkRead(this->addr_databaseUrl, this->databaseUrl);
    this->maxCharacter = 0;
    return temp;
  }
  String readDatabaseUrlNode()
  {
    return String("/user-" + this->readUserID() + "/nodes/node-" + this->readNodeID());
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
  int addr_databaseUrl = 200;
  uint8_t maxCharacter = 0;

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
    for (unsigned int i = addr; i < this->maxCharacter || (addr + 50); i++)
    {
      uint8_t character = EEPROM.read(i);
      if (character <= 126 && character >= 32)
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

  bool writeKey(int addr, String tmp, uint8_t limit = 50)
  {
    int len_str = tmp.length();
    uint8_t checkLimit = this->maxCharacter ? this->maxCharacter : limit;
#ifdef _DEBUG_
    Serial.println(checkLimit);
#endif
    if (len_str > checkLimit)
    {
      return false;
    }
    else
    {
      EEPROM.begin(this->size);
      int start = 0;
      for (uint8_t i = addr; i < addr + checkLimit; i++)
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
      EEPROM.commit();
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

  void updateDatabaseNode(String uid = "", String nid = "")
  {
    this->DATABASE_NODE = String("/user-" + uid + "/nodes/node-" + nid);
    this->checkAccess();
  }
  void checkAccess()
  {
    String nodeId = this->readNodeID();
    String userId = this->readUserID();
    String dbURL = this->readDatabaseUrl();
    if (nodeId.length() > 0 && userId.length() > 0 && dbURL.length() > 0)
    {
      this->canAccess = true;
    }
    else
    {
      this->canAccess = false;
    }
  }
};

// [********* INSTANCE *********]

// => EEPROM
EepromMiru eeprom(400, "esp8266-device-db-default-rtdb.firebaseio.com");

// => SERVER
ESP8266WebServer server(80);

// => TASKSHEDULE
Scheduler runner;

// => FIREBASE
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// TASK
Task miruCheckRequestComming(500, TASK_FOREVER, &checkRequestComing, &runner, true);
Task miruSetupWifiStationMode(200, TASK_ONCE, &setupWifiModeStation, &runner, true);
Task miruFirebaseCheck(200, TASK_ONCE, &checkFirebaseInit, &runner);
Task miruFirebaseFollowData(200, TASK_FOREVER, &firebaseFollowData, &runner);
Task mirucheckRam(1000, TASK_FOREVER, &checkRam, &runner, true);
Task miruPoolingWiFi(2000, TASK_FOREVER, &checkWifi, &runner, true);

// WIFI MODE - AP
String mode_ap_ssid = "esp8266-";
String mode_ap_pass = "44448888";
String mode_ap_ip = "192.168.1.120";
String mode_ap_gateway = "192.168.1.1";
String mode_ap_subnet = "255.255.255.0";
String pathFile = "/";

// WIFI MODE - STATION

void setup()
{
#ifdef _DEBUG_
  Serial.begin(115200);
  Serial.println("");
  Serial.println("[---PROGRAM START---]");
#endif

  LittleFS.begin();

  eeprom.begin();
#ifdef _DEBUG_
  viewEEPROM();
#endif
  config.database_url = eeprom.databaseUrl;
  config.signer.test_mode = true;
  GEN_ID_BY_MAC.replace(":", "");
  ID_DEVICE = String("device-" + GEN_ID_BY_MAC + "-1");
  Firebase.begin(&config, &auth);

  setUpPinMode();
  setupWebserverModeAP();

  WiFi.hostname(CHost);
  WiFi.mode(WIFI_AP_STA);
  WiFi.persistent(true);
  WiFi.softAP(String(mode_ap_ssid + GEN_ID_BY_MAC), mode_ap_pass);

  runner.startNow();
}

void loop()
{
  // put your main code here, to run repeatedly:
  runner.execute();
}

#ifdef _DEBUG_
void viewEEPROM()
{
  String ssid = eeprom.readSSID();
  String password = eeprom.readPassword();
  String url = eeprom.readDatabaseUrl();
  String nodeID = eeprom.readNodeID();
  String userID = eeprom.readUserID();
  String dbNode = eeprom.DATABASE_NODE;
  String urlNode = eeprom.readDatabaseUrlNode();

  Serial.println("WIFI NAME = " + ssid + " | length = " + String(ssid.length()));
  Serial.println("WIFI PASS = " + password + " | length = " + String(password.length()));
  Serial.println("WIFI DATATBASE URL = " + url + " | length = " + String(url.length()));
  Serial.println("STATE URL = " + String(eeprom.canAccess));
  Serial.println("WIFI DATATBASE URL - NODE = " + dbNode + " | length = " + String(dbNode.length()));
  Serial.println("WIFI NODE ID = " + nodeID + " | length = " + String(nodeID.length()));
  Serial.println("WIFI USER ID = " + userID + " | length = " + String(userID.length()));
}
#endif

void checkFirebaseInit()
{
  // [Check] - NodeID is exist

  if (eeprom.canAccess)
  {
    String pathDevice = String(eeprom.DATABASE_NODE + "/devices");
    FirebaseJson json;

    json.set("device-1/state", false);
    json.set("device-1/type", TYPE_DEVICE);

    json.set("device-2/state", false);
    json.set("device-2/type", TYPE_DEVICE);

    json.set("device-3/state", false);
    json.set("device-3/type", TYPE_DEVICE);

    Firebase.RTDB.updateNodeAsync(&fbdo, pathDevice, &json);
    json.clear();
  }
  miruFirebaseFollowData.restart();
}

void firebaseFollowData()
{
  if (eeprom.canAccess)
  {
    String pathValue = String(eeprom.DATABASE_NODE + "/devices");
    Firebase.RTDB.getJSON(&fbdo, pathValue);
  }
  if (fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_json)
  {
#ifdef _DEBUG_
    Serial.println(fbdo.jsonString());
#endif
  }
  fbdo.clear();
}

void checkWifi()
{
#ifdef _DEBUG_
  Serial.println(String("Interation Firebase Check = " + String(miruFirebaseCheck.getIterations())));
#endif
  if (WiFi.status() == WL_CONNECTED)
  {
    miruFirebaseCheck.enable();
  }else if(WiFi.status() == WL_DISCONNECTED) {
    miruFirebaseCheck.setIterations(TASK_ONCE);
  }
}

void setupWifiModeStation()
{
  // ACTIVE MODE AP
  String ssid = eeprom.readSSID();
  String password = eeprom.readPassword();
#ifdef _DEBUG_
  Serial.println("<--- STATION WIFI --->");
  Serial.println("[SSID] = " + ssid);
  Serial.println("[PASSWORD] = " + password);
#endif
  if (ssid.length() > 0 && password.length() > 0)
  {
    WiFi.begin(ssid, password);
    WiFi.setAutoConnect(true);
    WiFi.setAutoConnect(true);
  }
}

void setUpPinMode()
{
  pinMode(PIN_OUT, OUTPUT);
  digitalWrite(PIN_OUT, STATUS_PIN ? HIGH : LOW);
}

void checkRequestComing()
{
  server.handleClient();
}

void setupWebserverModeAP()
{
  // [GET] - ROUTE: '/' => Render UI interface
  server.serveStatic("/", LittleFS, "/index.minify.html");
  // [GET] - ROUTE: '/reset-config' => Reset config WIFI
  // server.on("/scan-network", HTTP_GET, scanListNetwork);
  // [GET] - ROUTE: '/is-config' => Check WIFI is configuration
  server.on("/is-config", HTTP_GET, checkConfiguration);
  // [POST] - ROUTE: '/reset-config-wifi' => Reset config WIFI
  server.on("/reset-config-wifi", HTTP_POST, resetConfiguration);
  // [POST] - ROUTE: '/config-wifi' => Goto config WIFI save below EEPROM
  server.on("/config-wifi", HTTP_POST, addConfiguration);
  // [POST] - ROUTE: '/link-app' => Goto config firebase save below EEPROM
  server.on("/link-app", HTTP_POST, linkAppication);
  // [GET] - ROUTE: '/is-link-app' => Check configuration link-app
  server.on("/is-link-app", HTTP_GET, checkLinkAppication);

  // START WEBSERVER
  server.begin();
}

// [********* Func Request *********]

// [POST]
void linkAppication()
{
  if (server.hasArg("plain"))
  {
    if (eeprom.canAccess)
    {
      Firebase.RTDB.deleteNode(&fbdo, eeprom.DATABASE_NODE);
    }
    deserializeJson(bufferBodyPaserModeAP, server.arg("plain"));
    bufferBodyPaserModeAP.shrinkToFit();
    JsonObject body = bufferBodyPaserModeAP.as<JsonObject>();
    bool checkIDUser = body["idUser"].isNull();
    bool checkIDNode = body["idNode"].isNull();
    String idUser = body["idUser"];
    String idNode = body["idNode"];
#ifdef _DEBUG_
    Serial.println("Link App: idUser = " + idUser + " - idNode = " + idNode);
#endif
    if (!checkIDUser && !checkIDNode)
    {
      bool stateSaveNodeId = eeprom.saveNodeID(idNode);
      bool stateSaveUserId = eeprom.saveUserID(idUser);
      if(stateSaveNodeId && stateSaveUserId) {
        miruFirebaseCheck.restart();
        server.send(200, "application/json", "{\"message\":\"LINK APP HAS BEEN SUCCESSFULLY\"}");
      }else {
        server.send(500, "application/json", "{\"message\":\"FAILURE SAVE PAYLOADS\"}");
      }
    }
    else
    {
      if (checkIDUser)
      {
        server.send(200, "application/json", "{\"message\":\"ID USER IS NULL\"}");
      }
      else
      {
        server.send(200, "application/json", "{\"message\":\"ID NODE IS NULL\"}");
      }
    }
  }
  else
  {
    server.send(403, "application/json", "{\"message\":\"NOT FOUND PAYLOAD\"}");
  }
  bufferBodyPaserModeAP.clear();
}

// [POST]
void addConfiguration()
{
  if (server.hasArg("plain"))
  {
    deserializeJson(bufferBodyPaserModeAP, server.arg("plain"));
    bufferBodyPaserModeAP.shrinkToFit();
    JsonObject body = bufferBodyPaserModeAP.as<JsonObject>();

#ifdef _DEBUG_
    Serial.println("Add config: " + String(body));
#endif
    String ssid = body["ssid"];
    String password = body["password"];
    eeprom.saveSSID(ssid);
    eeprom.savePassword(password);
    // Serial.println("[Save config EEPROM]");
    server.send(200, "application/json", "{\"message\":\"CONFIGURATION WIFI SUCCESSFULLY\"}");
    miruSetupWifiStationMode.restart();
  }
  else
  {
    server.send(403, "application/json", "{\"message\":\"NOT FOUND PAYLOAD\"}");
  }
  bufferBodyPaserModeAP.clear();
}

// [POST]
void resetConfiguration()
{
  String ssid = eeprom.readSSID();
  String password = eeprom.readPassword();
#ifdef _DEBUG_
  Serial.println("reset config: wifi = " + ssid + " - password = " + password);
#endif
  if (ssid.length() > 0 && password.length() > 0)
  {
    eeprom.saveSSID("");
    eeprom.savePassword("");
  }
  server.send(200, "application/json", "{\"message\":\"RESET CONFIG SUCCESSFULLY\"}");
}

// [GET]
void checkLinkAppication()
{
  String nodeID = eeprom.readNodeID();
  String userID = eeprom.readUserID();
#ifdef _DEBUG_
  Serial.println("check link app: nodeID = " + nodeID + " - userID = " + userID);
#endif
  if (nodeID.length() && userID.length())
  {
    String response_str;
    DynamicJsonDocument response_json(200);
    JsonObject payload = response_json.to<JsonObject>();
    payload["nodeID"] = nodeID;
    payload["userID"] = userID;
    payload["message"] = "CONFIG IS FOUND";
    serializeJson(payload, response_str);
    server.send(200, "application/json", response_str);
    response_json.clear();
  }
  else
  {
    server.send(403, "application/json", "{\"message\":\"CONFIG NOT FOUND\"}");
  }
}

// [GET]
void checkConfiguration()
{
  String ssid = eeprom.readSSID();
  String password = eeprom.readPassword();
#ifdef _DEBUG_
  Serial.println("check config: ssid = " + ssid + " - password = " + password);
#endif
  if (ssid.length() > 0 && password.length() > 0)
  {
    String response_str;
    DynamicJsonDocument response_json(200);
    JsonObject payload = response_json.to<JsonObject>();
    payload["ssid"] = ssid;
    payload["password"] = password;
    payload["ip-station"] = WiFi.localIP();
    payload["status-station"] = WiFi.status();
    payload["quality-station"] = WiFi.RSSI();
    payload["message"] = "WIFI HAS BEEN CONFIG";
    serializeJson(payload, response_str);
    server.send(200, "application/json", response_str);
    response_json.clear();
  }
  else
  {
    server.send(200, "application/json", "{\"message\":\"WIFI NOT YET CONFIG\"}");
  }
}

// [GET]
void scanListNetwork()
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

#ifdef _DEBUG_
  Serial.println("list Wifi: " + responseTemp);
#endif
  server.send(200, "application/json", responseTemp);
  bufferResponseModeAP.clear();
}

void checkRam()
{
  ramSize = ESP.getFreeHeap();
#ifdef _DEBUG_
  Serial.println(String(((float_t)ramSize / flashSize * percent)) + " (kb)");
#endif
}
