#ifndef SENSOR_H
#define SENSOR_H

#include <base64.h>
#include "tools.h";

typedef String json;

struct knob_t {
  knob_t(int min, int max, const char *step, const char *title, const char *unit) {
    this->min   = min;
    this->max   = max;
    this->step  = step;
    this->title = title;
    this->unit  = unit;
  }
  int min, max;
  const char *step, *title, *unit;
};

class device {
  public:
    typedef enum list_t {out = 1, in = 2};
    typedef std::function<void(void)> initFn_t;
    typedef std::function<float(void)> dataFn_t;
    
    device(knob_t *knob, list_t list, byte address, const char *name, initFn_t init, dataFn_t data, byte log, device *next) {
      this->knob = knob;
      this->list = list;
      this->address = address;
      this->name = name;
      this->init = init;
      this->data = data;
      this->log  = log ? new float[log]{0} : 0;
      this->next = next;
    }
    device(list_t *list) {}

    knob_t *knob;
    list_t list = out;
    byte address;
    const char *name;    
    initFn_t init;
    dataFn_t data;
    bool status = false;
    medianFilter_t lastDimension;
    float *log;
    byte logPosition = 0;
    device *next;
};

class sensors {    
  public:
    /*
       Добавление нового датчика в список
       Необходимо передать:
        - параметры конфигурации плагина Knob
        - место расположения датчика device::out или device::in
        - адрес на i2c шине
        - идентификатор датчика (уникальное имя)
        - имя функции отвечающую за инициализацию датчика в системе, одна функция на один адрес (не обязательный параметр)
        - имя функции отвечающую за сбор данных с датчика
        - логическое значение, отвечающее за ведение лога (по умолчанию выключено)

        void initF() { }
        float dataF() { }
        
        int min = 0;
        int max = 100;
        const char *step = "1";
        knob_t *kparam = new knob_t(min, max, step, "Заголовок", "ед.измер.");

        bool log = true;
        sensors.add(kparam, device::out, 0x01, "sensor_1", initF, dataF, log);
    */
    bool add(knob_t *knob, device::list_t list, byte address, const char *name, device::initFn_t init, device::dataFn_t data, bool log);
    bool add(knob_t *knob, device::list_t list, byte address, const char *name, device::dataFn_t data, bool log);

    bool add(knob_t *knob, byte address, const char *name, device::initFn_t init, device::dataFn_t data, bool log);
    bool add(knob_t *knob, byte address, const char *name, device::dataFn_t data, bool log);

    bool add(knob_t *knob, device::list_t list, const char *name, device::dataFn_t data, bool log);

    bool add(knob_t *knob, const char *name, device::dataFn_t data, bool log);
    
    /*
       Ищет объект сенсора по его имени
       Возвращает указатель на объект или 0
    */
    device *find(const char *name);

    /*
       Активирует и деактивирует сенсор
    */
    void disable(const char *name);
    void enable(const char *name);

    /*
       Возвращает статус сенсора
    */
    bool status(device *sensor);
    bool status(const char *name);
    json status(bool edging);

    /*
       Возвращает последнее полученное значение от сенсора
    */
    float get(const char *name);
    json get(bool edging);

    /*
       Возвращает суточный лог сенсора
    */
    json log(device *sensor);
    json log(const char *name);
    json log();

    /*
       Производит обновление лога
       Обновляет лог конкретного сенсора если передано его имя или указатель на него
       Если без параметра, то обновляет логи по всем доступным сенсорам
    */
    void logUpdate(device *sensor);
    void logUpdate(const char *name);
    void logUpdate();

    /*
       Производит обновление данных
       По переденным параметрам копирует поведение лога
    */
    void dataUpdate(device *sensor);
    void dataUpdate(const char *name);
    void dataUpdate();

    /*
       Производит проверку доступности сенсора по его адресу на i2c шине
       Принимает в качестве параметра указатель на сенсор или его имя
       Если параметр не задан, то производит проверку всех сенсоров
    */
    void checkLine(device *sensor);
    void checkLine(const char *name);
    void checkLine();

    /*
       Возвращает полное описание всех сенсоров в системе
    */
    json list(bool edging);
    
  private:
    /*
       Очищает данные от мусора
       Используется для уменьшения объема передаваемых данных о логах при формировании ответа по запросу через API
    */
    String clear(float value);

    device *sensorsList = 0;
    byte logSize = 144;
} sensors;

/* */
bool sensors::add(knob_t *knob, device::list_t list, byte address, const char *name, device::initFn_t init, device::dataFn_t data, bool log = false) {
  if (!this->find(name)) {
    device *sensor = new device(knob, list, address, name, init, data, log ? this->logSize : 0, this->sensorsList);
    this->sensorsList = sensor;

    return true;
  } return false;
}

/* */
bool sensors::add(knob_t *knob, device::list_t list, byte address, const char *name, device::dataFn_t data, bool log = false) {
  return this->add(knob, list, address, name, [](){}, data, log);
}

bool sensors::add(knob_t *knob, byte address, const char *name, device::initFn_t init, device::dataFn_t data, bool log = false) {
  return this->add(knob, device::out, address, name, init, data, log);
}

bool sensors::add(knob_t *knob, byte address, const char *name, device::dataFn_t data, bool log = false) {
  return this->add(knob, device::out, address, name, [](){}, data, log);
}

bool sensors::add(knob_t *knob, device::list_t list, const char *name, device::dataFn_t data, bool log = false) {
  return this->add(knob, list, 0x00, name, [](){}, data, log);
}

bool sensors::add(knob_t *knob, const char *name, device::dataFn_t data, bool log = false) {
  return this->add(knob, device::out, 0x00, name, [](){}, data, log);
}

/*  */
device *sensors::find(const char *name) {
  if (this->sensorsList) {
    device *sensor = this->sensorsList;
    while (sensor) {
      byte i = 0;
      while (i <= 0xff) {
        if (name[i] != sensor->name[i]) break;
        else if(name[i] == '\0') return sensor;
        ++i;
      }
      sensor = sensor->next;
    }
  } return 0;
}

/*  */
void sensors::disable(const char *name) {
  device *sensor = this->find(name);
  if (sensor) sensor->status = false;
}

/*  */
void sensors::enable(const char *name) {
  device *sensor = this->find(name);
  if (sensor) sensor->status = true;
}

/*  */
bool sensors::status(device *sensor) {
  return sensor ? sensor->status : false;
}

/*  */
bool sensors::status(const char *name) {
  return this->status(this->find(name));
}

/*  */
json sensors::status(bool edging = true) {
  String answer;
  if (this->sensorsList) {
    device *sensor = this->sensorsList;
    while (sensor) {
      if (answer.length()) answer += ",";
      answer += "\"" + String(sensor->name) + "\":" + (sensor->status ? "true" : "false");
      sensor = sensor->next;
    }
  } return edging ? "{" + answer + "}" : answer;
}

/*  */
float sensors::get(const char *name) {
  device *sensor = this->find(name);
  return !sensor? 0 : (float)sensor->lastDimension;
}

/*  */
json sensors::get(bool edging = true) {
  String answer;
  if (this->sensorsList) {
    device *sensor = this->sensorsList;
    while (sensor) {
      if (answer.length()) answer += ",";
      answer += "\"" + String(sensor->name) + "\":" + String(sensor->lastDimension);
      sensor = sensor->next;
    }
  } return edging ? "{" + answer + "}" : answer;
}

/*  */
json sensors::log(device *sensor) {
  String log;
  if (sensor) {
    if (sensor->log) {
      for (byte i = sensor->logPosition; i < this->logSize; i++) {
        log += (log.length() ? "," : "") + this->clear(sensor->log[i]);
      }
      for (byte i = 0; i < sensor->logPosition; i++) {
        log += (log.length() ? "," : "") + this->clear(sensor->log[i]);
      }
      if (log.length()) log = "\"" + String(sensor->name) + "\":[" + log + "]";
    }
  } return log;
}

/*  */
json sensors::log(const char *name) {
  String log = this->log(this->find(name));
  return log.length() ? "{" + log + ",\"timeAdjustment\":" + String(cron.lastRun("httpSensorsLog")) + "}" : "{}";
}

/*  */
json sensors::log() {
  String log;
  String answer = "\"timeAdjustment\":" + String(cron.lastRun("httpSensorsLog"));
  if (this->sensorsList) {
    device *sensor = this->sensorsList;
    while (sensor) {
      log = this->log(sensor);
      if (log.length()) answer += "," + log;
      log = "";
      sensor = sensor->next;
      yield();
    }
  } return "{" + answer + "}";
}

/*  */
void sensors::logUpdate(device *sensor) {
  if (sensor) {
    if (sensor->log) {
      sensor->log[sensor->logPosition++] = sensor->lastDimension;
      if (sensor->logPosition >= this->logSize) sensor->logPosition = 0;
    }
  }
}

/*  */
void sensors::logUpdate(const char *name) {
  this->logUpdate(this->find(name));
}    

/*  */
void sensors::logUpdate() {
  if (this->sensorsList) {
    device *sensor = this->sensorsList;
    while (sensor) {
      this->logUpdate(sensor);
      sensor = sensor->next;
    }
  }
}

/*  */
void sensors::dataUpdate(device *sensor) {
  if (sensor) {
    float data = 0;
    if (sensor->status or sensor->address == 0x00) {
      data = sensor->data();
      if (isnan(data)) {
        if(sensor->status) sensor->status = false;
        data = 0;
      }
    } sensor->lastDimension = data;
  }
}

/*  */
void sensors::dataUpdate(const char *name) {
  this->dataUpdate(this->find(name));
}

/*  */
void sensors::dataUpdate() {
  if (this->sensorsList) {
    device *sensor = this->sensorsList;
    while (sensor) {
      this->dataUpdate(sensor);
      sensor = sensor->next;
    }
  }
}

/*  */
void sensors::checkLine(device *sensor) {
  if (sensor) {
    if (sensor->address != 0x00) {
      Wire.beginTransmission(sensor->address);
      bool oldStatus = sensor->status;
      sensor->status = (Wire.endTransmission() == 0);
      if (!oldStatus and oldStatus != sensor->status) sensor->init();
    }
  }
}

/*  */
void sensors::checkLine(const char *name) {
  this->checkLine(this->find(name));
}

/*  */
void sensors::checkLine() {
  if (this->sensorsList) {
    device *sensor = this->sensorsList;
    while (sensor) {
      this->checkLine(sensor);
      sensor = sensor->next;
      yield();
    }
  }
}

/*  */
json sensors::list(bool edging = true) {
  String answer, knob;
  if (this->sensorsList) {
    device *sensor = this->sensorsList;
    while (sensor) {
      knob += "\"name\":\""  + String(sensor->name)        + "\","; // Имя сенсора
      knob += "\"list\":"    + String(sensor->list)        + ",";   // В каком разделе отобразить датчик
      knob += "\"log\":"     + String(sensor->log != 0)    + ",";   // Отметка ведения лога
      knob += "\"min\":"     + String(sensor->knob->min)   + ",";   // Минимальное возможное значение
      knob += "\"max\":"     + String(sensor->knob->max)   + ",";   // Максимальное возможное значение
      knob += "\"step\":\""  + String(sensor->knob->step)  + "\","; // Шаг
      knob += "\"title\":\"" + String(sensor->knob->title) + "\","; // Заголовок для индикатора
      knob += "\"unit\":\""  + String(sensor->knob->unit)  + "\"";  // Единицы измерения
      
      answer += String(answer.length() ? "," : "");
      answer += "{" + knob + "}";
      sensor = sensor->next;
      knob = "";
    }
  } return edging ? "[" + answer + "]" : answer;
}

/*  */
String sensors::clear(float value) {
  if ((int)value == 0) return "0";
  else if (value - (int)value == 0) return String((int)value);
  else return String(value);
}

#endif
