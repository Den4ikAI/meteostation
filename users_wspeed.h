#ifndef USERS_H
#define USERS_H

/* Параметры индикаторов web интерфейса для плагина Knob
                       Мин  Макс   Шаг    Заголовок      Ед. измер.
|---------------------|----|------|------|--------------|---------| */
knob_t *S = new knob_t(0,   40,    ".1",  "Скорость в.", "м/c");

/* Параметры конфигурации для расчета скорости ветра */
#define  windSpeed_Pin 14       // GPIO микроконтроллера к которому подключен чашечный анемометр
#define  windSpeed_Pulses 1     // Количество импульсов на один оборот чашечного анемометра
#define  windSpeed_Correction 0 // Коэффициент поправки. Разница в скорости м/с между измеренным и фактическим значением полученная при калибровки
#define  windSpeed_Radius 100   // Радиус чашечного анемометра от центра до середины чашечки в милиметрах

uint32_t windSpeed_Hz = 0;       // Тут будет зафиксирована частота за ПЛАВАЮЩИЙ интервал времени
float    windSpeed = 0;          // Тут будет зафиксирована скорость ветра

/* Функция, регистрирующая импульсы с чашечного анемометра */
void ICACHE_RAM_ATTR pulseDetected() {
  ++windSpeed_Hz;
}

/* Функция, производящая расчет скорости ветра */
void pulseCounter() {
  detachInterrupt(windSpeed_Pin); // Отключаем регистрацию импульсов на время расчета
  /* Расчет скорости ветра */
  if (windSpeed_Hz) {
    float time = cron.lastRun("Wind Speed Calculation") / 1000;         // Время прошедшее с последнего расчета скорости ветра в секундах
    float circumference = 3.14 * 2 * windSpeed_Radius;                  // Длинна окружности анемометра в милиметрах
    float distance = (windSpeed_Hz / windSpeed_Pulses) * circumference; // Пройденое растояние в милиметрах
    float speed = distance / time / 1000;                               // Скорость в метрах в секунду (грязная)
    windSpeed = speed + windSpeed_Correction;                           // Скорость ветра с учетом коректировки
  } else windSpeed = 0;
  /* Сброс частоты */
  windSpeed_Hz = 0;
  attachInterrupt(windSpeed_Pin, pulseDetected, FALLING); // Возобновляем регистрацию импульсов
}

/* Функция, возвращающая последнее расчетное значение скорости ветра */
float getWindSpeed() {
  return windSpeed;
}

void sensors_config() {
  pinMode(windSpeed_Pin, INPUT_PULLUP);                             // Конфигурируем порт микроконтроллера как вход с активным встроенным подтягивающим резистором
  attachInterrupt(windSpeed_Pin, pulseDetected, FALLING);           // Регистрация импульсов с чашечного анемометра по прерыванию на землю
  cron.add(cron::time_10s, pulseCounter, "Wind Speed Calculation"); // Задача в планировщике для расчета скорости ветра
  sensors.add(S, device::out, "windSpeed", getWindSpeed);           // Добавляем сенсор скорости ветра в web интерфейс
}

#endif
