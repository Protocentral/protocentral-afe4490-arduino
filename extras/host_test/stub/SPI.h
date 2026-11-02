// SPI stub backed by a fake AFE44xx register file (tests only)
#pragma once
#include "Arduino.h"

#define MSBFIRST 1
#define SPI_MODE0 0

struct SPISettings {
    SPISettings(uint32_t, int, int) {}
    SPISettings() {}
};

class SPIClass {
public:
    void begin() {}
    void beginTransaction(SPISettings) {}
    void endTransaction() {}
    uint8_t transfer(uint8_t b);
};
extern SPIClass SPI;

// Fake chip state, inspected by the tests
namespace fake {
extern uint32_t regs[0x31];
extern bool spiRead;
extern bool absent;  // no chip on the bus: SOMI reads 0
void reset();
void csEdge(bool low);
}
