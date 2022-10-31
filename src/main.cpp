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

// JSON DOCUMENT
DynamicJsonDocument bufferBodyPaserModeAP(8192);
DynamicJsonDocument bufferResponseModeAP(8192);

// FUNCTION PROTOTYPE - TASK
void eepromReset();
void setupWifiModeAP();
void setupWebserverModeAP();

// FUNCTION PROTOTYPE - COMPONENT
bool checkHasWifiCached();
void clearCachedWifiConfig();
void cachedWifiConfig(String ssid, String password);
void eepromWriteConfigWifi(String ssid, String password);
String eepromReadWifi(String key);

// INSTANCE
ESP8266WebServer server_mode_ap(80);
Scheduler runner;

// TASK
Task miruSetupWifiAPMode(TASK_IMMEDIATE, TASK_ONCE, &setupWifiModeAP, &runner);
Task miruSetupWebServerAPMode(TASK_IMMEDIATE, TASK_FOREVER, &setupWebserverModeAP, &runner);

// WIFI MODE - AP
String mode_ap_ssid = "esp8266-miru";
String mode_ap_pass = "44448888";
String mode_ap_ip = "192.168.1.120";
String mode_ap_gateway = "192.168.1.1";
String mode_ap_subnet = "255.255.255.0";
String pathFile = "/";

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(eepromSize);
  LittleFS.begin();
  // Serial.println();
  // Serial.print("ssid: ");
  // Serial.println(eepromReadWifi("ssid"));
  // Serial.print("password: ");
  // Serial.println(eepromReadWifi("password"));
  String ssid = eepromReadWifi("ssid");
  String password = eepromReadWifi("password");
  cachedWifiConfig(ssid, password);
  runner.startNow();
  miruSetupWifiAPMode.enable();
}

void loop()
{
  // put your main code here, to run repeatedly:
  runner.execute();
}

void setupWifiModeAP()
{
  // ACTIVE MODE AP
  while (!WiFi.softAP(mode_ap_ssid, mode_ap_pass))
    ;
  WiFi.softAPIP();
  // Serial.printf("[WIFI-AP]( IS SETUP WITH IP: ");
  // Serial.print(ip_mode_ap);
  // Serial.printf(" )\n");
  while (!miruSetupWebServerAPMode.enable())
    ;
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
      if(checkCached == true) {
        isPass = true;
      }else {
        String ssid = eepromReadWifi("ssid");
        String password = eepromReadWifi("password");
        if(ssid != "" && password != "") {
          isPass = true;
          cachedWifiConfig(ssid, password);
        }
      }
      if(isPass) {
        server_mode_ap.send(200, "application/json", "{\"message\":\"WIFI HAS BEEN CONFIG\"}");
      }else {
        server_mode_ap.send(200, "application/json", "{\"message\":\"WIFI NOT YET CONFIG\"}");
      }
    });
    // [POST] - ROUTE: '/reset-config' => Reset config WIFI
    server_mode_ap.on("/reset-config", HTTP_POST, [](){
      bool checkCached = checkHasWifiCached();
      if(checkCached == true) {
        eepromReset();
        clearCachedWifiConfig();
      }
      server_mode_ap.send(200, "application/json", "{\"message\":\"RESET CONFIG SUCCESSFULLY\"}");
    });
    // [POST] - ROUTE: '/config' => Goto config WIFI save below EEPROM
    server_mode_ap.on("/config", HTTP_POST, []() {
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
      }else {
        server_mode_ap.send(403, "application/json", "{\"message\":\"NOT FOUND PAYLOAD\"}");
      }
    });

    // START WEBSERVER
    server_mode_ap.begin();
  }
  server_mode_ap.handleClient();
}

void eepromReset() {
  for(size_t i = 0; i < eepromSize; i++) {
    EEPROM.write(0x0F + i, '*');
  }
  EEPROM.commit();
}

void eepromWriteConfigWifi(String ssid, String password) {
  String temp_wifi = "ssid:" + ssid + "*" + "password:" + password;
  eepromReset();
  for(size_t i = 0; i <= temp_wifi.length(); i++) {
    EEPROM.write(0x0F + i, temp_wifi[i]);
  }
  EEPROM.commit();
}

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
    if(val[i] != '*') {
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
