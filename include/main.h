#ifndef MAIN_H
#define MAIN_H

// main arduino library
#include <Arduino.h>

// Async WebServer
#include <Arduino_JSON.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// WiFi functionality
#include <WiFi.h>

// USB and Serial communication
#include <USB.h>
#include <USBCDC.h>

// for OTA update
#include <ArduinoOTA.h>

// DNS responder
#include <ESPmDNS.h>

// LED driver
#include <driver/ledc.h>

// SNTP
#include <esp_sntp.h>

#include <math.h>
#include <time.h>

class json_pair_t
{
public:
    String key;
    String value;
    json_pair_t(String key, String value) : key(key), value(value) {}
};

extern QueueHandle_t websocket_report_queue;
BaseType_t websocket_queue_send(String key, String value);

void server_handle_task(void* param);



extern QueueHandle_t wifi_switch_target;

typedef struct
{
    String ssid;
    String pass;
} id_pass_t;

const char* wifi_status(wl_status_t status);
void print_wifi_config(Stream& st);


extern QueueHandle_t rgb_period_queue;
extern float rgb_period;

extern uint8_t base_val;
extern int rgb_levels;
extern uint8_t color[];
extern int col_index;
extern bool rgb_led_on;

void process_char(Stream& st, char c);
void process_string(Stream& st, const char* buf);
void process_char_loop(Stream& st);

#endif // MAIN_H