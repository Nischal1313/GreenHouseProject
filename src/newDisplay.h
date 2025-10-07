commit 3a0a39327245733101d898da9931eaccdeef0c88
Author: Nischal1313 <nischaga@metropolia.fi>
Date:   Tue Oct 7 10:11:30 2025 +0300

    BACKUP: Current state before rollback

diff --git a/src/displayMenu.h b/src/displayMenu.h
index c1a6092..c186f2a 100644
--- a/src/displayMenu.h
+++ b/src/displayMenu.h
@@ -1,5 +1,5 @@
-#ifndef DISPLAY_MENU
-#define DISPLAY_MENU
+#ifndef DISPLAY_MENU_H
+#define DISPLAY_MENU_H
 
 #include <memory>
 #include "FreeRTOS.h"
@@ -11,30 +11,47 @@
 #include "produalMIO.h"
 #include "rotaryEncoder.h"
 #include "relayController.h"
-#include "pico/stdio.h"
+#include "setCredentials.h"
+#include "pico/stdlib.h"
+
+enum class MenuState {
+    MAIN,
+    WIFI
+};
 
 struct DisplayParams {
     GMP252* gmpSensor;
     HMP60* hmpSensor;
     ModbusMIO* modbusSystem;
     RotaryEncoder* encoder;
+    SetCredentials* credentials;
 };
 
 class DisplayManager {
 public:
     DisplayManager();
+
     void setParams(DisplayParams* displayParams);
+    [[noreturn]] void displayTask();
+    static void taskEntry(void* pvParameters);
 
-    // This function runs inside a FreeRTOS task
-    [[noreturn]] void displayTask() const;
+    // State machine controls
+    void changeMenu(); // switch between MAIN <-> WIFI
+    MenuState getMenuState() const;
 
-    // Static entry point for FreeRTOS
-    static void taskEntry(void* pvParameters);
+    // Called from button IRQ handlers

 private:
     std::shared_ptr<PicoI2C> i2cBus;
     std::shared_ptr<ssd1306os> oLed;
     DisplayParams* params;
+    static DisplayManager* instance; // singleton pointer for IRQ access
+    MenuState menuState;
+
+    // Menu display modes
+    void drawMainMenu();
+    void drawWifiMenu();
 };
 
 #endif
