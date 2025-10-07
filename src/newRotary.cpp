commit 3a0a39327245733101d898da9931eaccdeef0c88
Author: Nischal1313 <nischaga@metropolia.fi>
Date:   Tue Oct 7 10:11:30 2025 +0300

    BACKUP: Current state before rollback

diff --git a/src/rotaryEncoder.cpp b/src/rotaryEncoder.cpp
index 3e612e2..b146ddc 100644
--- a/src/rotaryEncoder.cpp
+++ b/src/rotaryEncoder.cpp
@@ -1,21 +1,18 @@
 #include "rotaryEncoder.h"
+#include "mutexGuard.h"
 #include <algorithm>
-#include <cstdio>
 
-#define EEPROM_CO2_ADDR 0x00
+volatile int RotaryEncoder::rotationAccum = 0;
+volatile bool RotaryEncoder::buttonPressedFlag = false;
+volatile bool RotaryEncoder::rotationEnabled = true;
 
-RotaryEncoder::RotaryEncoder()
+RotaryEncoder::RotaryEncoder(Eeprom& eeprom, SemaphoreHandle_t eepromMutex)
     : desiredCO2(500),
-      lastA(0), lastB(0),
-      eeprom(i2c0, 0x50, 2) // EEPROM on I2C0
+      lastA(0),
+      lastB(0),
+      eeprom(eeprom),
+      eepromMutex(eepromMutex)
 {
-    // Initialize EEPROM I2C bus
-    i2c_init(i2c0, 400000);
-    gpio_set_function(16, GPIO_FUNC_I2C);
-    gpio_set_function(17, GPIO_FUNC_I2C);
-    gpio_pull_up(16);
-    gpio_pull_up(17);
-
     // Encoder GPIO setup
     gpio_init(PIN_A);
     gpio_init(PIN_B);
@@ -24,7 +21,12 @@ RotaryEncoder::RotaryEncoder()
     gpio_pull_up(PIN_A);
     gpio_pull_up(PIN_B);
 
-    // Read stored CO2 target
+    // Button pin setup (no IRQ)
+    gpio_init(PIN_BTN);
+    gpio_set_dir(PIN_BTN, GPIO_IN);
+    gpio_pull_up(PIN_BTN);
+
+    // Load saved CO2 target
     readFromEEPROM();
 
     lastA = gpio_get(PIN_A);
@@ -35,12 +37,44 @@ int RotaryEncoder::currentRotationValue() const {
     return desiredCO2;
 }
 
+bool RotaryEncoder::consumeButtonPress() {
+    bool pressed = false;
+    taskENTER_CRITICAL();
+    pressed = buttonPressedFlag;
+    buttonPressedFlag = false;
+    taskEXIT_CRITICAL();
+    return pressed;
+}
+
+int RotaryEncoder::consumeRotationDelta() {
+    int v = 0;
+    taskENTER_CRITICAL();
+    v = rotationAccum;
+    rotationAccum = 0;
+    taskEXIT_CRITICAL();
+    return v;
+}
+
+void RotaryEncoder::setRotationEnabled(bool on) {
+    taskENTER_CRITICAL();
+    rotationEnabled = on;
+    taskEXIT_CRITICAL();
+}
+
 void RotaryEncoder::readFromEEPROM() {
-    uint8_t buf[2] = {0};
-    if (eeprom.readBlock(EEPROM_CO2_ADDR, buf, 2)) {
-        desiredCO2 = (buf[0] << 8) | buf[1];
-        desiredCO2 = std::clamp(desiredCO2, 200, 1500);
+    uint8_t buf[2] = {0, 0};
+
+    // Use RAII mutex guard
+    MutexGuard lock(eepromMutex);
+    if (lock.owns_lock()) {
+        if (eeprom.readBlock(EEPROM_CO2_ADDR, buf, 2)) {
+            int v = (buf[0] << 8) | buf[1];
+            desiredCO2 = std::clamp(v, 0, 2000);
+        } else {
+            desiredCO2 = 500;
+        }
     } else {
+        // Mutex acquisition failed - use default value
         desiredCO2 = 500;
     }
 }
@@ -50,32 +84,53 @@ void RotaryEncoder::writeToEEPROM() {
         static_cast<uint8_t>(desiredCO2 >> 8),
         static_cast<uint8_t>(desiredCO2 & 0xFF)
     };
-    eeprom.writeBlock(EEPROM_CO2_ADDR, buf, 2);
+
+    // Use RAII mutex guard
+    MutexGuard lock(eepromMutex);
+    if (lock.owns_lock()) {
+        eeprom.writeBlock(EEPROM_CO2_ADDR, buf, 2);
+    }
 }
 
+// Polling task (should be started from main after objects are created)
 void RotaryEncoder::encoderTask(void* pv) {
     auto* self = static_cast<RotaryEncoder*>(pv);
     TickType_t lastWriteTick = xTaskGetTickCount();
+    static bool lastBtn = true;
 
     while (true) {
-        const int a = gpio_get(PIN_A);
-        const int b = gpio_get(PIN_B);
+        int a = gpio_get(PIN_A);
+        int b = gpio_get(PIN_B);
+        bool btn = gpio_get(PIN_BTN);
 
+        // --- Rotation handling ---
         if (a != self->lastA) {
-            if (b != a)
-                self->desiredCO2 += 10;
-            else
-                self->desiredCO2 -= 10;
+            int delta = (a == b) ? +1 : -1;
+
+            taskENTER_CRITICAL();
+            rotationAccum += delta;
+            if (rotationEnabled) {
+                self->desiredCO2 = std::clamp(self->desiredCO2 + delta * 10, 0, 2000);
+            }
+            taskEXIT_CRITICAL();
 
-            self->desiredCO2 = std::clamp(self->desiredCO2, 200, 1500);
             self->lastA = a;
         }
 
-        if (xTaskGetTickCount() - lastWriteTick > pdMS_TO_TICKS(2000)) {
+        // --- Button press handling (falling edge) ---
+        if (lastBtn && !btn) {
+            taskENTER_CRITICAL();
+            buttonPressedFlag = true;
+            taskEXIT_CRITICAL();
+        }
+        lastBtn = btn;
+
+        // --- Periodically save to EEPROM ---
+        if ((xTaskGetTickCount() - lastWriteTick) >= pdMS_TO_TICKS(10000)) {
             self->writeToEEPROM();
             lastWriteTick = xTaskGetTickCount();
         }
 
-        vTaskDelay(pdMS_TO_TICKS(5));
+        vTaskDelay(pdMS_TO_TICKS(5)); // Poll every 5ms
     }
 }
