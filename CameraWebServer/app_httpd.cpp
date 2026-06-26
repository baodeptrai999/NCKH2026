#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// Khởi tạo LCD: Địa chỉ 0x27, 16 cột, 2 hàng
// Nếu màn hình không sáng, hãy thử đổi 0x27 thành 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2); 

void setup() {
  // Cấu hình chân I2C cho ESP8266 (SDA = D2, SCL = D1)
  // Lệnh này giúp đảm bảo ESP8266 nhận diện đúng chân
  Wire.begin(4, 5); // 4 là GPIO4 (D2), 5 là GPIO5 (D1)

  lcd.init();                      // Khởi tạo LCD
  lcd.backlight();                 // Bật đèn nền
  
  // Hiển thị dòng 1
  lcd.setCursor(0, 0);             // Cột 0, Dòng 0
  lcd.print("ESP8266 Online!");
  
  // Hiển thị dòng 2
  lcd.setCursor(0, 1);             // Cột 0, Dòng 1
  lcd.print("I2C LCD Test OK");
}

void loop() {
  // Hiệu ứng nhấp nháy chữ đơn giản
  lcd.setCursor(13, 1);
  lcd.print("...");
  delay(500);
  lcd.setCursor(13, 1);
  lcd.print("   ");
  delay(500);
}