#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <memory>
#include "FreeRTOS.h"
#include "event_groups.h"
#include "gmp252.h"
#include "pico/stdlib.h"
#include "task.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "uart/PicoOsUart.h"
#include "modbus/ModbusClient.h"
#include "semphr.h"
#include "produalMIO.h"
#include "mutexGuard.h"
#include "hmp60.h"
#include "sdp610.h"
#include "IPStack.h"

extern "C" {
uint32_t read_runtime_ctr(void) {
    return time_us_32();
}
}

constexpr int TASK_HIGH_PRIORITY = 2 + tskIDLE_PRIORITY;
constexpr int WATCHDOG_PRIORITY = 2 + tskIDLE_PRIORITY;
constexpr int TASK_LOW_PRIORITY = 1 + tskIDLE_PRIORITY;

constexpr int BUTTON1_PIN = 7;
constexpr int BUTTON2_PIN = 8;
constexpr int BUTTON3_PIN = 9;

// Event group bits for watchdog
constexpr EventBits_t BIT_TASK_FAN = (1 << 0);
constexpr EventBits_t BIT_TASK_GMP = (1 << 1);
constexpr EventBits_t BIT_TASK_HMP = (1 << 2);
constexpr EventBits_t BIT_TASK_SDP = (1 << 3);
constexpr EventBits_t ALL_TASK_BITS = BIT_TASK_FAN | BIT_TASK_GMP | BIT_TASK_HMP | BIT_TASK_SDP;


// I2C instance for SDP610
i2c_inst_t *i2c_instance = i2c1;
constexpr uint SDA_PIN = 14;
constexpr uint SCL_PIN = 15;


// // Uart init
auto uart = std::make_shared<PicoOsUart>(0, 0, 1, 9600, 1, 256, 256);

// Modbus client
auto modbus = std::make_shared<ModbusClient>(uart);

// Mutex handlers
SemaphoreHandle_t modbusMutex;
SemaphoreHandle_t i2cMutex;
SemaphoreHandle_t buttonMutex;

// Sensor objects
ModbusMIO modbusFan(modbus, modbusMutex); // Address 1
GMP252 gmpSensor(modbus, modbusMutex); // Address 240
HMP60 hmpSensor(modbus, modbusMutex); // Address 241
SDP610 pressureSensor(i2c_instance, SDA_PIN, SCL_PIN, i2cMutex, 0x40);

// Queue handle for debug events
QueueHandle_t syslog_q;
constexpr int DEBUG_QUEUE_LENGTH = 10;

// Event group handle
EventGroupHandle_t eventGroup;

// 30s watchdog timer
constexpr TickType_t WATCHDOG_TIMER = pdMS_TO_TICKS(30000);

// Debug event structure
struct debugEvent {
    char debug[64]; // message
    uint32_t timestamp; // tick count when event created
};

// Parameter structure for tasks
struct TaskParams {
    int buttonPin;
    EventBits_t eventBit;
};

void debug(const char *message) {
    debugEvent e{};
    e.timestamp = xTaskGetTickCount();
    snprintf(e.debug, sizeof(e.debug), "%s", message);
    xQueueSend(syslog_q, &e, pdMS_TO_TICKS(10));
}


void initFunction() {
    // Setup buttons
    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);

    gpio_init(BUTTON3_PIN);
    gpio_set_dir(BUTTON3_PIN, GPIO_IN);
    gpio_pull_up(BUTTON3_PIN);

    // Initialize mutexes
    modbusMutex = xSemaphoreCreateMutex();
    i2cMutex = xSemaphoreCreateMutex();
    buttonMutex = xSemaphoreCreateMutex();
    // Create event group and debug queue
    eventGroup = xEventGroupCreate();
    syslog_q = xQueueCreate(DEBUG_QUEUE_LENGTH, sizeof(debugEvent));
    // Initialize I2C sensor
    if (!pressureSensor.init()) {
        debug("Failed to initialize SDP610 pressure sensor\n");
    }
}


[[noreturn]] void debugTask(void *pvParameters) {
    debugEvent e{};
    while (true) {
        // Wait indefinitely for a debug event
        if (xQueueReceive(syslog_q, &e, portMAX_DELAY) == pdPASS) {
            std::cout << "[" << e.timestamp << "] " << e.debug;
        }
    }
}


[[noreturn]] void http_task(void *pvParameters) {
    (void) pvParameters;

#if 1
#define HTTP_SERVER        "3.224.58.169"
    //#define HTTP_SERVER        "api.thingspeak.com"
#define BUFSIZE 2048
#endif

#if 0
    // this works
    const char *req = "POST /talkbacks/52920/commands/execute.json HTTP/1.1\r\n"
                      "Host: api.thingspeak.com\r\n"
                      "User-Agent: PicoW\r\n"
                      "Accept: */*\r\n"
                      "Content-Length: 24\r\n"
                      "Content-Type: application/x-www-form-urlencoded\r\n"
                      "\r\n"
                      "api_key=371DAWENQKI6J8DD";
#endif
#if 1
     // Execute (= get and remove) next command from talkback queue - tested to work
    const char *req = "POST /talkbacks/52920/commands/execute.json HTTP/1.1\r\n"
                      "Host: api.thingspeak.com\r\n"
                      "Content-Length: 24\r\n"
                      "Content-Type: application/x-www-form-urlencoded\r\n"
                      "\r\n"
                      "api_key=371DAWENQKI6J8DD";
#endif
#if 0
    // Update fields using a POST request and execute (= get and remove) next command from talkback queue - tested to work
    const char *req = "POST /update.json HTTP/1.1\r\n"
                      "Host: api.thingspeak.com\r\n"
                      "User-Agent: PicoW\r\n"
                      "Accept: */*\r\n"
                      "Content-Length: 65\r\n"
                      "Content-Type: application/x-www-form-urlencoded\r\n"
                      "\r\n"
                      "field1=370&api_key=1WWH2NWXSM53URR5&talkback_key=371DAWENQKI6J8DD";
#endif
#if 0
    // Update fields using a GET request - tested to work
    const char *req = "GET /update?api_key=1WWH2NWXSM53URR5&field1=440&field2=44.7 HTTP/1.1\r\n"
                      "Host: api.thingspeak.com\r\n"
                      "User-Agent: PicoW\r\n"
                      "Accept: */*\r\n"
                      "Content-Length: 0\r\n"
                      "Content-Type: application/x-www-form-urlencoded\r\n"
                      "\r\n";
#endif
#if 0
    // Update fields using a minimal GET request - tested to work
    const char *req = "GET /update?api_key=1WWH2NWXSM53URR5&field1=410&field2=45.7 HTTP/1.1\r\n"
                      "Host: api.thingspeak.com\r\n"
                      "\r\n";
#endif
#if 0
    // List all talkback commands - tested to work
    const char *req = "GET /talkbacks/52920/commands.json?api_key=371DAWENQKI6J8DD HTTP/1.1\r\n"
                      "Host: api.thingspeak.com\r\n"
                      "\r\n";
#endif
    printf("\nconnecting...\n");

    unsigned char *buffer = new unsigned char[BUFSIZE];
    // todo: Add failed connection handling
    //IPStack ipstack("SmartIotMQTT", "SmartIot"); // example
    IPStack ipstack(WIFI_SSID, WIFI_PASSWORD); // Set env in CLion CMAKE setting

    const uint led_pin = 22;
    // Initialize LED pin
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

     while(true) {
        //std::cout << "button event\n";
        gpio_put(led_pin, 1);
        vTaskDelay(300);
        gpio_put(led_pin, 0);
        vTaskDelay(300);
         //Use button3 where "button is referenced"
        if(gpio_get(button) == 0) {
            int rc = ipstack.connect(HTTP_SERVER, 80);
            if (rc == 0) {
                ipstack.write((unsigned char *) (req), strlen(req), 1000);
                auto rv = ipstack.read(buffer, BUFSIZE, 2000);
                buffer[rv] = 0;
                printf("rv=%d\n%s\n", rv, buffer);
                ipstack.disconnect();
            }
            else {
                printf("rc from TCP connect is %d\n", rc);
            }
        }
    }
}

[[noreturn]] void watchDogTimer(void *pvParameter) {
    TickType_t lastOK = xTaskGetTickCount();

    while (true) {
        EventBits_t result = xEventGroupWaitBits(
            eventGroup,
            ALL_TASK_BITS,
            pdTRUE, // clear bits on exit
            pdTRUE, // wait for all bits
            WATCHDOG_TIMER
        );

        if ((result & ALL_TASK_BITS) == ALL_TASK_BITS) {
            TickType_t now = xTaskGetTickCount();
            char buf[64];
            snprintf(buf, sizeof(buf), "Watchdog: OK, %lu ms since last OK\n",
                     static_cast<unsigned long>((now - lastOK) * portTICK_PERIOD_MS));
            debug(buf);
            lastOK = now;
        } else {
            EventBits_t missingBits = ALL_TASK_BITS & ~result;
            debug("Watchdog FAIL! Missing tasks:\n");
            if (missingBits & BIT_TASK_FAN) debug("  Fan control task\n");
            if (missingBits & BIT_TASK_GMP) debug("  GMP252 CO2 sensor task\n");
            if (missingBits & BIT_TASK_HMP) debug("  HMP60 humidity/temp task\n");
            if (missingBits & BIT_TASK_SDP) debug("  SDP610 pressure sensor task\n");

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

[[noreturn]] void modbusFanTask(void *pvParameters) {
    constexpr TickType_t taskDelay = pdMS_TO_TICKS(5000); // Run every 5 seconds

    while (true) {
        constexpr float desiredFanSpeed = 100.0f;
        bool success = modbusFan.setFanSpeed(desiredFanSpeed);
        bool fanRunning = modbusFan.isFanRunning();

        if (success && fanRunning) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Fan speed set to %.1f%%, fan is running\n", desiredFanSpeed);
            debug(buf);
        } else {
            char buf[64];
            snprintf(buf, sizeof(buf), "Fan control failed! Success: %d, Running: %d\n", success, fanRunning);
            debug(buf);
        }

        // Notify watchdog
        xEventGroupSetBits(eventGroup, BIT_TASK_FAN);
        vTaskDelay(taskDelay);
    }
}

[[noreturn]] void modbusGmpTask(void *pvParameters) {
    constexpr TickType_t taskDelay = pdMS_TO_TICKS(2000); // Run every 2 seconds

    while (true) {
        const float co2 = gmpSensor.readMeasuredCO2();
        const float compensation = gmpSensor.readCompensationT();
        const float measuredT = gmpSensor.readMeasuredT();

        if (!std::isnan(co2) && !std::isnan(compensation) && !std::isnan(measuredT)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "GMP252 - CO2: %.1f ppm, Comp T: %.1f°C, Meas T: %.1f°C\n",
                     co2, compensation, measuredT);
            debug(buf);
        } else {
            debug("GMP252 sensor read failed!\n");
        }

        // Notify watchdog
        xEventGroupSetBits(eventGroup, BIT_TASK_GMP);
        vTaskDelay(taskDelay);
    }
}

[[noreturn]] void modbusHmpTask(void *pvParameters) {
    constexpr TickType_t taskDelay = pdMS_TO_TICKS(3000); // Run every 3 seconds

    while (true) {
        const float humidity = hmpSensor.readHumidity();
        const float temperature = hmpSensor.readTemperature();

        if (!std::isnan(humidity) && !std::isnan(temperature)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "HMP60 - Humidity: %.1f%%, Temperature: %.1f°C\n",
                     humidity, temperature);
            debug(buf);
        } else {
            debug("HMP60 sensor read failed!\n");
        }

        // Notify watchdog
        xEventGroupSetBits(eventGroup, BIT_TASK_HMP);
        vTaskDelay(taskDelay);
    }
}

[[noreturn]] void I2cPressureSensorTask(void *pvParameters) {
    constexpr TickType_t taskDelay = pdMS_TO_TICKS(4000); // Run every 4 seconds

    while (true) {
        const float pressure = pressureSensor.readPressurePa();

        if (!std::isnan(pressure)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "SDP610 - Pressure: %.2f Pa\n", pressure);
            debug(buf);
        } else {
            debug("SDP610 pressure sensor read failed!\n");
        }

        // Notify watchdog
        xEventGroupSetBits(eventGroup, BIT_TASK_SDP);
        vTaskDelay(taskDelay);
    }
}

[[noreturn]] int main() {
    stdio_init_all();
    initFunction();

    printf("Program started.\n");

    xTaskCreate(debugTask, "DebugTask", 256,
                nullptr,
                TASK_LOW_PRIORITY, nullptr);

    xTaskCreate(watchDogTimer, "WatchDogTimer", 256,
                nullptr,
                WATCHDOG_PRIORITY, nullptr);

    xTaskCreate(modbusFanTask, "ModbusTask", 256,
                nullptr,
                TASK_HIGH_PRIORITY, nullptr);

    xTaskCreate(modbusGmpTask, "ModbusGmpTask", 256,
                nullptr, TASK_HIGH_PRIORITY, nullptr);

    xTaskCreate(modbusHmpTask, "ModbusHmpTask", 256,
                nullptr, TASK_HIGH_PRIORITY, nullptr);

    xTaskCreate(I2cPressureSensorTask, "PressureSensorTask", 256,
                nullptr, TASK_HIGH_PRIORITY, nullptr);

    debug("All tasks created. Starting scheduler.\n");

    vTaskStartScheduler();
    while (true); // should never reach
}
