// Khai báo chân cắm
const int flameSensorPin = 4; // Chân D2 trên ESP8266 là GPIO 4
const int buzzerPin = 0;      // Chân D3 trên ESP8266 là GPIO 0

void setup() {
  Serial.begin(115200);
  
  // Thiết lập chế độ chân
  pinMode(flameSensorPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  
  Serial.println("He thong bat dau theo doi...");
}

void loop() {
  // Đọc tín hiệu từ cảm biến (thường mức THẤP là phát hiện lửa)
  int flameDetected = digitalRead(flameSensorPin);

  if (flameDetected == LOW) {
    // Phát hiện lửa
    Serial.println("CANH BAO: Phat hien co lua!");
    digitalWrite(buzzerPin, HIGH); // Bat coi
    delay(100); 
    digitalWrite(buzzerPin, LOW);  // Tao tieng keu bip bip
    delay(100);
  } else {
    // Khong co lua
    digitalWrite(buzzerPin, LOW);  // Tat coi
  }
  
  delay(100); // Cho mot chut truoc khi doc lai
}