#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <TFT_eSPI.h>
#include <Fonts/GFXFF/gfxfont.h>
#include "DHT.h"

// direct gpio port read
static inline __attribute__((always_inline))
bool directRead(int pin) {
    if ( pin < 32 )
        return !!(GPIO.in & ((uint32_t)1 << pin));
    else 
        return !!(GPIO.in1.val & ((uint32_t)1 << (pin - 32)));
}


// direct gpio port write low
static inline __attribute__((always_inline))
void directWriteLow(int pin) {
    if ( pin < 32 )
        GPIO.out_w1tc = ((uint32_t)1 << pin);
    else if (pin < 54)
        GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
}

// direct gpio port write high
static inline __attribute__((always_inline))
void directWriteHigh(int pin) {
    if ( pin < 32 )
        GPIO.out_w1ts = ((uint32_t)1 << pin);
    else if (pin < 54)
        GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
}

/**
 * @brief structure for pulse data queue
 * 
 */
typedef struct {
    int64_t hightime;
    int64_t lowtime;
} pulse_data_t;

/**
 * @brief structure for dht22 data queue
 * 
 */
typedef struct {
    float temp;
    float humid;
} dht22_data_t;


// lcd stuff
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite txt_spr(&tft);

// pin change interrupt time stamp
static int64_t risetime = 0;
static int64_t falltime = 0;

// mh-z19 and dht22 data queue
static QueueHandle_t pulse_data_q;
static QueueHandle_t dht22_data_q;

/**
 * @brief level change interrupt handler for measuring MH-Z19 pulse
 *
 * measures pulse width in us, then push to pulse data queue
 *
 */
static void mhz19_callback() {

    // immediately get interrupt timestamp
    int64_t time = esp_timer_get_time();
    static pulse_data_t pulse_data;

    if (directRead(4)) {    // if rising edge
        if (falltime > 0) { // valid sample
            // calculate on time and off time (duty cycle)
            pulse_data.hightime = falltime - risetime;
            pulse_data.lowtime = time - falltime;
            // pulse to queue
            xQueueSendFromISR(pulse_data_q, &pulse_data, NULL);
        }
        risetime = time;
    } else { // if falling edge
        falltime = time;
    }
}

/**
 * @brief task to poll DHT22 data
 * 
 * @param param unused
 */
void dht22_poll_task(void* param) {

    // initialize and begin DHT object
    DHT dht(16, DHT22);
    dht.begin();
    static dht22_data_t dht22_data;

    while (true) {
        // 2 sec delay for every measurement
        vTaskDelay(2000);

        // poll measurements and push to queue
        dht22_data.humid = dht.readHumidity();
        dht22_data.temp = dht.readTemperature();
        xQueueSend(dht22_data_q, &dht22_data, 0xfffffffful);
    }
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);

    // create mhz19 and dht22 data queue
    pulse_data_q = xQueueCreate(1, sizeof(pulse_data_t));
    dht22_data_q = xQueueCreate(1, sizeof(dht22_data_t));
    
    // configure MH-Z19 pins and level change interrupt
    pinMode(4, INPUT);
    attachInterrupt(4, mhz19_callback, CHANGE);

    // start DHT22 task
    xTaskCreate(dht22_poll_task, "dht22_tsk", 4096, NULL, 1, NULL);

    // initialize tft lcd, fill with black
    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    // initialize text sprite, set font and text color
    txt_spr.createSprite(240, 70);
    txt_spr.setTextColor(TFT_WHITE);
    txt_spr.setFreeFont(&FreeMonoBold18pt7b);
}

void loop() {

    // static placeholder for xQueueReceive
    static pulse_data_t pulse_data;
    static dht22_data_t dht22_data;

    // display CO2 ppm
    if (xQueueReceive(pulse_data_q, &pulse_data, 0) == pdTRUE) {

        /** Calculate and Display CO2 ppm **/
        const uint64_t htime = pulse_data.hightime;
        const uint64_t total = pulse_data.lowtime + htime;
        // see MH-Z19B datasheet, ppm = 2000 * (h - 2ms) / (h + l - 4ms)
        double co2_ppm = 2000.0 * (htime - 2000) / (total - 4000);

        txt_spr.fillSprite(TFT_BLACK);
        txt_spr.drawString("CO2 (ppm):", 10, 0);
        txt_spr.drawString(String(co2_ppm), 10, 35);
        txt_spr.pushSprite(0, 0);

        Serial.printf("CO2 ppm: %lf\n", co2_ppm);
    }

    // display DHT22 data
    if (xQueueReceive(dht22_data_q, &dht22_data, 0) == pdTRUE) {

        txt_spr.fillSprite(TFT_BLACK);
        txt_spr.drawString("Temp (C):", 10, 0);
        txt_spr.drawString(String(dht22_data.temp), 10, 35);
        txt_spr.pushSprite(0, 70);

        txt_spr.fillSprite(TFT_BLACK);
        txt_spr.drawString("Humid (%):", 10, 0);
        txt_spr.drawString(String(dht22_data.humid), 10, 35);
        txt_spr.pushSprite(0, 140);
        
        Serial.printf("Temp (C): %f, Humid (%%): %f\n", dht22_data.temp, dht22_data.humid);
    }
}