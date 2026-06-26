#include <ESP8266WiFi.h>
#include <espnow.h>
#include <FirebaseESP8266.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#include <WiFiManager.h>

#define FIREBASE_HOST "baochay-e788a-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "wQLVgrdgWuC4eENASCjn8U7KeWV8G4Ku0lrzdYPh"

// ========== KHAI BÁO CHÂN AN TOÀN (Theo chuẩn của bạn) ==========
#define DHTPIN D6      // Cảm biến DHT cắm chân D6
#define BUZZER_PIN D7  // Còi cắm chân D7
#define DHTTYPE DHT11

// Ngưỡng nhiệt độ nguy hiểm để tự báo động cục bộ (phòng khi ESP-NOW lỗi)
#define NHIET_DO_NGUY_HIEM 45.0

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Cấu trúc nhận dữ liệu từ ESP32 (ESP-NOW)
typedef struct struct_message {
  int bao_chay;
} struct_message;
struct_message myData;

int buzzerState = 0;        // Báo động từ ESP-NOW (ESP32 báo cháy)
bool localOverheat = false; // Báo động cục bộ (DHT phòng này tự đo thấy quá nhiệt)
unsigned long lastReadTime = 0;

// Các biến phục vụ thuật toán đoán tình huống (Situation Estimation)
float prevTemp = -999.0;
unsigned long prevTempTime = 0;
float tempRate = 0.0;
bool preAlarmState = false;

// ================= HÀM NHẬN SÓNG ESP-NOW =================
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&myData, incomingData, sizeof(myData));

  // Nếu nhận được số 1 từ ESP32 -> Kích hoạt báo động
  if (myData.bao_chay == 1) {
    buzzerState = 1;
  } else {
    buzzerState = 0;
  }
}

// ================= CÀI ĐẶT BAN ĐẦU =================
void setup() {
  Serial.begin(115200);

  // 1. Khởi tạo Còi
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);  // Tắt còi lúc mới bật

  // 2. Khởi tạo DHT
  dht.begin();

  // 3. Khởi tạo Màn hình I2C (Ép cứng chân chuẩn như bạn làm)
  Wire.begin(4, 5);  // 4 là D2 (SDA), 5 là D1 (SCL)
  lcd.init();
  lcd.backlight();

  // Màn hình chào mừng
  lcd.setCursor(0, 0);
  lcd.print("He Thong An Toan");
  lcd.setCursor(0, 1);
  lcd.print("Khoi dong...");

  // 4. Kết nối WiFi
  lcd.setCursor(0, 0);
  lcd.print("Cai dat WiFi... ");
  lcd.setCursor(0, 1);
  lcd.print("FireSentinel_Phu");

  WiFiManager wm;
  wm.setConfigPortalTimeout(60); // 60 giây chờ kết nối hoặc cấu hình mạng mới
  
  if (wm.autoConnect("FireSentinel_Phu_Setup")) {
    lcd.clear();
    lcd.print("WiFi Connected!");
    delay(1000);
    
    // 5. Kết nối Firebase
    config.database_url = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
  } else {
    lcd.clear();
    lcd.print("Local Mode!");
    delay(2000);
  }

  // 6. Khởi tạo ESP-NOW (Chạy song song, bất kể có WiFi hay không)
  WiFi.mode(WIFI_STA); // Đảm bảo chuyển sang chế độ STA để nhận sóng ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Lỗi khởi tạo ESP-NOW");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  delay(1000);
  lcd.clear();
}

// ================= VÒNG LẶP CHÍNH =================
void loop() {
  // ƯU TIÊN 1: XỬ LÝ BÁO CHÁY (ESP-NOW truyền sang HOẶC quá nhiệt cục bộ)
  static bool wasAlarmActive = false;
  bool isAlarmActive = (buzzerState == 1 || localOverheat);

  if (isAlarmActive) {
    digitalWrite(BUZZER_PIN, HIGH);  // Bật còi
    lcd.setCursor(0, 0);

    if (buzzerState == 1) {
      // Báo động đến từ ESP32 qua ESP-NOW
      lcd.print("Phat Hien Nguy Hiem!!");
      lcd.setCursor(0, 1);
      lcd.print("MAU THOAT HIEM! ");
    } else {
      // Báo động cục bộ: DHT phòng này tự đo thấy quá nhiệt
      // (phòng trường hợp ESP-NOW mất kết nối / ESP32 lỗi)
      lcd.print("CANH BAO QUA NHIET");
      lcd.setCursor(0, 1);
      lcd.print("KIEM TRA CHAY HOAN!");
    }
    wasAlarmActive = true;
  } else {
    digitalWrite(BUZZER_PIN, LOW);  // Tắt còi
    if (wasAlarmActive) {
      lcd.clear();
      lastReadTime = 0; // Đọc và hiển thị lại nhiệt ẩm ngay lập tức
      wasAlarmActive = false;
    }
  }

  // ƯU TIÊN 2: ĐO NHIỆT ẨM & HIỂN THỊ (Đọc mỗi 3 giây 1 lần để cập nhật Firebase và tự động reset trạng thái)
  if (millis() - lastReadTime > 3000) {
    lastReadTime = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Kiểm tra lỗi cảm biến
    if (isnan(h) || isnan(t)) {
      lcd.setCursor(0, 0);
      lcd.print("Loi cam bien DHT");
      lcd.setCursor(0, 1);
      lcd.print("Kiem tra day D6 ");
      if (WiFi.status() == WL_CONNECTED) {
        Firebase.setInt(fbdo, "/Loi_DHT", 1);
      }
    }
    // Nếu đọc thành công
    else {
      if (WiFi.status() == WL_CONNECTED) {
        Firebase.setInt(fbdo, "/Loi_DHT", 0);
      }

      // ===== KIỂM TRA QUÁ NHIỆT TUYỆT ĐỐI (báo động cục bộ, tự reset khi hạ nhiệt) =====
      if (t > NHIET_DO_NGUY_HIEM) {
        localOverheat = true;
      } else {
        localOverheat = false;
      }

      // Thuật toán đoán tình huống: Tính toán dT/dt (Tốc độ tăng nhiệt độ °C/giây)
      unsigned long currentTime = millis();
      if (prevTemp > -100.0) {
        float dt = (currentTime - prevTempTime) / 1000.0; // Đổi ra giây
        if (dt > 1.0) {
          float dT = t - prevTemp;
          tempRate = dT / dt; // °C/s
          
          // Dự đoán cháy sớm (Pre-alarm): Chỉ cảnh báo khi nhiệt độ đã vượt quá 45°C và đang tăng nhanh
          if (tempRate > 0.15 && t > 45.0) {
            preAlarmState = true;
          } else {
            preAlarmState = false;
          }
        }
      }
      prevTemp = t;
      prevTempTime = currentTime;

      // Hiển thị kết quả lên LCD (Chỉ cập nhật khi không ở trạng thái báo động chính)
      if (!isAlarmActive) {
        if (preAlarmState) {
          lcd.setCursor(0, 0);
          lcd.print("TIEN CANH BAO!  ");
          lcd.setCursor(0, 1);
          lcd.print("NHIET TANG NHANH");
          
          // Cảnh báo bíp bíp ngắn (không hú liên tục như cháy thật)
          digitalWrite(BUZZER_PIN, HIGH);
          delay(100);
          digitalWrite(BUZZER_PIN, LOW);
        } else {
          lcd.setCursor(0, 0);
          lcd.print("Nhiet do: ");
          lcd.print(t, 1);
          lcd.print(" C ");

          lcd.setCursor(0, 1);
          lcd.print("Do am   : ");
          lcd.print(h, 1);
          lcd.print(" % ");
        }
      }

      // Đẩy dữ liệu lên Firebase (Lên Web App)
      if (WiFi.status() == WL_CONNECTED) {
        Firebase.setFloat(fbdo, "/Nhiet_Do", t);
        Firebase.setFloat(fbdo, "/Do_Am", h);
        Firebase.setFloat(fbdo, "/Toc_Do_Nhiet", tempRate);
        Firebase.setInt(fbdo, "/Tien_Canh_Bao", preAlarmState ? 1 : 0);
      }
    }
  }

  // ƯU TIÊN 3: LẮNG NGHE LỆNH "TẮT CÒI" TỪ APP WEB (FIREBASE)
  if (WiFi.status() == WL_CONNECTED && (buzzerState == 1 || localOverheat)) {
    if (Firebase.getInt(fbdo, "/Lenh_Tat_Bao_Dong")) {
      if (fbdo.intData() == 1) {
        buzzerState = 0;      // Tắt còi tại phòng ngủ
        localOverheat = false; // Tắt cả báo động cục bộ
        digitalWrite(BUZZER_PIN, LOW);
        lcd.clear();
        lcd.print("DA TAT BAO DONG");
        delay(2000);
        lcd.clear();
      }
    }
  }

  // Chống treo ESP
  delay(10);
}
