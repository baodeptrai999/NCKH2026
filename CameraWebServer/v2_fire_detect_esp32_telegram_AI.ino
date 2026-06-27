/* Edge Impulse Arduino examples
 * Copyright (c) 2022 EdgeImpulse Inc.
 * (Giu nguyen license goc, ben duoi la phan minh THEM: WiFi + gui Telegram)
 *
 * ================================================================
 * BAN GHEP: file goc Edge Impulse (camera + inference, KHONG SUA)
 *           + WiFi va gui canh bao qua Telegram khi phat hien "co_lua"
 * ================================================================
 *
 * CAN DIEN TRUOC KHI NAP CODE (tim chu "THAY_DOI" o duoi):
 *   - WIFI_SSID, WIFI_PASS
 *   - TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID
 *   - (tuy chon) FIRE_THRESHOLD, ALERT_COOLDOWN_MS
 */

// These sketches are tested with 2.0.4 ESP32 Arduino Core
// https://github.com/espressif/arduino-esp32/releases/tag/2.0.4

/* Includes ---------------------------------------------------------------- */
#include <baonguyengia-project-1_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ====================== THAY_DOI: WIFI & TELEGRAM ======================
const char* WIFI_SSID          = "Bao Dep Trai";
const char* WIFI_PASS          = "44888888";
const char* TELEGRAM_BOT_TOKEN = "8684444816:AAFT3wo2YgtssDNvu7xj6yHXFcfTgfQwCuE";   // lay tu @BotFather
const char* TELEGRAM_CHAT_ID   = "8281428035";     // lay tu getUpdates
// ========================================================================

// Nhan (label) cho lop "co lua" - phai khop CHINH XAC voi ten ban dat tren Edge Impulse
#define FIRE_LABEL "fire"

// Nguong tin cay de coi la phat hien lua (0.0 - 1.0). Tang len neu bi bao gia (false positive).
const float FIRE_THRESHOLD = 0.8;

// Thoi gian (ms) giua 2 lan gui canh bao lien tiep, tranh spam Telegram
const unsigned long ALERT_COOLDOWN_MS = 60000; // 60 giay
unsigned long lastAlertTime = 0;

// Select camera model - find more camera models in camera_pins.h file here
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/camera_pins.h

//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#if defined(CAMERA_MODEL_ESP_EYE)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23

#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM   5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

#elif defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#else
#error "Camera model not selected"
#endif

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static bool is_initialised = false;
uint8_t *snapshot_buf; //points to the output of the capture

static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

/* Function definitions ------------------------------------------------------- */
bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) ;

// ===== THAY_DOI: cac ham minh them cho WiFi + Telegram =====
void connectWiFi();
bool sendPhotoToTelegram();

/**
* @brief      Arduino setup function
*/
void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    //comment out the below line to start inference immediately after upload
    while (!Serial);
    Serial.println("Edge Impulse Inferencing Demo");
    if (ei_camera_init() == false) {
        ei_printf("Failed to initialize Camera!\r\n");
    }
    else {
        ei_printf("Camera initialized\r\n");
    }

    // ===== THAY_DOI: ket noi WiFi truoc khi bat dau =====
    connectWiFi();

    ei_printf("\nStarting continious inference in 2 seconds...\n");
    ei_sleep(2000);
}

/**
* @brief      Get data and run inferencing
*
* @param[in]  debug  Get debug info if true
*/
void loop()
{

    // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
    if (ei_sleep(5) != EI_IMPULSE_OK) {
        return;
    }

    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);

    // check if allocation was successful
    if(snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        free(snapshot_buf);
        return;
    }

    // print the predictions
    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    ei_printf("Object detection bounding boxes:\r\n");
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) {
            continue;
        }
        ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                bb.label,
                bb.value,
                bb.x,
                bb.y,
                bb.width,
                bb.height);

        // ===== THAY_DOI: kiem tra neu la nhan lua va du tin cay =====
        if (strcmp(bb.label, FIRE_LABEL) == 0 && bb.value > FIRE_THRESHOLD) {
            handleFireDetected(bb.value);
        }
    }

    // Print the prediction results (classification)
#else
    ei_printf("Predictions:\r\n");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        ei_printf("  %s: ", ei_classifier_inferencing_categories[i]);
        ei_printf("%.5f\r\n", result.classification[i].value);

        // ===== THAY_DOI: kiem tra neu la nhan lua va du tin cay =====
        if (strcmp(ei_classifier_inferencing_categories[i], FIRE_LABEL) == 0
            && result.classification[i].value > FIRE_THRESHOLD) {
            handleFireDetected(result.classification[i].value);
        }
    }
#endif

    // Print anomaly result (if it exists)
#if EI_CLASSIFIER_HAS_ANOMALY
    ei_printf("Anomaly prediction: %.3f\r\n", result.anomaly);
#endif

#if EI_CLASSIFIER_HAS_VISUAL_ANOMALY
    ei_printf("Visual anomalies:\r\n");
    for (uint32_t i = 0; i < result.visual_ad_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.visual_ad_grid_cells[i];
        if (bb.value == 0) {
            continue;
        }
        ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                bb.label,
                bb.value,
                bb.x,
                bb.y,
                bb.width,
                bb.height);
    }
#endif


    free(snapshot_buf);

}

// ===========================================================================
// ===== THAY_DOI: TU DAY LA PHAN MINH THEM - WIFI + GUI TELEGRAM =====
// ===========================================================================

/**
 * @brief  Ket noi WiFi, in IP ra Serial khi xong
 */
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Dang ket noi WiFi");
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nDa ket noi WiFi, IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nKHONG ket noi duoc WiFi! Se thu lai trong loop.");
    }
}

/**
 * @brief  Goi khi phat hien lua voi do tin cay du cao.
 *         Co kiem tra cooldown de tranh spam Telegram.
 */
void handleFireDetected(float confidence) {
    ei_printf(">>> PHAT HIEN LUA! Do tin cay: %.2f\r\n", confidence);

    unsigned long now = millis();
    if (now - lastAlertTime < ALERT_COOLDOWN_MS) {
        ei_printf("(Da gui canh bao gan day, bo qua de tranh spam)\r\n");
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        ei_printf("WiFi mat ket noi, dang thu ket noi lai...\r\n");
        connectWiFi();
    }

    if (WiFi.status() == WL_CONNECTED) {
        bool ok = sendPhotoToTelegram();
        ei_printf(ok ? "Da gui anh canh bao qua Telegram\r\n" : "Gui Telegram THAT BAI\r\n");
        lastAlertTime = now;
    }
}

/**
 * @brief  Chup 1 anh JPEG MOI (chat luong cao hon, rieng cho gui Telegram,
 *         khac voi snapshot_buf nho dung cho inference) va gui qua Telegram
 *         bang API sendPhoto (multipart/form-data).
 */
bool sendPhotoToTelegram() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ei_printf("Chup anh de gui Telegram THAT BAI\r\n");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // bo qua kiem tra SSL cert cho don gian

    if (!client.connect("api.telegram.org", 443)) {
        ei_printf("Khong ket noi duoc Telegram API\r\n");
        esp_camera_fb_return(fb);
        return false;
    }

    String boundary = "----ESP32CAMBoundary";
    String head = "--" + boundary + "\r\n"
                  "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
                  String(TELEGRAM_CHAT_ID) + "\r\n" +
                  "--" + boundary + "\r\n"
                  "Content-Disposition: form-data; name=\"caption\"\r\n\r\n"
                  "CANH BAO: Phat hien lua!\r\n" +
                  "--" + boundary + "\r\n"
                  "Content-Disposition: form-data; name=\"photo\"; filename=\"fire.jpg\"\r\n"
                  "Content-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";

    uint32_t totalLen = head.length() + fb->len + tail.length();

    client.println("POST /bot" + String(TELEGRAM_BOT_TOKEN) + "/sendPhoto HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println("Content-Length: " + String(totalLen));
    client.println();
    client.print(head);

    // Gui du lieu anh theo tung khoi nho de tranh tran bo nho
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n += 1024) {
        size_t chunkSize = (fbLen - n > 1024) ? 1024 : (fbLen - n);
        client.write(fbBuf + n, chunkSize);
    }

    client.print(tail);

    // Doc phan hoi (khong bat buoc nhung giup debug)
    long startTime = millis();
    while (client.connected() && millis() - startTime < 5000) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") break;
        }
    }
    client.stop();
    esp_camera_fb_return(fb);
    return true;
}

// ===========================================================================
// ===== HET PHAN MINH THEM, TU DAY TRO XUONG LA FILE GOC EDGE IMPULSE =====
// ===========================================================================

/**
 * @brief   Setup image sensor & start streaming
 *
 * @retval  false if initialisation failed
 */
bool ei_camera_init(void) {

    if (is_initialised) return true;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x\n", err);
      return false;
    }

    sensor_t * s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1); // flip it back
      s->set_brightness(s, 1); // up the brightness just a bit
      s->set_saturation(s, 0); // lower the saturation
    }

#if defined(CAMERA_MODEL_M5STACK_WIDE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#elif defined(CAMERA_MODEL_ESP_EYE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
    s->set_awb_gain(s, 1);
#endif

    is_initialised = true;
    return true;
}

/**
 * @brief      Stop streaming of sensor data
 */
void ei_camera_deinit(void) {

    //deinitialize the camera
    esp_err_t err = esp_camera_deinit();

    if (err != ESP_OK)
    {
        ei_printf("Camera deinit failed\n");
        return;
    }

    is_initialised = false;
    return;
}


/**
 * @brief      Capture, rescale and crop image
 *
 * @param[in]  img_width     width of output image
 * @param[in]  img_height    height of output image
 * @param[in]  out_buf       pointer to store output image, NULL may be used
 *                           if ei_camera_frame_buffer is to be used for capture and resize/cropping.
 *
 * @retval     false if not initialised, image captured, rescaled or cropped failed
 *
 */
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;

    if (!is_initialised) {
        ei_printf("ERR: Camera is not initialized\r\n");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        ei_printf("Camera capture failed\n");
        return false;
    }

   bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);

   esp_camera_fb_return(fb);

   if(!converted){
       ei_printf("Conversion failed\n");
       return false;
   }

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS)
        || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        do_resize = true;
    }

    if (do_resize) {
        ei::image::processing::crop_and_interpolate_rgb888(
        out_buf,
        EI_CAMERA_RAW_FRAME_BUFFER_COLS,
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
        out_buf,
        img_width,
        img_height);
    }


    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        // Swap BGR to RGB here
        // due to https://github.com/espressif/esp32-camera/issues/379
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix+=3;
        pixels_left--;
    }
    // and done!
    return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif
