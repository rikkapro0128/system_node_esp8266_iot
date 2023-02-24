#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <addons/RTDBHelper.h>
#include <Firebase_ESP_Client.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.h"

// Timer Setup
// Select a Timer Clock
#define USING_TIM_DIV1 false  // for shortest and most accurate timer
#define USING_TIM_DIV16 false // for medium time and medium accurate timer
#define USING_TIM_DIV256 true // for longest timer but least accurate. Default

#define TIMER_INTERVAL_MS 1000

// GLOBAL VARIABLE
#define ERROR_TIMER 10
#define PIN_OUT 3
#define NUMS_TIMER 30
#define MAX_NAME_INDEX_FIREBASE 25
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
bool timeControll = false;
bool isFirst = true;
size_t numTimer = 0;
int idTimerRuning;
unsigned long sendDataPrevMillis = 0;
unsigned long timerStack[NUMS_TIMER][3];
bool stateDevice[3] = {false, false, false};
char indexTimerStack[NUMS_TIMER][MAX_NAME_INDEX_FIREBASE];
unsigned long epochTime;
FirebaseJson jsonNewDevice;
FirebaseJson timerParseArray;
FirebaseJsonData timerJson;
FirebaseJsonData deviceJson;

// JSON DOCUMENT
DynamicJsonDocument bufferBodyPaserModeAP(400);

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
void removeTimerFirebase(uint8_t index);
#ifdef _DEBUG_
void PrintListTimer();
#endif
void timerActive();
void IRAM_ATTR TimerHandler();
void parserTimerJson(FirebaseStream &data, uint8_t numberDevice, bool isInit = true);
void parserDeviceStatus(FirebaseStream &data, uint8_t numberDevice);
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void controllDevice(uint8_t numDevice, bool state, bool syncFirebase = false);
void readTimer(FirebaseJson &fbJson, uint8_t numberDevice, String keyAdd = "");
void removeTimer(unsigned long stack[][3], char stackName[][MAX_NAME_INDEX_FIREBASE], String key, bool isCallBack = false);
void sortTimer(unsigned long stack[][3], char stackName[][MAX_NAME_INDEX_FIREBASE]);
void setupTimer(unsigned long stack[][3]);

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

// Init ESP8266 only and only Timer 1
ESP8266Timer ITimer;
ESP8266_ISR_Timer ISR_Timer;

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
  fbdo.setBSSLBufferSize(1024 /* Rx in bytes, 512 - 16384 */, 1024 /* Tx in bytes, 512 - 16384 */);
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);
  checkFirebaseInit();
}

void loop()
{
  server.handleClient();
  timeClient.update();
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 2500 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();
    if (timerStack[0][1] != NULL && timeControll)
    {
      PrintListTimer();
      epochTime = timeClient.getEpochTime();
      bool expire = timerStack[0][1] > epochTime && timerStack[0][1] != NULL ? false : true;
      if (expire)
      {
        unsigned long rangeTimer = epochTime - timerStack[0][1];
        if (rangeTimer < 4)
        {
#ifdef _RELEASE_
          bool state = timerStack[0][2] == 1 ? true : timerStack[0][2] == 2           ? false
                                                  : stateDevice[timerStack[0][0] - 1] ? false
                                                                                      : true;
          controllDevice(timerStack[0][0], state, true);
#endif
        }
#ifdef _DEBUG_
        Serial.printf("Controll timer = %d\n", timerStack[0][1]);
        Serial.printf("[Epoch Time] = %d", rangeTimer);
#endif
        removeTimer(timerStack, indexTimerStack, indexTimerStack[0]);
        if (timerStack[0][1] == NULL)
        {
          timeControll = false;
        }
      }
    }
  }
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
  Serial.println(ESP.getFreeHeap());
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
        bool state = data.to<boolean>();
        controllDevice(numDevice, state);
        stateDevice[numDevice - 1] = state;
      }
    }
  }
  else if (dataType == d_json)
  {
    if (dataPath.equals("/") && isFirst)
    { // execute init state all device
#ifdef _DEBUG_
      Serial.println("Init Status");
#endif
      parserTimerJson(data, 1);
      parserTimerJson(data, 2);
      parserTimerJson(data, 3);

      sortTimer(timerStack, indexTimerStack);

      parserDeviceStatus(data, 1);
      parserDeviceStatus(data, 2);
      parserDeviceStatus(data, 3);
      timeControll = true;
      isFirst = false;
    }
    else if (dataPath.indexOf("timer") > 0 && numDevice)
    {
      parserTimerJson(data, numDevice, false); // add timer to list timer
      sortTimer(timerStack, indexTimerStack);
      timeControll = true;
    }
#ifdef _DEBUG_
    PrintListTimer();
#endif
  }
  else if (dataType == d_null)
  {
    removeTimer(timerStack, indexTimerStack, dataPath.substring(29, 49), true);
#ifdef _DEBUG_
    PrintListTimer();
#endif
  }
}

#ifdef _DEBUG_
void PrintListTimer()
{
  Serial.println("\nTimer Unix Stack: ");
  for (size_t i = 0; i < NUMS_TIMER; i++)
  {
    if (timerStack[i][0] == NULL)
    {
      Serial.print("NULL");
      break;
    }
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
    if (timerStack[i + 1][0] == NULL && timerStack[i + 1][1] == NULL && timerStack[i + 1][2] == NULL)
    {
      Serial.print("]");
      break;
    }
  }
  Serial.println("\nTimer Name Stack: ");
  for (size_t i = 0; i < NUMS_TIMER; i++)
  {
    if (strcmp(indexTimerStack[i], "") == 0)
    {
      Serial.print("NULL");
      break;
    }
    if (i == 0)
    {
      Serial.print("[");
    }
    Serial.print(String(indexTimerStack[i]) + (i == numTimer - 1 ? "" : ", "));
    if (strcmp(indexTimerStack[i + 1], "") == 0)
    {
      Serial.print("]");
      break;
    }
  }
  Serial.println();
}
#endif

void setupTimer(unsigned long stack[][3])
{
  epochTime = timeClient.getEpochTime();
  for (size_t i = 0; i < NUMS_TIMER; i++)
  {
#ifdef _DEBUG_
    Serial.printf("[COUNT] = %d", i);
#endif
    if (stack[i][1] == NULL)
    {
      break;
    }
    else
    {
      bool expire = stack[i][1] > epochTime ? false : true;
#ifdef _DEBUG_
      Serial.printf("[DELETED STATE] = %d", expire);
#endif
      /* code */
      if (expire)
      {
        removeTimer(stack, indexTimerStack, String(indexTimerStack[i]));
      }
      if (stack[i + 1][0] == NULL)
      {
        break;
      }
    }
  }
}

void timerActive()
{
#ifdef _DEBUG_
  Serial.println("[Ended] ITimer millis = " + String(millis()));
#endif
  // removeFirstTimerFirebase();
  String pathRemove = eeprom.DATABASE_NODE + "/devices/" + ID_DEVICE + "-" + String(timerStack[0][0]) + "/timer/" + indexTimerStack[0];
  String pathControl = eeprom.DATABASE_NODE + "/devices/" + ID_DEVICE + "-" + String(timerStack[0][0]) + "/state";

  Firebase.RTDB.setBool(&fbdo, pathControl, timerStack[0][2] == 1 ? true : false);
  Firebase.RTDB.deleteNode(&fbdo, pathRemove);
  removeTimer(timerStack, indexTimerStack, String(indexTimerStack[0]));
  ISR_Timer.deleteTimer(idTimerRuning);
  idTimerRuning = 0;
  setupTimer(timerStack);
#ifdef _DEBUG_
  PrintListTimer();
#endif
}

void IRAM_ATTR TimerHandler()
{
  ISR_Timer.run();
}

void removeTimer(unsigned long stack[][3], char stackName[][MAX_NAME_INDEX_FIREBASE], String key, bool isCallBack)
{
  size_t indexFind;
  bool isFind = false;
  unsigned long numDevice;
  for (size_t i = 0; i < NUMS_TIMER; i++)
  {
    if (isFind)
    {
      break;
    }
    if (strcmp(stackName[i], key.c_str()) == 0)
    {
      indexFind = i;
      isFind = true;
      numDevice = stack[i][0];
      for (size_t j = i; j < NUMS_TIMER; j++)
      {
        if (strcmp(stackName[j + 1], "") == 0)
        {
          strcpy(stackName[j], "");
          break;
        }
        strcpy(stackName[j], stackName[j + 1]);
      }
    }
  }
  if (isFind)
  {
    for (size_t i = indexFind; i < NUMS_TIMER; i++)
    {
      if (stack[i + 1][0] == NULL)
      {
        stack[i][0] = NULL;
        stack[i][1] = NULL;
        stack[i][2] = NULL;
        break;
      }
      else
      {
        stack[i][0] = stack[i + 1][0];
        stack[i][1] = stack[i + 1][1];
        stack[i][2] = stack[i + 1][2];
      }
    }
    if (!isCallBack)
    {
      String pathRemove = eeprom.DATABASE_NODE + "/devices/" + ID_DEVICE + "-" + String(numDevice) + "/timer/" + key;
      Firebase.RTDB.deleteNode(&fbdo, pathRemove);
      setupStreamFirebase();
#ifdef _DEBUG_
      Serial.println("[DELETED PATH] = " + pathRemove);
      Serial.printf("[TIME END] = %d", millis());
#endif
    }
  }
}

void sortTimer(unsigned long stack[][3], char stackName[][MAX_NAME_INDEX_FIREBASE])
{
  char tempSortIndex[MAX_NAME_INDEX_FIREBASE] = "";
  unsigned long tempSort;
  for (size_t i = 0; i < NUMS_TIMER; i++)
  {
    if (stack[i][1] == NULL)
    {
      break;
    }
    for (size_t j = i + 1; j < NUMS_TIMER; j++)
    {
      if (stack[j][1] == NULL)
      {
        break;
      }
      if (stack[j][1] < stack[i][1])
      {
        // assign unix
        tempSort = stack[i][1];
        stack[i][1] = stack[j][1];
        stack[j][1] = tempSort;
        // assign index device
        tempSort = stack[i][0];
        stack[i][0] = stack[j][0];
        stack[j][0] = tempSort;
        // assign control device
        tempSort = stack[i][2];
        stack[i][2] = stack[j][2];
        stack[j][2] = tempSort;

        // assign index name firebase
        strcpy(tempSortIndex, stackName[i]);
        strcpy(stackName[i], stackName[j]);
        strcpy(stackName[j], tempSortIndex);
      }
    }
  }
}

void parserDeviceStatus(FirebaseStream &data, uint8_t numberDevice)
{
  String deviceField = "device-" + GEN_ID_BY_MAC + "-" + numberDevice;
  if (data.jsonObject().get(deviceJson, deviceField + "/state"))
  {
    if (deviceJson.typeNum == 7) // check is boolean
    {
      bool state = deviceJson.to<bool>();
      controllDevice(numberDevice, state);
      stateDevice[numberDevice - 1] = state;
    }
  }
}

void controllDevice(uint8_t numDevice, bool state, bool syncFirebase)
{
#ifdef _RELEASE_
  Serial.println("188" + String(state ? 'n' : 'f') + String(numDevice));
  if (syncFirebase)
  {
    String pathControll = eeprom.DATABASE_NODE + "/devices/" + ID_DEVICE + "-" + String(numDevice) + "/state";
    Firebase.RTDB.setBoolAsync(&fbdo, pathControll, state);
    stateDevice[numDevice - 1] = state;
  }
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
    readTimer(data.jsonObject(), numberDevice, data.dataPath().substring(29, 49));
  }
}

void readTimer(FirebaseJson &fbJson, uint8_t numberDevice, String keyAdd)
{
  size_t numTimerPayload = fbJson.iteratorBegin();
  FirebaseJson::IteratorValue timerItem;
  size_t indexStart;
  for (size_t i = 0; i < NUMS_TIMER; i++)
  {
    if (timerStack[i][0] == NULL && timerStack[i][1] == NULL && timerStack[i][2] == NULL)
    {
      indexStart = i;
      break;
    }
  }
  for (size_t i = 0; i < numTimerPayload; i++)
  {
    if (indexStart < 30)
    {
      timerItem = fbJson.valueAt(i);
      if (timerItem.key.equals("unix"))
      {
        timerStack[indexStart][0] = numberDevice;
        timerStack[indexStart][1] = timerItem.value.toInt();
      }
      else if (timerItem.key.equals("value"))
      {
        timerStack[indexStart][2] = timerItem.value.toInt();
        if (keyAdd.length() > 0)
        {
          strcpy(indexTimerStack[indexStart], keyAdd.c_str());
        }
        indexStart++;
      }
      else
      {
        strcpy(indexTimerStack[indexStart], timerItem.key.c_str());
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

  if (eeprom.canAccess)
  {
    String pathDevice = String(eeprom.DATABASE_NODE + "/devices");
    bool statePath = Firebase.RTDB.pathExisted(&fbdo, pathDevice);
#ifdef _DEBUG_
    Serial.println(String("PATH Device = " + String(pathDevice)));
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

      Firebase.RTDB.setJSON(&fbdo, pathDevice, &jsonNewDevice);

#ifdef _DEBUG_
      Serial.println("[CREATED] - NEW LIST DEVICE");
#endif
    }
    setupStreamFirebase();
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
  DynamicJsonDocument bufferBodyPaserModeAP(400);
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
        checkFirebaseInit();
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
#if defined(_DEBUG_) || defined(_RELEASE_)
  Serial.println(String(((float_t)ramSize / flashSize * percent)) + " (kb)");
#endif
}
