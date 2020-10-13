#ifndef WIFI_H2
#define WIFI_H2

#define DEBUG_ESP_MDNS

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "config.h"
#include "cron.h"

extern "C" {
#include "user_interface.h"
}

class wifi {
  public:
    wifi() { WiFi.mode(WIFI_OFF); }
    /* */
    void start();
    void handleEvents();

    /* */
    bool ap();
    bool sta();
    bool handleHomeAPsearch();
    bool transferDataPossible();
    bool isConnected() { return this->connected; }
    void disconnect();

    /* info */
    IPAddress ip();
    void info(/* HardwareSerial &console */);
    
    /* перевод кодов состояний и режимов в читаемый вид */
    String wifiMode();
    String wifiMode(WiFiMode_t mode);
    String wifiPhyMode();
    String wifiPhyMode(WiFiPhyMode_t mode);
    String wifiAuthMode(AUTH_MODE mode);
    String wifiDisconnectReasonStr(WiFiDisconnectReason reason);    
    String wifiStatusStr();
    String wifiStatusStr(wl_status_t status);

  private:
    /* события в режиме клиента */
    void staConnected(const WiFiEventStationModeConnected &evt);
    void staDisconnected(const WiFiEventStationModeDisconnected &evt);
    void staAuthModeChanged(const WiFiEventStationModeAuthModeChanged &evt);
    void staGotIP(const WiFiEventStationModeGotIP &evt);
    void staDHCPTimeout();
    
    /* события в режиме точки доступа */
    void apStationConnected(const WiFiEventSoftAPModeStationConnected &evt);
    void apStationDisconnected(const WiFiEventSoftAPModeStationDisconnected &evt);
    void apProbeRequestReceived(const WiFiEventSoftAPModeProbeRequestReceived &evt);

    /* mdns */
    void mdnsSetup();
    
    /* обработчики событий */
    WiFiEventHandler staConnectedHandler;
    WiFiEventHandler staDisconnectedHandler;
    WiFiEventHandler staAuthModeChangedHandler;
    WiFiEventHandler staGotIPHandler;
    WiFiEventHandler staDHCPTimeoutHandler;
    WiFiEventHandler apStationConnectedHandler;
    WiFiEventHandler apStationDisconnectedHandler;
    WiFiEventHandler apProbeRequestReceivedHandler;

    /* */
    bool connected = false;

    /* */
    struct timer_t {
      public:
        timer_t(unsigned long interval) { this->interval = interval; }
        bool isRun() { return time? true : false; }
        void start() { this->time = millis(); }
        void stop()  { this->time = 0; }
        bool ready(bool restart = false) {
          if (!this->isRun()) return false;
          unsigned long currentTime = millis();
          if (((currentTime > this->time) ? (currentTime - this->time) : (currentTime + (0xffffffff - this->time) + 1)) > this->interval) {
            restart ? this->start() : this->stop();
            return true;
          } return false;
        }
      private:
        unsigned long time = 0;
        unsigned long interval = 0;
    };
    timer_t timerChangeMode   = 5000;
    timer_t timerHomeAPsearch = 300000;
} wifi;

bool wifi::ap() {
  #ifdef console
    console.println(F("handle: start AP"));
  #endif
  blink.setMode(smartBlink::mode_off);
  this->disconnect();
  /* сетевые настройки контроллера в аварийном режиме */
  IPAddress ip4(192, 168, 4, 1);
  IPAddress *gw = &ip4;
  IPAddress net(255, 255, 255, 0);
  /* поднятие точки доступа */  
  if (!WiFi.mode(WIFI_AP)) return false; if (!WiFi.softAPConfig(ip4, *gw, net)) return false;
  if (!WiFi.softAP(
    conf.param("softap_ssid").length() ? conf.param("softap_ssid").c_str() : "WeatherStation",
    conf.param("softap_pass").length() ? conf.param("softap_pass").c_str() : NULL)
  ) return false;
  #ifdef console
    this->info();
  #endif
  this->mdnsSetup();
  //MDNS.notifyAPChange();
  blink.setMode(smartBlink::mode_flash2);
  return true;
}

bool wifi::sta() {
  if (!conf.param("client_ssid").length()) return false;
  #ifdef console
    console.println(F("handle: start STA"));
  #endif
  blink.setMode(smartBlink::mode_off);
  /* выходим из режима точки доступа */
  this->disconnect();
  /* переходим в режим клиента */
  if (!WiFi.mode(WIFI_STA)) return false;
  if (!WiFi.begin(conf.param("client_ssid").c_str(), conf.param("client_pass").length() ? conf.param("client_pass").c_str() : NULL)) return false;
  /* */
  timer_t wait = 15000;
  WiFi.setAutoReconnect(true);
  while (!WiFi.isConnected()) {
    /*
    switch (WiFi.status()) {
      //case WL_IDLE_STATUS:     // интерфейс находится в процессе изменения состояния
      //case WL_CONNECTION_LOST: // соединение потеряно
      case WL_NO_SSID_AVAIL:   // сконфигурированная сеть не найдена
      case WL_CONNECT_FAILED:  // пароль не верен
        wait.stop();
        return false;
    }    
    */
    if (!wait.isRun()) wait.start();
    else if (wait.ready()) {
      WiFi.setAutoReconnect(false);
      return false;
    }
    yield();
  }
  /* запоминаем MAC адрес маршрутизатора (необходимо для подключения к скрытой сети) */
  //memcpy(this->lastAPmacAddress, WiFi.BSSID(), sizeof(uint8_t)*6);
  String bssid = WiFi.BSSIDstr();
  if (conf.param("client_bmac") != bssid) conf.param("client_bmac", bssid).write();
  #ifdef console
    this->info();
  #endif
  this->mdnsSetup();
  //MDNS.notifyAPChange();
  WiFi.setAutoReconnect(false);
  return true;
}

void wifi::mdnsSetup() {
  /* 
     MDNS 
     По неизвестным причинам хост может перестать отвечать по имени и подключиться к контроллеру можно только по IP, 
     и то, при условии, что вы его знаете. Из-за этого дополнительно, в режиме STA, периодически перезапускаем listener.
     А вообще listener перезапускается автоматически при смене режима работы. Возможно, проблема кроется совсем не в этом, 
     но на время обкатки оставим перезапуск в коде.
  */
  String hostname = conf.param("mdns_hostname").length() ? conf.param("mdns_hostname") : "espws";
  if (!MDNS.begin(hostname.c_str())) { console.println("Error setting up MDNS responder!"); }
  MDNS.addService("http", "tcp", 80);
  //cron.add(cron::time_10m, [&](){ if (this->isConnected()) MDNS.notifyAPChange(); });
}

/* */
void wifi::start() {
  /* базовые настройки WiFi */
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);

  /* список отслеживаемых событий */
  staConnectedHandler = WiFi.onStationModeConnected([&](const WiFiEventStationModeConnected &evt) { this->staConnected(evt); });
  staDisconnectedHandler = WiFi.onStationModeDisconnected([&](const WiFiEventStationModeDisconnected &evt) { this->staDisconnected(evt); });
  staAuthModeChangedHandler = WiFi.onStationModeAuthModeChanged([&](const WiFiEventStationModeAuthModeChanged &evt) { this->staAuthModeChanged(evt); });
  staGotIPHandler = WiFi.onStationModeGotIP([&](const WiFiEventStationModeGotIP &evt) { this->staGotIP(evt); });
  staDHCPTimeoutHandler = WiFi.onStationModeDHCPTimeout([&]() { this->staDHCPTimeout(); });
  apStationConnectedHandler = WiFi.onSoftAPModeStationConnected([&](const WiFiEventSoftAPModeStationConnected &evt) { this->apStationConnected(evt); });
  apStationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected([&](const WiFiEventSoftAPModeStationDisconnected &evt) { this->apStationDisconnected(evt); });
  //apProbeRequestReceivedHandler = WiFi.onSoftAPModeProbeRequestReceived([&](const WiFiEventSoftAPModeProbeRequestReceived &evt) { this->apProbeRequestReceived(evt); });

  /* тестовая функция для планировщика */
  #ifdef console
    cron.add(cron::time_1m, [&](){
      if (WiFi.getMode() == WIFI_STA and this->isConnected()) {
        console.printf("WIFI: ssid %s, rssi %d dBm ", WiFi.SSID().c_str(), WiFi.RSSI());
      } console.printf("HW: vcc %.2fv RAM: TotalAvailableMemory %i, LargestAvailableBlock %i, Fragmentation %f\n", ESP.getVcc() * 0.001, memory.getTotalAvailableMemory(), memory.getLargestAvailableBlock(), memory.getFragmentation());
    });
  #endif;

  this->sta()?:this->ap();
}

/* */
void wifi::handleEvents() {
  MDNS.update();
  switch (WiFi.getMode()) {
    case WIFI_STA:
      if (!this->isConnected()) {
        if (!timerChangeMode.isRun()) timerChangeMode.start();
        else if (timerChangeMode.ready()) this->ap();
      } else if (timerChangeMode.isRun()) timerChangeMode.stop();
      break;
    case WIFI_AP:
      if (!timerHomeAPsearch.isRun()) timerHomeAPsearch.start();
      else if (timerHomeAPsearch.ready()) {
        if (this->handleHomeAPsearch()) this->sta();
        else timerHomeAPsearch.start();
      }
      break;
    default: this->start();
  }
}

/* */
bool wifi::handleHomeAPsearch() {
  if (!conf.param("client_ssid").length()) return false;
  #ifdef console
    console.println(F("handle: home access point search (STA mode)"));
  #endif
  blink.setMode(smartBlink::mode_burn);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int ap = WiFi.scanNetworks(false, true);
  while (--ap >= 0) {
  #ifdef console
    console.printf("%s (%d dBm) %s\n",
      WiFi.BSSIDstr(ap).c_str(),
      WiFi.RSSI(ap),
      WiFi.isHidden(ap) ? "*hidden" : WiFi.SSID(ap).c_str()
    );
  #endif
    if (WiFi.isHidden(ap)) {
      /* для скрытых сетей идет сравнение MAC адресов */
      return (conf.param("client_bmac").length() and conf.param("client_bmac") == WiFi.BSSIDstr(ap));
      /*
      if (!this->lastAPmacAddress[0]) break;
      uint8_t *bmac = WiFi.BSSID(found);
      for (byte b = 0; b < 6; b++) {
        if (this->lastAPmacAddress[b] != bmac[b]) break;
        if (b == 5) return true;
      }
      */
    } else {
      if (WiFi.SSID(ap) == conf.param("client_ssid")) return true;
    }
  }
  #ifdef console
    console.println("handle: home access point not found");
  #endif
  this->ap();
  return false;
}

void wifi::disconnect() {
  switch(WiFi.getMode()) {
    case WIFI_STA: WiFi.disconnect(); break;
    case WIFI_AP:  WiFi.softAPdisconnect(); break;
    default: return;
  } delay(1000);
}

/* */
bool wifi::transferDataPossible() {
  return (WiFi.getMode() == WIFI_STA and this->isConnected());
}

/* обработка событий */
void wifi::staConnected(const WiFiEventStationModeConnected &evt) {
  #ifdef console
    console.printf("event staConnected: ssid %s, channel %d\n", 
      evt.ssid.c_str(),
      evt.channel
    );
  #endif;
  this->connected = true;
  blink.setMode(smartBlink::mode_flash1);
  WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
}

void wifi::staDisconnected(const WiFiEventStationModeDisconnected &evt) {
  #ifdef console
    console.printf("event staDisconnected: ssid %s, reason %d (%s)\n",
      evt.ssid.c_str(),
      evt.reason,
      this->wifiDisconnectReasonStr(evt.reason).c_str()
    );
  #endif;
  this->connected = false;
  blink.setMode(smartBlink::mode_off);
}

void wifi::staAuthModeChanged(const WiFiEventStationModeAuthModeChanged &evt) {
  #ifdef console
    console.printf("event staAuthModeChanged: %s to %s\n",
      this->wifiAuthMode((AUTH_MODE) evt.oldMode).c_str(),
      this->wifiAuthMode((AUTH_MODE) evt.newMode).c_str()
    );
  #endif
}
void wifi::staGotIP(const WiFiEventStationModeGotIP &evt) {
  MDNS.notifyAPChange();
  #ifdef console
    console.printf("event staGotIP: ip %s, mask %s, gw %s\n", 
      evt.ip.toString().c_str(),
      evt.mask.toString().c_str(),
      evt.gw.toString().c_str()
    );
  #endif
}
void wifi::staDHCPTimeout() {
  #ifdef console
    console.println(F("event staDHCPTimeout"));
  #endif
}
void wifi::apStationConnected(const WiFiEventSoftAPModeStationConnected &evt) {
  #ifdef console
    console.printf("event apStationConnected: mac %02x:%02x:%02x:%02x:%02x:%02x, aid %d\n",
      evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4], evt.mac[5],
      evt.aid
    );
  #endif
}
void wifi::apStationDisconnected(const WiFiEventSoftAPModeStationDisconnected &evt) {
  #ifdef console  
    console.printf("event apStationDisconnected: mac %02x:%02x:%02x:%02x:%02x:%02x, aid %d\n",
      evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4], evt.mac[5],
      evt.aid
    );
  #endif
}
void wifi::apProbeRequestReceived(const WiFiEventSoftAPModeProbeRequestReceived &evt) {
  #ifdef console
    console.printf("event apProbeRequestReceived: rssi %d, mac %02x:%02x:%02x:%02x:%02x:%02x\n",
      evt.rssi,
      evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4], evt.mac[5]
    );
  #endif
}

/* info */
IPAddress wifi::ip() {
  switch(WiFi.getMode()) {
    case WIFI_AP_STA:
    case WIFI_STA: return WiFi.localIP();
    case WIFI_AP:  return WiFi.softAPIP();
    default: return IPAddress (0, 0, 0, 0);
  }
}

void wifi::info(/* HardwareSerial &console */) {
#ifdef console
  if(WiFi.getMode() == WIFI_OFF) return;
  console.printf("- channel: %d\n- physical mode: %s\n- mode: %s\n",
    WiFi.channel(),
    this->wifiPhyMode(WiFi.getPhyMode()).c_str(),
    this->wifiMode(WiFi.getMode()).c_str()
  );
  switch(WiFi.getMode()) {
    case WIFI_AP_STA:
    case WIFI_STA:
      console.printf("- mac: %s\n- hostname: %s\n- ip: %s\n- subnet mask: %s\n- gateway: %s\n- dns server #1: %s\n- dns server #2: %s\n- ssid: %s\n- password: %s\n- bmac: %s\n- rssi: %d dBm\n",
        WiFi.macAddress().c_str(),
        WiFi.hostname().c_str(),
        this->ip().toString().c_str(),
        WiFi.subnetMask().toString().c_str(),
        WiFi.gatewayIP().toString().c_str(),
        WiFi.dnsIP().toString().c_str(),
        WiFi.dnsIP(1).toString().c_str(),
        WiFi.SSID().c_str(),
        WiFi.psk().c_str(),
        WiFi.BSSIDstr().c_str(),
        WiFi.RSSI()
      );
      break;
    case WIFI_AP:
      console.printf("- ip: %s\n- mac: %s\n",
        this->ip().toString().c_str(), 
        WiFi.softAPmacAddress().c_str()
      );
      break;
  }
#endif
}

/* mode & code description */
String wifi::wifiMode() { return this->wifiMode(WiFi.getMode()); }
String wifi::wifiMode(WiFiMode_t mode) {
#ifdef console
  switch (mode) {
    case WIFI_OFF:    return "OFF";
    case WIFI_STA:    return "STA";
    case WIFI_AP:     return "AP";
    case WIFI_AP_STA: return "AP+STA";
  }
#endif
  return "UNKNOWN";
}

String wifi::wifiPhyMode() { return this->wifiPhyMode(WiFi.getPhyMode()); }
String wifi::wifiPhyMode(WiFiPhyMode_t mode) {
#ifdef console
  switch (mode) {
    case WIFI_PHY_MODE_11B: return "802.11B";
    case WIFI_PHY_MODE_11G: return "802.11G";
    case WIFI_PHY_MODE_11N: return "802.11N";
  }
#endif
  return "UNKNOWN";
}
String wifi::wifiAuthMode(AUTH_MODE mode) {
#ifdef console
  switch (mode) {
    case AUTH_OPEN:         return "OPEN";
    case AUTH_WEP:          return "WEP";
    case AUTH_WPA_PSK:      return "WPA-PSK";
    case AUTH_WPA2_PSK:     return "WPA2-PSK";
    case AUTH_WPA_WPA2_PSK: return "AUTO (WPA/WPA2)";
  }
#endif
  return "UNKNOWN";
}
String wifi::wifiDisconnectReasonStr(WiFiDisconnectReason reason) {
#ifdef console
  switch (reason) {
    case WIFI_DISCONNECT_REASON_UNSPECIFIED:              return "UNSPECIFIED";
    case WIFI_DISCONNECT_REASON_AUTH_EXPIRE:              return "AUTH EXPIRE";
    case WIFI_DISCONNECT_REASON_AUTH_LEAVE:               return "AUTH LEAVE";
    case WIFI_DISCONNECT_REASON_ASSOC_EXPIRE:             return "ASSOC EXPIRE";
    case WIFI_DISCONNECT_REASON_ASSOC_TOOMANY:            return "ASSOC TOOMANY";
    case WIFI_DISCONNECT_REASON_NOT_AUTHED:               return "NOT AUTHED";
    case WIFI_DISCONNECT_REASON_NOT_ASSOCED:              return "NOT ASSOCED";
    case WIFI_DISCONNECT_REASON_ASSOC_LEAVE:              return "ASSOC LEAVE";
    case WIFI_DISCONNECT_REASON_ASSOC_NOT_AUTHED:         return "ASSOC NOT AUTHED";
    case WIFI_DISCONNECT_REASON_DISASSOC_PWRCAP_BAD:      return "DISASSOC PWRCAP BAD";
    case WIFI_DISCONNECT_REASON_DISASSOC_SUPCHAN_BAD:     return "DISASSOC SUPCHAN BAD";
    case WIFI_DISCONNECT_REASON_IE_INVALID:               return "IE INVALID";
    case WIFI_DISCONNECT_REASON_MIC_FAILURE:              return "MIC FAILURE";
    case WIFI_DISCONNECT_REASON_4WAY_HANDSHAKE_TIMEOUT:   return "4WAY HANDSHAKE TIMEOUT";
    case WIFI_DISCONNECT_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP KEY UPDATE TIMEOUT";
    case WIFI_DISCONNECT_REASON_IE_IN_4WAY_DIFFERS:       return "IE IN 4WAY DIFFERS";
    case WIFI_DISCONNECT_REASON_GROUP_CIPHER_INVALID:     return "GROUP CIPHER INVALID";
    case WIFI_DISCONNECT_REASON_PAIRWISE_CIPHER_INVALID:  return "PAIRWISE CIPHER INVALID";
    case WIFI_DISCONNECT_REASON_AKMP_INVALID:             return "AKMP INVALID";
    case WIFI_DISCONNECT_REASON_UNSUPP_RSN_IE_VERSION:    return "UNSUPP RSN IE VERSION";
    case WIFI_DISCONNECT_REASON_INVALID_RSN_IE_CAP:       return "INVALID RSN IE CAP";
    case WIFI_DISCONNECT_REASON_802_1X_AUTH_FAILED:       return "802 1X AUTH FAILED";
    case WIFI_DISCONNECT_REASON_CIPHER_SUITE_REJECTED:    return "CIPHER SUITE REJECTED";
    /* 20X */
    case WIFI_DISCONNECT_REASON_BEACON_TIMEOUT:           return "BEACON TIMEOUT";
    case WIFI_DISCONNECT_REASON_NO_AP_FOUND:              return "NO AP FOUND";
    case WIFI_DISCONNECT_REASON_AUTH_FAIL:                return "AUTH FAIL";
    case WIFI_DISCONNECT_REASON_ASSOC_FAIL:               return "ASSOC FAIL";
    case WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT:        return "HANDSHAKE TIMEOUT";
  }
#endif
  return "UNKNOWN";
}

String wifi::wifiStatusStr() { return this->wifiStatusStr(WiFi.status()); }
String wifi::wifiStatusStr(wl_status_t status) {
#ifdef console
  switch (status) {
    case WL_NO_SHIELD:       return "NO SHIELD";
    case WL_IDLE_STATUS:     return "IDLE STATUS";
    case WL_NO_SSID_AVAIL:   return "NO SSID AVAIL";
    case WL_SCAN_COMPLETED:  return "SCAN COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
  }
#endif
  return "UNKNOWN";
}
#endif
