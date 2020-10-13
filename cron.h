#ifndef CRON_H
#define CRON_H

class cronEvent {
  public:
    typedef std::function<void(void)> cronUserFunction_t;
    cronEvent(unsigned long interval, cronUserFunction_t fn, cronEvent *next, const char *id = 0) {
      this->interval = interval;
      this->function = fn;
      this->time = millis();
      this->next = next;
      this->id = id;
    }
    unsigned long interval = 0;
    unsigned long time = 0;
    cronUserFunction_t function;
    cronEvent *next = 0;
    const char *id;
};

class cron {  
  public:
    /* Читаемые имена для самых часто используемых интервалов времени. */
    enum {
      time_1s  = 1000,
      time_5s  = 5000,
      time_10s = 10000,
      time_15s = 15000,
      time_30s = 30000,
      time_1m  = 60000,
      time_5m  = 300000,
      time_10m = 600000,
      time_15m = 900000,
      time_30m = 1800000,
      time_1h  = 3600000,
      time_5h  = 18000000,
      time_10h = 36000000,
      time_12h = 43200000,
      time_1d  = 86400000,
    };
    /* Cокращения для фундаментальных значений. */
    enum {
      second = cron::time_1s,
      minute = cron::time_1m,
      hour   = cron::time_1h,
      day    = cron::time_1d
    };
    /*
       Добавление нового задания в планировщик.
       В качестве аргумента принимает интервал вызова в ms и имя вызываемой функции.
       Последний, не обязательный, параметр устанавливает идентификатор для поиска задачи.
       Параметр coldStart при значении true заставит произвести первый запуск при добавлении задачи в очередь, а не по таймеру.
    */
    void add(unsigned long interval, cronEvent::cronUserFunction_t fn, const char *id);
    void add(unsigned long interval, cronEvent::cronUserFunction_t fn, bool coldStart, const char *id);
    /*
       Обработчик очереди заданий.
       Прописывается в основном цикле программы как cron.handleEvents();
    */
    void handleEvents();
    /*
       Поиск задания по имени.
    */
    cronEvent *find(const char *id);
    /*
       Возвращает количество ms прошедших с момента последнего вызова задания.
       В качестве аргумента принимает указатель или идентификатор задачи.
    */
    unsigned long lastRun(cronEvent *event);
    unsigned long lastRun(const char *id);
    /*
       Обновление таймера задания.
       Можно задать новый интервал времени.
    */
    void update(const char *id);
    void update(const char *id, unsigned long interval);
    /*
       Остановка выполнения задания.
    */
    void stop(const char *id);
    /*
       Проверяет активна задача или нет.
    */
    bool isActive(const char *id);

  private:
    cronEvent *eventList = 0;
} cron;

/*  */
void cron::add(unsigned long interval, cronEvent::cronUserFunction_t fn, const char *id = 0) {
  cronEvent *newEvent = new cronEvent(interval, fn, this->eventList, id);
  this->eventList = newEvent;
}

/*  */
void cron::add(unsigned long interval, cronEvent::cronUserFunction_t fn, bool coldStart, const char *id = 0) {
  this->add(interval, fn, id);
  if (coldStart) fn();
}

/*  */
void cron::handleEvents() {
  if (this->eventList) {
    cronEvent *currentEvent = this->eventList;
    while (currentEvent) {
      if (currentEvent->interval and this->lastRun(currentEvent) > currentEvent->interval) {
        currentEvent->function();
        currentEvent->time = millis();
        yield();
      } currentEvent = currentEvent->next;
    }
  }
}

/*  */
cronEvent *cron::find(const char *id) {
  if (this->eventList) {
    cronEvent *currentEvent = this->eventList;
    while (currentEvent) {
      if (currentEvent->id and currentEvent->id == id) return currentEvent;
      currentEvent = currentEvent->next;
    }
  } return 0;
}

/*  */
unsigned long cron::lastRun(cronEvent *event) {
  if (event) {
    unsigned long currentTime = millis();
    return (currentTime > event->time) ? currentTime - event->time : currentTime + (0xffffffff - event->time) + 1;
  } return 0;
}

/*  */
unsigned long cron::lastRun(const char *id) {
  cronEvent *event = this->find(id);
  return !event? 0 : this->lastRun(event);
}

/*  */
void cron::update(const char *id) {
  cronEvent *event = this->find(id);
  if (event) event->time = millis();
}

/*  */
void cron::update(const char *id, unsigned long interval) {
  cronEvent *event = this->find(id);
  if (event) {
    event->interval = interval;
    event->time = millis();
  }
}

/*  */
void cron::stop(const char *id) {
  cronEvent *event = this->find(id);
  if (event) event->interval = 0;
}

/*  */
bool cron::isActive(const char *id) {
  cronEvent *event = this->find(id);
  return event ? event->interval != 0 : false;
}

#endif
