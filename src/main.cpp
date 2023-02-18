#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <addons/RTDBHelper.h>
#include <Firebase_ESP_Client.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// GLOBAL VARIABLE
#define PIN_OUT 3
#define POOLING_WIFI 5000

#define _DEBUG_
#define _RELEASE_

const char *CHost = "plant.io";
String getMac = WiFi.macAddress();
String GEN_ID_BY_MAC = String(getMac);
String ID_DEVICE;
String TYPE_DEVICE = "LOGIC";
String removeAfter;
bool STATUS_PIN = false;
String DATABASE_URL = "";
uint32_t ramSize;
float_t flashSize = 80000;
float_t percent = 100;
bool reConnect = false;
bool reLinkApp = false;
bool isStream = false;
bool restartConfig = false;
size_t numTimer = 0;
unsigned long timerStack[30][3];
unsigned long epochTime;
FirebaseJson jsonNewDevice;
FirebaseJson timerParseArray;
FirebaseJsonData timerJson;
FirebaseJsonData deviceJson;

// JSON DOCUMENT
DynamicJsonDocument bufferBodyPaserModeAP(8192);
DynamicJsonDocument FirebaseDataBuffer(8192);

// FUNCTION PROTOTYPE - COMPONENT
void setupStreamFirebase();
void checkRam();
void checkFirebaseInit();
void setupWifiModeStation();
void setupWebserverModeAP();
#ifdef _DEBUG_
void viewEEPROM();
#endif
void updateTimestamp();
void checkLinkAppication();
void linkAppication();
void addConfiguration();
void resetConfiguration();
void checkConfiguration();
#ifdef _DEBUG_
void PrintListTimer();
#endif
void parserTimerJson(FirebaseStream &data, uint8_t numberDevice, bool isInit = true);
void parserDeviceStatus(FirebaseStream &data, uint8_t numberDevice);
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void controllDevice(uint8_t numDevice, bool state);
void readTimer(FirebaseJson &fbJson, uint8_t numberDevice);

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
  unsigned int size = 1024;
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

// => Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// => FIREBASE
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

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
  GEN_ID_BY_MAC.replace(":", "");
  ID_DEVICE = String("device-" + GEN_ID_BY_MAC);

#ifdef _DEBUG_
  Serial.begin(115200);
#endif
#ifdef _RELEASE_
  Serial.begin(115200);
#endif

#ifdef _DEBUG_
  Serial.println("");
  Serial.println("[---PROGRAM START---]");
#endif

  timeClient.begin();
  eeprom.begin();

#ifdef _DEBUG_
  viewEEPROM();
#endif

  setupWebserverModeAP();

  WiFi.hostname(CHost);
  WiFi.mode(WIFI_AP_STA);
  WiFi.persistent(true);
  WiFi.softAP(String(mode_ap_ssid + GEN_ID_BY_MAC), mode_ap_pass, 1, false, 1);

  setupWifiModeStation();

#ifdef _DEBUG_
  Serial.println("Connect Wifi");
#endif
  while (WiFi.status() != WL_CONNECTED)
  {
#ifdef _DEBUG_
    Serial.print(".");
#endif
    delay(500);
  }
#ifdef _DEBUG_
  Serial.println("");
#endif

  config.database_url = eeprom.databaseUrl;
  config.signer.test_mode = true;
  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);
#if defined(ESP8266)
  fbdo.setBSSLBufferSize(2048 /* Rx in bytes, 512 - 16384 */, 512 /* Tx in bytes, 512 - 16384 */);
#endif
  checkFirebaseInit();
  setupStreamFirebase();
}

void loop()
{
  // put your main code here, to run repeatedly:
  server.handleClient();
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

void setupStreamFirebase()
{
  Firebase.RTDB.setStreamCallback(&fbdo, streamCallback, streamTimeoutCallback);

  if (!Firebase.RTDB.beginStream(&fbdo, eeprom.DATABASE_NODE + "/devices"))
  {
#ifdef _DEBUG_
    Serial_Printf("stream begin error, %s\n\n", fbdo.errorReason().c_str());
#endif
    isStream = false;
  }
  else
  {
    isStream = true;
#ifdef _DEBUG_
    Serial.println(String("Stream OK!"));
#endif
  }
}

void streamCallback(FirebaseStream data)
{
#ifdef _DEBUG_
  Serial_Printf("sream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
                data.streamPath().c_str(),
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.eventType().c_str());
  Serial.println();
  Serial_Printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());
#endif
  String dataPath = data.dataPath();
  uint8_t dataType = data.dataTypeEnum();
  uint8_t numDevice = (uint8_t)(dataPath.substring(21, 22).toInt());
  if (dataType == d_boolean)
  {
    if (dataPath.indexOf("state") > 0 || numDevice) // execute controll turn on device by [INDEX]
    {
      if (data.dataTypeEnum() == fb_esp_rtdb_data_type_boolean)
      {
        controllDevice(numDevice, data.to<boolean>());
      }
    }
  }
  else if (dataType == d_json)
  {
    if (dataPath.equals("/"))
    { // execute init state all device
#ifdef _DEBUG_
      Serial.println("Init Status");
#endif
      parserTimerJson(data, 1);
      parserTimerJson(data, 2);
      parserTimerJson(data, 3);

      parserDeviceStatus(data, 1);
      parserDeviceStatus(data, 2);
      parserDeviceStatus(data, 3);
    }
    else if (dataPath.indexOf("timer") > 0 && numDevice)
    {
      if (data.eventType().equals("put"))
      {
#ifdef _DEBUG_
        Serial.println(data.jsonString());
#endif
        parserTimerJson(data, numDevice, false);
      }
    }
    PrintListTimer();
  }
}

#ifdef _DEBUG_
void PrintListTimer()
{
  for (size_t i = 0; i < numTimer; i++)
  {
    if (i == 0)
    {
      Serial.print("[");
    }
    for (size_t j = 0; j < 3; j++)
    {
      /* code */
      if (j == 0)
      {
        Serial.print("[");
      }
      Serial.print(String(timerStack[i][j]));
      if (j == 2)
      {
        Serial.print("]");
      }
      else
      {
        Serial.print(", ");
      }
    }
    if (i == numTimer - 1)
    {
      Serial.print("]");
    }
  }
}
#endif

void parserDeviceStatus(FirebaseStream &data, uint8_t numberDevice)
{
  String deviceField = "device-" + GEN_ID_BY_MAC + "-" + numberDevice;
  if (data.jsonObject().get(deviceJson, deviceField + "/state"))
  {
    if (deviceJson.typeNum == 7) // check is boolean
    {
      controllDevice(numberDevice, deviceJson.to<bool>());
    }
  }
}

void controllDevice(uint8_t numDevice, bool state)
{
#ifdef _RELEASE_
  Serial.println("188" + String(state ? 'n' : 'f') + String(numDevice));
#endif
}

void parserTimerJson(FirebaseStream &data, uint8_t numberDevice, bool isInit)
{
  String deviceField = "device-" + GEN_ID_BY_MAC + "-" + numberDevice;
  if (isInit)
  {
    if (data.jsonObject().get(timerJson, deviceField + "/timer"))
    {
      timerJson.get<FirebaseJson>(timerParseArray);
      readTimer(timerParseArray, numberDevice);
      timerJson.clear();
    }
  }
  else
  {
    readTimer(data.jsonObject(), numberDevice);
  }
}

void readTimer(FirebaseJson &fbJson, uint8_t numberDevice)
{
  size_t numTimerPayload = fbJson.iteratorBegin();
  FirebaseJson::IteratorValue timerItem;
  for (size_t i = 0; i < numTimerPayload; i++)
  {
    if (numTimer < 30)
    {
      timerItem = fbJson.valueAt(i);
      if (timerItem.key.equals("unix"))
      {
        timerStack[numTimer][0] = numberDevice;
        timerStack[numTimer][1] = timerItem.value.toInt();
      }
      else if (timerItem.key.equals("value"))
      {
        timerStack[numTimer][2] = timerItem.value.toInt();
        numTimer++;
      }
    }
    else
    {
      break;
    }
  }
  fbJson.iteratorEnd();
}

void streamTimeoutCallback(bool timeout)
{
  if (timeout)
  {
#ifdef _DEBUG_
    Serial.println("stream timed out, resuming...\n");
#endif
  }

  if (!fbdo.httpConnected())
  {
#ifdef _DEBUG_
    Serial_Printf("error code: %d, reason: %s\n\n", fbdo.httpCode(), fbdo.errorReason().c_str());
#endif
  }
}

void updateTimestamp()
{
  timeClient.update();
  epochTime = timeClient.getEpochTime();
}

void checkFirebaseInit()
{
  // [Check] - NodeID is exist

  if (eeprom.canAccess && Firebase.ready())
  {
    String pathDevice = String(eeprom.DATABASE_NODE + "/devices");
    bool statePath = Firebase.RTDB.pathExisted(&fbdo, pathDevice);
#ifdef _DEBUG_
    Serial.println(String("State Path = " + String(statePath)));
#endif
    if (!statePath)
    {
      jsonNewDevice.clear();

      jsonNewDevice.set("device-" + GEN_ID_BY_MAC + "-1/state", false);
      jsonNewDevice.set("device-" + GEN_ID_BY_MAC + "-1/type", TYPE_DEVICE);

      jsonNewDevice.set("device-" + GEN_ID_BY_MAC + "-2/state", false);
      jsonNewDevice.set("device-" + GEN_ID_BY_MAC + "-2/type", TYPE_DEVICE);

      jsonNewDevice.set("device-" + GEN_ID_BY_MAC + "-3/state", false);
      jsonNewDevice.set("device-" + GEN_ID_BY_MAC + "-3/type", TYPE_DEVICE);

      Firebase.RTDB.updateNodeAsync(&fbdo, pathDevice, &jsonNewDevice);

#ifdef _DEBUG_
      Serial.println("[CREATED] - NEW LIST DEVICE");
#endif
    }
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
    if (WiFi.isConnected())
    {
      WiFi.disconnect(true);
    }
    WiFi.begin(ssid, password);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    // reConnect = true;
  }
}

void setupWebserverModeAP()
{
  // [GET] - ROUTE: '/' => Render UI interface
  // server.serveStatic("/", LittleFS, "/index.minify.html");
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
      if (WiFi.status() != WL_CONNECTED)
      {
        removeAfter = eeprom.DATABASE_NODE;
      }
      else
      {
        Firebase.RTDB.deleteNode(&fbdo, eeprom.DATABASE_NODE);
      }
    }
    deserializeJson(bufferBodyPaserModeAP, server.arg("plain"));
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
      if (stateSaveNodeId && stateSaveUserId)
      {
        reLinkApp = true;
        server.send(200, "application/json", "{\"message\":\"LINK APP HAS BEEN SUCCESSFULLY\"}");
      }
      else
      {
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
    restartConfig = true;
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

void checkRam()
{
  ramSize = ESP.getFreeHeap();
#ifdef _DEBUG_
  Serial.println(String(((float_t)ramSize / flashSize * percent)) + " (kb)");
#endif
}
