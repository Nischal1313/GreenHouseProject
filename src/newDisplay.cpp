commit 3a0a39327245733101d898da9931eaccdeef0c88
Author: Nischal1313 <nischaga@metropolia.fi>
Date:   Tue Oct 7 10:11:30 2025 +0300

    BACKUP: Current state before rollback

diff --git a/src/displayMenu.cpp b/src/displayMenu.cpp
index 8250629..f269f51 100644
--- a/src/displayMenu.cpp
+++ b/src/displayMenu.cpp
@@ -1,53 +1,128 @@
 #include "displayMenu.h"
 #include <cstdio>
 
+DisplayManager* DisplayManager::instance = nullptr;
+
 DisplayManager::DisplayManager()
     : i2cBus(std::make_shared<PicoI2C>(1, 400000)),
       oLed(std::make_shared<ssd1306os>(i2cBus)),
-      params(nullptr) {
+      params(nullptr),
+      menuState(MenuState::MAIN)
+{
+    instance = this;
 }
 
-void DisplayManager::setParams(DisplayParams *displayParams) {
+void DisplayManager::setParams(DisplayParams* displayParams) {
     this->params = displayParams;
 }
 
-void DisplayManager::taskEntry(void *pvParameters) {
-    auto *self = static_cast<DisplayManager *>(pvParameters);
-    self->displayTask(); // Run the member task loop
+void DisplayManager::taskEntry(void* pvParameters) {
+    auto* self = static_cast<DisplayManager*>(pvParameters);
+    self->displayTask();
 }
 
-[[noreturn]] void DisplayManager::displayTask() const {
-    char buf[128];
-
+[[noreturn]] void DisplayManager::displayTask() {
     while (true) {
         oLed->fill(0);
 
-        // Read all values via getters
-        const float co2 = params->gmpSensor->readMeasuredCO2();
-        const float temp = params->hmpSensor->readTemperature();
-        const float hum = params->hmpSensor->readHumidity();
-        const int desiredCO2 = params->encoder->currentRotationValue();
-        const float fanSpeed = params->modbusSystem->readFanSpeed();
-        const bool valveState = params->modbusSystem->valveStatus();
+        switch (menuState) {
+            case MenuState::MAIN:
+                drawMainMenu();
+                break;
+            case MenuState::WIFI:
+                drawWifiMenu();
+                break;
+        }
+
+        oLed->show();
+        vTaskDelay(pdMS_TO_TICKS(200));
+    }
+}
+
+// ==============================
+//   MENU DRAW FUNCTIONS
+// ==============================
+
+void DisplayManager::drawMainMenu() {
+    char buf[128];
 
-        snprintf(buf, sizeof(buf), "CO2: %.0f ppm", co2);
-        oLed->text(buf, 2, 5);
+    const float co2 = params->gmpSensor->readMeasuredCO2();
+    const float temp = params->hmpSensor->readTemperature();
+    const float hum = params->hmpSensor->readHumidity();
+    const int desiredCO2 = params->encoder->currentRotationValue();
+    const float fanSpeed = params->modbusSystem->readFanSpeed();
+    const bool valveState = params->modbusSystem->valveStatus();
 
-        snprintf(buf, sizeof(buf), "Set: %d ppm", desiredCO2);
-        oLed->text(buf, 2, 15);
+    snprintf(buf, sizeof(buf), "CO2: %.0f ppm", co2);
+    oLed->text(buf, 2, 5);
 
-        snprintf(buf, sizeof(buf), "Temp: %.1fC", temp);
-        oLed->text(buf, 2, 25);
+    snprintf(buf, sizeof(buf), "Set: %d ppm", desiredCO2);
+    oLed->text(buf, 2, 15);
 
-        snprintf(buf, sizeof(buf), "Hum: %.1f%%", hum);
-        oLed->text(buf, 2, 35);
-        snprintf(buf, sizeof(buf), "Fan speed: %.0f%%", fanSpeed);
-        oLed->text(buf, 2, 45);
-        snprintf(buf, sizeof(buf), "Valve: %s", valveState ? "OPEN" : "CLOSED");
-        oLed->text(buf, 2, 55);
+    snprintf(buf, sizeof(buf), "Temp: %.1fC", temp);
+    oLed->text(buf, 2, 25);
 
-        oLed->show();
+    snprintf(buf, sizeof(buf), "Hum: %.1f%%", hum);
+    oLed->text(buf, 2, 35);
+
+    snprintf(buf, sizeof(buf), "Fan: %.0f%%", fanSpeed);
+    oLed->text(buf, 2, 45);
+
+    snprintf(buf, sizeof(buf), "Valve: %s", valveState ? "OPEN" : "CLOSED");
+    oLed->text(buf, 2, 55);
+}
+
+void DisplayManager::drawWifiMenu() {
+    char buf[128];
+
+    auto* cred = params->credentials;
+    snprintf(buf, sizeof(buf), "%s", cred->getCurrentFieldName());
+    oLed->text(buf, 2, 5);
+
+    snprintf(buf, sizeof(buf), "%s", cred->getCurrentBuffer());
+    oLed->text(buf, 2, 15);
+
+    snprintf(buf, sizeof(buf), "%c", cred->getCurrentChar());
+    oLed->text(buf, 2, 25);
+
+    oLed->text("[7] Menu", 2, 35);
+    oLed->text("[8] Next Field", 2, 45);
+    oLed->text("[9] CharSet", 2, 55);
+}
+
+// ==============================
+//   STATE HANDLING
+// ==============================
+
+void DisplayManager::changeMenu() {
+    if (menuState == MenuState::MAIN)
+        menuState = MenuState::WIFI;
+    else
+        menuState = MenuState::MAIN;
+}
+
+MenuState DisplayManager::getMenuState() const {
+    return menuState;
+}
+
+// ==============================
+//   IRQ HANDLER
+// ==============================
+
+void DisplayManager::buttonIRQ(uint gpio, uint32_t events) {
+    if (!instance) return;
 
-        vTaskDelay(pdMS_TO_TICKS(100));
+    switch (gpio) {
+        case 7:  // Change between menus
+            instance->changeMenu();
+            break;
+        case 8:  // Next field in credentials
+            instance->params->credentials->nextField();
+            break;
+        case 9:  // Change character set
+            instance->params->credentials->nextCharset();
+            break;
+        default:
+            break;
     }
 }
