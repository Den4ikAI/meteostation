#ifndef USERS_H
#define USERS_H

#include <OneWire.h>
#include <DallasTemperature.h>

DallasTemperature ds18b20(new OneWire(14));

/* Параметры индикаторов web интерфейса для плагина Knob
                       Мин  Макс   Шаг    Заголовок      Ед. измер.
|---------------------|----|------|------|--------------|---------| */
knob_t *T = new knob_t( -40,   125,  ".1", "Температура", "°C");

DeviceAddress s0 = { 0x28, 0x1D, 0x39, 0x31, 0x2, 0x0, 0x0, 0xF0 };
DeviceAddress s1 = { 0x28, 0x1D, 0x39, 0x31, 0x2, 0x0, 0x0, 0xF1 };

void sensors_config() {  
  ds18b20.begin();
  
  cron.add(cron::time_5s, [&](){ ds18b20.requestTemperatures(); }, true);  
  
  sensors.add(T, device::in, "ds18b20_s0", [&](){ return ds18b20.getTempC(s0); });
  sensors.add(T, device::in, "ds18b20_s1", [&](){ return ds18b20.getTempC(s1); });

  /*
  // Тоже самое, что и выше, только идентификация не по UID, а по индексу
  sensors.add(T, device::in, "ds18b20_s0", [&](){ return ds18b20.getTempCByIndex(0); });
  sensors.add(T, device::in, "ds18b20_s1", [&](){ return ds18b20.getTempCByIndex(1); });
  */
}

#endif
