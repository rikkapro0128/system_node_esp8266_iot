#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include "LittleFS.h"

// GLOBAL VARIABLE
size_t eepromSize = 255;
String tempWifiSSIDModeAP = "";
String tempWifiPasswordModeAP = "";
String getMac = WiFi.macAddress();

// JSON DOCUMENT
DynamicJsonDocument bufferBodyPaserModeAP(8192);
DynamicJsonDocument bufferResponseModeAP(8192);

// FUNCTION PROTOTYPE - TASK
void setupWifiModeAP();
void setupWifiModeStation();
void setupWebserverModeAP();
void checkWifiConnection();
void setupWebserverModeStation();

// FUNCTION PROTOTYPE - COMPONENT
void eepromReset();
void eepromResetAll();
bool checkHasWifiCached();
void maybeSwitchMode();
void clearCachedWifiConfig();
void cachedWifiConfig(String ssid, String password);
void eepromWriteConfigWifi(String ssid, String password);
String eepromReadAll();
String eepromReadWifi(String key);
bool eepromWriteValueByKey(String key, String value);

// INSTANCE
ESP8266WebServer server_mode_ap(80);
ESP8266WebServer server_mode_station(80);
Scheduler runner;

// TASK
Task miruSetupWifiAPMode(TASK_IMMEDIATE, TASK_ONCE, &setupWifiModeAP, &runner);
Task miruSetupWebServerAPMode(TASK_IMMEDIATE, TASK_FOREVER, &setupWebserverModeAP, &runner);
Task miruSetupWifiStationMode(TASK_IMMEDIATE, TASK_ONCE, &setupWifiModeStation, &runner);
Task miruSetupWebServerStationMode(TASK_IMMEDIATE, TASK_FOREVER, &setupWebserverModeStation, &runner);
Task miruCheckWifiConnection(TASK_SECOND, TASK_FOREVER, &checkWifiConnection, &runner);

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
  EEPROM.begin(eepromSize);
  LittleFS.begin();
  
  // eepromResetAll();
  // Serial.println("[EEPROM - Reseted]");
  String readEEPROM = eepromReadAll();
  Serial.print(String("[readEEPROM]: |" + readEEPROM + "|"));
  Serial.println();
  Serial.println("[EEPROM - Write]: Writing...");
  bool check = eepromWriteValueByKey(String("name"), String("miru"));
  if(check) {
    readEEPROM = eepromReadAll();
    Serial.print(String("[readEEPROM]: |" + readEEPROM + "|"));
    Serial.println();
  }
  // Serial.println();
  // Serial.print("ssid: ");
  // Serial.println(eepromReadWifi("ssid"));
  // Serial.print("password: ");
  // Serial.println(eepromReadWifi("password"));
  runner.startNow();
  maybeSwitchMode();
}

void loop()
{
  // put your main code here, to run repeatedly:
  runner.execute();
}

void setupWifiModeStation() {
  // ACTIVE MODE AP
  // WiFi.mode(WIFI_STA);
  WiFi.begin(tempWifiSSIDModeAP, tempWifiPasswordModeAP);
  while(!miruCheckWifiConnection.enable());
}

void setupWifiModeAP()
{
  // ACTIVE MODE AP
  // WiFi.mode(WIFI_AP);
  String genIdBymac = getMac;
  genIdBymac.replace(":", "");
  while (!WiFi.softAP(String(mode_ap_ssid + genIdBymac), mode_ap_pass))
    ;
  // Serial.printf("[WIFI-AP]( IS SETUP WITH IP: ");
  // Serial.print(ip_mode_ap);
  // Serial.printf(" )\n");
  while (!miruSetupWebServerAPMode.enable())
    ;
}

void checkWifiConnection() {
  // enable check wifi connection
  if (WiFi.status() == WL_CONNECTED)
  {
    while(!miruSetupWebServerStationMode.enableIfNot());
    miruCheckWifiConnection.cancel();
  }
}

void setupWebserverModeStation() {
  if (miruSetupWebServerStationMode.isFirstIteration()) {
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
    server_mode_ap.on("/scan-network", HTTP_GET, [](){
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
      bufferResponseModeAP.clear();
    });
    // [GET] - ROUTE: '/is-config' => Check WIFI is configuration
    server_mode_ap.on("/is-config", HTTP_GET, [](){
      bool checkCached = checkHasWifiCached();
      bool isPass = false;
      String ssid;
      String password;
      if(checkCached == true) {
        isPass = true;
        ssid = tempWifiSSIDModeAP;
        password = tempWifiPasswordModeAP;
      }else {
        ssid = eepromReadWifi("ssid");
        password = eepromReadWifi("password");
        if(ssid != "" && password != "") {
          isPass = true;
          cachedWifiConfig(ssid, password);
        }
      }
      if(isPass) {
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
      }
    });
    // [POST] - ROUTE: '/reset-config-wifi' => Reset config WIFI
    server_mode_ap.on("/reset-config-wifi", HTTP_POST, [](){
      bool checkCached = checkHasWifiCached();
      if(checkCached == true) {
        eepromReset();
        clearCachedWifiConfig();
      }
      server_mode_ap.send(200, "application/json", "{\"message\":\"RESET CONFIG SUCCESSFULLY\"}");
      WiFi.disconnect();
    });
    // [POST] - ROUTE: '/config-wifi' => Goto config WIFI save below EEPROM
    server_mode_ap.on("/config-wifi", HTTP_POST, []() {
      if(server_mode_ap.hasArg("plain")) {
        deserializeJson(bufferBodyPaserModeAP, server_mode_ap.arg("plain"));
        bufferBodyPaserModeAP.shrinkToFit();
        JsonObject body = bufferBodyPaserModeAP.as<JsonObject>();
        // Serial.print("ssid: ");
        // Serial.println(String(body["ssid"]));
        // Serial.print("password: ");
        // Serial.println(String(body["password"]));
        String ssid = body["ssid"];
        String password = body["password"];
        eepromWriteConfigWifi(ssid, password);
        cachedWifiConfig(ssid, password);
        // Serial.println("[Save config EEPROM]");
        server_mode_ap.send(200, "application/json", "{\"message\":\"CONFIGURATION WIFI SUCCESSFULLY\"}");
        WiFi.begin(ssid, password);
      }else {
        server_mode_ap.send(403, "application/json", "{\"message\":\"NOT FOUND PAYLOAD\"}");
      }
      bufferBodyPaserModeAP.clear();
    });
    // [POST] - ROUTE: '/link-app' => Goto config firebase save below EEPROM
    server_mode_ap.on("/link-app", HTTP_POST, []() {
      if(server_mode_ap.hasArg("plain")) {
        deserializeJson(bufferBodyPaserModeAP, server_mode_ap.arg("plain"));
        bufferBodyPaserModeAP.shrinkToFit();
        JsonObject body = bufferBodyPaserModeAP.as<JsonObject>();
        // Serial.print("ssid: ");
        // Serial.println(String(body["ssid"]));
        // Serial.print("password: ");
        // Serial.println(String(body["password"]));
        String idUser = body["idUser"];
        String idNode = body["idNode"];
        Serial.println(String("[ID_USER]: " + idUser));
        Serial.println(String("[ID_NODE]: " + idNode));
        // Serial.println("[Save config EEPROM]");
        server_mode_ap.send(200, "application/json", "{\"message\":\"LINK APP HAS BEEN SUCCESSFULLY\"}");
      }else {
        server_mode_ap.send(403, "application/json", "{\"message\":\"NOT FOUND PAYLOAD\"}");
      }
      bufferBodyPaserModeAP.clear();
    });

    // START WEBSERVER
    server_mode_ap.begin();
  }
  server_mode_ap.handleClient();
}

void eepromReset() {
  for(size_t i = 0; i < eepromSize; i++) {
    EEPROM.write(0x0F + i, '$');
  }
  EEPROM.commit();
}

void eepromWriteConfigWifi(String ssid, String password) {
  String temp_wifi = "ssid:" + ssid + "$" + "password:" + password;
  eepromReset();
  for(size_t i = 0; i <= temp_wifi.length(); i++) {
    EEPROM.write(0x0F + i, temp_wifi[i]);
  }
  EEPROM.commit();
}

// [FUNC TEST]

bool eepromWriteValueByKey(String key, String value) {

  if(key.length() > 0 && value.length() > 0) {
    String summary = key + ":" + value;
    for(size_t i = 6; i < eepromSize; i++)
    {
      uint8_t space_first = EEPROM.read(0x0F + i);
      uint8_t space_second = EEPROM.read(0x0F + i + 1);
      if(space_first == NULL && space_second == NULL) {
        Serial.println(String("[CO KHA NANG VIET]: " + summary));
        const uint8_t index_start = i + 1;
        const uint8_t length_str = summary.length();
        Serial.println(String("[INDEX START] => " + String(index_start)));
        Serial.println(String("[LENGTH STRING] => " + String(length_str)));
        // example: index_start = 1 | length_str = 9
        // so write: index_start ------- length_str
        if(eepromSize - index_start > length_str) {
          // start write key
          uint8_t start = 0;
          for(size_t j = index_start; j < index_start + length_str; j++) {
            Serial.println(String("write => " + String(summary[start])));
            EEPROM.write(j, (uint8_t)summary[start]);
            start++;
          }
          // EEPROM.end();
          // while (!EEPROM.commit());
        }else {
          // can't enough capicite
          return false;
        }
        return true;
      }else {
        if(i == eepromSize - 1) {
          return false;
        }else {
          continue;
        }
      }
    }
  }else {
    return false;
  }

  return false;
}

void eepromResetAll() {
  for(size_t i = 0; i < eepromSize; i++)
  {
    EEPROM.write(0x0F + i, NULL);
  }
  EEPROM.commit();
}

String eepromReadAll() {
  String temp;
  for(size_t i = 0; i < eepromSize; i++)
  {
    uint8_t num = EEPROM.read(0x0F + i);
    temp = String(temp + (char)num);
  }
  return temp;
}

// [FUNC TEST]

String eepromReadWifi(String key) {
  String val;
  // EXTRACT CONFIGURATION FROM EEPROM
  for(size_t i = 0; i < eepromSize; i++)
  {
    uint8_t num = EEPROM.read(0x0F + i);
    if(num >= 32 && num <= 126) {
      val += char(num);
    }
  }
  // START SEARCH
  String result = "";
  int position = val.indexOf(key);
  if(position == -1) return result;
  position = (position + key.length() + 1);
  for(size_t i = position; i < val.length(); i++)
  {
    if(val[i] != '$') {
      result += val[i];
    }else {
      break;
    }
  }
  return result;
}

bool checkHasWifiCached() {
  if(tempWifiSSIDModeAP != "" && tempWifiPasswordModeAP != "") {
    return true;
  }else {
    return false;
  }
}

void clearCachedWifiConfig() {
  tempWifiSSIDModeAP = "";
  tempWifiPasswordModeAP = "";
}

void cachedWifiConfig(String ssid, String password) {
  tempWifiSSIDModeAP = ssid;
  tempWifiPasswordModeAP = password;
}

void turnOffModeAP() {
  server_mode_ap.close();
  miruSetupWebServerAPMode.cancel();
  WiFi.softAPdisconnect(true);
}

void maybeSwitchMode() {
  String ssid = eepromReadWifi("ssid");
  String password = eepromReadWifi("password");
  WiFi.mode(WIFI_AP_STA);
  if(ssid != "" && password != "") {
    // cached wifi config
    cachedWifiConfig(ssid, password);
    // turn on network(mode - STATION)
    miruSetupWifiStationMode.enable();
  }
  // turn on network(mode - AP)
  miruSetupWifiAPMode.enable();
}
