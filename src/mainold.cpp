#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h"          // disable brownout problems
#include "soc/rtc_cntl_reg.h" // disable brownout problems
#include "esp_http_server.h"
#include <LittleFS.h>

HardwareSerial mod(2);

#include "soilSensor.h"

// Replace with your network credentials
const char *ssid = "Sensor Tanah 12";
const char *password = ""; // Ensure this is intentional

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

static esp_err_t file_handler(httpd_req_t *req, const char *filepath, const char *mime_type)
{
    File file = LittleFS.open(filepath, "r");
    if (!file)
    {
        httpd_resp_send_404(req);
        Serial.printf("File %s not found\n", filepath);
        return ESP_FAIL;
    }

    size_t size = file.size();
    uint8_t *buffer = (uint8_t *)malloc(size);
    if (!buffer)
    {
        file.close();
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    file.read(buffer, size);
    file.close();

    httpd_resp_set_type(req, mime_type);
    esp_err_t res = httpd_resp_send(req, (const char *)buffer, size);
    free(buffer);

    return res;
}

#define DEFINE_URI_HANDLER(handler_name, uri_path, mime_type) \
    static esp_err_t handler_name(httpd_req_t *req)           \
    {                                                         \
        return file_handler(req, uri_path, mime_type);        \
    }

DEFINE_URI_HANDLER(aktivitas_handler, "/aktivitas.png", "image/png")
DEFINE_URI_HANDLER(brix_png_handler, "/brix.png", "image/png")
DEFINE_URI_HANDLER(cek_cuaca_png_handler, "/cek-cuaca.png", "image/png")
DEFINE_URI_HANDLER(cek_cuaca_handler, "/cuaca.html", "text/html")
DEFINE_URI_HANDLER(cek_tanah_handler, "/cek-tanah.html", "text/html")
DEFINE_URI_HANDLER(cek_tanah_png_handler, "/cek-tanah.png", "image/png")
DEFINE_URI_HANDLER(contoh_hama_png_handler, "/contoh-hama.png", "image/png")
DEFINE_URI_HANDLER(favicon_handler, "/favicon.ico", "image/x-icon")
DEFINE_URI_HANDLER(hama_handler, "/hama.png", "image/png")
DEFINE_URI_HANDLER(home_handler, "/home.html", "text/html")
DEFINE_URI_HANDLER(index_handler, "/index.html", "text/html")
DEFINE_URI_HANDLER(kemanisan_buah_handler, "/kemanisan-buah.html", "text/html")
DEFINE_URI_HANDLER(kemanisan_buah_png_handler, "/kemanisan-buah.png", "image/png")
DEFINE_URI_HANDLER(lapor_aktivitas_handler, "/lapor-aktivitas.html", "text/html")
DEFINE_URI_HANDLER(lapor_hama_handler, "/lapor-hama.html", "text/html")
DEFINE_URI_HANDLER(lapor_pemupukan_handler, "/lapor-pemupukan.html", "text/html")
DEFINE_URI_HANDLER(lapor_penyemprotan_handler, "/lapor-penyemprotan.html", "text/html")
DEFINE_URI_HANDLER(lapor_html_handler, "/lapor.html", "text/html")
DEFINE_URI_HANDLER(lapor_png_handler, "/lapor.png", "image/png")
DEFINE_URI_HANDLER(logo_handler, "/logo_dark.png", "image/png")
DEFINE_URI_HANDLER(mikroskop_handler, "/mikroskop.html", "text/html")
DEFINE_URI_HANDLER(mikroskop_png_handler, "/mikroskop.png", "image/png")
DEFINE_URI_HANDLER(peptisida_handler, "/pestisida.png", "image/png")
DEFINE_URI_HANDLER(pupuk_handler, "/pupuk.png", "image/png")
DEFINE_URI_HANDLER(script_handler, "/script.js", "text/javascript")
DEFINE_URI_HANDLER(tambah_foto_handler, "/tambah-foto.png", "image/png")
DEFINE_URI_HANDLER(arrow_back_png_handler, "/arrow-back.png", "image/png")
DEFINE_URI_HANDLER(berawan_png_handler, "/berawan.png", "image/png")
DEFINE_URI_HANDLER(hujan_ringan_png_handler, "/hujan-ringan.png", "image/png")
DEFINE_URI_HANDLER(hujan_sedang_png_handler, "/hujan-sedang.png", "image/png")

#define DEFINE_DATA_HANDLER(handler_name, push_function)                      \
    static esp_err_t handler_name(httpd_req_t *req)                           \
    {                                                                         \
        httpd_resp_send(req, push_function().c_str(), HTTPD_RESP_USE_STRLEN); \
        return ESP_OK;                                                        \
    }

DEFINE_DATA_HANDLER(dsoiltemp_handler, pushsoiltemp)
DEFINE_DATA_HANDLER(dnitrogen_handler, pushnitrogen)
DEFINE_DATA_HANDLER(dphosphorous_handler, pushphosphorous)
DEFINE_DATA_HANDLER(dpotassium_handler, pushpotassium)
DEFINE_DATA_HANDLER(dph_handler, pushph)
DEFINE_DATA_HANDLER(dvwc_handler, pushvwc)
DEFINE_DATA_HANDLER(dec_handler, pushec)

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
    config.max_uri_handlers = 43;

    httpd_uri_t uris[] = {
        {"/", HTTP_GET, index_handler, NULL},
        {"/script.js", HTTP_GET, script_handler, NULL},
        {"/capture", HTTP_GET, capture_handler, NULL},
        {"/stream", HTTP_GET, stream_handler, NULL},
        {"/start_stream", HTTP_GET, start_stream_handler, NULL},
        {"/stop_stream", HTTP_GET, stop_stream_handler, NULL},
        {"/soiltemp", HTTP_GET, dsoiltemp_handler, NULL},
        {"/nitrogen", HTTP_GET, dnitrogen_handler, NULL},
        {"/phosphorous", HTTP_GET, dphosphorous_handler, NULL},
        {"/potassium", HTTP_GET, dpotassium_handler, NULL},
        {"/ph", HTTP_GET, dph_handler, NULL},
        {"/vwc", HTTP_GET, dvwc_handler, NULL},
        {"/ec", HTTP_GET, dec_handler, NULL},
        {"/logo_dark.png", HTTP_GET, logo_handler, NULL},
        {"/home.html", HTTP_GET, home_handler, NULL},
        {"/cek-tanah.html", HTTP_GET, cek_tanah_handler, NULL},
        {"/cek-tanah.png", HTTP_GET, cek_tanah_png_handler, NULL},
        {"/cek-cuaca.png", HTTP_GET, cek_cuaca_png_handler, NULL},
        {"/kemanisan-buah.png", HTTP_GET, kemanisan_buah_png_handler, NULL},
        {"/lapor.png", HTTP_GET, lapor_png_handler, NULL},
        {"/mikroskop.png", HTTP_GET, mikroskop_png_handler, NULL},
        {"/aktivitas.png", HTTP_GET, aktivitas_handler, NULL},
        {"/hama.png", HTTP_GET, hama_handler, NULL},
        {"/pestisida.png", HTTP_GET, peptisida_handler, NULL},
        {"/pupuk.png", HTTP_GET, pupuk_handler, NULL},
        {"/tambah-foto.png", HTTP_GET, tambah_foto_handler, NULL},
        {"/lapor-aktivitas.html", HTTP_GET, lapor_aktivitas_handler, NULL},
        {"/lapor-hama.html", HTTP_GET, lapor_hama_handler, NULL},
        {"/lapor-pemupukan.html", HTTP_GET, lapor_pemupukan_handler, NULL},
        {"/lapor-penyemprotan.html", HTTP_GET, lapor_penyemprotan_handler, NULL},
        {"/lapor.html", HTTP_GET, lapor_html_handler, NULL},
        {"/brix.png", HTTP_GET, brix_png_handler, NULL},
        {"/contoh-hama.png", HTTP_GET, contoh_hama_png_handler, NULL},
        {"/favicon.ico", HTTP_GET, favicon_handler, NULL},
        {"/kemanisan-buah.html", HTTP_GET, kemanisan_buah_handler, NULL},
        {"/mikroskop.html", HTTP_GET, mikroskop_handler, NULL},
        {"/cuaca.html", HTTP_GET, cek_cuaca_handler, NULL},
        {"/arrow-back.png", HTTP_GET, arrow_back_png_handler, NULL},
        {"/berawan.png", HTTP_GET, berawan_png_handler, NULL},
        {"/hujan-ringan.png", HTTP_GET, hujan_ringan_png_handler, NULL},
        {"/hujan-sedang.png", HTTP_GET, hujan_sedang_png_handler, NULL},
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

    if (!LittleFS.begin())
    {
        Serial.println("An Error has occurred while mounting LittleFS");
    }
    else
    {
        Serial.println("LittleFS mounted successfully");
    }

    Serial.print("Setting AP (Access Point)…");
    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    startCameraServer();
}

void loop()
{
    Nitrogen = getNitrogen();
    Serial.println(Nitrogen);
    delay(250);

    Phosphorous = getPhosphorous();
    Serial.println(Phosphorous);
    delay(250);

    Potassium = getPotassium();
    Serial.println(Potassium);
    delay(250);

    pH = float(getPH()) / 100;
    Serial.println(pH);
    delay(250);

    soiltemp = float(getSoilTemp()) / 10;
    Serial.println(soiltemp);
    delay(250);

    VWC = float(getVWC()) / 10;
    Serial.println(VWC);
    delay(250);

    EC = getEC();
    Serial.println(EC);
    delay(250);

    delay(1000);
}
