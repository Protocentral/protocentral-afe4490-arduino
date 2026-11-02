#include "SPI.h"

Stream Serial;
SPIClass SPI;

static uint32_t nowUs = 0;
uint32_t millis() { return nowUs / 1000; }
uint32_t micros() { return nowUs += 7; }  // time moves a little on every call
void delay(uint32_t ms) { nowUs += ms * 1000; }
void delayMicroseconds(uint32_t us) { nowUs += us; }
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t pin, uint8_t level) {
    if (pin == 7) fake::csEdge(level == LOW);  // tests use CS = 7
}
int digitalRead(uint8_t) { return LOW; }
int digitalPinToInterrupt(int pin) { return pin == 2 ? 0 : -1; }
void attachInterrupt(int, void (*)(), int) {}
void detachInterrupt(int) {}
void noInterrupts() {}
void interrupts() {}

namespace fake {
uint32_t regs[0x31];
bool spiRead = false;
bool absent = false;
static int byteIdx = 0;
static uint8_t addr = 0;
static uint32_t data = 0;

void reset() {
    memset(regs, 0, sizeof(regs));
    spiRead = false;
}

void csEdge(bool low) {
    if (low) {
        byteIdx = 0;
        data = 0;
        return;
    }
    if (byteIdx != 4 || absent) return;
    if (addr == 0) {
        spiRead = data & 1;
        if (data & 8) reset();  // SW_RST
    } else if (!spiRead && addr < 0x2A) {  // with SPI_READ set every transaction is a read
        regs[addr] = data & 0xFFFFFF;
    }
}
}  // namespace fake

uint8_t SPIClass::transfer(uint8_t b) {
    using namespace fake;
    uint8_t out = 0;
    if (byteIdx == 0) {
        addr = b;
    } else {
        int shift = 8 * (3 - byteIdx);
        data |= (uint32_t)b << shift;
        if (!absent && spiRead && addr != 0 && addr <= 0x30) out = (uint8_t)(regs[addr] >> shift);
    }
    byteIdx++;
    return out;
}
