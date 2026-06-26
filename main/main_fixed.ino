    #define BLYNK_PRINT Serial
    #define BLYNK_TEMPLATE_ID           "TMPL6VHV8uDUc"
    #define BLYNK_TEMPLATE_NAME         "testBlynkMQ2"

    #include <WiFi.h>
    #include <WiFiClient.h>
    #include <BlynkSimpleEsp32.h>
    #include <FirebaseESP32.h>
    #include <WiFiManager.h>

    #define FIREBASE_HOST "baochay-e788a-default-rtdb.firebaseio.com"
    #define FIREBASE_AUTH "wQLVgrdgWuC4eENASCjn8U7KeWV8G4Ku0lrzdYPh"

    FirebaseData fbdo;
    FirebaseConfig config;
    FirebaseAuth auth;

    #include <LiquidCrystal.h>
    #include <WebServer.h>
    #include <ESPmDNS.h>
    #include <EEPROM.h>
    #include "def.h"
    #include "config.h"
    #include "mybutton.h"
    #include <SimpleKalmanFilter.h>
    #include <ESP32Servo.h>
    #include <esp_now.h>

    uint8_t address_ESP8266[] = {0x8C, 0xAA, 0xB5, 0xF4, 0x5D, 0xCE};
    typedef struct struct_message { int bao_chay; } struct_message;
    struct_message myData;
    esp_now_peer_info_t peerInfo;

    void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
        Serial.printf("ESP-NOW: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
    }

    SimpleKalmanFilter kfilter(2, 2, 0.1);

    #define buttonPinMENU   5
    #define buttonPinDOWN   18
    #define buttonPinUP     19
    #define buttonPinONOFF  21
    #define BUTTON1_ID  1
    #define BUTTON2_ID  2
    #define BUTTON3_ID  3
    #define BUTTON4_ID  4

    Button buttonMENU, buttonDOWN, buttonUP, buttonONOFF;
    void button_press_short_callback(uint8_t button_id);
    void button_press_long_callback(uint8_t button_id);

    LiquidCrystal My_LCD(15, 13, 12, 14, 27, 26);

    #define SENSOR_MQ2      35
    #define SENSOR_FIRE     34
    #define SENSOR_FIRE_ON  0
    #define SENSOR_FIRE_OFF 1
    #define BUZZER          23
    #define BUZZER_ON       HIGH
    #define BUZZER_OFF      LOW
    #define ON    1
    #define OFF   0
    #define AUTO  1
    #define MANUAL 0
    #define AP_MODE  0
    #define STA_MODE 1

    // ============ NGƯỠNG GAS MẶC ĐỊNH (thay đổi ở đây) ============
    #define THRESSHOLD 100

    bool relay1State = OFF, relay2State = OFF;
    bool autoManual = AUTO;
    int  mq2Thresshold = THRESSHOLD;
    int  windowState = OFF;
    bool AP_STA_MODE = STA_MODE;
    volatile int buzzerON = 0;
    bool isMQ2Fault = false;

    // Blynk
    char BLYNK_AUTH_TOKEN[32] = "";
    bool blynkConnect = false;
    BlynkTimer blynkTimer;

    Servo myservo1, myservo2;
    WebServer server(80);
    WiFiClient client;

    TaskHandle_t TaskMainDisplay_handle = NULL;
    TaskHandle_t TaskButton_handle = NULL;

    // Khai báo nguyên mẫu hàm
    void TaskButton(void *pvParameters);
    void TaskFirebase(void *pvParameters);
    void TaskMainDisplay(void *pvParameters);
    void TaskBuzzer(void *pvParameters);
    void TaskBlynk(void *pvParameters);
    void myBlynkTimer();
    int  readMQ2();
    int  readFireSensor();
    void LCD1602_Init();
    void LCDPrint(int hang, int cot, const char *text, int clearOrNot);
    void controlRelay(int relay, int state);
    void openWindow();
    void closeWindow();
    void controlWindow(int onoff);
    void printRelayState();
    void printMode();
    void printMQ2();
    void printWindowState(int state);
    void buzzerWarning();
    void buzzerBip();
    void writeThresHoldEEPROM(int val);
    void triggerESPNow(int state);

    // =========================================================================
    // LCD
    // =========================================================================
    void LCDPrint(int hang, int cot, const char *text, int clearOrNot) {
        if (clearOrNot == 1) { My_LCD.clear(); delay(5); }
        My_LCD.setCursor(cot, hang);
        My_LCD.print(text);
    }

    void LCD1602_Init() {
        My_LCD.begin(16, 2);
        My_LCD.clear();
        LCDPrint(0, 2, "Fire Sentinel", 0);
        LCDPrint(1, 5, "System", 0);
    }

    void printMode() {
        LCDPrint(0, 15, autoManual == AUTO ? "A" : "M", 0);
    }

    void printRelayState() {
        LCDPrint(1, 0, relay1State ? "RL1:ON  " : "RL1:OFF ", 0);
        LCDPrint(1, 9, relay2State ? "RL2:ON " : "RL2:OFF", 0);
    }

    void printMQ2() {
        String s = "GAS:" + String(readMQ2()) + "ppm ";
        My_LCD.setCursor(0, 0);
        My_LCD.print(s);
    }

    void printWindowState(int state) {
        vTaskSuspend(TaskMainDisplay_handle);
        My_LCD.clear(); delay(5);
        LCDPrint(0, 2, state ? "OPEN WINDOW" : "CLOSE WINDOW", 0);
        delay(1000);
        My_LCD.clear(); delay(5);
        printMode(); printRelayState(); printMQ2();
        vTaskResume(TaskMainDisplay_handle);
    }

    // =========================================================================
    // SETUP
    // =========================================================================
    void setup() {
        Serial.begin(115200);
        EEPROM.begin(512);
        LCD1602_Init();
        delay(2000);
        My_LCD.clear();

        WiFi.mode(WIFI_AP_STA);
        pinMode(RELAY1, OUTPUT); pinMode(RELAY2, OUTPUT);
        controlRelay(RELAY1, OFF); controlRelay(RELAY2, OFF);
        pinMode(BUZZER, OUTPUT);
        digitalWrite(BUZZER, BUZZER_OFF);
        pinMode(SENSOR_FIRE, INPUT);

        ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
        ESP32PWM::allocateTimer(2); ESP32PWM::allocateTimer(3);
        myservo1.setPeriodHertz(50); myservo2.setPeriodHertz(50);
        myservo1.attach(SERVO1, 500, 2400);
        myservo2.attach(SERVO2, 500, 2400);
        closeWindow();

        // Đọc Blynk Auth Token từ EEPROM (địa chỉ 64 đến 95)
        String Etoken = "";
        for (int i = 64; i < 96; ++i) {
            char c = char(EEPROM.read(i));
            if (c != 0 && c != 255) Etoken += c;
        }
        if (Etoken.length() > 1) strcpy(BLYNK_AUTH_TOKEN, Etoken.c_str());

        // WiFiManager: tự phát WiFi cấu hình nếu chưa kết nối được
        LCDPrint(0, 0, "Cai dat WiFi...", 1);
        LCDPrint(1, 0, "FireSentinel_M", 0);

        WiFiManager wm;
        wm.setConfigPortalTimeout(60);

        WiFiManagerParameter custom_blynk_token("blynk", "Blynk Auth Token", BLYNK_AUTH_TOKEN, 32);
        wm.addParameter(&custom_blynk_token);
        
        if (wm.autoConnect("FireSentinel_Main_Setup")) {
            WiFi.mode(WIFI_AP_STA);
            LCDPrint(0, 0, "WiFi Connected!", 1);
            delay(1000);

            // Lưu token Blynk mới nếu người dùng nhập
            String token_val = custom_blynk_token.getValue();
            if (token_val.length() > 0 && token_val != String(BLYNK_AUTH_TOKEN)) {
                for (int i = 0; i < 32; ++i)
                    EEPROM.write(64 + i, i < (int)token_val.length() ? token_val[i] : 0);
                EEPROM.commit();
                strcpy(BLYNK_AUTH_TOKEN, token_val.c_str());
            }
            
            // Khởi động Firebase
            config.database_url = FIREBASE_HOST;
            config.signer.tokens.legacy_token = FIREBASE_AUTH;
            Firebase.begin(&config, &auth);
            Firebase.reconnectWiFi(true);
            Firebase.setInt(fbdo, "/Lenh_Tat_Bao_Dong", 0);
            LCDPrint(0, 0, "Firebase OK!", 1);
            delay(1000);
            xTaskCreatePinnedToCore(TaskFirebase, "Firebase", 4096, NULL, 5, NULL, 0);

            // Khởi động Blynk nếu có Token
            if (strlen(BLYNK_AUTH_TOKEN) > 1) {
                Blynk.config(BLYNK_AUTH_TOKEN);
                blynkConnect = Blynk.connect();
                if (blynkConnect) {
                    blynkTimer.setInterval(2000L, myBlynkTimer);
                    xTaskCreatePinnedToCore(TaskBlynk, "TaskBlynk", 3072, NULL, 5, NULL, 0);
                    LCDPrint(0, 0, "Blynk Connected", 1);
                    delay(1000);
                }
            }
        } else {
            WiFi.mode(WIFI_AP_STA);
            LCDPrint(0, 0, "WiFi Timeout!", 1);
            LCDPrint(1, 0, "Local Mode", 0);
            delay(3000);
        }

        // Khởi động ESP-NOW
        if (esp_now_init() != ESP_OK) {
            Serial.println("Loi ESP-NOW");
        } else {
            esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
            memcpy(peerInfo.peer_addr, address_ESP8266, 6);
            peerInfo.channel = 0; peerInfo.encrypt = false;
            esp_now_add_peer(&peerInfo);
        }

        // ÉP BUỘC ngưỡng gas = 100 (bỏ qua EEPROM cũ để test)
        mq2Thresshold = THRESSHOLD;
        writeThresHoldEEPROM(mq2Thresshold);
        Serial.printf("==> Nguong gas EP BUOC = %d\n", mq2Thresshold);

        autoManual = AUTO;
        EEPROM.write(201, AUTO);
        EEPROM.commit();

        Serial.printf("Nguong MQ2: %d | Mode: %s\n", mq2Thresshold, autoManual ? "AUTO" : "MANUAL");

        xTaskCreatePinnedToCore(TaskMainDisplay, "MainDisplay", 3072, NULL, 5, &TaskMainDisplay_handle, 0);
        xTaskCreatePinnedToCore(TaskBuzzer,      "Buzzer",      2048, NULL, 5, NULL, 0);
        xTaskCreatePinnedToCore(TaskButton,      "Button",      2048, NULL, 5, &TaskButton_handle, 0);
    }

    void loop() { vTaskDelete(NULL); }

    // =========================================================================
    // HELPER FUNCTIONS
    // =========================================================================
    void writeThresHoldEEPROM(int val) {
        EEPROM.write(202, val / 100);
        EEPROM.write(203, val % 100);
        EEPROM.commit();
    }

    void triggerESPNow(int state) {
        static int last_esp_now_state = -1;
        if (state != last_esp_now_state) {
            myData.bao_chay = state;
            esp_now_send(address_ESP8266, (uint8_t *)&myData, sizeof(myData));
            last_esp_now_state = state;
            Serial.printf("ESP-NOW: -> %d\n", state);
        }
    }

    int readMQ2() {
        int raw = analogRead(SENSOR_MQ2);
        isMQ2Fault = false; // Tạm tắt kiểm tra lỗi cảm biến
        float filtered = kfilter.updateEstimate(raw);
        return map((int)filtered, 0, 4095, 0, 10000);
    }

    int readFireSensor() { return digitalRead(SENSOR_FIRE); }
    void controlRelay(int relay, int state) { digitalWrite(relay, state); }
    void openWindow()  { myservo1.write(0);  myservo2.write(180); }
    void closeWindow() { myservo1.write(90); myservo2.write(90);  }
    void controlWindow(int onoff) { onoff ? openWindow() : closeWindow(); }
    void buzzerBip() {}
    void buzzerWarning() {
        digitalWrite(BUZZER, BUZZER_ON);  delay(2000);
        digitalWrite(BUZZER, BUZZER_OFF); delay(500);
    }

    // =========================================================================
    // TASKS
    // =========================================================================
    void TaskFirebase(void *pvParameters) {
        static int lastBuzzer = -1;
        static int lastR1 = -1;
        static int lastR2 = -1;
        static int lastWin = -1;
        static int lastFault = -1;
        static int lastFire = -1;
        static int lastGasAlert = -1;

        while (1) {
            if (WiFi.status() == WL_CONNECTED) {
                Firebase.setInt(fbdo, "/Khoi_Gas", readMQ2());

                // Đồng bộ trạng thái lỗi cảm biến Gas lên Web App
                if (isMQ2Fault != lastFault) {
                    Firebase.setInt(fbdo, "/Loi_MQ2", isMQ2Fault ? 1 : 0);
                    lastFault = isMQ2Fault;
                }

                // Đọc lệnh Demo Cháy từ Firebase
                if (Firebase.getInt(fbdo, "/Trang_Thai_Chay")) {
                    int fireVal = fbdo.intData();
                    if (fireVal != lastFire) {
                        lastFire = fireVal;
                        buzzerON = fireVal;
                        if (fireVal == 1) {
                            relay1State = ON; relay2State = ON;
                            controlRelay(RELAY1, ON); controlRelay(RELAY2, ON);
                            windowState = 1; controlWindow(1);
                            triggerESPNow(1);
                        } else {
                            relay1State = OFF; relay2State = OFF;
                            controlRelay(RELAY1, OFF); controlRelay(RELAY2, OFF);
                            windowState = 0; controlWindow(0);
                            triggerESPNow(0);
                        }
                        Serial.printf("--> App Web: Demo Chay -> %d\n", fireVal);
                    }
                }

                // Cập nhật trạng thái rò rỉ Gas lên Firebase
                int currentGasAlert = ((readMQ2() >= mq2Thresshold) && !isMQ2Fault) ? 1 : 0;
                if (currentGasAlert != lastGasAlert) {
                    Firebase.setInt(fbdo, "/Trang_Thai_Gas", currentGasAlert);
                    lastGasAlert = currentGasAlert;
                }

                // Đồng bộ trạng thái Cháy lên Firebase
                int currentFire = (readFireSensor() == SENSOR_FIRE_ON || (buzzerON && !currentGasAlert)) ? 1 : 0;
                if (currentFire != lastFire) {
                    Firebase.setInt(fbdo, "/Trang_Thai_Chay", currentFire);
                    lastFire = currentFire;
                }

                // Đọc lệnh điều khiển Máy Bơm (Relay 1) từ Firebase
                if (Firebase.getInt(fbdo, "/Relay1")) {
                    int r1Val = fbdo.intData();
                    if (r1Val != lastR1) {
                        relay1State = r1Val;
                        controlRelay(RELAY1, relay1State);
                        autoManual = MANUAL;
                        EEPROM.write(201, MANUAL); EEPROM.commit();
                        if (blynkConnect) {
                            Blynk.virtualWrite(V1, relay1State ? (relay2State ? 3 : 1) : (relay2State ? 2 : 0));
                            Blynk.virtualWrite(V4, autoManual);
                        }
                        Serial.printf("--> App Web: Dieu khien May Bom -> %d\n", r1Val);
                    }
                }

                // Đọc lệnh điều khiển Quạt hút (Relay 2) từ Firebase
                if (Firebase.getInt(fbdo, "/Relay2")) {
                    int r2Val = fbdo.intData();
                    if (r2Val != lastR2) {
                        relay2State = r2Val;
                        controlRelay(RELAY2, relay2State);
                        autoManual = MANUAL;
                        EEPROM.write(201, MANUAL); EEPROM.commit();
                        if (blynkConnect) {
                            Blynk.virtualWrite(V1, relay1State ? (relay2State ? 3 : 1) : (relay2State ? 2 : 0));
                            Blynk.virtualWrite(V4, autoManual);
                        }
                        Serial.printf("--> App Web: Dieu khien Quat -> %d\n", r2Val);
                    }
                }

                // Đọc lệnh điều khiển Cửa sổ (Cua_So / Servo) từ Firebase
                if (Firebase.getInt(fbdo, "/Cua_So")) {
                    int wVal = fbdo.intData();
                    if (wVal != lastWin) {
                        windowState = wVal;
                        controlWindow(windowState);
                        autoManual = MANUAL;
                        EEPROM.write(201, MANUAL); EEPROM.commit();
                        if (blynkConnect) {
                            Blynk.virtualWrite(V2, windowState);
                            Blynk.virtualWrite(V4, autoManual);
                        }
                        Serial.printf("--> App Web: Dieu khien Cua So (Servo) -> %d\n", wVal);
                    }
                }

                // Cập nhật trạng thái thực tế lên Firebase để đồng bộ giao diện Web
                if (relay1State != lastR1) {
                    Firebase.setInt(fbdo, "/Relay1", relay1State);
                    lastR1 = relay1State;
                }
                if (relay2State != lastR2) {
                    Firebase.setInt(fbdo, "/Relay2", relay2State);
                    lastR2 = relay2State;
                }
                if (windowState != lastWin) {
                    Firebase.setInt(fbdo, "/Cua_So", windowState);
                    lastWin = windowState;
                }

                // Lệnh TẮT BÁO ĐỘNG từ App Web
                if (Firebase.getInt(fbdo, "/Lenh_Tat_Bao_Dong")) {
                    if (fbdo.intData() == 1) {
                        buzzerON = 0;
                        relay1State = OFF; relay2State = OFF;
                        controlRelay(RELAY1, OFF); controlRelay(RELAY2, OFF);
                        windowState = 0; controlWindow(0);
                        triggerESPNow(0);
                        Firebase.setInt(fbdo, "/Lenh_Tat_Bao_Dong", 0);
                        Firebase.setInt(fbdo, "/Trang_Thai_Chay", 0);
                        Firebase.setInt(fbdo, "/Relay1", 0);
                        Firebase.setInt(fbdo, "/Relay2", 0);
                        Firebase.setInt(fbdo, "/Cua_So", 0);
                        lastBuzzer = 0; lastR1 = 0; lastR2 = 0; lastWin = 0;
                        if (blynkConnect) {
                            Blynk.virtualWrite(V1, 0);
                            Blynk.virtualWrite(V2, 0);
                        }
                        Serial.println("--> App: TAT BAO DONG!");
                    }
                }
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    void TaskMainDisplay(void *pvParameters) {
        for (int i = 15; i > 0; i--) {
            My_LCD.clear();
            LCDPrint(0, 1, "Sensor Warm-up", 0);
            char buf[16];
            sprintf(buf, "Waiting... %ds", i);
            LCDPrint(1, 2, buf, 0);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        My_LCD.clear();
        printRelayState(); printMode(); printMQ2();

        while (1) {
            int mq2  = readMQ2();
            int fire = readFireSensor();
            bool gasHigh = (mq2 >= mq2Thresshold) && !isMQ2Fault;
            bool fireOn  = (fire == SENSOR_FIRE_ON);

            Serial.printf("MQ2=%d | Threshold=%d | gasHigh=%d | fireOn=%d | buzzerON=%d | Free Heap=%d\n",
                        mq2, mq2Thresshold, gasHigh, fireOn, buzzerON, ESP.getFreeHeap());

            // ===== CHẾ ĐỘ AUTO: Báo động GIỮ LIÊN TỤC cho đến khi App tắt =====
            if (autoManual == AUTO) {
                if (gasHigh || fireOn) {
                    buzzerON = 1;
                    triggerESPNow(1);
                    relay1State = ON; relay2State = ON;
                    controlRelay(RELAY1, ON); controlRelay(RELAY2, ON);
                    windowState = 1;
                    controlWindow(1);
                    if (blynkConnect) {
                        Blynk.virtualWrite(V1, 3);
                        Blynk.virtualWrite(V2, 1);
                    }
                    vTaskDelay(3000 / portTICK_PERIOD_MS);
                }
                // KHÔNG tự tắt! Chỉ tắt qua App (Firebase /Lenh_Tat_Bao_Dong)
            }

            // ===== HIỂN THỊ LCD =====
            if (gasHigh && fireOn) {
                My_LCD.clear();
                LCDPrint(0, 4, "WARNING", 0);
                LCDPrint(1, 2, "GAS DETECTED", 0);
                buzzerON = 1; triggerESPNow(1);
                vTaskDelay(500 / portTICK_PERIOD_MS);
                My_LCD.clear();
                LCDPrint(0, 4, "WARNING", 0);
                LCDPrint(1, 2, "FIRE DETECTED", 0);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            } else if (gasHigh) {
                My_LCD.clear();
                LCDPrint(0, 4, "WARNING", 0);
                LCDPrint(1, 2, "GAS DETECTED", 0);
                buzzerON = 1; triggerESPNow(1);
            } else if (fireOn) {
                My_LCD.clear();
                LCDPrint(0, 4, "WARNING", 0);
                LCDPrint(1, 2, "FIRE DETECTED", 0);
                buzzerON = 1; triggerESPNow(1);
            } else {
                My_LCD.clear(); delay(5);
                printRelayState(); printMode(); printMQ2();
            }

            triggerESPNow(buzzerON);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    void TaskBuzzer(void *pvParameters) {
        while (1) {
            if (buzzerON) buzzerWarning();
            else digitalWrite(BUZZER, BUZZER_OFF);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }

    void TaskButton(void *pvParameters) {
        pinMode(buttonPinMENU, INPUT); pinMode(buttonPinDOWN, INPUT);
        pinMode(buttonPinUP, INPUT); pinMode(buttonPinONOFF, INPUT);
        button_init(&buttonMENU, buttonPinMENU, BUTTON1_ID);
        button_init(&buttonDOWN, buttonPinDOWN, BUTTON2_ID);
        button_init(&buttonUP,   buttonPinUP,   BUTTON3_ID);
        button_init(&buttonONOFF,buttonPinONOFF,BUTTON4_ID);
        button_pressshort_set_callback((void *)button_press_short_callback);
        button_presslong_set_callback((void *)button_press_long_callback);
        while (1) {
            handle_button(&buttonMENU); handle_button(&buttonDOWN);
            handle_button(&buttonUP);   handle_button(&buttonONOFF);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }

    // =========================================================================
    // BUTTON CALLBACKS - NÚT BẤM LUÔN HOẠT ĐỘNG TRONG MỌI HOÀN CẢNH
    // =========================================================================
    int modeSetThresHold = 0;

    void button_press_short_callback(uint8_t button_id) {
        switch (button_id) {

            case BUTTON1_ID:
                modeSetThresHold = 1 - modeSetThresHold;
                if (modeSetThresHold) {
                    vTaskSuspend(TaskMainDisplay_handle);
                    My_LCD.clear(); delay(5);
                    LCDPrint(0, 0, " SET Threshold ", 0);
                    char str[20];
                    sprintf(str, "%d", mq2Thresshold);
                    LCDPrint(1, 6, str, 0);
                } else {
                    LCDPrint(1, 1, " SUCCESSFULLY ", 0);
                    delay(1000);
                    My_LCD.clear(); delay(5);
                    printMQ2(); printRelayState(); printMode();
                    writeThresHoldEEPROM(mq2Thresshold);
                    vTaskResume(TaskMainDisplay_handle);
                }
            break;

            case BUTTON2_ID:
                if (modeSetThresHold) {
                    mq2Thresshold = min(mq2Thresshold + 50, 9999);
                    My_LCD.clear(); delay(5);
                    LCDPrint(0, 0, " SET Threshold ", 0);
                    char str2[20];
                    sprintf(str2, "%d", mq2Thresshold);
                    LCDPrint(1, 6, str2, 0);
                } else {
                    relay1State ^= 1;
                    controlRelay(RELAY1, relay1State);
                    autoManual = MANUAL;
                    EEPROM.write(201, MANUAL); EEPROM.commit();
                    My_LCD.clear(); delay(5);
                    printRelayState(); printMode(); printMQ2();
                }
            break;

            case BUTTON3_ID:
                if (modeSetThresHold) {
                    mq2Thresshold = max(mq2Thresshold - 50, 50);
                    My_LCD.clear(); delay(5);
                    LCDPrint(0, 0, " SET Threshold ", 0);
                    char str3[20];
                    sprintf(str3, "%d", mq2Thresshold);
                    LCDPrint(1, 6, str3, 0);
                } else {
                    relay2State ^= 1;
                    controlRelay(RELAY2, relay2State);
                    autoManual = MANUAL;
                    EEPROM.write(201, MANUAL); EEPROM.commit();
                    My_LCD.clear(); delay(5);
                    printRelayState(); printMode(); printMQ2();
                }
            break;

            case BUTTON4_ID:
                if (!modeSetThresHold) {
                    windowState ^= 1;
                    controlWindow(windowState);
                    printWindowState(windowState);
                }
            break;
        }
    }

    void button_press_long_callback(uint8_t button_id) {
        switch (button_id) {
            case BUTTON1_ID: break;
            case BUTTON2_ID: break;
            case BUTTON3_ID: break;
            case BUTTON4_ID:
                // Nhấn giữ nút 4: Tắt báo động thủ công bằng phần cứng
                buzzerON = 0;
                relay1State = OFF; relay2State = OFF;
                controlRelay(RELAY1, OFF); controlRelay(RELAY2, OFF);
                windowState = 0; controlWindow(0);
                autoManual = AUTO;
                EEPROM.write(201, AUTO); EEPROM.commit();
                triggerESPNow(0);
                My_LCD.clear(); delay(5);
                LCDPrint(0, 1, "ALARM STOPPED", 0);
                LCDPrint(1, 2, "by Hardware", 0);
                delay(2000);
                My_LCD.clear(); delay(5);
                printRelayState(); printMode(); printMQ2();
            break;
        }
    }

    // =========================================================================
    // BLYNK CALLBACKS & TASK
    // =========================================================================
    BLYNK_WRITE(V1) {
        int relayState = param.asInt();
        switch(relayState) {
            case 0: relay1State = 0; relay2State = 0; break;
            case 1: relay1State = 1; relay2State = 0; break;
            case 2: relay1State = 0; relay2State = 1; break;
            case 3: relay1State = 1; relay2State = 1; break;
        }
        controlRelay(RELAY1, relay1State);
        controlRelay(RELAY2, relay2State);
        printRelayState();
        autoManual = MANUAL;
        EEPROM.write(201, MANUAL); EEPROM.commit();
        printMode();
        Blynk.virtualWrite(V4, autoManual);
    }

    BLYNK_WRITE(V3) {
        mq2Thresshold = param.asInt();
        writeThresHoldEEPROM(mq2Thresshold);
        vTaskSuspend(TaskMainDisplay_handle);
        My_LCD.clear(); delay(100);
        LCDPrint(0, 0, "  Thresshold ", 0);
        char str[20];
        sprintf(str, "%d", mq2Thresshold);
        LCDPrint(1, 6, str, 0);
        delay(1000);
        My_LCD.clear(); delay(100);
        printRelayState(); printMQ2(); printMode();
        vTaskResume(TaskMainDisplay_handle);
    }

    BLYNK_WRITE(V4) {
        autoManual = param.asInt();
        EEPROM.write(201, autoManual); EEPROM.commit();
        printMode();
    }

    void myBlynkTimer() {
        if (blynkConnect) Blynk.virtualWrite(V0, readMQ2());
    }

    void TaskBlynk(void *pvParameters) {
        if (blynkConnect) {
            Blynk.virtualWrite(V0, readMQ2());
            Blynk.virtualWrite(V3, mq2Thresshold);
            Blynk.virtualWrite(V4, autoManual);
        }
        while (1) {
            if (blynkConnect) {
                Blynk.run();
                blynkTimer.run();
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }