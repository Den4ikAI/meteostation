



  

  
// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800,60000);

bool heating_season_Flag = 0;
bool RTC_Valid_Flag = 0;
byte tariff = 0;

/* RTC Setup */
void Pds(){
  // int a = BME.pres(BME280::PresUnit_torr);
 b = sensors.get("out_humidity");
  c = sensors.get("out_temperature");
timeClient.begin();
timeClient.update();
console.println(timeClient.getFormattedTime());
int hh = timeClient.getHours();
if(((hh)==21)&&((c)<15) && ((b)<55)){
  z = 1;
}

if(((hh)==21)&&((c)<14) && ((b)<62)){
  z = 1;
}

if(((hh)==21)&&((c)<13) && ((b)<59)){
  z = 1;
}

if(((hh)==21)&&((c)<12) && ((b)<56)){
  z = 1;
}

if(((hh)==21)&&((c)<11) && ((b)<64)){
  z = 1;
}

if(((hh)==21)&&((c)<10) && ((b)<61)){
 
  z = 1;
}

if(((hh)==21)&&((c)<9) && ((b)<78)){
  z = 1;
}

if(((hh)==21)&&((c)<8) && ((b)<67)){
  z = 1;
}

if(((hh)==21)&&((c)<7) && ((b)<75)){
  z = 1;
}

if(((hh)==21)&&((c)<6) && ((b)<73)){
  z = 1;
}

if(((hh)==21)&&((c)<5) && ((b)<82)){
  z = 1;
}

if(((hh)==21)&&((c)<4) && ((b)<81)){
  z = 1;
}

if(((hh)==21)&&((c)<3) && ((b)<81)){
  z = 1;
}

if(((hh)==21)&&((c)<2) && ((b)<90)){
  z = 1;
}

if(((hh)==21)&&((c)<1) && ((b)<91)){
  z = 1;
}


if(((c)<=0)){

 z=1;
}

if (((hh)> 7) && ((hh) < 21) && ((c)>0)){
  z = 0;
}
console.println(z);
console.println(c);
console.println(b);
console.println(hh);


}
