#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <MD5Builder.h>
#include <FS.h>
#include <ESP8266WebServer.h>
#include "detail/RequestHandlersImpl.h"

#include "config.h"
#include "cron.h"
#include "tools.h";

class http: public ESP8266WebServer {
  public:
    http(IPAddress addr, int port = 80): ESP8266WebServer(addr, port) {}
    http(int port = 80): ESP8266WebServer(port) { this->init(); }

    /* sys */
    void init();
    bool authorized();
    String codeTranslate(int code) { return ESP8266WebServer::responseCodeToString(code); }

    /* headers */
    void sendServerHeaders();

    /* handler */
    bool fsHandler(String path);

    /* api */
    void api_sensors();
    void api_sensors_structure();
    void api_sensors_log();
    void api_settings();
    void api_settings_gpio();
    void api_spiffs_upload();
    void api_spiffs_upload_handler();
    void api_spiffs_list();
    void api_spiffs_delete();
    void api_system_info();
    void api_system_reboot();
    void api_system_hardReset();
    void api_system_i2c_scaner();
    void api_system_update();
    void api_system_update_handler();

    String getContentType(String);
    
    /* api вспомогательные */
    String api_system_info_live(String);

    /* hash */
    String md5(String str);

    /* cookies */
    String cookiesKey  = this->md5(this->_getRandomHexString());
    String cookiesName = String(ESP.getChipId());

    /* secure */
    byte securityLevel = 0;
    byte securityFail  = 3;
    typedef enum { down = 0, up } security_t;
    bool security();
    void security(security_t action);
    
  private:
    #define headerJson F("application/json; charset=utf-8")
} http;

/*
   Инициализация web сервера микроконтроллера
*/
void http::init() {
  const char *headerkeys[] = {"User-Agent", "Cookie", "Accept-Encoding", "If-None-Match"};
  this->collectHeaders(headerkeys, sizeof(headerkeys) / sizeof(char*));

  this->on("/api/sensors",           HTTP_GET,  [this](){ api_sensors(); });
  this->on("/api/sensors/structure", HTTP_GET,  [this](){ api_sensors_structure(); });
  this->on("/api/sensors/log",       HTTP_GET,  [this](){ api_sensors_log(); });
  this->on("/api/settings",          HTTP_POST, [this](){ api_settings(); });
  this->on("/api/settings/gpio",     HTTP_GET,  [this](){ api_settings_gpio(); });
  this->on("/api/spiffs",            HTTP_POST, [this](){ api_spiffs_upload(); }, [this](){ api_spiffs_upload_handler(); });
  this->on("/api/spiffs",            HTTP_GET,  [this](){ api_spiffs_list(); });
  this->on("/api/spiffs/delete",     HTTP_POST, [this](){ api_spiffs_delete(); });
  this->on("/api/system/info",       HTTP_POST, [this](){ api_system_info(); });
  this->on("/api/system/reboot",     HTTP_POST, [this](){ api_system_reboot(); });
  this->on("/api/system/hardReset",  HTTP_POST, [this](){ api_system_hardReset(); });
  this->on("/api/system/i2c",        HTTP_GET,  [this](){ api_system_i2c_scaner(); });
  this->on("/api/system/update",     HTTP_POST, [this](){ api_system_update(); }, [this](){ api_system_update_handler(); });
  
  this->onNotFound([this](){ 
    this->sendServerHeaders();
    if(!this->fsHandler(this->uri())) this->send(404);
  });
  this->begin();
  
  /* задача для планировщика - плавный сброс ограничений доступа к панели управления */
  cron.add(cron::time_1m, [this](){ security(down); }, "httpSecurity");
}

/*
   Функция отвечает за авторизацию пользователя с использованием cookie и формы авторизации.
   Set-Cookie: NAME=VALUE; expires=DATE; path=PATH; domain=DOMAIN_NAME; secure
   https://ru.wikipedia.org/wiki/HTTP_cookie
*/
bool http::authorized() {
  /* проверка блокировки системы авторизации */
  if (!this->security()) return false;
  
  /* блок проверки агента */
  if (!this->hasHeader("User-Agent")) return false;
  if (!this->header("User-Agent").length()) return false;
  
  /* form (форма обязательно перед cookies - фикс бага с постоянной ошибкой авторизации при отказе удаления cookie) */  
  if (this->hasArg(F("login")) and this->hasArg(F("password"))) {
    if (this->arg(F("login")) == (conf.param("admin_login").length() ? conf.param("admin_login") : F("admin")) and
        this->arg(F("password")) == (conf.param("admin_pass").length() ? conf.param("admin_pass") : F("admin"))) {
      String clientKey = md5(this->_getRandomHexString());
      this->cookiesKey = md5(this->client().remoteIP().toString() + clientKey + this->header("User-Agent"));
      this->sendHeader("Set-Cookie", this->cookiesName + "=" + clientKey + "; path=/; HTTPonly");
      return true;
    }
    /* поднимаем уровень тревоги (подбор пароля через форму авторизации) */
    this->security(up);
    return false;
  }

  /* cookies (не допускайте переход к проверке cookies следом после проверки формы авторизации) */
  if (this->hasHeader("Cookie")) {
    unsigned int start;
    if ((start = this->header("Cookie").indexOf(this->cookiesName)) != -1) {
      start = start + this->cookiesName.length() + 1;
      String cookie = this->header("Cookie").substring(start, start + 32);
      if (md5(this->client().remoteIP().toString() + cookie + this->header("User-Agent")) == this->cookiesKey) return true;
      /* поднимаем уровень тревоги (подбор cookies) */
      this->security(up);
      this->sendHeader("Set-Cookie", this->cookiesName + "=; path=/; expires=Thu, 01 Jan 1970 00:00:00 GMT");
    }
  } return false;
}

String http::getContentType(String path) {
  if (path.endsWith(".html"))       return u8"text/html";
  else if (path.endsWith(".htm"))   return u8"text/html";
  else if (path.endsWith(".css"))   return u8"text/css";
  else if (path.endsWith(".json"))  return u8"application/json";
  else if (path.endsWith(".js"))    return u8"application/javascript";
  else if (path.endsWith(".png"))   return u8"image/png";
  else if (path.endsWith(".gif"))   return u8"image/gif";
  else if (path.endsWith(".jpg"))   return u8"image/jpeg";
  else if (path.endsWith(".ico"))   return u8"image/x-icon";
  else if (path.endsWith(".svg"))   return u8"image/svg+xml";
  else if (path.endsWith(".eot"))   return u8"font/eot"; 
  else if (path.endsWith(".woff"))  return u8"font/woff";
  else if (path.endsWith(".woff2")) return u8"font/woff2";
  else if (path.endsWith(".ttf"))   return u8"font/ttf";
  else if (path.endsWith(".xml"))   return u8"text/xml";
  else if (path.endsWith(".pdf"))   return u8"application/pdf";
  else if (path.endsWith(".zip"))   return u8"application/zip";
  else if (path.endsWith(".gz"))    return u8"application/x-gzip";
  else return u8"text/plain";
}

/*
   Функция вызывается в случае отсутствия заранее определенного поведения при событии http::on()
   В таком случае предполагается, что клиент запрашивает файл, находящийся во flash памяти.
   Функция возвращает true в случае обнаружения файла и false в случае неудачи.
   Нельзя допустить передачи файлов конфигурации клиенту иначе это приведет к очень печальным последствиям.
   Производится поиск как оригинального файла, так и его архивной gzip копии, последняя имеет приоритет для отправки клиенту.
   Поддерживается система кэширования ETag и если у клиента будет найдена актуальная копия файла, передача не состоится, а
   клиент получит соответствующий заголовок, инициализирующий использование клиентом кэше.
*/
bool http::fsHandler(String path) {
  if (path != conf.fileName()) {
    if (path.endsWith(F("/"))) path = F("/index.htm");
    String contentType = this->getContentType(path);
    
    /* Поддерживает ли клиент сжатие данных и имеется ли у нас необходимый файл в gzip? */
    if (this->hasHeader(F("Accept-Encoding")) and contentType != F("application/x-gzip") and contentType != F("application/octet-stream")) {
      if (this->header(F("Accept-Encoding")).indexOf(F("gzip")) != -1 and SPIFFS.exists(path + F(".gz"))) {
        this->sendHeader(F("Content-Encoding"), F("gzip"));
        path += F(".gz");
      } else if (!SPIFFS.exists(path)) return false;
    } else if (!SPIFFS.exists(path)) return false;

    File file = SPIFFS.open(path, "r");
    size_t size = file.size();
    #ifdef console
      console.printf("http: %s %s, ", this->client().remoteIP().toString().c_str(), path.c_str());
    #endif
    /* Имеется ли у клиента в кэше актуальная версия файла? */
    if (this->hasHeader(F("If-None-Match"))) {
      if (this->header(F("If-None-Match")) == String(size)) {
        this->send(304);
        file.close();
        #ifdef console
          console.println(F("304"));
        #endif
        return true;
      }
    }

    /* Не повезло, клиент пуст. Начинаем перекачивать файл. */
    #ifdef console
      console.println(F("200"));
    #endif
    this->sendHeader(F("ETag"), String(size));
    this->setContentLength(size);
    this->send(200, contentType, "");
    this->client().write(file, 2920);
    file.close();

    return true;
  } return false;
}

/*
   Формирование списка заголовков.
   Без заголовка Origin невозможна отладка web интерфейса на локальной машине без загрузки файлов во flash память.
*/
void http::sendServerHeaders() {
  //this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  //this->sendHeader(F("Access-Control-Allow-Credentials"), F("true"));
  this->sendHeader(F("Server"), F("sw: , hw: esp8266"));
}

/*
   Предоставляет последние актуальные данные с сенсоров.
*/
void http::api_sensors() {
  this->sendServerHeaders();
  this->send(200, headerJson, this->api_system_info_live(sensors.get(false)));
}

/*
   Предоставляет информацию о структуре дерева сенсоров.
*/
void http::api_sensors_structure() {
  this->sendServerHeaders();
  this->send(200, headerJson, sensors.list());
}

/*
   Предоставляет лог по всем инициализированным сенсорам.
*/
void http::api_sensors_log() {
  this->sendServerHeaders();
  String answer = this->hasArg("sensor") ? sensors.log(this->arg("sensor").c_str()) : sensors.log();
  this->send(200, headerJson, answer);
}

/*
   Формирует безопасный список настроек для предоставления в web интерфейс.
*/
void http::api_settings() {
  this->sendServerHeaders();
  this->sendHeader("Cache-Control", "no-store, no-cache");
  if (!this->security()) this->send(403);
  else if (this->authorized()) {
    if (this->hasArg(F("save"))) conf.write(this->arg(F("save")));
    this->send(200, headerJson, conf.showSecureConfig());
  } else this->send(401);
}

/*
   Список текущих настроек для GPIO.
*/
void http::api_settings_gpio() {
  this->sendServerHeaders();
  if (this->authorized()) {
    String answer;
    answer += "\"gpio12\":" + conf.param("gpio12") + ",";
    answer += "\"gpio13\":" + conf.param("gpio13");
    this->send(200, headerJson, "{" + answer + "}");
  } else this->send(401);
}

/*
   Загрузка файлов во flash память микроконтроллера.
*/
void http::api_spiffs_upload() { /* объеденено с обработчиком */ }
void http::api_spiffs_upload_handler() {
  static File uploadFile;
  static bool authorized;
  HTTPUpload &upload = this->upload();
  switch (upload.status) {
    case UPLOAD_FILE_START:
      if (!(authorized = this->authorized())) return;
      if (upload.filename == conf.fileName()) return;
      uploadFile = SPIFFS.open(upload.filename.startsWith(F("/")) ? upload.filename : "/" + upload.filename, "w");
      blink.setMode(smartBlink::mode_flash4);
      return;
    case UPLOAD_FILE_WRITE:
      if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
      return;
    case UPLOAD_FILE_ABORTED:
    case UPLOAD_FILE_END:
      this->sendServerHeaders();
      if (uploadFile) {
        blink.previous();
        uploadFile.close();
        if (upload.status != UPLOAD_FILE_END) this->send(400);
        else this->api_spiffs_list();
      } else this->send(authorized ? 500 : 401);
  }
}

/*
   Формирует список файлов, находящихся во flash памяти микроконтроллера.
   Обязательно исключайте из списка Ваши файлы конфигурации.
*/
void http::api_spiffs_list() {
  this->sendServerHeaders();
  if (this->authorized()) {
    String answer;
    FSInfo spiffs;
    SPIFFS.info(spiffs);
    Dir obj = SPIFFS.openDir(F("/"));
    while (obj.next()) {
      if (obj.fileName() == conf.fileName()) continue;
      if (answer.length()) answer += ',';
      answer += "\"" + obj.fileName().substring(1) + "\":" + String(obj.fileSize());
    }
    if (answer.length()) answer = "{" + answer + "}";
    this->send(200, headerJson, "{\"spiffs\":" + String(spiffs.totalBytes) + ",\"list\":[" + answer + "]}");
  } else this->send(401);
}

/*
   Удаляет файл из flash памяти микроконтроллера.
   Обязательно наличие проверки на удаление файла конфигурации, в противном случае можно поменяться в лице.
*/
void http::api_spiffs_delete() {
  this->sendServerHeaders();
  if (this->authorized()) {
    if (this->hasArg(F("file"))) {
      String deleteFile = "/" + this->arg(F("file"));
      if (deleteFile != conf.fileName() and SPIFFS.exists(deleteFile)) {
        SPIFFS.remove(deleteFile);
        this->api_spiffs_list();
        return;
      }
    } this->send(500);
  } else this->send(401);
}

/*
   Формирует список с данными отображаемыми в разделе "Система" панели управления.
*/
void http::api_system_info() {
  this->sendServerHeaders();
  if (this->authorized()) {
    FSInfo spiffs;
    SPIFFS.info(spiffs);
    
    String answer;
    answer += "\"bmac\":\""           + conf.param("client_bmac") + "\",";    
    answer += "\"ip\":\""             + wifi.ip().toString() + "\",";
    answer += "\"subnet\":\""         + WiFi.subnetMask().toString() + "\",";
    answer += "\"dns1\":\""           + WiFi.dnsIP().toString() + "\",";
    answer += "\"dns2\":\""           + WiFi.dnsIP(1).toString() + "\",";
    answer += "\"gateway\":\""        + WiFi.gatewayIP().toString() + "\",";
    answer += "\"rssi\":\""           + String(WiFi.RSSI()) + " dBm\",";
    answer += "\"mac\":\""            + String(WiFi.macAddress()) + "\",";        // uint8_t (array[6])
    answer += "\"channel\":"          + String(WiFi.channel()) + ",";
    answer += "\"mode\":\""           + wifi.wifiMode() + "\",";
    answer += "\"phyMode\":\""        + wifi.wifiPhyMode() + "\",";
    answer += "\"vcc\":\""            + String(ESP.getVcc() * 0.001) + " V\",";   // uint16_t
    answer += "\"freeHeap\":"         + String(ESP.getFreeHeap()) + ",";          // uint32_t
    answer += "\"chipId\":"           + String(ESP.getChipId()) + ",";            // uint32_t
    answer += "\"sdkVersion\":\""     + String(ESP.getSdkVersion()) + "\",";      // const char *
    answer += "\"coreVersion\":\""    + ESP.getCoreVersion() + "\",";
    answer += "\"cpuFreqMHz\":\""     + String(ESP.getCpuFreqMHz()) + " MHz\",";  // uint8_t
    answer += "\"flashRealSize\":"    + String(ESP.getFlashChipRealSize()) + ","; // uint32_t
    answer += "\"flashChipId\":"      + String(ESP.getFlashChipId()) + ",";       // uint32_t
    answer += "\"flashChipSpeed\":\"" + String(int(ESP.getFlashChipSpeed() * 0.000001)) + " MHz\","; // uint32_t
    answer += "\"spiffsTotalBytes\":" + String(spiffs.totalBytes) + ",";
    answer += "\"spiffsUsedBytes\":"  + String(spiffs.usedBytes) + ",";
    answer += "\"spiffsBlockSize\":"  + String(spiffs.blockSize) + ",";
    answer += "\"spiffsPageSize\":"   + String(spiffs.pageSize) + ",";
    answer += "\"sketchVersion\":\"v1.1 beta (16.08.2020)\",";
    answer += "\"sketchSize\":"       + String(ESP.getSketchSize()) + ",";        // uint32_t
    answer += "\"sketchMD5\":\""      + ESP.getSketchMD5() + "\",";
    answer += "\"freeSketchSpace\":"  + String(ESP.getFreeSketchSpace()) + ",";   // uint32_t
    answer += "\"resetReason\":\""    + ESP.getResetReason() + "\",";
    answer += "\"resetInfo\":\""      + ESP.getResetInfo() + "\",";
    answer += "\"bootVersion\":\""    + String(ESP.getBootVersion()) + "\",";      // uint8_t
    //answer += "\"bootMode\":\""     + String(ESP.getBootMode()) + "\",";         // uint8_t
    answer += "\"millis\":"           + String(millis());
    this->send(200, headerJson, "{" + answer + "}");
  } else this->send(401);
}

/*
   Наполняет стандартную телеграмму с показаниями датчиков информацией о некоторых характеристиках микроконтроллера.
   Данные будут переданы только авторизованному пользователю.
*/
String http::api_system_info_live(String answer) {
  if (this->hasArg(F("system"))) {
    if (this->authorized()) {
      String system;
      system += "\"rssi\":\""   + String(wifi.isConnected() ? WiFi.RSSI() : 0) + " dBm\",";
      system += "\"vcc\":\""    + String(ESP.getVcc() * 0.001) + " V\",";
      system += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
      system += "\"millis\":"   + String(millis());
      answer += ",\"system\":{" + system + "}";
    }
  } return "{" + answer + "}";
}

/*
   Перезагружает устройство.
*/
void http::api_system_reboot() {
  this->sendServerHeaders();
  if (this->authorized()) {
    this->send(202);
    delay(2000); // задержка обязательна, иначе контроллер уйдет на перезагрузку до завершения передачи!
    ESP.restart();
  } else this->send(401);
}

/*
   Сброс настроек микроконтроллера.
   Удаляет файл конфигурации и перезагружает устройство.
*/
void http::api_system_hardReset() {
  this->sendServerHeaders();
  if (this->authorized()) {
    this->send(202);
    conf.remove();
    delay(2000); // задержка обязательна, иначе контроллер уйдет на перезагрузку до завершения передачи!
    ESP.restart();
  } else this->send(401);
}

/*
   Сканер шины i2c.
   Формирует список устройств с их статусами.
*/
void http::api_system_i2c_scaner() {
  this->sendServerHeaders();
  if (this->authorized()) {
    String answer;
    for (byte address = 1; address < 127; address++) {
      Wire.beginTransmission(address);
      byte status = Wire.endTransmission();
      if (!status or status == 4) {
        answer += answer.length() ? "," : "";
        answer += "\"" + String((address < 16) ? "0" : "") + String(address, HEX) + "\":" + (!status ? '1' : '0');
      }
    }
    this->send(200, headerJson, "{\"list\":{" + answer + "}}");
  } else this->send(401);
}

/*
   Обновление программы микроконтроллера.
*/
void http::api_system_update() { /* объеденено с обработчиком */ }
void http::api_system_update_handler() {
  static bool status;
  static bool authorized;
  HTTPUpload& upload = this->upload();
  switch (upload.status) {
    case UPLOAD_FILE_START:    
      status = false;
      if (!(authorized = this->authorized())) return;
      if (upload.filename.length() != 32 or Update.isRunning()) return;
      status = true;
      Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
      Update.setMD5(upload.filename.c_str());
      blink.setMode(smartBlink::mode_flash4);
      return;
    case UPLOAD_FILE_WRITE:
      if (status) Update.write(upload.buf, upload.currentSize);
      return;
    case UPLOAD_FILE_ABORTED:
    case UPLOAD_FILE_END:
      this->sendServerHeaders();
      if (status) {
        blink.previous();
        String answer;
        answer += "\"status\":" + String(Update.hasError() ? "false" : "true") + ',';
        answer += "\"error\":"  + String(Update.getError());
        this->send(200, headerJson, "{" + answer + "}");
        delay(2000); // задержка обязательна, иначе контроллер уйдет на перезагрузку до завершения передачи!
        if (Update.end(!Update.hasError())) ESP.restart();
      } else this->send(authorized ? 500 : 401);
  }
}

/*
   Генерирует MD5 хэш из переданной строки.
*/
String http::md5(String str) {
  MD5Builder md5;
  md5.begin();
  md5.add(str);
  md5.calculate();
  return md5.toString();
}

/*
   Функция управляет уровнем безопасности для защиты от брутфорса пароля или cookies.
   Если вызвана без параметра, возвращает текущий статус безопасности - true если уровень приемлем и false если поднята тревога.
   В качестве параметра принимает security_t - действие которое необходимо провести с текущим уровнем:
    up   - поднять
    down - опустить
   Понижение уровня тревоги происходит автоматически с интервалом в 1 минуту.
   При изменении уровня тревоги, функция ничего не возвращает.
*/
bool http::security() { return this->securityLevel < this->securityFail ? true : false; }
void http::security(security_t action) {
  if (action and this->securityLevel < this->securityFail) this->securityLevel++;
  else if (!action and this->securityLevel > 0) this->securityLevel--;
  else return;
  cron.update("httpSecurity");
}
#endif
