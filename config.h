/*
   File system object (SPIFFS)
   https://github.com/esp8266/Arduino/blob/master/doc/filesystem.md#file-system-object-spiffs

   |--------------|-------|---------------|--|--|--|--|--|
   ^              ^       ^               ^     ^
   Sketch    OTA update   File system   EEPROM  WiFi config (SDK)
*/
#ifndef CONFIG_H
#define CONFIG_H

#include <FS.h>

class configParameter {
  public:
    configParameter(const char *name, String value, configParameter *next) {
      this->name  = name;
      this->value = value;
      this->next  = next;
    }
    const char *name;
    String value;
    configParameter *next = 0;
};

class config {
  public:
    /*
       Инициализация нового файла конфигурации.
       В качестве параметра принимает имя файла, начинающееся с символа. "/"
    */
    config(const char *file);
    /*
       Добавление нового параметра в конфиг
       Необходимо указать имя добавляемого параметра в конфиг, а при необходимости, и его значение.
       Возвращает true в случае успеха и false если параметр не был добавлен.
    */
    bool add(const char *name, String value);
    /*
       Поиск параметра по имени.
       Возвращает ссылку на параметр.
    */
    configParameter *find(const char *name);
    /*
       Возвращает значение указанного параметра.
    */
    String param(const char *name);
    /*
       Изменяет значение указанного параметра.
       Возвращает объект конфига.
    */
    config param(const char *name, String value);
    /*
       Выводит в консоль текущую структуру конфигурации.
       Используется только для отладки.
    */
    void print(/*HardwareSerial &console*/);
    /*
       Читает конфигурацию из flash памяти.
    */
    bool read();
    /*
       Записывает текущую конфигурации во flash память.
       Если в качестве параметра передать json строку, то перед записью во flash память будет произведено обновление
       значений параметров если они будут найдены в конфиге.
    */
    bool write();
    bool write(String apiSave);
    /*
       Удаляет файл конфигурации.
    */
    bool remove();
    /*
       Пересоздает файл конфигурации с записью текущей конфигурации из оперативной памяти во flash.
    */
    bool recreate();
    /*
       Возвращает json строку с текущей конфигурацией, но предварительно подготовленной для передачи по не безопасному каналу.
       Все активные пароли заменены на символы не несущие никакой информации, но дающие общающимся системам четкое понимание,
       что пароль установлен и используется.
       Перехват такой строки не дает атакующему никакой информации для доступа к критическим частям общающихся систем.
    */
    String showSecureConfig();
    /*
       Возвращает ссылку на имя файла конфигурации, используемого в конкретном экземпляре сласса конфигурации.
    */
    const char *fileName();
    
  private:
    configParameter *parameterList = 0;
    const char *spiffsFile;
} conf("/config.json");

/*  */
config::config(const char *file) {
  SPIFFS.begin();
  this->spiffsFile = file;
}

/*  */
bool config::add(const char *name, String value = "") {
  if (!this->find(name)) {
    configParameter *parameter = new configParameter(name, value, this->parameterList);
    this->parameterList = parameter;
    return true;
  } return false;
}

/*  */
configParameter *config::find(const char *name) {
  if (this->parameterList) {
    configParameter *parameter = this->parameterList;
    while (parameter) {
      if (parameter->name == name) return parameter;
      else parameter = parameter->next;
    }
  } return NULL;
}

/*  */
String config::param(const char *name) {
  configParameter *parameter = this->find(name);
  return parameter ? String(parameter->value) : "";
}

/*  */
config config::param(const char *name, String value) {
  configParameter *parameter = this->find(name);
  if (parameter) parameter->value = value;
  return *this;
}

/*  */
void config::print(/*HardwareSerial &console*/) {
#ifdef console
  if (this->parameterList) {
    configParameter *parameter = parameterList;
    while (parameter) {
      console.printf("%s: %s\n", parameter->name, parameter->value.c_str());
      parameter = parameter->next;
    } console.printf("json: %s\n", this->showSecureConfig().c_str());
  }
#endif
}

/*  */
bool config::read() {
  if (this->parameterList) {
    File configFile = SPIFFS.open(this->spiffsFile, "r");
    if (configFile) {
      DynamicJsonBuffer jsonBuffer;
      JsonObject &json = jsonBuffer.parseObject(configFile);
      configFile.close();
      if (json.success()) {
        configParameter *parameter = this->parameterList;
        while (parameter) {
          if (json.containsKey(parameter->name)) parameter->value = json[parameter->name].as<String>();
          parameter = parameter->next;
          yield();
        } return true;
      } SPIFFS.remove(spiffsFile);
    }
  } return false;
}

/*  */
bool config::write() {
  if (this->parameterList) {
    File configFile = SPIFFS.open(this->spiffsFile, "w");
    if (configFile) {
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      configParameter *parameter = this->parameterList;
      while (parameter) {
        json[parameter->name] = parameter->value;
        parameter = parameter->next;
      }
      if (json.printTo(configFile)) {
        configFile.close();
        return true;
      } configFile.close();
    }
  } return false;
}

/*  */
bool config::write(String apiSave) {
  if (this->parameterList) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(apiSave);
    if (json.success()) {
      configParameter *parameter = this->parameterList;
      while (parameter) {
        if (json.containsKey(parameter->name)) this->param(parameter->name, json[parameter->name]);
        parameter = parameter->next;
      } return this->write();
    }
  } return false;
}

/*  */
bool config::remove() {
  if (!SPIFFS.exists(this->spiffsFile)) return false;
  return SPIFFS.remove(this->spiffsFile);
}

/*  */
bool config::recreate() {
  this->remove();
  return this->write();
}

/*  */
String config::showSecureConfig() {
  if (parameterList) {
    String answer, key, val;
    configParameter *parameter = this->parameterList;
    while (parameter) {
      key = parameter->name;
      val = parameter->value;
      if (key.endsWith(F("pass"))) val = val.length() ? "********" : "";
      answer += ",\"" + key + "\":\"" + val + "\"";
      parameter = parameter->next;
    }
    return "{\"status\":true" + answer + "}";
  } return "{\"status\":false}";
}

/*  */
const char *config::fileName() {
  return this->spiffsFile;
}

#endif
