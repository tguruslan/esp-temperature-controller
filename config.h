#include <ArduinoJson.h>
#include "FS.h"
#include <LittleFS.h>

String loadConfig() {
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open wifi config file");
    return "[]";
  }
  
  size_t size = configFile.size();
  if (size > 1024) {Serial.println(" wifi config file size is too large");}
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  return buf.get();
}

bool saveConfig(String ssid, String password, String ip, String gateway, String subnet, String set_temp) {
  const size_t capacity = 1024;
  DynamicJsonDocument doc(capacity);

  doc["ssid"] = ssid;
  doc["password"] = password;
  doc["ip"] = ip;
  doc["gateway"] = gateway;
  doc["subnet"] = subnet;

  doc["set_temp"] = set_temp;

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open wifi config file for writing");
    return false;
  }
  serializeJson(doc, configFile);
  return true;
}

bool saveState(int state) {
  const size_t capacity = 1024;
  DynamicJsonDocument doc(capacity);

  doc["state"] = state;

  File configFile = LittleFS.open("/state.json", "w");
  if (!configFile) {
    Serial.println("Failed to open state config file for writing");
    return false;
  }
  serializeJson(doc, configFile);
  return true;
}

int loadState() {
  File configFile = LittleFS.open("/state.json", "r");
  if (!configFile) {
    Serial.println("Failed to open temperature config file");
    return 2;
  }

  size_t size = configFile.size();
  if (size > 1024) {Serial.println(" wifi config file size is too large");}
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  const size_t capacity = 1024;
  DynamicJsonDocument config_state(capacity);
  deserializeJson(config_state, buf.get());

  return config_state["state"].as<int>();
}

void configSetup() {
  Serial.println("Mounting FS...");

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
}
