#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h"          // disable brownout problems
#include "soc/rtc_cntl_reg.h" // disable brownout problems
#include "esp_http_server.h"
// #include <LittleFS.h>
#include "EmbeddedMqttBroker.h"
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

HardwareSerial mod(2);

#include "soilSensor.h"

const char *ssid = "Sensor Tanah 20";
const char *password = "";

using namespace mqttBrokerName;
uint16_t mqttPort = 1883;
MqttBroker broker(mqttPort);

int Nitrogen = 0, Phosphorous = 0, Potassium = 0, EC = 0;
float pH = 0, soiltemp = 0, VWC = 0;
int OldNitrogen = 0, OldPhosphorous = 0, OldPotassium = 0, OldEC = 0;
float OldpH = 0, Oldsoiltemp = 0, OldVWC = 0;

typedef struct
{
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

String pushsoiltemp() { return String(soiltemp); }
String pushnitrogen() { return String(Nitrogen); }
String pushphosphorous() { return String(Phosphorous); }
String pushpotassium() { return String(Potassium); }
String pushph() { return String(pH); }
String pushvwc() { return String(VWC); }
String pushec() { return String(EC); }

// static esp_err_t file_handler(httpd_req_t *req, const char *filepath, const char *mime_type)
// {
//     File file = LittleFS.open(filepath, "r");
//     if (!file)
//     {
//         httpd_resp_send_404(req);
//         Serial.printf("File %s not found\n", filepath);
//         return ESP_FAIL;
//     }

//     size_t size = file.size();
//     uint8_t *buffer = (uint8_t *)malloc(size);
//     if (!buffer)
//     {
//         file.close();
//         httpd_resp_send_500(req);
//         return ESP_FAIL;
//     }

//     file.read(buffer, size);
//     file.close();

//     httpd_resp_set_type(req, mime_type);
//     esp_err_t res = httpd_resp_send(req, (const char *)buffer, size);
//     free(buffer);

//     return res;
// }

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!index)
    {
        j->len = 0;
    }
    if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
    {
        return 0;
    }
    j->len += len;
    return len;
}

bool isStreaming = false;

static esp_err_t start_stream_handler(httpd_req_t *req)
{
    isStreaming = true;
    Serial.println("Streaming started");
    httpd_resp_sendstr(req, "Streaming started");
    return ESP_OK;
}

static esp_err_t stop_stream_handler(httpd_req_t *req)
{
    isStreaming = false;
    Serial.println("Streaming stopped");
    httpd_resp_sendstr(req, "Streaming stopped");
    return ESP_OK;
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    if (!isStreaming)
    {
        Serial.println("Streaming is currently disabled");
        httpd_resp_send_404(req); // Send a 404 response if streaming is disabled
        return ESP_FAIL;
    }
    Serial.println("Stream handler called");
    camera_fb_t *fb = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[128];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }

    while (true)
    {
        fb = esp_camera_fb_get();
        if (!fb)
        {
            res = ESP_FAIL;
        }
        else
        {
            if (fb->width > 400)
            {
                if (fb->format != PIXFORMAT_JPEG)
                {
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if (!jpeg_converted)
                    {
                        res = ESP_FAIL;
                    }
                }
                else
                {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
            }
        }

        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK)
        {
            size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }

        if (fb)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if (_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        if (res != ESP_OK)
        {
            break;
        }
    }

    return res;
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;

    printf("Capturing image...\n");
    fb = esp_camera_fb_get();
    if (!fb)
    {
        printf("Failed to capture image\n");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    printf("Image captured successfully\n");

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char ts[32];
    snprintf(ts, 32, "%ld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
    httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

    if (fb->format == PIXFORMAT_JPEG)
    {
        printf("Sending JPEG image...\n");
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    }
    else
    {
        printf("Encoding image to JPEG...\n");
        jpg_chunking_t jchunk = {req, 0};
        res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
        httpd_resp_send_chunk(req, NULL, 0);
    }

    esp_camera_fb_return(fb);
    printf("Image sent successfully\n");
    return res;
}

void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 5;

    httpd_uri_t uris[] = {
        {"/capture", HTTP_GET, capture_handler, NULL},
        {"/stream", HTTP_GET, stream_handler, NULL},
        {"/start_stream", HTTP_GET, start_stream_handler, NULL},
        {"/stop_stream", HTTP_GET, stop_stream_handler, NULL},
    };

    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        for (auto &uri : uris)
        {
            httpd_register_uri_handler(camera_httpd, &uri);
        }
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    if (httpd_start(&stream_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(stream_httpd, &uris[3]); // Stream URI
    }
}

void setup()
{
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector

    Serial.begin(115200);
    mod.begin(9600, SERIAL_8N1, 13, 14);

    delay(3000);

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound())
    {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    }
    else
    {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("Camera init failed with error 0x%x", err);
    }

    // if (!LittleFS.begin())
    // {
    //     Serial.println("An Error has occurred while mounting LittleFS");
    // }
    // else
    // {
    //     Serial.println("LittleFS mounted successfully");
    // }

    Serial.print("Setting AP (Access Point)…");
    WiFi.softAP(ssid, password);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Arduino OTA setup
    ArduinoOTA.onStart([]()
                       {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    Serial.println("Start updating " + type); });
    ArduinoOTA.onEnd([]()
                     { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
    ArduinoOTA.onError([](ota_error_t error)
                       {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
    ArduinoOTA.begin();

    Serial.println("OTA Ready");

    broker.setMaxNumClients(3); // set according to your system.
    broker.startBroker();
    Serial.println("broker started");

    startCameraServer();
}

void loop()
{
    ArduinoOTA.handle();
    static unsigned long lastPublishTime = 0;
    unsigned long currentTime = millis();

    if (currentTime - lastPublishTime >= 1000)
    {
        lastPublishTime = currentTime;
        int Nitrogen = getNitrogen();
        int Phosphorous = getPhosphorous();
        int Potassium = getPotassium();
        float pH = float(getPH()) / 100;
        float soiltemp = float(getSoilTemp()) / 10;
        float VWC = float(getVWC()) / 10;
        int EC = getEC();

        // Create JSON object
        StaticJsonDocument<200> doc;
        doc["Nitrogen"] = Nitrogen;
        doc["Phosphorous"] = Phosphorous;
        doc["Potassium"] = Potassium;
        doc["pH"] = pH;
        doc["soiltemp"] = soiltemp;
        doc["VWC"] = VWC;
        doc["EC"] = EC;

        char jsonBuffer[256];
        serializeJson(doc, jsonBuffer);

        // Create and configure PublishMqttMessage object
        PublishMqttMessage publishMqttMessage;
        publishMqttMessage.setTopic("sensor/data");
        publishMqttMessage.setPayLoad(jsonBuffer);
        publishMqttMessage.setQos(0);       // Quality of Service level
        publishMqttMessage.setMessageId(1); // Message ID

        // Publish the message
        broker.publishMessage(&publishMqttMessage);
        Serial.print("Published message: ");
        Serial.println(jsonBuffer);
    }
}