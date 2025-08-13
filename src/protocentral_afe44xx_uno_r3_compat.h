//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino Uno R3 Compatibility Fixes for AFE4490 Library
//    Optimizations for low memory and SPI timing issues
//
//    Copyright (c) 2018 ProtoCentral
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
/////////////////////////////////////////////////////////////////////////////////////////

#ifndef PROTOCENTRAL_AFE44XX_UNO_R3_COMPAT_H
#define PROTOCENTRAL_AFE44XX_UNO_R3_COMPAT_H

#include "Arduino.h"

// Platform detection for memory optimization
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  #define ARDUINO_UNO_R3_COMPAT
  #define USE_REDUCED_BUFFERS
  #define DISABLE_DYNAMIC_ALLOCATION
  #define USE_CONSERVATIVE_SPI_SPEED
  #define ENABLE_MEMORY_DEBUGGING
#endif

// Memory optimizations for Arduino Uno R3
#ifdef USE_REDUCED_BUFFERS
  // Reduced buffer sizes for Uno R3 (50 samples instead of 100)
  #define AFE44XX_BUFFER_SIZE 50
  #define AFE44XX_PPG_BUFFER_SIZE 25
  #define AFE44XX_RATE_HISTORY_SIZE 4
#else
  // Full buffer sizes for other platforms
  #define AFE44XX_BUFFER_SIZE 100
  #define AFE44XX_PPG_BUFFER_SIZE 50
  #define AFE44XX_RATE_HISTORY_SIZE 8
#endif

// SPI speed optimizations
#ifdef USE_CONSERVATIVE_SPI_SPEED
  #define AFE44XX_SPI_SPEED 1000000   // 1MHz for Uno R3 (more conservative)
#else
  #define AFE44XX_SPI_SPEED 2000000   // 2MHz for other platforms
#endif

// Memory debugging macros
#ifdef ENABLE_MEMORY_DEBUGGING
  #define DEBUG_MEMORY_USAGE() debugMemoryUsage()
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_MEMORY_USAGE()
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// Function to get free memory (AVR specific)
#ifdef ARDUINO_UNO_R3_COMPAT
extern "C" char __heap_start, *__brkval;
inline int getFreeMemory() {
  char top;
  return __brkval ? &top - __brkval : &top - &__heap_start;
}
#else
inline int getFreeMemory() {
  return -1; // Not implemented for other platforms
}
#endif

// Memory debugging function
inline void debugMemoryUsage() {
#ifdef ENABLE_MEMORY_DEBUGGING
  int freeMemory = getFreeMemory();
  DEBUG_PRINT("Free memory: ");
  DEBUG_PRINT(freeMemory);
  DEBUG_PRINTLN(" bytes");
  
  if (freeMemory < 200) {
    DEBUG_PRINTLN("WARNING: Low memory!");
  }
#endif
}

// Memory-safe allocation macros
#ifdef DISABLE_DYNAMIC_ALLOCATION
  #define SAFE_MALLOC(size) nullptr
  #define SAFE_FREE(ptr) do { if(ptr) { free(ptr); ptr = nullptr; } } while(0)
  #define USE_STATIC_ALLOCATION
#else
  #define SAFE_MALLOC(size) malloc(size)
  #define SAFE_FREE(ptr) do { if(ptr) { free(ptr); ptr = nullptr; } } while(0)
#endif

// Compile-time assertions for memory limits
#ifdef ARDUINO_UNO_R3_COMPAT
static_assert(AFE44XX_BUFFER_SIZE <= 50, "Buffer too large for Arduino Uno R3");
static_assert(AFE44XX_PPG_BUFFER_SIZE <= 25, "PPG buffer too large for Arduino Uno R3");
#endif

#endif // PROTOCENTRAL_AFE44XX_UNO_R3_COMPAT_H
