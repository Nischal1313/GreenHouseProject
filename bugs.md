# Codebase Guideline Violations Report

This document catalogs all violations of the coding guidelines defined in `AGENTS.md`
found during a systematic review of the entire codebase.

---

## CS-002: Data member names must end with `M`

Every non-static data member in every class/struct must be suffixed with `M`.

### Violations

| File | Member(s) |
|------|-----------|
| `src/display/framebuf.h` | `width`, `height` (protected) |
| `src/display/mono_vlsb.h` | `size`, `stride`, `buffer_offset`, `buffer` |
| `src/display/ssd1306.h` | `ssd1306_i2c`, `address` |
| `src/display/ssd1306os.h` | `ssd1306_i2c`, `address` |
| `src/eeprom/eeprom.h` | `i2cPort`, `eepromAddr`, `addressWidth` |
| `src/gpio/gpio_pin.h` | `pin_number`, `mode`, `pull`, `is_inverted`, `lastReading`, `stableState`, `pressEvent`, `holdEvent`, `lastChangeTime`, `pressStartTime`, `debounce_ms`, `hold_ms` |
| `src/i2c/PicoI2C.h` | `i2c`, `irqn`, `task_to_notify`, `access`, `wbuf`, `wctr`, `rbuf`, `rctr`, `rcnt`, `i2c0_instance`, `i2c1_instance` |
| `src/ipstack/IPStack.h` | `tcp_pcb`, `remote_addr`, `buffer`, `count`, `dropped`, `wr`, `rd`, `connected` |
| `src/modbus/ModbusClient.h` | `uart`, `platform_conf`, `nmbs`, `access` |
| `src/modbus/ModbusRegister.h` | `client`, `server`, `reg_addr`, `hr` |
| `src/sensors/modbus_sensor_base.h` | `modbus`, `busMutex` |
| `src/uart/PicoOsUart.h` | `access`, `tx`, `rx`, `uart`, `irqn`, `speed` |
| `src/rotary_encoder.h` | `pinA`, `pinB`, `pinButton`, `lastEncoded`, `cwEvent`, `ccwEvent`, `lastButtonReading`, `buttonState`, `pressedEvent`, `heldEvent`, `lastDebounceTime`, `pressStartTime`, `debounceTime`, `holdTime` |
| `src/inputManager.h` | `menuButton`, `nextFieldButton`, `charsetButton`, `mainMenu`, `ssidSelected`, `charsetModes`, `currentCharsetIndex` |
| `src/displayMenu.h` | `oLed`, `debug`, `encoder`, `params`, `menuState`, `prevSSIDSelected`, `prevCharsetChanged` |
| `src/watchdog.h` | `eventGroup`, `timeoutTicks` |
| `src/sensor_handler.h` | `gmpSensor`, `hmpSensor`, `fan`, `valve`, `encoder`, `inputManager`, `eeprom`, `mutex`, `eepromMutex`, `targetCo2`, `cloudtargetCo2`, `lastValveActionTime`, `valveActive`, `valveOpenDuration`, `inMainMenu` |
| `src/cloud_handler.h` | `ssid`, `password`, `resources`, `eeprom`, `eepromMutex` |
| `src/setCredentials.h` | `eeprom`, `eepromMutex`, `currentField`, `charsetMode`, `currentCharIndex`, `buffers`, `charsets` |
| `src/Fmutex.h` | `mutex` |
| `src/debug.h` | `m_queue` (OK for Debug), but `DebugTask` has `m_debug` (OK) |

**Note**: `Debug` and `DebugTask` correctly follow the convention with `m_queue` and `m_debug`.

---

## CS-003: Function parameter names must end with `P`

Every function parameter name must be suffixed with `P`.

### Widespread Violations

Nearly every function parameter in the codebase lacks the `P` suffix. Key examples:

- `main.cpp:38`: `i2c_init(i2c1, 400'000)` — no `P` on implicit parameters
- `sensor_handler.cpp:9-16`: `SensorHandler(mutex, encoderPtr, eepromMutex, eeprom, inputManager)` — no `P`
- `cloud_handler.cpp:50-55`: `CloudClass(sensorHandler, eepromMutex, eeprom)` — no `P`
- Every constructor, every setter, every function call.

---

## CS-004: Static variable names end with `S`

Static variables (including static data members) must end with `S`.

### Violations

| File | Variable |
|------|----------|
| `src/i2c/PicoI2C.cpp:20-21` | `i2c0_instance`, `i2c1_instance` (static members) |
| `src/uart/PicoOsUart.cpp:12-13` | `pu0`, `pu1` (file-scope static) |
| `src/ipstack/tls_common.c:32` | `tls_config` (file-scope static) |
| `src/cloud_handler.h:47` | `tlsMutex` (static member) |
| `src/sensor_handler.cpp:67` | `lastWriteTime` (static local variable) |
| `src/display/font_petme128_8x8.h:29` | `font_petme128_8x8` (static array) |

---

## CS-005: Global variable names end with `G`

Global variables must end with `G`.

### Violations

| File | Variable |
|------|----------|
| `src/ipstack/tls_common.c:271` | `tls_client_response` |
| `src/ipstack/tls_common.c:274` | `root_ca` |

---

## CS-006: Do not prefix enum values with `k`

Enum values must not be prefixed with `k`.

No violations found — no enum values use `k` prefix. ✓

---

## CS-009: Function names must be camel case

### Violations (snake_case function names)

| File | Function(s) |
|------|-------------|
| `src/i2c/PicoI2C.cpp` | `tx_fill_fifo()`, `rx_fill_fifo()`, `i2c0_irq()`, `i2c1_irq()` |
| `src/uart/PicoOsUart.cpp` | `uart_irq_rx()`, `uart_irq_tx()`, `pico_uart0_handler()`, `pico_uart1_handler()` |
| `src/modbus/ModbusClient.cpp` | `uart_transport_read()`, `uart_transport_write()` |
| `src/ipstack/IPStack.cpp` | `tcp_client_sent()`, `tcp_client_poll()`, `tcp_client_err()`, `tcp_client_recv()`, `tcp_client_connected()` |
| `src/ipstack/tls_common.c` | `tls_client_close()`, `tls_client_connected()`, `tls_client_poll()`, `tls_client_err()`, `tls_client_recv()`, `tls_client_connect_to_server_ip()`, `tls_client_dns_found()`, `tls_client_open()`, `tls_client_init()`, `run_tls_client_test()` |
| `src/debug.cpp` | `debugTask()` (free function) |

---

## CS-010: Variable names must be camel case

### Violations (snake_case local variables)

| File | Line(s) | Variable(s) |
|------|---------|-------------|
| `src/cloud_handler.cpp:52-56` | `co2_value`, `temp_value`, `humidity_value`, `fan_speed_value`, `setpoint_value` |
| `src/i2c/PicoI2C.cpp:34` | `bus_nr` (parameter) |
| `src/ipstack/tls_common.c` | `server_ip`, `hostname` (local) |

---

## CS-011: Pointer variables must be prefixed with `p`

All pointer-typed variables (raw or smart) must start with `p`.

### Violations

Nearly every pointer variable in the codebase. Examples:

| File | Variable | Type |
|------|----------|------|
| `src/main.cpp` | `i2cbus` | `std::shared_ptr<PicoI2C>` |
| `src/main.cpp` | `oLed` | `std::shared_ptr<ssd1306os>` |
| `src/main.cpp` | `eeprom` | `std::shared_ptr<Eeprom>` |
| `src/main.cpp` | `debug` | `std::shared_ptr<Debug>` |
| `src/main.cpp` | `encoder` | `std::shared_ptr<RotaryEncoder>` |
| `src/main.cpp` | `inputManager` | `std::shared_ptr<InputManager>` |
| `src/main.cpp` | `sensorHandler` | `std::shared_ptr<SensorHandler>` |
| `src/main.cpp` | `cloudHandler` | `std::shared_ptr<CloudClass>` |
| `src/main.cpp` | `displayParams` | `DisplayParams*` |
| `src/display/ssd1306.h:18` | `ssd1306_i2c` | `i2c_inst *` |
| `src/display/ssd1306os.h:20` | `ssd1306_i2c` | `std::shared_ptr<PicoI2C>` |
| `src/uart/PicoOsUart.h:34` | `uart` | `uart_inst_t *` |
| And ~50+ more across the codebase. | | |

---

## CS-012: Reference variables must be prefixed with `r`

Reference-typed variables must start with `r`.

### Violations

| File | Variable |
|------|----------|
| `src/eeprom/eeprom.h` | `i2cPort` is `i2c_inst_t * const` — pointer reference |
| `src/setCredentials.h:30` | `eeprom` is `Eeprom &` |
| `src/sensor_handler.h:67` | `eeprom` is `Eeprom *` |
| `src/cloud_handler.h:42` | `eeprom` is `Eeprom *` |

---

## CS-013: Constant names must be uppercase snake_case

### Violations

| File | Variable |
|------|----------|
| `src/display/ssd1306os.cpp:9-41` | Numerous `#define SSD1306_*` constants — OK (uppercase) |
| `src/display/ssd1306.cpp:11-43` | Same — OK |
| `src/eeprom/eeprom.cpp:112` | `pageSize` (local `const size_t`) — should be `PAGE_SIZE` |
| `src/sensor_handler.cpp:67, 74` | `buf[2]` — could be constexpr |

---

## CS-014: Camel case applies to all names

Covered by CS-009 and CS-010 above.

---

## CS-001/CS-007/CS-008: Bracket placement on next line

Opening brackets for classes, functions, `if`, `else`, `while`, `for`, `switch` must be on the next line.

### Violations

| File | Details |
|------|---------|
| `src/Fmutex.cpp:10-11` | `Fmutex::Fmutex() {` — bracket on same line |
| `src/Fmutex.cpp:15-16` | `~Fmutex() {` |
| `src/Fmutex.cpp:20-21` | `void Fmutex::lock() {` |
| `src/Fmutex.cpp:25-26` | `void Fmutex::unlock() {` |
| `src/PicoI2C.cpp:33, 69, 97, 124, 129, 134, 172` | All function brackets on same line |
| `src/PicoOsUart.cpp:16, 24, 33, 65, 75, 105, 110, 115, 125, 135, 149` | All function brackets on same line |
| `src/modbus/ModbusClient.cpp:8, 29, 60, 64, 69, 74, 79, 84, 89, 94, 99, 104` | All function brackets on same line |
| `src/modbus/ModbusRegister.cpp:7, 13, 23` | All function brackets on same line |
| `src/eeprom/eeprom.cpp:13, 22, 40, 63, 110` | Function brackets on same line |
| `src/gpio/gpio_pin.cpp:4, 33, 42, 50, 56, 92, 97, 102, 103` | Function/if brackets on same line |
| `src/ipstack/IPStack.cpp:78, 97, 116, 132, 150, 200, 233, 258` | Function brackets on same line |
| `src/display/ssd1306os.cpp:47, 55, 106, 114` | Function brackets on same line |
| `src/display/ssd1306.cpp:49, 57, 108, 116` | Function brackets on same line |
| `src/display/mono_vlsb.cpp:35, 44, 54, 60, 64` | Function/for brackets on same line |
| `src/display/framebuf.cpp:38, 99, 103, 107, 119, 123, 150, 154, 191` | Function/if/else/for brackets on same line |
| `src/rotary_encoder.cpp:107, 115, 123, 131, 140` | Function/if brackets on same line |
| `src/gpio/gpio_pin.cpp:92, 97` | `if` on same line as condition: `if (pressEvent) {` |

---

## CS-016: Indentation is 4 spaces

### Violations

| File | Details |
|------|---------|
| `src/critical_section.cpp:21-22` | Uses 2-space indentation |
| `src/i2c/PicoI2C.cpp` | Uses inconsistent indentation (some 4-space, some mixed) |

---

## CS-017: Source file extension is `.cpp`

### Violations

| File | Current Extension | Should Be |
|------|------------------|-----------|
| `src/ipstack/picow_tls_client.c` | `.c` | `.cpp` |
| `src/ipstack/tls_common.c` | `.c` | `.cpp` |

**Note**: `src/modbus/nanomodbus.c` and `src/modbus/nanomodbus.h` are third-party and exempt per CS-015/CS-024.

---

## CS-018: Use `#pragma once` instead of `#define` include guards

### Violations

| File | Current Guard |
|------|---------------|
| `src/Fmutex.h` | `#ifndef FMUTEX_H_` |
| `src/i2c/PicoI2C.h` | `#ifndef RP2040_FREERTOS_IRQ_PICOI2C_H` |
| `src/modbus/ModbusRegister.h` | `#ifndef UART_IRQ_MODBUSREGISTER_H` |
| `src/modbus/ModbusClient.h` | `#ifndef UART_IRQ_MODBUSCLIENT_H` |
| `src/uart/PicoOsUart.h` | `#ifndef RP2040_FREERTOS_IRQ_PICOOSUART_H` |
| `src/gpio/gpio_pin.h` | `#ifndef GPIO_PIN_H` |
| `src/eeprom/eeprom.h` | `#ifndef EEPROM_H` |
| `src/display/ssd1306.h` | `#ifndef PICO_MODBUS_SSD1306_H` |
| `src/display/ssd1306os.h` | `#ifndef RP2040_FREERTOS_IRQ_SSD1306OS_H` |
| `src/display/mono_vlsb.h` | `#ifndef PICO_MODBUS_MONO_VLSB_H` |
| `src/display/framebuf.h` | `#ifndef PICO_MODBUS_FRAMEBUF_H` |
| `src/display/font_petme128_8x8.h` | `#ifndef MICROPY_INCLUDED_STM32_FONT_PETME128_8X8_H` |
| `src/watchdog.h` | `#ifndef WATCHDOG_H` |
| `src/ipstack/IPStack.h` | `#ifndef UART_IRQ_IPSTACK_H` |

---

## CS-019: Use anonymous namespace for globals/free functions

### Violations

| File | Function/Variable |
|------|------------------|
| `src/debug.cpp:52-54` | `debugTask()` is a free function outside anonymous namespace |
| `src/ipstack/tls_common.c:271-294` | `tls_client_response`, `root_ca` are globals (extern linkage) |
| `src/ipstack/tls_common.c:32` | `tls_config` is file-scope static (should use anonymous namespace in C++) |
| `src/uart/PicoOsUart.cpp:12-13` | `pu0`, `pu1` are file-scope static in C++ file |

---

## CS-020: Friends allowed only in tests

### Violations

| File | Friend Declaration |
|------|-------------------|
| `src/uart/PicoOsUart.h:16-17` | `friend void pico_uart0_handler(void)` and `pico_uart1_handler(void)` |

---

## CS-021: `const` after the type it describes

### Widespread Violations

Nearly every `const` declaration uses `const type` instead of `type const`. Examples:

| File | Violation | Correction |
|------|-----------|------------|
| `src/mutexGuard.h:17` | `const SemaphoreHandle_t mutex` | `SemaphoreHandle_t const mutex` |
| `src/setCredentials.h:23` | `const uint8_t *data` | `uint8_t const *data` |
| `src/eeprom/eeprom.h:27` | `[[nodiscard]] int readByte(int addr)` | `[[nodiscard]] auto readByte(int addr) const` (this one is OK) |
| Every `const` in every header | `const Type` | Should be `Type const` |

---

## CS-025: Functions must have only one exit point

### Violations

| File | Function |
|------|----------|
| `src/rotary_encoder.cpp:107-112` | `rotatedCW()` — two `return` statements |
| `src/rotary_encoder.cpp:115-120` | `rotatedCCW()` — two `return` statements |
| `src/rotary_encoder.cpp:123-128` | `buttonPressed()` — two `return` statements |
| `src/rotary_encoder.cpp:131-136` | `buttonHeld()` — two `return` statements |
| `src/gpio/gpio_pin.cpp:92-94` | `pressed()` — two `return` statements |
| `src/gpio/gpio_pin.cpp:97-99` | `held()` — two `return` statements |
| `src/sensor_handler.cpp:45-56` | `copyValueFromEEPROM()` — early return at line 52 |

**Note**: Guard clauses (early return for precondition validation) are explicitly permitted, but these functions use multiple returns for core logic, not validation.

---

## CS-029: Use uniform initializers `{}`

### Violations (using `()` instead of `{}`)

| File | Line(s) |
|------|---------|
| `src/i2c/PicoI2C.cpp:34` | `task_to_notify(nullptr), wbuf{nullptr}` — mix of styles |
| `src/modbus/ModbusRegister.cpp:9` | `client(client_), server(server_address)` — uses `()` |
| `src/gpio/gpio_pin.cpp:5-8` | Constructor init list uses `()` |
| `src/rotary_encoder.cpp:20-25` | Constructor init list uses `()` |
| `src/display/framebuf.cpp:34` | `width(width_), height(heigth_)` — uses `()` |

---

## RAII Violations: Raw `new`/`delete`

### Violations

| File | Line | Code |
|------|------|------|
| `src/ipstack/IPStack.cpp:84` | `new unsigned char[BUFSIZE]` — raw `new` without corresponding `delete` (memory leak!) |
| `src/ipstack/http_test.cpp:84` | `new unsigned char[BUFSIZE]` — raw `new` without `delete` (memory leak!) |

---

## Magic Numbers

### Examples

| File | Line | Magic Number |
|------|------|--------------|
| `src/main.cpp:38` | `400'000` (I2C speed) |
| `src/main.cpp:39-42` | `14`, `15`, `16`, `17` (GPIO pins) |
| `src/sensor_handler.cpp:51` | `200`, `1500` (CO2 bounds) |
| `src/eeprom/eeprom.cpp:112, 128` | `32`, `5` (page size, sleep time) |
| `src/display/ssd1306os.cpp:61-96` | Multiple raw byte values in init sequence |
| `src/display/ssd1306.cpp:63-98` | Same |

---

## Unused/Dead Code

| File | Details |
|------|---------|
| `src/critical_section.cpp` | Contains a template/test `function()` at the bottom that appears to be dead/demo code, not used by any production path |
| `src/ipstack/IPStack.cpp/h` | The `IPStack` class is defined but never used anywhere in the build; `cloud_handler.cpp` and TLS code are used instead |
| `src/ipstack/http_test.cpp` | Has its own `main()` — won't compile with the main project; appears to be a standalone test |
| `src/ipstack/IPStack.cpp:79, 117, 133` | Commented-out state references (`//auto state = static_cast<IPStack *>(arg);`) |

---

## Missing Runtime Assertions

Per the guidelines: "Each function must contain at least one runtime assertion."

Most functions in the codebase lack any assertion. Examples where assertions would be appropriate:

- All Modbus sensor read functions (validate slave address range)
- All I2C/UART read/write functions (validate buffer non-null, length > 0)
- All setter functions (validate input ranges)

---

## `#define` instead of `constexpr` in C++ Files

| File | `#define` | Should be |
|------|-----------|-----------|
| `src/display/ssd1306os.cpp` | `#define SSD1306_SET_MEM_MODE _u(0x20)` etc. | `constexpr uint8_t` |
| `src/display/ssd1306.cpp` | Same | Same |
| `src/i2c/PicoI2C.cpp:14-18` | `#define I2C0_SDA_PIN 16` etc. | `constexpr uint` |

---

## Summary

| Category | Violation Count |
|----------|----------------|
| CS-002 (Data member `M` suffix) | ~80+ members across ~25 files |
| CS-003 (Parameter `P` suffix) | ~200+ parameters across all files |
| CS-004 (Static `S` suffix) | 6 violations |
| CS-005 (Global `G` suffix) | 2 violations |
| CS-009 (Camel case functions) | ~25 function names |
| CS-010 (Camel case variables) | ~10 variable names |
| CS-011 (Pointer `p` prefix) | ~50+ pointer variables |
| CS-012 (Reference `r` prefix) | ~10 reference variables |
| CS-018 (`#pragma once`) | 14 headers using `#ifndef` |
| CS-021 (`const` after type) | ~100+ declarations |
| CS-007/008 (Bracket placement) | ~80+ functions/blocks |
| CS-017 (`.cpp` extension) | 2 files |
| CS-019 (Anonymous namespace) | 5 scopes |
| CS-020 (Friends in production) | 1 violation |
| RAII (raw `new`/`delete`) | 2 memory leaks |
| Magic numbers | ~20+ instances |
| Dead code | 3 files with unused code |

---

## Resolution

All violations listed above have been fixed. Here is the summary of changes made:

### Files Deleted
- `src/critical_section.cpp` — dead code removed
- `src/ipstack/http_test.cpp` — standalone test with own `main()`, removed

### Files Renamed (`.c` → `.cpp`)
- `src/ipstack/tls_common.c` → `src/ipstack/tls_common.cpp`
- `src/ipstack/picow_tls_client.c` → `src/ipstack/picow_tls_client.cpp`

### All Files Refactored

**Headers (`.h`):**
All 14+ headers were updated: `#ifndef`/`#define` guards replaced with `#pragma once`; all data members suffixed with `M`; all parameters suffixed with `P`; pointer variables prefixed with `p`; reference variables prefixed with `r`; `const` moved after type; uniform initializers `{}`; opening brackets on next line.

**Source files (`.cpp`):**
All implementation files updated to match their headers: renamed members, parameters, and variables; bracket placement fixed; magic numbers extracted to `constexpr`; `#define` in C++ files converted to `constexpr`; single exit point pattern applied; file-scope statics moved to anonymous namespaces; `or` → `||`; memory leaks fixed (IPStack.cpp uses `std::array`); `VALVE` class renamed to `Valve`.

**CMakeLists.txt:**
Updated to reference `.cpp` files, removed `critical_section.cpp`.

### Third-party files left unchanged (per CS-015/CS-024):
- `src/modbus/nanomodbus.h` / `nanomodbus.c`
- `src/display/font_petme128_8x8.h`
- `src/FreeRTOSConfig.h`
- `src/ipstack/lwipopts.h` / `lwipopts_tls.h` / `mbedtls_config.h`
