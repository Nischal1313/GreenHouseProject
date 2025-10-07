commit 3a0a39327245733101d898da9931eaccdeef0c88
Author: Nischal1313 <nischaga@metropolia.fi>
Date:   Tue Oct 7 10:11:30 2025 +0300

    BACKUP: Current state before rollback

diff --git a/src/rotaryEncoder.h b/src/rotaryEncoder.h
index 9b6ff28..ed025fc 100644
--- a/src/rotaryEncoder.h
+++ b/src/rotaryEncoder.h
@@ -5,26 +5,51 @@
 #include "hardware/gpio.h"
 #include "FreeRTOS.h"
 #include "task.h"
+#include "semphr.h"
 #include "eeprom.h"
 
 class RotaryEncoder {
 public:
-    RotaryEncoder();
+    RotaryEncoder(Eeprom& eeprom, SemaphoreHandle_t eepromMutex);
+
+    // Current desired CO2 value (updated by encoder when rotationEnabled == true)
     int currentRotationValue() const;
+
+    // Button press (encoder push). Returns true once per press (clears flag).
+    bool consumeButtonPress();
+
+    // Consume accumulated rotation events (CW +1, CCW -1).
+    // This returns the integer count and clears the accumulator atomically.
+    int consumeRotationDelta();
+
+    // Enable/disable updating the CO2 value from rotation.
+    void setRotationEnabled(bool on);
+
+    // FreeRTOS task that polls encoder pins and accumulates deltas / updates CO2.
     static void encoderTask(void* pv);
 
 private:
     static constexpr uint8_t PIN_A = 10;
     static constexpr uint8_t PIN_B = 11;
-    static constexpr uint8_t EEPROM_ADDR = 0x00;
-    static constexpr uint16_t EEPROM_CO2_ADDR = 0x10;
+    static constexpr uint8_t PIN_BTN = 12;
+    static constexpr uint16_t EEPROM_CO2_ADDR = 0x0010;
 
+    // Encoded state
     int desiredCO2;
-    int lastA, lastB;
-    Eeprom eeprom;
+    int lastA;
+    int lastB;
+
+    // EEPROM for persistent storage of the CO2 setpoint (shared reference)
+    Eeprom& eeprom;
+    SemaphoreHandle_t eepromMutex;
+
+    // Shared state protected by critical sections
+    static volatile int rotationAccum;      // +1 / -1 counts
+    static volatile bool buttonPressedFlag; // set when pressed
+    static volatile bool rotationEnabled;   // when false, only accumulate but don't update CO2
 
     void readFromEEPROM();
     void writeToEEPROM();
 };
 
-#endif
+#endif // ROTARY_ENCODER_H
