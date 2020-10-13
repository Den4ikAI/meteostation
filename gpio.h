#ifndef GPIO_H
#define GPIO_H

/* Выбираем через какой логический уровень будем управлять нагрузкой */
bool gpio_enable = LOW; // HIGH

/*
   Управление нагрузкой при превышении определенных показаний:
    GPIO 12 (D6) - превышение установленной температуры
    GPIO 13 (D7) - превышение установленной влажности
*/
void gpio_12_13() {
  /* Все будет работать если определен датчик температуры и влажности */
  if (sensors.find("out_temperature") and sensors.find("out_humidity")) {
    pinMode(12, OUTPUT); digitalWrite(12, !gpio_enable);
    pinMode(13, OUTPUT); digitalWrite(13, !gpio_enable);
    /* Добавляем в планировщик задачу по контролю портов */
    cron.add(cron::time_5s, [&](){      
      /* Контроль превышения температуры */
      if (sensors.status("out_temperature")) {
        int temperature = sensors.get("out_temperature");
        int temperature_max = conf.param("gpio12").toInt();
        /* Гистерезис 2 градуса */
        if (isnan(temperature)) digitalWrite(12, !gpio_enable);
        else if (temperature > temperature_max and digitalRead(12) != gpio_enable) digitalWrite(12, gpio_enable);
        else if (temperature < temperature_max - 2 and digitalRead(12) == gpio_enable) digitalWrite(12, !gpio_enable);
      } else if (digitalRead(12) == gpio_enable) digitalWrite(12, !gpio_enable);
      /* Контроль превышения влажности */
      if (sensors.status("out_humidity")) {
        int humidity = sensors.get("out_humidity");
        int humidity_max = conf.param("gpio13").toInt();
        /* Гистерезис 2% */
        if (isnan(humidity)) digitalWrite(13, !gpio_enable);
        else if (humidity > humidity_max and digitalRead(13) == !gpio_enable) digitalWrite(13, gpio_enable);
        else if (humidity < humidity_max - 2 and digitalRead(13) == gpio_enable) digitalWrite(13, !gpio_enable);
      } else if (digitalRead(13) == gpio_enable) digitalWrite(13, !gpio_enable);
    });
  }
}

/*
   Управление нагрузкой при расхождении расчетной абсолютной влажности между двух датчиков
    GPIO 14 (D5) - не регулируется через WEB интерфейс т.к регулировать по сути нечего, есть только гистерезис
*/
void gpio_14() {
  /* Необходимо наличие данных о температуре и влажности внутри и снаружи помещения */
  if (sensors.find("out_temperature") and sensors.find("out_humidity") and sensors.find("in_temperature") and sensors.find("in_humidity")) {
    pinMode(14, OUTPUT); digitalWrite(14, !gpio_enable);
    /* Добавление задачи в планировщик */
    cron.add(cron::time_5s, [&](){
      if (sensors.status("out_temperature") and sensors.status("in_temperature") and sensors.status("out_humidity") and sensors.status("in_humidity")) {
        int out_hum = (int)absoluteHumidity(sensors.get("out_temperature"), sensors.get("out_humidity"));
        int in_hum  = (int)absoluteHumidity(sensors.get("in_temperature"), sensors.get("in_humidity"));
        /* Разница в показаниях должна быть больше 2 грамм на кубический сантиметр */
        if ((in_hum - out_hum) > 2 and digitalRead(14) != gpio_enable) digitalWrite(14, gpio_enable);
        else if (in_hum <= out_hum and digitalRead(14) == gpio_enable) digitalWrite(14, !gpio_enable);
      } else if (digitalRead(14) == gpio_enable) digitalWrite(14, !gpio_enable);
    });
  }
}

#endif
