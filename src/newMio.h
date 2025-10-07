commit 3a0a39327245733101d898da9931eaccdeef0c88
Author: Nischal1313 <nischaga@metropolia.fi>
Date:   Tue Oct 7 10:11:30 2025 +0300

    BACKUP: Current state before rollback

diff --git a/src/produalMIO.h b/src/produalMIO.h
index c638bda..8f032ff 100644
--- a/src/produalMIO.h
+++ b/src/produalMIO.h
@@ -32,7 +32,6 @@ private:
     RELAYCONTROL valve;
     uint8_t slaveAddress;
     SemaphoreHandle_t busMutex;
-    RotaryEncoder encoder;
 };
 
 #endif
