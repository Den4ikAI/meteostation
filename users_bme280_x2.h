#ifndef USERS_H
#define USERS_H

#include <BME280I2C.h> // https://github.com/finitespace/BME280

BME280I2C::Settings
settings_out(
  BME280::OSR_X1,
  BME280::OSR_X1,
  BME280::OSR_X1,
  BME280::Mode_Forced,
  BME280::StandbyTime_1000ms,
  BME280::Filter_Off,
  BME280::SpiEnable_False,
  BME280I2C::I2CAddr_0x76
),
settings_in(
  BME280::OSR_X1,
  BME280::OSR_X1,
  BME280::OSR_X1,
  BME280::Mode_Forced,
  BME280::StandbyTime_1000ms,
  BME280::Filter_Off,
  BME280::SpiEnable_False,
  BME280I2C::I2CAddr_0x77
);

BME280I2C BME_OUT(settings_out), BME_IN(settings_in);

/* Параметры индикаторов web интерфейса для плагина Knob
                       Мин  Макс   Шаг    Заголовок      Ед. измер.
|---------------------|----|------|------|--------------|---------| */
knob_t *T  = new knob_t( -40,   125,  ".1", "Температура", "°C");
knob_t *P  = new knob_t(-500,  9000, ".01", "Давление",    "mm");
knob_t *H  = new knob_t(   0,   100, ".01", "Влажность",   "%");
knob_t *AH = new knob_t(   0,    50,   "1", "Влажность",   "г/м³");
knob_t *DP = new knob_t( -40,   125,  ".1", "Точка росы",  "°C");

/* Функции, описывающие инициализацию датчиков */
void out_init() { BME_OUT.begin(); }
void in_init()  { BME_IN.begin(); }

/* Функции, описывающие как получить от внешнего датчика те или иные данные */
float out_temp() { return BME_OUT.temp(BME280::TempUnit_Celsius); } 
float out_hum()  { return BME_OUT.hum(); }
float out_pres() { return BME_OUT.pres(BME280::PresUnit_torr); }

/* Функции, описывающие как получить от внутреннего датчика те или иные данные */
float in_temp()  { return BME_IN.temp(BME280::TempUnit_Celsius); }
float in_hum()   { return BME_IN.hum(); }
float in_pres()  { return BME_IN.pres(BME280::PresUnit_torr); }

/* Добавление датчиков в систему */
void sensors_config() {
  Wire.begin(4, 5);
  
  /* Внешний датчик */
  sensors.add(P, device::out, 0x76, "out_pressure",    out_pres, true);
  sensors.add(H, device::out, 0x76, "out_humidity",    out_hum,  true);  
  sensors.add(T, device::out, 0x76, "out_temperature", out_init, out_temp, true);

  sensors.add(AH, device::out, "out_absoluteHumidity", [&](){ return absoluteHumidity(sensors.get("out_temperature"), sensors.get("out_humidity")); });
  sensors.add(DP, device::out, "out_dewPoint", [&](){ return dewPoint(sensors.get("out_temperature"), sensors.get("out_humidity")); });
  
  /* Внутренний датчик */
  sensors.add(P, device::in, 0x77, "in_pressure",    in_pres, true);
  sensors.add(H, device::in, 0x77, "in_humidity",    in_hum,  true);
  sensors.add(T, device::in, 0x77, "in_temperature", in_init, in_temp, true);

  sensors.add(AH, device::in, "in_absoluteHumidity", [&](){ return absoluteHumidity(sensors.get("in_temperature"), sensors.get("in_humidity")); });
  sensors.add(DP, device::in, "in_dewPoint", [&](){ return dewPoint(sensors.get("in_temperature"), sensors.get("in_humidity")); });
}

#endif
