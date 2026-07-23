

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RTClib.h>

U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, 5, 13, 17);

RTC_DS1307 rtc;

void initHardware();
void updateDisplay(DateTime now);

void setup() {
  initHardware();
}

void loop() {
  DateTime now = rtc.now();
  updateDisplay(now);
  delay(1000);
}

void initHardware() {
  Serial.begin(115200);
  delay(500);

  Wire.begin();
  
  if (!rtc.begin()) {
    Serial.println("Помилка: RTC DS1307 не знайдено!");
    while (1) delay(10);
  }

  if (!rtc.isrunning()) {
    Serial.println("RTC не працює, встановлюю час на компіляцію...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  SPI.begin(12, -1, 11, 5);
  u8g2.begin();
  
  Serial.println("Система успішно запущена!");
}

void updateDisplay(DateTime now) {
  char timeBuffer[9];
  char dateBuffer[16];

  sprintf(timeBuffer, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  const char* daysOfTheWeek[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  
  sprintf(dateBuffer, "%s %02d.%02d.%04d", 
          daysOfTheWeek[now.dayOfTheWeek()], 
          now.day(), 
          now.month(), 
          now.year());

  u8g2.clearBuffer();                      
  
  u8g2.setFont(u8g2_font_ncenR08_tf); 
  u8g2.drawStr(10, 20, dateBuffer);        

  u8g2.setFont(u8g2_font_fub14_tf); 
  u8g2.drawStr(5, 50, timeBuffer);         

  u8g2.sendBuffer();                       
}

