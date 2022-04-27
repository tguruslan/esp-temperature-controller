#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "ota.h"
#include "sensors.h"
#include "config.h"

#ifndef APSSID
#define APSSID "TempSensor"
#define APPSK  "TempSensor"
#endif

const char* www_username;
const char* www_password;

const char *ap_ssid = APSSID;
const char *ap_password = APPSK;

long lastUpdateTime = 0;
long previousMillis = millis();
long interval = 30000;

int work_mode;
int open_settings = 1;
int open_mode = 1;

const size_t capacity = 1024;
DynamicJsonDocument config_settings(capacity);
StaticJsonDocument<256> temp_data;
String json_data;

IPAddress ip_fin;
IPAddress gateway_fin;
IPAddress subnet_fin;

ESP8266WebServer server(80);

String getHeader(String title){
String html = "<!DOCTYPE html>";
html += "<header>";
html += "<meta charset='utf-8'>";
html += "<title>"+title+"</title>";
html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/materialize/1.0.0/css/materialize.min.css'>";
html += "<script src='https://cdnjs.cloudflare.com/ajax/libs/materialize/1.0.0/js/materialize.min.js'></script>";
html += "</header>";
html += "<body>";
html += "<div class='container'>";
html += "<center><h1>"+title+"</h1></center>";
return html;
}


String getFooter(){
String html = "</div>";
html += "</body>";
html += "</html>";
return html;
}

String getTitle(String name){
  StaticJsonDocument<256> doc;
  JsonObject object = doc.to<JsonObject>();

  object["ssid"]="Назва мережі";
  object["password"]="Пароль";
  object["ip"]="IP адреса";
  object["gateway"]="Основний шлюз";
  object["subnet"]="Маска підмережі";
  object["set_temp"]="Задана температура";
  object["calibrate_temp"]="Калібрування температури";
  object["www_username"]="Логін для веб налаштувань";
  object["www_password"]="Пароль для веб налаштувань";
  object["humidity"]="Вологість повітря";
  object["temperature"]="Температура";
  object["dew_point"]="Точка роси";
  object["altitude"]="Висота над рівнем моря";
  object["pressure"]="Атмосферний тиск";
  object["co2"]="Рівень CO2";
  object["tvoc"]="Леткі органічні речовини";

  return object[name];
}

String getUnit(String name){
  StaticJsonDocument<256> doc;
  JsonObject object = doc.to<JsonObject>();

  object["humidity"]="%";
  object["temperature"]="°C";
  object["dew_point"]="°C";
  object["altitude"]="метрів";
  object["pressure"]="міліметрів ртутного стовпчика";
  object["co2"]="частин на мільйон";
  object["tvoc"]="частин на мільярд";

  return object[name];
}

void handleNotFound() {
  String content = "Not Found\n\n";
  content += "URI: ";
  content += server.uri();
  content += "\nMethod: ";
  content += (server.method() == HTTP_GET) ? "GET" : "POST";
  content += "\nArguments: ";
  content += server.args();
  content += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    content += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", content);
}

void String_to_IP(String str,IPAddress& IP_addr)
{
  int c1 = str.indexOf('.');
  int c2 = str.indexOf('.',c1+1);
  int c3 = str.indexOf('.',c2+1);
  int ln = str.length();
  IP_addr[0] = str.substring(0,c1).toInt();
  IP_addr[1] = str.substring(c1+1,c2).toInt();
  IP_addr[2] = str.substring(c2+1,c3).toInt();
  IP_addr[3] = str.substring(c3+1,ln).toInt();
}

String genInput(String name, String val){
  String out;
         out += "<div class='input-field col s6'>";
         out += "<label for='"+name+"'>"+getTitle(name)+"</label>";
         out += "<input type='text' name='"+name+"' value='"+val+"' /><br>";
         out += "</div>";
  return out;
}

void setup(void) {
  Serial.begin(115200);
  configSetup();

  pinMode(D3, OUTPUT);
  digitalWrite(D3, HIGH);

  if (open_settings == 1) {
    deserializeJson(config_settings, loadConfig());
    open_settings = 0;
  }

  if (open_mode == 1) {
    work_mode = loadState();
    open_mode = 0;
  }

  int run_app=0;

  if (config_settings["ssid"].as<String>() != "null" && config_settings["ssid"].as<String>() != "") {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config_settings["ssid"].as<String>(), config_settings["password"].as<String>());

    if (config_settings["ip"].as<String>() != "null" && config_settings["ip"].as<String>() != "") {
      String_to_IP(config_settings["ip"].as<String>(), ip_fin);
      String_to_IP(config_settings["gateway"].as<String>(), gateway_fin);
      String_to_IP(config_settings["subnet"].as<String>(), subnet_fin);

      WiFi.config(ip_fin, gateway_fin, subnet_fin);
    }

    Serial.println("");

    int boot_try_count = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      boot_try_count++;
      Serial.print(".");
      if (boot_try_count > 20) {
        run_app = 1;
      }
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(config_settings["ssid"].as<String>());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }else{
    Serial.print("RUN APP");
    run_app = 1;
  }
  if (run_app == 1) {
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  otaSetup();

  Wire.begin();
  sensorsSetup();

  www_username = config_settings["www_username"];
  www_password = config_settings["www_password"];

  server.on("/", []() {
    json_data = getData(config_settings["calibrate_temp"].as<int>());
    server.send(200, "application/json", json_data);
  });

  server.on("/preview", []() {
    json_data = getData(config_settings["calibrate_temp"].as<int>());
    deserializeJson(temp_data, json_data);
    JsonObject root = temp_data.as<JsonObject>();
    String content;
    content += getHeader("Данні датчиків");
    content += "<table class='responsive-table'><tbody>";
    content += "<tr><td>Показник</td><td>Значення</td><td>Одиниці виміру</td></tr>";

    for (JsonPair sensors : root) {
      JsonObject sensors_obj=sensors.value().as<JsonObject>();
      for (JsonPair kv : sensors_obj){
        content += "<tr><td>"+getTitle(kv.key().c_str())+"</td><td>"+kv.value().as<String>()+"</td><td>"+getUnit(kv.key().c_str())+"</td></tr>";
      }
    }
    content += "</table></tbody>";
    content += getFooter();

    server.send(200, "text/html", content);
  });

  server.on("/off", []() {
    if (config_settings["www_password"].as<String>() != "null" && config_settings["www_password"].as<String>() != "")
    {
      if (!server.authenticate(www_username, www_password)) {return server.requestAuthentication();}
    }

    open_mode = 1;
    saveState(0);
    lastUpdateTime = 900000;
    Serial.println("Вимкнення реле");

    String content;
    content += getHeader("Вимкнення реле");
    content += "<script>window.setTimeout(function(){window.location.href = '/config';}, 3000);</script>";
    content += getFooter();

    server.send(200, "text/html", content);
  });

  server.on("/on", []() {
    if (config_settings["www_password"].as<String>() != "null" && config_settings["www_password"].as<String>() != "")
    {
      if (!server.authenticate(www_username, www_password)) {return server.requestAuthentication();}
    }
    open_mode = 1;
    saveState(1);
    lastUpdateTime = 900000;
    Serial.println("Ввімкнення реле");

    String content;
    content += getHeader("Ввімкнення реле");
    content += "<script>window.setTimeout(function(){window.location.href = '/config';}, 3000);</script>";
    content += getFooter();

    server.send(200, "text/html", content);
  });

  server.on("/auto", []() {
    if (config_settings["www_password"].as<String>() != "null" && config_settings["www_password"].as<String>() != "")
    {
      if (!server.authenticate(www_username, www_password)) {return server.requestAuthentication();}
    }
    open_mode = 1;
    saveState(2);
    lastUpdateTime = 900000;
    Serial.println("Авторежим реле");

    String content;
    content += getHeader("Авторежим реле");
    content += "<script>window.setTimeout(function(){window.location.href = '/config';}, 3000);</script>";
    content += getFooter();

    server.send(200, "text/html", content);
  });

  server.on("/config", []() {
    if (config_settings["www_password"].as<String>() != "null" && config_settings["www_password"].as<String>() != "")
    {
      if (!server.authenticate(www_username, www_password)) {return server.requestAuthentication();}
    }
    String content;
    content += getHeader("Налаштування");

    if (server.arg("ssid") != "") {
      if (!saveConfig(
        server.arg("ssid"),
        server.arg("password"),
        server.arg("ip"),
        server.arg("gateway"),
        server.arg("subnet"),
        server.arg("set_temp"),
        server.arg("www_username"),
        server.arg("www_password"),
        server.arg("calibrate_temp")
      )) {
        content += "<h3>Помилка збереження</h3>";
      } else {
        content += "<h3>Конфігурацію збережено</h3>";
        open_settings = 1;
      }
    }

    String class_auto;
    String class_on;
    String class_off;
    switch (work_mode) {
      case 0:
        class_off="disabled";
        break;
      case 1:
        class_on="disabled";
        break;
      default:
        class_auto="disabled";
        break;
    }

    content += "<div class='row'>";
    content += "<h3>Налаштування режимів</h3>";
    content += "<a class='waves-effect waves-light btn "+class_auto+"' href='/auto'>Авто режим</a>&nbsp;";
    content += "<a class='waves-effect waves-light btn "+class_on+"' href='/on'>Постійно ввімкнутий</a>&nbsp;";
    content += "<a class='waves-effect waves-light btn "+class_off+"' href='/off'>Постійно вимкнутий</a>";
    content += "</div>";
    content += "<form>";
    deserializeJson(config_settings, loadConfig());
    content += "<div class='row'>";
    content += "<h3>Налаштування мережі</h3>";
    for(String param : {
      "ssid",
      "password",
      "ip",
      "gateway",
      "subnet"
    })
    {
        content += genInput(param,config_settings[param]);
    }
    content += "</div>";
    content += "<div class='row'>";
    content += "<h3>Інші налаштування</h3>";
    for(String param : {
      "www_username",
      "www_password",
      "set_temp",
      "calibrate_temp"
    })
    {
        content += genInput(param,config_settings[param]);
    }
    content += "</div>";
    content += "<div class='row'>";
    content += "<button class='btn waves-effect waves-light' type='submit'>Зберегти</button>";
    content += "</div>";
    content += "</form>";
    content += getFooter();

    server.send(200, "text/html", content);
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  ArduinoOTA.handle();

  if (config_settings["ssid"].as<String>() != "null" && config_settings["ssid"].as<String>() != "") {
    if (WiFi.status() != WL_CONNECTED){
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >=interval) {
        ESP.restart();
      }
      previousMillis = currentMillis;
    }
  }


  if (open_settings == 1) {
    deserializeJson(config_settings, loadConfig());
    open_settings = 0;
  }

  if (open_mode == 1) {
    work_mode = loadState();
    open_mode = 0;
  }

  if (lastUpdateTime == 0 || (millis() - lastUpdateTime > 30000))
  {
     lastUpdateTime = millis();

    json_data = getData(config_settings["calibrate_temp"].as<int>());
    deserializeJson(temp_data, json_data);

     if (work_mode == 0) {
       Serial.println("Вимкнення реле");
       digitalWrite(D3, HIGH);
     }else if(work_mode == 1){
       Serial.println("Ввімкнення реле");
       digitalWrite(D3, LOW);
     }else{
       if(temp_data["hdc1080"]["temperature"].as<int>() < (config_settings["set_temp"].as<int>() - 2)){
         Serial.println("Ввімкнення реле");
         digitalWrite(D3, LOW);
       } else if (temp_data["hdc1080"]["temperature"].as<int>() > (config_settings["set_temp"].as<int>() + 2)) {
         Serial.println("Вимкнення реле");
         digitalWrite(D3, HIGH);
       } else {
         Serial.println("Дії не потрібні");
       }
     }
   }
  server.handleClient();
}
