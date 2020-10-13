#ifndef USERS_H
#define USERS_H
/*
   Выбор подключаемых датчиков осуществляется путем выставления соответствующего состояния (ON/OFF) рядом с его именем:
      #define SENSOR_BME280 ON  <- Включен
      #define SENSOR_BMP085 OFF <- Выключен
   Но предварительно должны быть установлены библиотеки, используемые этими датчиками, а также обязательные библиотеки.
   Все ссылки ниже в описании.

   ВАЖНО: На i2c шине не допускается использование двух устройств с одним адресом.
   Если датчик BME280 не задействован, то отдельные датчики, снимающие аналогичные показания, начинают конкурировать
   между собой. Каждый следующий датчик в группе имеет приоритет над предыдущим.

   I2C
    |- BH1750
    |    |- MAX44009
    |
    |- BME280
    |    |- HDC1080 / SI7021 / HTU21D
    |    |- BMP085
    |
    |- CCS811
    ?
*/

/* Возможные состояния датчика */
#define ON  true
#define OFF false

/* Поддерживаемые датчики освещенности */
#define SENSOR_BH1750   OFF // https://github.com/claws/BH1750
#define SENSOR_MAX44009 OFF  // https://github.com/RobTillaart/Arduino/tree/master/libraries/Max44009
/* Поддерживаемые датчики атмосферного давления, температуры и влажности (BME отключает ниже стоящие датчики) */
#define SENSOR_BME280   ON // https://github.com/finitespace/BME280
/* Поддерживаемые датчики влажности и температуры в порядке их приоритета (одновременно только один) */
#define SENSOR_HDC1080  OFF // https://github.com/closedcube/ClosedCube_HDC1080_Arduino
#define SENSOR_SI7021   OFF// https://github.com/LowPowerLab/SI7021
#define SENSOR_HTU21D   OFF  // https://github.com/sparkfun/SparkFun_HTU21D_Breakout_Arduino_Library
/* Поддерживаемые датчики атмосферного давления и температуры */
#define SENSOR_BMP085   OFF  // https://github.com/adafruit/Adafruit-BMP085-Library
/* Поддерживаемые датчики CO2(eCO2), TVOC */
#define SENSOR_CCS811   OFF // https://github.com/maarten-pennings/CCS811

/* Параметры индикаторов web интерфейса для плагина Knob
                       Мин  Макс   Шаг    Заголовок          Ед. измер.
|---------------------|----|------|------|------------------|---------| */
knob_t *T  = new knob_t( -40,   125,  ".1", "Температура",     "°C");
knob_t *P  = new knob_t(-500,  9000, ".01", "Давление",        "mm");
knob_t *H  = new knob_t(   0,   100, ".01", "Влажность",       "%");
knob_t *L  = new knob_t(   0, 65000,   "1", "Освещенность",    "lx");
knob_t *C  = new knob_t(   0,  8192,   "1", "eCO<sub>2</sub>", "ppm");
knob_t *AH = new knob_t(   0,    50,   "1", "Влажность",   "г/м³");
knob_t *DP = new knob_t( -40,   125,  ".1", "Точка росы",  "°C");
knob_t *ZAM = new knob_t( 0,   1,  ".1", "Заморозок",  "ед");

/*****************************************************************************************************************************
 * Ниже описан порядок инициализации сенсоров в зависимости от выбранной библиотеки, а также пример инициализации датчиков.  *
 *****************************************************************************************************************************/

/* Освещенность */
#if SENSOR_BH1750
  #include <BH1750.h>
  BH1750 BH1750;
#elif SENSOR_MAX44009
  #include <Max44009.h>
  Max44009 MAX44009(0x4A);
#endif

/* Барометр + влажность + температура */
#if SENSOR_BME280
  #include <BME280I2C.h>
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
  );
  BME280I2C BME(settings_out);
#else
  /* Влажность + температура */
  #if SENSOR_HDC1080
    #include <ClosedCube_HDC1080.h>
    ClosedCube_HDC1080 HDC1080;
  #elif SENSOR_SI7021
    #include <SI7021.h>
    SI7021 SI7021;
  #elif SENSOR_HTU21D
    #include <SparkFunHTU21D.h>
    HTU21D HTU21D;
  #endif

  /* Барометр + температура */
  #if SENSOR_BMP085
    #include <Adafruit_BMP085.h>
    Adafruit_BMP085 BMP085;
  #endif
#endif

/* CO2 */
#if SENSOR_CCS811
  #include <ccs811.h>
  CCS811 ccs811;
#endif

/*
   Функция описывает пример конфигурации датчиков в зависимости от
*/
void sensors_config() {
  Wire.begin(4, 5);
  BME.begin();
  
// int a = BME.pres(BME280::PresUnit_torr);
 //b = BME.hum();
 // c = BME.temp(BME280::TempUnit_Celsius);
  
  /* пример программного сенсора рассчитывающего абсолютную влажность */
  

  /* датчики CO2 */
  #if SENSOR_CCS811
    sensors.add(C, device::out, 0x5A, "out_co2", [&](){ 
      ccs811.begin(); 
      ccs811.start(CCS811_MODE_1SEC); 
    }, [&](){ 
      uint16_t eco2, etvoc, errstat, raw;
      ccs811.set_envdata(sensors.get("out_temperature"), sensors.get("out_humidity"));
      ccs811.read(&eco2, &etvoc, &errstat, &raw); 
      return eco2; 
    }, true);
  #endif

  /* датчики освещенности */
  #if SENSOR_BH1750
    sensors.add(L, device::out, 0x23, "out_light", [&](){ BH1750.begin(); }, [&](){ return BH1750.readLightLevel(); }, true);
  #elif SENSOR_MAX44009
    sensors.add(L, device::out, 0x4A, "out_light", [&](){ MAX44009.setAutomaticMode(); }, [&](){ return MAX44009.getLux(); }, true);
  #endif
    
  #if SENSOR_BME280
    /* датчик 3 в 1 */        
    sensors.add(P, device::out, 0x76, "out_pressure",                           [&](){ return BME.pres(BME280::PresUnit_torr); }, true);
    sensors.add(H, device::out, 0x76, "out_humidity",                           [&](){ return BME.hum(); }, true);
    sensors.add(T, device::out, 0x76, "out_temperature", [&](){ BME.begin(); }, [&](){ return BME.temp(BME280::TempUnit_Celsius); }, true);
     sensors.add(ZAM, device::out, "ед",[&](){ 
    return z; 
  });

  
  #else
    /* датчики влажности */
    #if SENSOR_HDC1080
      sensors.add(H, device::out, 0x40, "out_humidity", [&](){ HDC1080.begin();    }, [&](){ return HDC1080.readHumidity(); }, true);
    #elif SENSOR_SI7021
      sensors.add(H, device::out, 0x40, "out_humidity", [&](){ SI7021.begin(4, 5); }, [&](){ return SI7021.getHumidityPercent(); }, true);
      
    #elif SENSOR_HTU21D
      sensors.add(H, device::out, 0x40, "out_humidity", [&](){ HTU21D.begin();     }, [&](){ return HTU21D.readHumidity(); }, true);
    #endif
    /* датчик давления и температуры */
    #if SENSOR_BMP085
      sensors.add(P, device::out, 0x77, "out_pressure",                              [&](){ return BMP085.readPressure() / 133.3; }, true);
      sensors.add(T, device::out, 0x77, "out_temperature", [&](){ BMP085.begin(); }, [&](){ return BMP085.readTemperature(); }, true);
    #endif
  #endif

  /* пример еще нескольких программных сенсоров */
  
    sensors.add(DP, device::out, "out_dewPoint", [&](){
      return dewPoint(sensors.get("out_temperature"), sensors.get("out_humidity"));
    });
  
  /*
  sensors.add(new knob_t(-100, 0, "1", "RSSI", "dbm"), device::in, "rssi",[&](){ 
    return wifi.isConnected() ? WiFi.RSSI() : 0; 
  });
  sensors.add(new knob_t(0, 5, ".01", "Питание", "V"), device::in, "vcc", [&](){ 
    return ESP.getVcc() * 0.001; 
  });
  sensors.add(new knob_t(0, 81920, "1", "RAM", "Byte"), device::in, "ram", [&](){
    return 81920 - ESP.getFreeHeap();
  });
  */
  
  /* тестовый вывод *//*
  cron.add(cron::time_10m + cron::minute, [&](){
    #ifdef console
      //console.println(sensors.list());
      console.println(sensors.status());
      console.println(sensors.get());
      console.println(sensors.log());
    #endif
  });
  */
}

#endif
