/*
*   FreeRTOS-Clock
* ===================
*  Uwe Berger; 20216
*
* Eine einfache Uhr, welche:
*   * sich ueber einen NTP-Server synchronisiert (Verwendung von 
*     Arduino-ESP32-SNTP etc.)
*   * als Anzeige ein OLED verwendet, um:
*       * Datum/Uhrzeit
*       * Status der Netzwerkverbindung
*       * Zeitpunkt des letzen NTP-Sync
*       * Anzeige des "Smooth"-Sync-Status
*     zu visualisieren.
*
* Als Hardware wurde verwendet:
*   * ESP32-P4-ETH
*   * SSD1306-OLED
*
* Die Firmware verwendet extensiv FreeRTOS-Mechanismen:
*   * Tasks
*   * Mutexe
*   * Queues
* um die diversen asynchronen Ereignisse entsprechend auf dem
* Display auszugeben.
*
* ---------
* Have fun!
*
*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <ETH.h>
#include "esp_sntp.h"

// lokale Zeitzone
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03" 

// OLED-Dimension
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// OLED-Display-Object...
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// FreeRTOS Mutexe...
SemaphoreHandle_t mutex_oled = NULL;

// FreeRTOS Oueues
QueueHandle_t queue_eth2oled;
QueueHandle_t queue_lastsync2oled;

#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   31
#define ETH_PHY_MDIO  52
#define ETH_PHY_POWER 51
#define ETH_CLK_MODE  EMAC_CLK_EXT_IN

#define HOSTNAME      "esp32-p4-clock"

bool eth_connected = false;

// ********************************************************************************
// ETH-Events
void onEvent(arduino_event_id_t event) {
    char q_msg[32];
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            snprintf(q_msg, sizeof(q_msg), "%s", "ETH Started");
            Serial.println(q_msg);
            ETH.setHostname(HOSTNAME);
            break;
        case ARDUINO_EVENT_ETH_CONNECTED: 
            snprintf(q_msg, sizeof(q_msg), "%s", "ETH Connected");
            Serial.println(q_msg);
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            snprintf(q_msg, sizeof(q_msg), "%s", "ETH Got IP");
            Serial.println(q_msg);
            Serial.println(ETH);
            eth_connected = true;
            break;
        case ARDUINO_EVENT_ETH_LOST_IP:
            snprintf(q_msg, sizeof(q_msg), "%s", "ETH Lost IP");
            Serial.println(q_msg);
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            snprintf(q_msg, sizeof(q_msg), "%s", "ETH Disconnected");
            Serial.println(q_msg);
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_STOP:
            snprintf(q_msg, sizeof(q_msg), "%s", "ETH Stopped");
            Serial.println(q_msg);
            eth_connected = false;
            break;
        default: 
            break;
    }
    // Statustext an entspr. OLED-Task senden
    xQueueSend(queue_eth2oled, q_msg, portMAX_DELAY);
}

// ********************************************************************************
// Callback bei erfolgreichem NTP-Sync
void time_sync_cb(struct timeval *tv)
{
    char buf[32];
    struct tm sync_tm;
    localtime_r(&tv->tv_sec, &sync_tm);
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &sync_tm);
    Serial.println(buf);
    // letzten NTP-Sync-Zeitpunkt an entspr. OLED-Task senden
    xQueueSend(queue_lastsync2oled, buf, 0);
}

// ********************************************************************************
void task_time2oled(void *parameter) {
    time_t now; // Unixzeit
    tm tm;      // Structure tm enthaelt die Zeitinformationen
    char buf[12];
    int old_sec = -1;
    while(1) {
        time(&now);
        localtime_r(&now, &tm);
        if (old_sec != tm.tm_sec) { 
            if (xSemaphoreTake(mutex_oled, 1000)) {
                old_sec = tm.tm_sec;
                oled.fillRect(0, 0, SCREEN_WIDTH, 36, BLACK);
                oled.setTextSize(2);
                oled.setTextColor(WHITE);
                oled.setCursor(0, 0);
                strftime(buf, sizeof(buf), "%d.%m.%Y", &tm);
                oled.println(buf);
                strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
                oled.println(buf);
                oled.display();
                xSemaphoreGive(mutex_oled);
            }
        }
    }
}

// ********************************************************************************
void task_eth2oled(void *parameter) {
    char buf[32];
    while(1) {
        if (xQueueReceive(queue_eth2oled, buf, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(mutex_oled, 1000)) {
                oled.fillRect(0, 38, SCREEN_WIDTH, 8, BLACK);
                oled.setTextSize(1);
                oled.setTextColor(WHITE);
                oled.setCursor(0, 38);
                if (eth_connected == true) {
                    oled.println(ETH.localIP());
                } else {
                    oled.println(buf);
                }
                oled.display();
                xSemaphoreGive(mutex_oled);
            }
        }
    }
}

// ********************************************************************************
void task_lastsync2oled(void *parameter) {
    char buf[32];
    while(1) {
        if (xQueueReceive(queue_lastsync2oled, buf, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(mutex_oled, 1000)) {
                oled.fillRect(0, 48, SCREEN_WIDTH-10, 8, BLACK);
                oled.setTextSize(1);
                oled.setTextColor(WHITE);
                oled.setCursor(0, 48);
                oled.println(buf);
                oled.display();
                xSemaphoreGive(mutex_oled);
            }
        }
    }
}

// ********************************************************************************
void task_syncstatus2oled(void *parameter) {
    sntp_sync_status_t status;
    static sntp_sync_status_t last_status = SNTP_SYNC_STATUS_RESET;
    
    while(1) {
        status = sntp_get_sync_status();
        if (last_status != status) {
            last_status = status;
            if (xSemaphoreTake(mutex_oled, 1000)) {
                oled.fillCircle(SCREEN_WIDTH-5, 51, 3, BLACK);
                if (status == SNTP_SYNC_STATUS_IN_PROGRESS) {
                    oled.fillCircle(SCREEN_WIDTH-5, 51, 3, WHITE);
                    Serial.println("Smooth Sync: läuft…");
                } else {
                    Serial.println("Smooth Sync: abgeschlossen");
                }
                oled.display();
                xSemaphoreGive(mutex_oled);
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// ********************************************************************************
void setup() {

    Serial.begin(115200);

    // OLED via I2C und Adresse 0x3C
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Problem bei Initialisierung SSD1306 OLED"));
    while (1);
    }
    oled.clearDisplay();

    // FreeRTOS Mutexe...
    mutex_oled = xSemaphoreCreateMutex();
    if (mutex_oled == NULL) {
        Serial.println("Problem bei Erzeugung mutex_oled!");
        while (1);
    }
    
    // FreeRTOS Queues...
    queue_eth2oled = xQueueCreate(5, sizeof(char[32]));
    if (queue_eth2oled == NULL) {
        Serial.println("Problem bei Erzeugung queue_eth2oled!");
        while (1);
    }
    queue_lastsync2oled = xQueueCreate(5, sizeof(char[32]));
    if (queue_lastsync2oled == NULL) {
        Serial.println("Problem bei Erzeugung queue_lastsync2oled!");
        while (1);
    }
        
    // Tasks initialisieren/starten (Heap-Größen könnten noch optimiert werden...)
    xTaskCreatePinnedToCore(task_time2oled, "task_time2oled", 10000, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_eth2oled, "task_eth2oled", 10000, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_lastsync2oled, "task_lastsync2oled", 10000, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_syncstatus2oled, "task_syncstatus2oled", 10000, NULL, 1, NULL, 1);

    // Ethernet
    Network.onEvent(onEvent);  // onEvent() wird von einem anderen Thread aufgerufen...
    ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);

    // NTP initialisieren, Zeitzone, etc.
    // ...lokale Zeitzone
    setenv("TZ", MY_TZ, 1);
    tzset();
    // ...SNTP konfigurieren
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // ...mehrere NTP-Server (als Fallback)
    sntp_setservername(0, "timepi");        // mein Zeitserver :-)
    sntp_setservername(1, "10.1.1.1");      // mein Internet-Router...
    // ...NTP-Sync-Intervall
    sntp_set_sync_interval(30 * 60 * 1000);  // 30min
    // ...Smooth Sync dauerhaft aktivieren
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    // ... ein Callback registrieren
    sntp_set_time_sync_notification_cb(time_sync_cb);
    // ...SNTP starten
    sntp_init(); 
   
}

// ********************************************************************************
// ********************************************************************************
// ********************************************************************************
void loop() {
    // ...wir machen FreeRTOS und starten die Tasks entsprechend in setup()...
}
