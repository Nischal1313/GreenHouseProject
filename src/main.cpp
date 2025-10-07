#include <cmath>
#include <cstdio>
#include <memory>
#include "FreeRTOS.h"
#include "task.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"

#include "PicoOsUart.h"
#include "PicoI2C.h"
#include "ssd1306os.h"

#include "gmp252.h"
#include "hmp60.h"
#include "produalMIO.h"
#include "displayMenu.h"
#include "rotaryEncoder.h"
#include "debug.h"
#include "event_groups.h"
#include "eeprom.h"
#include "encoderHandler.h"
#include "inputManager.h"


extern "C" {
    uint32_t read_runtime_ctr(void) {
        return time_us_32();
    }
}

// --- Global Resources ---
SemaphoreHandle_t modbusMutex;
std::shared_ptr<PicoOsUart> uart;
std::shared_ptr<ModbusClient> modbusClient;

// CHANGE: Use pointers for Modbus-dependent and I2C-dependent objects
GMP252* gmpSensor = nullptr;
HMP60* hmpSensor = nullptr;
ModbusMIO* modbusSystem = nullptr;
RotaryEncoder* encoder = nullptr; // RotaryEncoder uses I2C for EEPROM

// --- Tasks ---
[[noreturn]] void displayTask(void *pvParameters) {
    static_cast<DisplayManager*>(pvParameters)->displayTask();
}
// --- Modbus Control Task ---
[[noreturn]] void modbusControlTask(void* pvParameters) {
    const auto* debug = static_cast<Debug*>(pvParameters);
    while (true) {
        // Use -> to access members
        const int desiredCO2 = encoder->currentRotationValue();
        const int currentCO2 = static_cast<int>(gmpSensor->readMeasuredCO2());
        modbusSystem->controlLoop(*gmpSensor, *encoder); // Pass dereferenced objects

        char buf[128];
        snprintf(buf, sizeof(buf), "CO2=%d ppm, Desired=%d ppm\n", currentCO2, desiredCO2);
        debug->print(buf);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main() {
    stdio_init_all();
    printf("1 - System Init Start\n");

    // --- I2C setup ---
    i2c_init(i2c1, 400000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);

    // --- Modbus setup ---
    modbusMutex = xSemaphoreCreateMutex();
    uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
    modbusClient = std::make_shared<ModbusClient>(uart);
    gmpSensor = new GMP252(modbusClient, modbusMutex);
    hmpSensor = new HMP60(modbusClient, modbusMutex);
    modbusSystem = new ModbusMIO(modbusClient, modbusMutex);

    encoder = new RotaryEncoder();

    // --- EEPROM + credentials setup ---
    Eeprom eeprom(i2c0, 0x50, 2); // EEPROM on I2C0
    SemaphoreHandle_t eepromMutex = xSemaphoreCreateMutex();
    SetCredentials credentials(eeprom, eepromMutex);

    // --- Input manager setup ---
    InputManager inputManager{}; // GPIO pins for buttons

    // --- Display manager setup ---
    DisplayParams displayParams {
        gmpSensor,
        hmpSensor,
        modbusSystem,
        encoder,
        &inputManager,
        &credentials
    };

    DisplayManager displayManager;
    displayManager.setParams(&displayParams);

    // --- Task creation ---
    xTaskCreate(modbusControlTask, "modbusControlTask", 2048, nullptr, 2, nullptr);
    xTaskCreate(InputManager::taskEntry, "InputTask", 512, &inputManager, tskIDLE_PRIORITY + 3, nullptr);
    xTaskCreate(DisplayManager::taskEntry, "DisplayTask", 2048, &displayManager, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(RotaryEncoder::encoderTask, "EncoderPoll", 512, encoder, tskIDLE_PRIORITY + 2, nullptr);

    vTaskStartScheduler();
}













//
//
//
//
//
//
//
// #include "pico/stdlib.h"
// #include "hardware/i2c.h"
// #include "FreeRTOS.h"
// #include "task.h"
// #include <stdio.h>
// #include <string.h>
//
//
// extern "C" {
//     uint32_t read_runtime_ctr(void) {
//         return time_us_32();
//     }
// }
// // --- EEPROM class (simplified for this file) ---
// class Eeprom {
// public:
//     Eeprom(i2c_inst_t *i2cPort, uint8_t eepromAddr, uint8_t addressWidth)
//         : i2cPort(i2cPort), eepromAddr(eepromAddr), addressWidth(addressWidth) {}
//
//     bool writeByte(int addr, uint8_t data) {
//         uint8_t buf[3];
//         buildAddressBytes(addr, buf);
//         buf[addressWidth] = data;
//         if(i2c_write_blocking(i2cPort, eepromAddr, buf, addressWidth+1, false) != (addressWidth+1)) return false;
//         sleep_ms(5);
//         return true;
//     }
//
//     int readByte(int addr) {
//         uint8_t buf[2], data;
//         buildAddressBytes(addr, buf);
//         if(i2c_write_blocking(i2cPort, eepromAddr, buf, addressWidth, true) != addressWidth) return -1;
//         if(i2c_read_blocking(i2cPort, eepromAddr, &data, 1, false) != 1) return -1;
//         return data;
//     }
//
// private:
//     void buildAddressBytes(int addr, uint8_t *out) const {
//         if(addressWidth==2){ out[0]=(addr>>8)&0xFF; out[1]=addr&0xFF; }
//         else { out[0]=addr&0xFF; }
//     }
//
//     i2c_inst_t *i2cPort;
//     uint8_t eepromAddr;
//     uint8_t addressWidth;
// };
//
// // --- Button & encoder pins ---
// constexpr uint PIN_CLEAR   = 7;
// constexpr uint PIN_NEXT    = 8;
// constexpr uint PIN_CHARSET = 9;
// constexpr uint PIN_ENCODER_A = 10;
// constexpr uint PIN_ENCODER_B = 11;
// constexpr uint PIN_ENCODER_BTN = 12;
//
// // --- Charset modes ---
// enum CharsetMode { LOWERCASE, UPPERCASE, NUMBERS };
//
// // --- WiFi credentials manager ---
// class WifiCredentials {
// public:
//     WifiCredentials(Eeprom &e) : eeprom(e) {
//         memset(ssid, 0, sizeof(ssid));
//         memset(password, 0, sizeof(password));
//         field = 0; charsetMode = LOWERCASE; charIndex = 0;
//         loadFromEEPROM();
//     }
//
//     const char* getCurrentFieldName() { return (field==0) ? "SSID" : "Password"; }
//     const char* getCurrentBuffer() {
//         return (field==0) ? (ssidEmpty ? "EMPTY" : ssid)
//                           : (pwdEmpty ? "EMPTY" : password);
//     }
//     CharsetMode getCharsetMode() { return charsetMode; }
//     char getCurrentChar() { return currentChar(); }
//
//     void rotateChar(int delta) {
//         const char* set = charset();
//         int pos = charIndex;
//         pos = (pos + delta + strlen(set)) % strlen(set);
//         charIndex = pos;
//     }
//
//     void confirmChar() {
//         char* buf = (field==0) ? ssid : password;
//         bool &emptyFlag = (field==0) ? ssidEmpty : pwdEmpty;
//         emptyFlag = false;
//         size_t len = strlen(buf);
//         if(len < sizeof(ssid)-1){
//             buf[len] = currentChar();
//             buf[len+1] = 0;
//             saveToEEPROM();
//         }
//     }
//
//     void nextField() {
//         field = 1-field;
//         charIndex = 0;
//     }
//
//     void nextCharset() {
//         charsetMode = static_cast<CharsetMode>((charsetMode+1)%3);
//         charIndex = 0;
//     }
//
// private:
//     char ssid[32], password[32];
//     bool ssidEmpty = true, pwdEmpty = true;
//     int field; // 0=SSID,1=Password
//     CharsetMode charsetMode;
//     int charIndex;
//     Eeprom &eeprom;
//
//     void loadFromEEPROM() {
//         for(int i=0;i<32;i++){
//             int b=eeprom.readByte(i);
//             if(b>=32 && b<127) ssid[i]=b; else ssid[i]=0;
//         }
//         for(int i=0;i<32;i++){
//             int b=eeprom.readByte(0x20+i);
//             if(b>=32 && b<127) password[i]=b; else password[i]=0;
//         }
//         ssidEmpty = (ssid[0]==0);
//         pwdEmpty = (password[0]==0);
//     }
//
//     void saveToEEPROM() {
//         char* buf = (field==0) ? ssid : password;
//         for(int i=0;i<32;i++){
//             eeprom.writeByte((field==0?i:i+0x20), buf[i]);
//         }
//     }
//
//     const char* charset() {
//         switch(charsetMode){
//             case LOWERCASE: return "abcdefghijklmnopqrstuvwxyz";
//             case UPPERCASE: return "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
//             case NUMBERS:   return "0123456789";
//             default: return "?";
//         }
//     }
//
//     char currentChar() {
//         const char* set = charset();
//         return set[charIndex % strlen(set)];
//     }
// };
//
// // --- Simple debounce ---
// bool debounce(uint pin) {
//     static uint32_t lastTime[40]={0};
//     const uint32_t delayMs=200;
//     if(!gpio_get(pin)) { // pressed
//         uint32_t now = to_ms_since_boot(get_absolute_time());
//         if(now - lastTime[pin] > delayMs) { lastTime[pin]=now; return true; }
//     }
//     return false;
// }
//
// // --- Display manager ---
// class DisplayManager {
// public:
//     DisplayManager() {
//         displayOk = initDisplay();
//     }
//
//     void drawWifiMenu(WifiCredentials &cred) {
//         char buf[128];
//
//         // --- Simulated LED Display Output ---
//         if(displayOk){
//             // Replace this with real OLED draw code
//             printf("[OLED DISPLAY OUTPUT]\n");
//             snprintf(buf,sizeof(buf),"Field: %s", cred.getCurrentFieldName());
//             oledDrawText(buf,0);
//             snprintf(buf,sizeof(buf),"Value: %s", cred.getCurrentBuffer());
//             oledDrawText(buf,1);
//             const char* modeName = (cred.getCharsetMode()==LOWERCASE)?"abc":(cred.getCharsetMode()==UPPERCASE)?"ABC":"123";
//             snprintf(buf,sizeof(buf),"Charset: %s", modeName);
//             oledDrawText(buf,2);
//             snprintf(buf,sizeof(buf),"Current char: [%c]", cred.getCurrentChar());
//             oledDrawText(buf,3);
//             oledDrawText("Back=Save&Exit",4);
//         }
//
//         // --- Always show debug version as well ---
//         debugDraw(cred);
//     }
//
// private:
//     bool displayOk;
//
//     bool initDisplay() {
//         // Try initializing your OLED/LED here
//         // return true if successful
//         return false; // Force debug mode fallback for testing
//     }
//
//     void oledDrawText(const char* text, int line) {
//         // Simulate real OLED draw function
//         printf("OLED Line %d: %s\n", line, text);
//     }
//
//     void debugDraw(WifiCredentials &cred) {
//         char buf[128];
//         printf("\033[2J\033[H"); // clear console for clean redraw
//         snprintf(buf,sizeof(buf),"Field: %s", cred.getCurrentFieldName());
//         printf("%s\n", buf);
//         snprintf(buf,sizeof(buf),"Value: %s", cred.getCurrentBuffer());
//         printf("%s\n", buf);
//         const char* modeName = (cred.getCharsetMode()==LOWERCASE)?"abc":(cred.getCharsetMode()==UPPERCASE)?"ABC":"123";
//         snprintf(buf,sizeof(buf),"Charset: %s", modeName);
//         printf("%s\n", buf);
//         snprintf(buf,sizeof(buf),"Current char: [%c]", cred.getCurrentChar());
//         printf("%s\n", buf);
//         printf("Back=Save&Exit\n\n");
//     }
// };
//
// // --- FreeRTOS task ---
// void menuTask(void* pvParameters){
//     WifiCredentials *cred = (WifiCredentials*) pvParameters;
//     DisplayManager display;
//     while(true){
//         // --- Buttons ---
//         if(debounce(PIN_CLEAR)) { printf("Clear pressed\n"); }
//         if(debounce(PIN_NEXT)) { cred->nextField(); }
//         if(debounce(PIN_CHARSET)) { cred->nextCharset(); }
//         if(debounce(PIN_ENCODER_BTN)) { cred->confirmChar(); }
//
//         // --- Encoder rotation ---
//         static int lastA=0,lastB=0;
//         int a = gpio_get(PIN_ENCODER_A);
//         int b = gpio_get(PIN_ENCODER_B);
//         int delta=0;
//         if(a!=lastA){
//             delta = (a!=b) ? 1 : -1;
//             cred->rotateChar(delta);
//         }
//         lastA=a; lastB=b;
//
//         // --- Draw menu ---
//         display.drawWifiMenu(*cred);
//
//         vTaskDelay(pdMS_TO_TICKS(400));
//     }
// }
//
// // --- Main ---
// int main() {
//     stdio_init_all();
//
//     // Init pins
//     gpio_init(PIN_CLEAR);   gpio_set_dir(PIN_CLEAR, GPIO_IN);   gpio_pull_up(PIN_CLEAR);
//     gpio_init(PIN_NEXT);    gpio_set_dir(PIN_NEXT, GPIO_IN);    gpio_pull_up(PIN_NEXT);
//     gpio_init(PIN_CHARSET); gpio_set_dir(PIN_CHARSET, GPIO_IN); gpio_pull_up(PIN_CHARSET);
//     gpio_init(PIN_ENCODER_A); gpio_set_dir(PIN_ENCODER_A, GPIO_IN);
//     gpio_init(PIN_ENCODER_B); gpio_set_dir(PIN_ENCODER_B, GPIO_IN);
//     gpio_init(PIN_ENCODER_BTN); gpio_set_dir(PIN_ENCODER_BTN, GPIO_IN); gpio_pull_up(PIN_ENCODER_BTN);
//
//     // Init I2C & EEPROM
//     i2c_init(i2c0, 400000);
//     gpio_set_function(16, GPIO_FUNC_I2C);
//     gpio_set_function(17, GPIO_FUNC_I2C);
//     gpio_pull_up(16); gpio_pull_up(17);
//     Eeprom eeprom(i2c0, 0x50, 2);
//
//     // WiFi credentials
//     WifiCredentials credentials(eeprom);
//
//     // Create FreeRTOS task
//     xTaskCreate(menuTask, "MenuTask", 4096, &credentials, 1, NULL);
//
//     vTaskStartScheduler();
//     while(true){}
// }
