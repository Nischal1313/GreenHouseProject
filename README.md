Ventilation Control System
A FreeRTOS-based embedded system for monitoring indoor air quality and controlling ventilation using multiple sensors and Modbus communication.

📋 System Overview
This system runs on a Raspberry Pi Pico and monitors:

CO₂ levels (GMP252 sensor)

Temperature & Humidity (HMP60 sensor)

Differential Pressure (SDP610 sensor)

Fan Control (Produal MIO actuator)

The system uses a watchdog timer to ensure all sensor tasks are running correctly and provides real-time debugging output.

🏗️ Architecture
Task Structure
Task	Priority	|Interval	 |Purpose
FanTask	High	5 seconds	Controls ventilation fan speed
GmpTask	High	2 seconds	Reads CO₂, compensation temp, measured temp
HmpTask	High	3 seconds	Reads humidity and temperature
PressureTask	High	4 seconds	Reads differential pressure
WatchDog	High	30s timeout	Monitors all sensor tasks
DebugTask	Low	Continuous	Handles debug message output
Sensor Configuration
Sensor	Type	Communication	Address	Parameters
GMP252	CO₂ Sensor	Modbus RTU	240	CO₂, Temperature
HMP60	Humidity/Temp	Modbus RTU	241	Humidity, Temperature
SDP610	Pressure	I²C	0x40	Differential Pressure
Produal MIO	Actuator	Modbus RTU	1	Fan Control
🔧 Hardware Setup
Pin Configuration
UART0: Modbus communication (TX: GP0, RX: GP1)

I²C1: Pressure sensor & OLED (SDA: GP14, SCL: GP15)

Buttons: GP7, GP8, GP9 (for future use)

Power Requirements
3.3V operation

All sensors powered from Pico 3.3V rail

RS-485 transceiver required for Modbus

📁 Code Structure
text
src/
├── main.cpp                 # Main application entry point
├── gmp252.h/cpp            # CO₂ sensor driver
├── hmp60.h/cpp             # Humidity/temperature sensor driver  
├── sdp610.h/cpp            # Pressure sensor driver
├── produalMIO.h/cpp        # Fan control driver
├── ModbusClient.h/cpp      # Modbus communication layer
├── mutexGuard.h            # RAII mutex wrapper
└── PicoOsUart.h/cpp        # UART driver for Pico
🔄 How It Works
1. Initialization
cpp
// Setup communication interfaces
auto uart = std::make_shared<PicoOsUart>(0, 0, 1, 9600, 1, 256, 256);
auto modbus = std::make_shared<ModbusClient>(uart);

// Create sensor objects
GMP252 gmpSensor(modbus, modbusMutex);
HMP60 hmpSensor(modbus, modbusMutex);
SDP610 pressureSensor(i2c1, 14, 15, i2cMutex, 0x40);
ModbusMIO modbusFan(modbus, 1, modbusMutex);
2. Task Execution
Each sensor task follows this pattern:

cpp
[[noreturn]] void sensorTask() {
    while (true) {
        // Read sensor data
        float value = sensor.readValue();
        
        // Handle errors
        if (!std::isnan(value)) {
            // Log successful reading
            debug("Sensor value: %.1f\n", value);
        } else {
            // Log error
            debug("Sensor read failed!\n");
        }
        
        // Notify watchdog
        xEventGroupSetBits(eventGroup, TASK_BIT);
        vTaskDelay(pdMS_TO_TICKS(interval));
    }
}
3. Watchdog Monitoring
The watchdog task ensures all sensors are responsive:

cpp
[[noreturn]] void watchDogTimer() {
    while (true) {
        // Wait for all tasks to report within 30 seconds
        EventBits_t result = xEventGroupWaitBits(eventGroup, ALL_BITS, true, true, 30000);
        
        if (allTasksReported(result)) {
            debug("Watchdog: OK\n");
        } else {
            debug("Watchdog FAIL! Missing tasks...\n");
        }
    }
}
🛠️ Key Features
Thread Safety
Mutex Protection: All shared resources (Modbus, I²C) use mutexes

RAII Pattern: Automatic mutex management with MutexGuard

Staggered Polling: Prevents bus contention with different task intervals

Error Handling
NaN Checking: Invalid readings are detected and logged

Watchdog Timeouts: Stalled tasks are identified

Modbus Error Codes: Communication errors are properly handled

Debugging
Queue-based Logging: Non-blocking debug message system

Timestamped Output: Each message includes tick count

Formatted Output: Support for printf-style formatting

🚀 Building and Flashing
Prerequisites
Raspberry Pi Pico SDK

CMake 3.15+

ARM GCC toolchain

Build Steps
bash
mkdir build
cd build
cmake ..
make
Flashing
Hold BOOTSEL button while connecting Pico to USB

Drag and drop .uf2 file to RPI-RP2 drive

Open serial monitor at 115200 baud to view debug output

📊 Expected Output
When running successfully, you should see:

text
[12345] Program started. Initializing sensors...
[12350] GMP252 - CO₂: 450.5 ppm, Comp T: 22.1°C, Meas T: 22.3°C
[12352] HMP60 - Humidity: 45.2%, Temperature: 22.1°C  
[12355] SDP610 - Pressure: 12.5 Pa
[12356] Fan speed set to 100.0%, fan is running
[12360] Watchdog: OK, 30000 ms since last OK
🔧 Troubleshooting
Common Issues
No Sensor Readings

Check Modbus addresses match sensor configuration

Verify RS-485 wiring and termination

Confirm UART baud rate matches sensors (typically 9600)

Watchdog Timeouts

Check if any task is blocking indefinitely

Verify mutex timeouts are properly handled

Monitor stack usage with FreeRTOS tools

I²C Communication Failures

Confirm pull-up resistors on SDA/SCL lines

Check for address conflicts between devices

Verify I²C speed compatibility

Debugging Tips
Use debug() function for system status messages

Monitor FreeRTOS task states using vTaskList()

Check mutex ownership with uxSemaphoreGetCount()

📈 Future Enhancements
PID Control: Implement closed-loop fan control based on CO₂ levels

Data Logging: SD card storage for historical data

Network Connectivity: Ethernet/WiFi for remote monitoring

Web Interface: Real-time dashboard for system status

Alarm System: Threshold-based alerts for poor air quality

📄 License
This project is licensed under the MIT License - see the LICENSE file for details.

🤝 Contributing
Fork the repository

Create a feature branch

Commit your changes

Push to the branch

Open a Pull Request



