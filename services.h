#ifndef SERVICES_H
#define SERVICES_H

#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include "webserver.h"

WiFiClient wifiClient;
PubSubClient mqttAPI(wifiClient);

String httpCodeStr(int code) {
  switch(code) {
    case -1:  return "CONNECTION REFUSED";
    case -2:  return "SEND HEADER FAILED";
    case -3:  return "SEND PAYLOAD FAILED";
    case -4:  return "NOT CONNECTED";
    case -5:  return "CONNECTION LOST";
    case -6:  return "NO STREAM";
    case -7:  return "NO HTTP SERVER";
    case -8:  return "TOO LESS RAM";
    case -9:  return "ENCODING";
    case -10: return "STREAM WRITE";
    case -11: return "READ TIMEOUT";
     default: return  http.codeTranslate(code);
  }
}

String mqttCodeStr(int code) {
  switch (code) {
    case -4: return "CONNECTION TIMEOUT";
    case -3: return "CONNECTION LOST";
    case -2: return "CONNECT FAILED";
    case -1: return "MQTT DISCONNECTED";
    case  0: return "CONNECTED";
    case  1: return "CONNECT BAD PROTOCOL";
    case  2: return "CONNECT BAD CLIENT ID";
    case  3: return "CONNECT UNAVAILABLE";
    case  4: return "CONNECT BAD CREDENTIALS";
    case  5: return "CONNECT UNAUTHORIZED";
    default: return String(code);
  }
}

bool mqttPublish(String topic, String data) {
  yield();
  if (conf.param("mqtt_path").length()) topic = conf.param("mqtt_path") + "/" + topic;
  return mqttAPI.publish(topic.c_str(), data.c_str(), true);
}
bool mqttPublish(String topic, float data) { return mqttPublish(topic, String(data)); }
bool mqttPublish(String topic, int32_t data) { return mqttPublish(topic, String(data)); }
bool mqttPublish(String topic, uint32_t data) { return mqttPublish(topic, String(data)); }

void restAPIsend(String host, uint16_t port, String query) {
  HTTPClient restAPI;
  restAPI.setUserAgent("Weather Station " + WiFi.hostname());
  restAPI.setTimeout(3000);
  restAPI.begin(host, port, query);
  int code = restAPI.GET();
  #ifdef console
    console.printf("answer: %s\n", httpCodeStr(code).c_str());
  #endif
  restAPI.end();
  yield();
}


void sendDataToMQTT() {
  if (wifi.transferDataPossible() and conf.param("mqtt_server").length()) {
    #ifdef console
      console.println(F("services: send data to MQTT server"));
    #endif
    // баг при прямой передаче значения (c_str) из конфига в setServer (не забыть поправить!)
    String server = conf.param("mqtt_server");
    mqttAPI.setServer(server.c_str(), 1883);
    mqttAPI.connect(WiFi.hostname().c_str(),
      (conf.param("mqtt_login").length() ? conf.param("mqtt_login").c_str() : 0),
      (conf.param("mqtt_pass").length() ? conf.param("mqtt_pass").c_str() : 0)
    );
    if (mqttAPI.connected()) {
      mqttPublish("light",       sensors.get("out_light"));
      mqttPublish("temperature", sensors.get("out_temperature"));
      mqttPublish("humidity",    sensors.get("out_humidity"));
      mqttPublish("pressure",    sensors.get("out_pressure"));
      //mqttPublish("co2",         sensors.get("out_co2"));
      #ifdef console
        console.println(F("answer: OK"));
      #endif
      mqttAPI.disconnect();
    } else {
      #ifdef console
        console.printf("answer: %s\n", mqttCodeStr(mqttAPI.state()).c_str());
      #endif
    }
  }
}

/* https://thingspeak.com/ */
void sendDataToThingSpeak() {
  if (wifi.transferDataPossible() and conf.param("thingspeak_key").length()) {
    #ifdef console
      console.println(F("services: send data to ThingSpeak"));
    #endif

    String query;
    query += "&field1=" + String(sensors.get("out_light"));
    query += "&field2=" + String(sensors.get("out_temperature"));
    query += "&field3=" + String(sensors.get("out_humidity"));
    query += "&field4=" + String(sensors.get("out_pressure"));
    //query += "&field5=" + String(sensors.get("out_co2"));

    restAPIsend("api.thingspeak.com", 80, "/update?api_key=" + conf.param("thingspeak_key") + query);
  }
}

/* https://narodmon.ru/ */
void sendDataToNarodmon() {
  if (wifi.transferDataPossible() and conf.param("narodmon_id").length()) {
    #ifdef console
      console.println(F("services: send data to Narodmon"));
    #endif

    String query;
    query += "&L1="  + String(sensors.get("out_light"));
    query += "&T1="  + String(sensors.get("out_temperature"));
    query += "&H1="  + String(sensors.get("out_humidity"));
    query += "&P1="  + String(sensors.get("out_pressure"));
    //query += "&CO2=" + String(sensors.get("out_co2"));
    //query += "&H2="  + String(sensors.get("out_absoluteHumidity"));

    restAPIsend("192.168.2.128", 23,query);
  }
}

#endif
