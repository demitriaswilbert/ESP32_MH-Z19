#include "main.h"
#include "DHT.h"
#include "USB.h"
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <Arduino_JSON.h>
#include <Fonts/GFXFF/gfxfont.h>
#include <TFT_eSPI.h>
#include <ESPmDNS.h>
#include "driver/temp_sensor.h"

#define HOSTNAME "dwsensor"

// direct gpio port read
static inline __attribute__((always_inline)) bool directRead(int pin)
{
    if (pin < 32)
        return !!(GPIO.in & ((uint32_t)1 << pin));
    else
        return !!(GPIO.in1.val & ((uint32_t)1 << (pin - 32)));
}

// direct gpio port write low
static inline __attribute__((always_inline)) void directWriteLow(int pin)
{
    if (pin < 32)
        GPIO.out_w1tc = ((uint32_t)1 << pin);
    else if (pin < 54)
        GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
}

// direct gpio port write high
static inline __attribute__((always_inline)) void directWriteHigh(int pin)
{
    if (pin < 32)
        GPIO.out_w1ts = ((uint32_t)1 << pin);
    else if (pin < 54)
        GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
}

/**
 * @brief structure for pulse data queue
 *
 */
typedef struct
{
    int64_t hightime;
    int64_t lowtime;
} pulse_data_t;

/**
 * @brief structure for dht22 data queue
 *
 */
typedef struct
{
    float temp;
    float humid;
} dht22_data_t;

// lcd stuff
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite txt_spr(&tft);

// mh-z19 and dht22 data queue
QueueHandle_t pulse_data_q;
QueueHandle_t dht22_data_q;

QueueHandle_t websocket_report_queue = NULL;
QueueHandle_t wifi_switch_target = NULL;

std::vector<id_pass_t>id_passes = {
    {"Tapanuli 45 EXT", "4567379000"},
    {"Tapanuli 45", "4567379000"},
    {"DW iPhone", "4567379000"},
    {"MyRepublic SST", "4567379sst"},
};
int id_pass_idx = 0;


/**
 * @brief level change interrupt handler for measuring MH-Z19 pulse
 *
 * measures pulse width in us, then push to pulse data queue
 *
 */
static void mhz19_callback()
{

    // interrupt time stamp
    static int64_t rise_time = 0;
    static int64_t fall_time = 0;

    // immediately get interrupt timestamp
    int64_t time = esp_timer_get_time();
    static pulse_data_t pulse_data;

    if (directRead(4))
    { // if rising edge
        if (fall_time > 0)
        { // received sample
            // calculate on time and off time (duty cycle)
            int64_t hightime = fall_time - rise_time;
            int64_t lowtime = time - fall_time;

            pulse_data.hightime = hightime < 2000? 2000 : hightime;
            pulse_data.lowtime = lowtime < 2000? 2000 : lowtime;

            // pulse to queue
            xQueueSendFromISR(pulse_data_q, &pulse_data, NULL);
        }
        rise_time = time;
    }
    else
    { // get falling edge time
        fall_time = time;
    }
}

/**
 * @brief task to poll DHT22 data
 *
 * @param param unused
 */
void dht22_poll_task(void* param)
{

    // initialize and begin DHT object
    DHT dht(16, DHT22);
    dht.begin();

    dht22_data_t dht22_data;

    while (true)
    {
        // 2 sec delay for every measurement
        vTaskDelay(2000);

        // poll measurements and push to queue
        dht22_data.humid = dht.readHumidity();
        dht22_data.temp = dht.readTemperature();
        xQueueSend(dht22_data_q, &dht22_data, 0xfffffffful);
    }
    vTaskDelete(NULL);
}

#define RMT_DATA(D0, L0, D1, L1)                                               \
    ((uint32_t)D0 | (((uint32_t)L0) << 15UL) | (((uint32_t)D1) << 16UL) |      \
     (((uint32_t)L1) << 31UL))

#define BUILTIN_RGBLED_PIN 48
rmt_data_t led_data[24];
rmt_obj_t* rmt_send = NULL;

void rmt_write_to_led(uint8_t red, uint8_t green, uint8_t blue)
{
    int col, bit;
    int i = 0;

    int led_color[3] = {red, green, blue};
    static uint32_t rmt_1 = RMT_DATA(8, 1, 4, 0);
    static uint32_t rmt_0 = RMT_DATA(4, 1, 8, 0);

    for (col = 0; col < 3; col++)
    {
        for (bit = 0; bit < 8; bit++)
        {
            bool bit_state = !!(led_color[col] & (1 << (7 - bit)));
            led_data[i++].val = (bit_state) ? rmt_1 : rmt_0;
        }
    }

    rmtWrite(rmt_send, led_data, 24);
}

QueueHandle_t rgb_period_queue = NULL;
float rgb_period = 3000.0;
uint8_t base_val = 0;
uint8_t color[] = {base_val, base_val, base_val}; // RGB value
int col_index = 0;
int rgb_levels = 768;
bool rgb_led_on = false;

void rgb_task(void* param)
{
    float a = 0;
    int delay_by = 0;

    if (rgb_period_queue == NULL)
        rgb_period_queue = xQueueCreate(1, sizeof(float));

    if ((rmt_send = rmtInit(48, RMT_TX_MODE, RMT_MEM_64)) == NULL)
        Serial.println("init sender failed\n");

    float realTick = rmtSetTick(rmt_send, 100);
    Serial.printf("real tick set to: %fns\n", realTick);

    static uint8_t private_color[3]; // RGB value
    static float rgb_period_tmp;

    if (!rgb_led_on)
        rmt_write_to_led(0, 0, 0);

    while (true)
    {
        if (xQueueReceive(rgb_period_queue, &rgb_period_tmp, 0) == pdTRUE)
        {
            if (rgb_period_tmp < 50) {
                rgb_led_on = !rgb_led_on;
                if (!rgb_led_on)
                    rmt_write_to_led(0, 0, 0);
                websocket_queue_send("led", String(rgb_led_on? "ON" : "OFF"));

            } else {
                rgb_period = rgb_period_tmp;
                websocket_queue_send("period", String(rgb_period));
            }
        }
        
        if (rgb_led_on) {
            while (a < 100.f/6) {
                
                if (color[col_index] < 255)
                {
                    color[col_index]++;
                    if (color[(col_index + 2) % 3] > base_val)
                        color[(col_index + 2) % 3]--;
                }
                else
                    col_index = (col_index + 1) % 3;

                a += rgb_period / rgb_levels;
            }
            delay_by = a;
            a -= delay_by;
            rmt_write_to_led(color[1], color[0], color[2]);
        }
        vTaskDelay(rgb_led_on? delay_by : 100);
    }
    vTaskDelete(NULL);
}

static const char* wifi_err_reason_str(uint8_t errcode)
{
    switch (errcode)
    {
        case WIFI_REASON_UNSPECIFIED:
            return "WIFI_REASON_UNSPECIFIED";
        case WIFI_REASON_AUTH_EXPIRE:
            return "WIFI_REASON_AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE:
            return "WIFI_REASON_AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE:
            return "WIFI_REASON_ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY:
            return "WIFI_REASON_ASSOC_TOOMANY";
        case WIFI_REASON_NOT_AUTHED:
            return "WIFI_REASON_NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED:
            return "WIFI_REASON_NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE:
            return "WIFI_REASON_ASSOC_LEAVE";
        case WIFI_REASON_ASSOC_NOT_AUTHED:
            return "WIFI_REASON_ASSOC_NOT_AUTHED";
        case WIFI_REASON_DISASSOC_PWRCAP_BAD:
            return "WIFI_REASON_DISASSOC_PWRCAP_BAD";
        case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
            return "WIFI_REASON_DISASSOC_SUPCHAN_BAD";
        case WIFI_REASON_BSS_TRANSITION_DISASSOC:
            return "WIFI_REASON_BSS_TRANSITION_DISASSOC";
        case WIFI_REASON_IE_INVALID:
            return "WIFI_REASON_IE_INVALID";
        case WIFI_REASON_MIC_FAILURE:
            return "WIFI_REASON_MIC_FAILURE";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            return "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
            return "WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT";
        case WIFI_REASON_IE_IN_4WAY_DIFFERS:
            return "WIFI_REASON_IE_IN_4WAY_DIFFERS";
        case WIFI_REASON_GROUP_CIPHER_INVALID:
            return "WIFI_REASON_GROUP_CIPHER_INVALID";
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
            return "WIFI_REASON_PAIRWISE_CIPHER_INVALID";
        case WIFI_REASON_AKMP_INVALID:
            return "WIFI_REASON_AKMP_INVALID";
        case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
            return "WIFI_REASON_UNSUPP_RSN_IE_VERSION";
        case WIFI_REASON_INVALID_RSN_IE_CAP:
            return "WIFI_REASON_INVALID_RSN_IE_CAP";
        case WIFI_REASON_802_1X_AUTH_FAILED:
            return "WIFI_REASON_802_1X_AUTH_FAILED";
        case WIFI_REASON_CIPHER_SUITE_REJECTED:
            return "WIFI_REASON_CIPHER_SUITE_REJECTED";
        case WIFI_REASON_INVALID_PMKID:
            return "WIFI_REASON_INVALID_PMKID";
        case WIFI_REASON_BEACON_TIMEOUT:
            return "WIFI_REASON_BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND:
            return "WIFI_REASON_NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL:
            return "WIFI_REASON_AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL:
            return "WIFI_REASON_ASSOC_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "WIFI_REASON_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_CONNECTION_FAIL:
            return "WIFI_REASON_CONNECTION_FAIL";
        case WIFI_REASON_AP_TSF_RESET:
            return "WIFI_REASON_AP_TSF_RESET";
        case WIFI_REASON_ROAMING:
            return "WIFI_REASON_ROAMING";
        default:
            break;
    }
    return "UNKNOWN";
}
static const char* auth_mode_str(int authmode)
{
    switch (authmode)
    {
        case WIFI_AUTH_OPEN:
            return ("OPEN");
        case WIFI_AUTH_WEP:
            return ("WEP");
        case WIFI_AUTH_WPA_PSK:
            return ("WPA_PSK");
        case WIFI_AUTH_WPA2_PSK:
            return ("WPA2_PSK");
        case WIFI_AUTH_WPA_WPA2_PSK:
            return ("WPA_WPA2_PSK");
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return ("WPA2_ENTERPRISE");
        case WIFI_AUTH_WPA3_PSK:
            return ("WPA3_PSK");
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return ("WPA2_WPA3_PSK");
        case WIFI_AUTH_WAPI_PSK:
            return ("WPAPI_PSK");
        default:
            break;
    }
    return ("UNKNOWN");
}
static const char* wifi_event_id_to_str(arduino_event_id_t id)
{
    switch (id)
    {
        case ARDUINO_EVENT_WIFI_READY:
            return "ARDUINO_EVENT_WIFI_READY";
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            return "ARDUINO_EVENT_WIFI_SCAN_DONE";
        case ARDUINO_EVENT_WIFI_STA_START:
            return "ARDUINO_EVENT_WIFI_STA_START";
        case ARDUINO_EVENT_WIFI_STA_STOP:
            return "ARDUINO_EVENT_WIFI_STA_STOP";
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            return "ARDUINO_EVENT_WIFI_STA_CONNECTED";
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            return "ARDUINO_EVENT_WIFI_STA_DISCONNECTED";
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            return "ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE";
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            return "ARDUINO_EVENT_WIFI_STA_GOT_IP";
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            return "ARDUINO_EVENT_WIFI_STA_GOT_IP6";
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            return "ARDUINO_EVENT_WIFI_STA_LOST_IP";
        case ARDUINO_EVENT_WIFI_AP_START:
            return "ARDUINO_EVENT_WIFI_AP_START";
        case ARDUINO_EVENT_WIFI_AP_STOP:
            return "ARDUINO_EVENT_WIFI_AP_STOP";
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            return "ARDUINO_EVENT_WIFI_AP_STACONNECTED";
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            return "ARDUINO_EVENT_WIFI_AP_STADISCONNECTED";
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            return "ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED";
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
            return "ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED";
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            return "ARDUINO_EVENT_WIFI_AP_GOT_IP6";
        case ARDUINO_EVENT_WIFI_FTM_REPORT:
            return "ARDUINO_EVENT_WIFI_FTM_REPORT";
        case ARDUINO_EVENT_ETH_START:
            return "ARDUINO_EVENT_ETH_START";
        case ARDUINO_EVENT_ETH_STOP:
            return "ARDUINO_EVENT_ETH_STOP";
        case ARDUINO_EVENT_ETH_CONNECTED:
            return "ARDUINO_EVENT_ETH_CONNECTED";
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            return "ARDUINO_EVENT_ETH_DISCONNECTED";
        case ARDUINO_EVENT_ETH_GOT_IP:
            return "ARDUINO_EVENT_ETH_GOT_IP";
        case ARDUINO_EVENT_ETH_GOT_IP6:
            return "ARDUINO_EVENT_ETH_GOT_IP6";
        case ARDUINO_EVENT_WPS_ER_SUCCESS:
            return "ARDUINO_EVENT_WPS_ER_SUCCESS";
        case ARDUINO_EVENT_WPS_ER_FAILED:
            return "ARDUINO_EVENT_WPS_ER_FAILED";
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
            return "ARDUINO_EVENT_WPS_ER_TIMEOUT";
        case ARDUINO_EVENT_WPS_ER_PIN:
            return "ARDUINO_EVENT_WPS_ER_PIN";
        case ARDUINO_EVENT_WPS_ER_PBC_OVERLAP:
            return "ARDUINO_EVENT_WPS_ER_PBC_OVERLAP";
        case ARDUINO_EVENT_SC_SCAN_DONE:
            return "ARDUINO_EVENT_SC_SCAN_DONE";
        case ARDUINO_EVENT_SC_FOUND_CHANNEL:
            return "ARDUINO_EVENT_SC_FOUND_CHANNEL";
        case ARDUINO_EVENT_SC_GOT_SSID_PSWD:
            return "ARDUINO_EVENT_SC_GOT_SSID_PSWD";
        case ARDUINO_EVENT_SC_SEND_ACK_DONE:
            return "ARDUINO_EVENT_SC_SEND_ACK_DONE";
        case ARDUINO_EVENT_PROV_INIT:
            return "ARDUINO_EVENT_PROV_INIT";
        case ARDUINO_EVENT_PROV_DEINIT:
            return "ARDUINO_EVENT_PROV_DEINIT";
        case ARDUINO_EVENT_PROV_START:
            return "ARDUINO_EVENT_PROV_START";
        case ARDUINO_EVENT_PROV_END:
            return "ARDUINO_EVENT_PROV_END";
        case ARDUINO_EVENT_PROV_CRED_RECV:
            return "ARDUINO_EVENT_PROV_CRED_RECV";
        case ARDUINO_EVENT_PROV_CRED_FAIL:
            return "ARDUINO_EVENT_PROV_CRED_FAIL";
        case ARDUINO_EVENT_PROV_CRED_SUCCESS:
            return "ARDUINO_EVENT_PROV_CRED_SUCCESS";
        case ARDUINO_EVENT_MAX:
            return "ARDUINO_EVENT_MAX";
        default:
            break;
    }
    return "UNKNOWN";
};

void arduino_wifi_event_cb(arduino_event_t* event_data)
{
    Serial.printf("[WiFi]: %s\n", wifi_event_id_to_str(event_data->event_id));

    // if (tmp != IPAddress(192, 168, 2, 169))
    //     WiFi.config(IPAddress(192, 168, 2, 169), WiFi.gatewayIP(),
    //             WiFi.subnetMask());

    if (event_data->event_id == ARDUINO_EVENT_WIFI_STA_GOT_IP)
    { // STA received an IP
        ip_event_got_ip_t* event = &event_data->event_info.got_ip;

        IPAddress tmp = IPAddress(IP2STR(&event->ip_info.ip));

        Serial.printf("[WiFi]: STA Got %s IP: %s \n",
                      event->ip_changed ? "New" : "Same",
                      tmp.toString().c_str());

        print_wifi_config(Serial);

    }
    else if (event_data->event_id == ARDUINO_EVENT_WIFI_STA_LOST_IP)
    { // STA received an IP
        Serial.printf("[WiFi]: STA IP Lost\n");
    }
    else if (event_data->event_id == ARDUINO_EVENT_WIFI_STA_CONNECTED)
    { // STA connected
        wifi_event_sta_connected_t* event =
            &event_data->event_info.wifi_sta_connected;
        Serial.printf("[WiFi]: STA Connected: SSID: %s, BSSID: " MACSTR
                      ", Channel: %u, Auth: %s\n",
                      event->ssid, MAC2STR(event->bssid), event->channel,
                      auth_mode_str(event->authmode));
    }
    else if (event_data->event_id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t* event =
            &event_data->event_info.wifi_sta_disconnected;
        Serial.printf("[WiFi]: STA Disconnected: SSID: %s, BSSID: " MACSTR
                      ", Reason: %s\n",
                      event->ssid, MAC2STR(event->bssid),
                      wifi_err_reason_str(event->reason));
    }
}
/**
 * @brief WiFi monitor task
 *
 * @param param
 */
void wifi_monitor_task(void* param)
{
    WiFi.onEvent(arduino_wifi_event_cb);

    WiFi.begin(id_passes[id_pass_idx].ssid.c_str(),
               id_passes[id_pass_idx].pass.c_str());
    MDNS.begin(HOSTNAME);
    while (true)
    {
        static int64_t connect_timer = esp_timer_get_time();

        static bool prev_button = true;
        bool button = !!(GPIO.in & ((uint32_t)1 << 0));
        if (button && !prev_button)
        {
            id_pass_idx = (id_pass_idx + 1) % id_passes.size();
            xQueueSend(wifi_switch_target, &id_pass_idx, 0);
        }
        prev_button = button;

        if (WiFi.isConnected())
            connect_timer = esp_timer_get_time();
        else if (esp_timer_get_time() - connect_timer > 5000000)
        {
            connect_timer = esp_timer_get_time();
            Serial.printf("[WiFi monitor] Reconnecting\n");
            xQueueSend(wifi_switch_target, &id_pass_idx, 0);
        }

        if (xQueueReceive(wifi_switch_target, &id_pass_idx, 0) == pdTRUE)
        {
            WiFi.disconnect();
            id_pass_idx = id_pass_idx % id_passes.size();
            Serial.printf("[WiFi monitor] Trying to connect to %s\n",
                          id_passes[id_pass_idx].ssid.c_str());
            WiFi.begin(id_passes[id_pass_idx].ssid.c_str(),
                       id_passes[id_pass_idx].pass.c_str());
        }
        vTaskDelay(200);
    }
    vTaskDelete(NULL);
}

void print_line_to_tft(TFT_eSprite& tftsprite, String str, int line)
{
    tftsprite.fillSprite(TFT_BLACK);
    tftsprite.drawString(str, 10, 3);
    tftsprite.pushSprite(0, line * 25);
}

/**
 * @brief send json pair to queue
 * 
 * @param key 
 * @param value 
 * @return BaseType_t 
 */
BaseType_t websocket_queue_send(String key, String value) 
{
    json_pair_t* tmp_report_data = new json_pair_t(key, value);

    BaseType_t ret = xQueueSend(websocket_report_queue, &tmp_report_data, 100);
    if (ret != pdTRUE)
        delete tmp_report_data;
    
    return ret;
}

void setup()
{

    Serial.begin(115200);
    // Serial.setDebugOutput(true);

    temp_sensor_config_t tsens = TSENS_CONFIG_DEFAULT();
    temp_sensor_set_config(tsens);
    temp_sensor_start();

    // create mhz19 and dht22 data queue
    pulse_data_q = xQueueCreate(1, sizeof(pulse_data_t));
    dht22_data_q = xQueueCreate(1, sizeof(dht22_data_t));
    websocket_report_queue = xQueueCreate(100, sizeof(json_pair_t*));
    wifi_switch_target = xQueueCreate(1, sizeof(size_t));

    // configure MH-Z19 pins and level change interrupt
    pinMode(4, INPUT_PULLUP);
    attachInterrupt(4, mhz19_callback, CHANGE);

    pinMode(0, INPUT_PULLUP);
    pinMode(35, OUTPUT);

    // start DHT22 task
    xTaskCreate(rgb_task, "rgb_task", 4096, NULL, 1, NULL);
    xTaskCreate(dht22_poll_task, "dht22_tsk", 4096, NULL, 2, NULL);

    // wifi initialization

    // initialize tft lcd, fill with black
    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    // initialize text sprite, set font and text color
    txt_spr.createSprite(240, 25);
    txt_spr.setTextColor(TFT_WHITE);
    txt_spr.setFreeFont(&FreeMonoBold12pt7b);

    xTaskCreate(wifi_monitor_task, "wifi_task", 4096, NULL, 1,
                NULL);
    xTaskCreate(server_handle_task, "webserver_tsk", 4096, NULL, 2, NULL);
}



void loop()
{
    // static placeholder for xQueueReceive
    static pulse_data_t pulse_data;
    static dht22_data_t dht22_data;
    static int64_t wifi_display_timer = esp_timer_get_time() + 1000000;

    if (esp_timer_get_time() > wifi_display_timer)
    { 
        wifi_display_timer = esp_timer_get_time() + 1000000;
        int line = 0;
        String wifi_rssi = String(WiFi.RSSI());

        print_line_to_tft(txt_spr, id_passes[id_pass_idx].ssid, 0);
        print_line_to_tft(txt_spr, String(wifi_status(WiFi.status())) + " (" + wifi_rssi + ")", 1);
        print_line_to_tft(txt_spr, WiFi.localIP().toString(), 2);
        print_line_to_tft(txt_spr, HOSTNAME, 3);
        
        websocket_queue_send("rssi", wifi_rssi);
    }

    // display CO2 ppm
    if (xQueueReceive(pulse_data_q, &pulse_data, 0) == pdTRUE)
    {
        /** Calculate and Display CO2 ppm **/
        const uint64_t htime = pulse_data.hightime;
        const uint64_t total = pulse_data.lowtime + htime;
        // see MH-Z19B datasheet, ppm = 2000 * (h - 2ms) / (h + l - 4ms)
        double co2_ppm = 2000.0 * (htime - 2000) / (total - 4000);

        print_line_to_tft(txt_spr, "CO2 : " + String(co2_ppm), 4);

        websocket_queue_send("co2_ppm", String(co2_ppm));

        // Serial.printf("CO2 ppm: %lf\n", co2_ppm);
    }

    // display DHT22 data
    if (xQueueReceive(dht22_data_q, &dht22_data, 0) == pdTRUE)
    {
        float cpu_temp_c = NAN;
        uint32_t tmp_cycle_start, tmp_cycle_end;
        temp_sensor_read_celsius(&cpu_temp_c); 
        
        print_line_to_tft(txt_spr, "CPU : " + String(cpu_temp_c) + " C",
                          5);
        print_line_to_tft(txt_spr, "Temp: " + String(dht22_data.temp) + " C",
                          6);
        print_line_to_tft(txt_spr, "Hum : " + String(dht22_data.humid) + " %",
                          7);

        websocket_queue_send("temp", String(dht22_data.temp));
        websocket_queue_send("humid", String(dht22_data.humid));
        websocket_queue_send("cputemp", String(cpu_temp_c));

        // Serial.printf("Temp (C): %f, Humid (%%): %f\n", dht22_data.temp,
        // dht22_data.humid);
    }
    process_char_loop(Serial);
}