// Minimal Arduino API for compiling the library on the host (tests only)
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef bool boolean;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define RISING 3
#define HEX 16
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t level);
int digitalRead(uint8_t pin);
int digitalPinToInterrupt(int pin);
void attachInterrupt(int irq, void (*fn)(), int mode);
void detachInterrupt(int irq);
void noInterrupts();
void interrupts();

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))

class Stream {
public:
    void print(const __FlashStringHelper* s) { fputs(reinterpret_cast<const char*>(s), stdout); }
    void print(char c) { putchar(c); }
    void print(unsigned long v, int base) { printf(base == HEX ? "%lX" : "%lu", v); }
    void print(unsigned int v, int base) { print((unsigned long)v, base); }
    void print(unsigned char v, int base) { print((unsigned long)v, base); }
    void println(unsigned long v, int base) { print(v, base); putchar('\n'); }
};
extern Stream Serial;
