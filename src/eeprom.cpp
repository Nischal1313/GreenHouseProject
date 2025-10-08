// #include "eeprom.h"
// #include "pico/stdlib.h"
//
// Eeprom::Eeprom(i2c_inst_t *i2cPort, uint8_t eepromAddr, uint8_t addressWidth)
//     : i2cPort(i2cPort), eepromAddr(eepromAddr), addressWidth(addressWidth) {}
//
// /**
//  * @brief Helper: convert 16-bit/8-bit address into bytes
//  */
// void Eeprom::buildAddressBytes(int addr, uint8_t *out) const {
//     if (addressWidth == 2) {
//         out[0] = static_cast<uint8_t>((addr >> 8) & 0xFF);
//         out[1] = static_cast<uint8_t>(addr & 0xFF);
//     } else {
//         out[0] = static_cast<uint8_t>(addr & 0xFF);
//     }
// }
//
// int Eeprom::readByte(const int addr) const {
//     uint8_t addrBuf[2];
//     buildAddressBytes(addr, addrBuf);
//
//     uint8_t data = 0;
//     int written = i2c_write_blocking(i2cPort, eepromAddr, addrBuf, addressWidth, true);
//     if (written != addressWidth) {
//         return -1; // Failed to send address
//     }
//
//     int read = i2c_read_blocking(i2cPort, eepromAddr, &data, 1, false);
//     if (read != 1) {
//         return -1; // Failed to read
//     }
//
//     return data;
// }
//
// bool Eeprom::writeByte(const int addr, const uint8_t data) const {
//     int existing = readByte(addr);
//     if (existing == data) {
//         return true; // Skip write if value unchanged
//     }
//
//     uint8_t buf[3];
//     buildAddressBytes(addr, buf);
//     buf[addressWidth] = data;
//
//     int written = i2c_write_blocking(i2cPort, eepromAddr, buf, addressWidth + 1, false);
//     if (written != (addressWidth + 1)) {
//         return false;
//     }
//
//     // Wait until EEPROM is ready (poll ACK instead of fixed delay)
//     while (i2c_write_blocking(i2cPort, eepromAddr, buf, addressWidth, true) < 0) {
//         sleep_ms(1);
//     }
//
//     return true;
// }
//
// bool Eeprom::readBlock(const int addr, uint8_t *buffer, const size_t length) const {
//     uint8_t addrBuf[2];
//     buildAddressBytes(addr, addrBuf);
//
//     if (i2c_write_blocking(i2cPort, eepromAddr, addrBuf, addressWidth, true) != addressWidth) {
//         return false;
//     }
//
//     return (i2c_read_blocking(i2cPort, eepromAddr, buffer, length, false) == (int)length);
// }
//
//
// bool Eeprom::writeBlock(int addr, const uint8_t *buffer, size_t length) {
//     // We assume the caller (RotaryEncoder) is only writing 2 bytes (CO2 value),
//     // which fits within a single page write.
//
//     // Calculate the necessary buffer size: 2 address bytes + data length
//     const size_t i2c_buffer_size = addressWidth + length;
//     uint8_t i2c_buffer[4]; // Max size is 2 (addr) + 2 (data) = 4
//
//     if (i2c_buffer_size > sizeof(i2c_buffer)) {
//         // Data length exceeds the safe internal buffer size
//         return false;
//     }
//
//     // 1. Build the address bytes at the start of the temporary I2C buffer
//     buildAddressBytes(addr, i2c_buffer);
//
//     // 2. Copy the data block after the address
//     for (size_t i = 0; i < length; ++i) {
//         i2c_buffer[addressWidth + i] = buffer[i];
//     }
//
//     // 3. Write address and data in one go
//     int written = i2c_write_blocking(i2cPort, eepromAddr, i2c_buffer, i2c_buffer_size, false);
//
//     if (written != (int)i2c_buffer_size) {
//         return false; // Write failed
//     }
//
//     // Wait for the EEPROM write cycle to complete (typically 5ms for 24LCxx series)
//     // This is the simplest way to ensure the write operation finishes.
//     sleep_ms(5);
//
//     return true; // Write succeeded
// }
// eeprom.cpp - Enhanced version with better write reliability and debugging
#include "eeprom.h"
#include "pico/stdlib.h"
#include <cstdio>

Eeprom::Eeprom(i2c_inst_t *i2cPort, uint8_t eepromAddr, uint8_t addressWidth)
    : i2cPort(i2cPort), eepromAddr(eepromAddr), addressWidth(addressWidth) {
    printf("[EEPROM] Initialized at address 0x%02X with %d-byte addressing\n",
           eepromAddr, addressWidth);
}

/**
 * @brief Helper: convert 16-bit/8-bit address into bytes
 */
void Eeprom::buildAddressBytes(int addr, uint8_t *out) const {
    if (addressWidth == 2) {
        out[0] = static_cast<uint8_t>((addr >> 8) & 0xFF);
        out[1] = static_cast<uint8_t>(addr & 0xFF);
    } else {
        out[0] = static_cast<uint8_t>(addr & 0xFF);
    }
}

int Eeprom::readByte(const int addr) const {
    uint8_t addrBuf[2];
    buildAddressBytes(addr, addrBuf);

    uint8_t data = 0;
    int written = i2c_write_blocking(i2cPort, eepromAddr, addrBuf, addressWidth, true);
    if (written != addressWidth) {
        printf("[EEPROM] readByte: Failed to send address 0x%04X\n", addr);
        return -1; // Failed to send address
    }

    int read = i2c_read_blocking(i2cPort, eepromAddr, &data, 1, false);
    if (read != 1) {
        printf("[EEPROM] readByte: Failed to read from address 0x%04X\n", addr);
        return -1; // Failed to read
    }

    return data;
}

bool Eeprom::writeByte(const int addr, const uint8_t data) const {
    // Read current value first
    int existing = readByte(addr);
    if (existing == data) {
        printf("[EEPROM] writeByte: Skipping write to 0x%04X (value unchanged: 0x%02X)\n",
               addr, data);
        return true; // Skip write if value unchanged
    }

    printf("[EEPROM] writeByte: Writing 0x%02X to address 0x%04X (was 0x%02X)\n",
           data, addr, existing);

    uint8_t buf[3];
    buildAddressBytes(addr, buf);
    buf[addressWidth] = data;

    int written = i2c_write_blocking(i2cPort, eepromAddr, buf, addressWidth + 1, false);
    if (written != (addressWidth + 1)) {
        printf("[EEPROM] writeByte: Failed to write to 0x%04X\n", addr);
        return false;
    }

    // Wait until EEPROM is ready (poll ACK instead of fixed delay)
    int retries = 0;
    while (i2c_write_blocking(i2cPort, eepromAddr, buf, addressWidth, true) < 0) {
        sleep_ms(1);
        retries++;
        if (retries > 20) {  // Max 20ms wait
            printf("[EEPROM] writeByte: Timeout waiting for EEPROM ready\n");
            return false;
        }
    }

    printf("[EEPROM] writeByte: Write completed in %dms\n", retries);
    return true;
}

bool Eeprom::readBlock(const int addr, uint8_t *buffer, const size_t length) const {
    printf("[EEPROM] readBlock: Reading %zu bytes from address 0x%04X\n", length, addr);

    uint8_t addrBuf[2];
    buildAddressBytes(addr, addrBuf);

    if (i2c_write_blocking(i2cPort, eepromAddr, addrBuf, addressWidth, true) != addressWidth) {
        printf("[EEPROM] readBlock: Failed to set address\n");
        return false;
    }

    int bytesRead = i2c_read_blocking(i2cPort, eepromAddr, buffer, length, false);
    if (bytesRead != (int)length) {
        printf("[EEPROM] readBlock: Read %d bytes instead of %zu\n", bytesRead, length);
        return false;
    }

    printf("[EEPROM] readBlock: Successfully read %zu bytes\n", length);
    return true;
}

bool Eeprom::writeBlock(int addr, const uint8_t *buffer, size_t length) {

    // Check for page boundary crossing (assuming 32-byte pages for 24LC64)
    const size_t PAGE_SIZE = 32;
    size_t page_offset = addr % PAGE_SIZE;
    size_t space_in_page = PAGE_SIZE - page_offset;

    if (length > space_in_page) {
        printf("[EEPROM] writeBlock: WARNING - Write crosses page boundary!\n");
        printf("[EEPROM]   Address 0x%04X is at offset %zu in page\n", addr, page_offset);
        printf("[EEPROM]   Attempting to write %zu bytes but only %zu bytes fit in page\n",
               length, space_in_page);

        // Split the write into multiple page writes
        size_t bytes_written = 0;
        while (bytes_written < length) {
            size_t chunk_size = ((length - bytes_written) > PAGE_SIZE) ?
                               PAGE_SIZE : (length - bytes_written);

            // Ensure we don't cross page boundary
            size_t current_page_offset = (addr + bytes_written) % PAGE_SIZE;
            if (current_page_offset + chunk_size > PAGE_SIZE) {
                chunk_size = PAGE_SIZE - current_page_offset;
            }

            printf("[EEPROM] Writing chunk: %zu bytes at offset %zu\n",
                   chunk_size, bytes_written);

            // Write this chunk
            if (!writePageAligned(addr + bytes_written,
                                 buffer + bytes_written,
                                 chunk_size)) {
                printf("[EEPROM] writeBlock: Failed to write chunk at offset %zu\n",
                       bytes_written);
                return false;
            }

            bytes_written += chunk_size;
        }

        printf("[EEPROM] writeBlock: Successfully wrote %zu bytes in multiple chunks\n",
               bytes_written);
        return true;
    }

    // Single page write
    return writePageAligned(addr, buffer, length);
}

bool Eeprom::writePageAligned(int addr, const uint8_t *buffer, size_t length) {
    // Calculate the necessary buffer size: address bytes + data length
    const size_t i2c_buffer_size = addressWidth + length;
    uint8_t i2c_buffer[34]; // Max size is 2 (addr) + 32 (max page) = 34

    if (i2c_buffer_size > sizeof(i2c_buffer)) {
        printf("[EEPROM] writePageAligned: Buffer too large (%zu bytes)\n", i2c_buffer_size);
        return false;
    }

    // 1. Build the address bytes at the start of the temporary I2C buffer
    buildAddressBytes(addr, i2c_buffer);

    // 2. Copy the data block after the address
    for (size_t i = 0; i < length; ++i) {
        i2c_buffer[addressWidth + i] = buffer[i];
    }

    // 3. Write address and data in one go
    int written = i2c_write_blocking(i2cPort, eepromAddr, i2c_buffer, i2c_buffer_size, false);

    if (written != (int)i2c_buffer_size) {
        printf("[EEPROM] writePageAligned: Write failed (sent %d of %zu bytes)\n",
               written, i2c_buffer_size);
        return false;
    }

    // Wait for the EEPROM write cycle to complete
    // Use ACK polling for faster response
    int wait_ms = 0;
    uint8_t dummy_addr[2];
    buildAddressBytes(addr, dummy_addr);

    while (wait_ms < 20) {  // Max 20ms timeout
        if (i2c_write_blocking(i2cPort, eepromAddr, dummy_addr, addressWidth, true) >= 0) {
            // EEPROM acknowledged - write is complete
            break;
        }
        sleep_ms(1);
        wait_ms++;
    }

    if (wait_ms >= 20) {
        printf("[EEPROM] writePageAligned: WARNING - Write cycle timeout\n");
    }

    return true;
}

bool Eeprom::verifyWrite(int addr, const uint8_t *expected, size_t length) {
    printf("[EEPROM] verifyWrite: Verifying %zu bytes at address 0x%04X\n", length, addr);

    uint8_t readBuffer[64];  // Temp buffer for verification
    if (length > sizeof(readBuffer)) {
        printf("[EEPROM] verifyWrite: Length too large for verification buffer\n");
        return false;
    }

    if (!readBlock(addr, readBuffer, length)) {
        printf("[EEPROM] verifyWrite: Failed to read back data\n");
        return false;
    }

    bool matches = true;
    for (size_t i = 0; i < length; i++) {
        if (readBuffer[i] != expected[i]) {
            printf("[EEPROM] verifyWrite: Mismatch at offset %zu: wrote 0x%02X, read 0x%02X\n",
                   i, expected[i], readBuffer[i]);
            matches = false;
        }
    }

    if (matches) {
        printf("[EEPROM] verifyWrite: ✓ All %zu bytes verified successfully\n", length);
    } else {
        printf("[EEPROM] verifyWrite: ✗ Verification failed!\n");
    }

    return matches;
}

void Eeprom::dumpMemory(int startAddr, int length) {
    printf("\n[EEPROM] Memory dump from 0x%04X for %d bytes:\n", startAddr, length);
    printf("        00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
    printf("        ------------------------------------------------\n");

    for (int row = 0; row < (length + 15) / 16; row++) {
        printf("0x%04X: ", startAddr + row * 16);

        // Print hex values
        for (int col = 0; col < 16; col++) {
            int addr = startAddr + row * 16 + col;
            if (addr < startAddr + length) {
                int byte = readByte(addr);
                if (byte >= 0) {
                    printf("%02X ", byte);
                } else {
                    printf("?? ");
                }
            } else {
                printf("   ");
            }
        }

        printf(" |");

        // Print ASCII representation
        for (int col = 0; col < 16; col++) {
            int addr = startAddr + row * 16 + col;
            if (addr < startAddr + length) {
                int byte = readByte(addr);
                if (byte >= 32 && byte < 127) {
                    printf("%c", byte);
                } else {
                    printf(".");
                }
            } else {
                printf(" ");
            }
        }

        printf("|\n");
    }
    printf("\n");
}