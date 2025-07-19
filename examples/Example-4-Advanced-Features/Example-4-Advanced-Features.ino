//////////////////////////////////////////////////////////////////////////////////////////
//
//    Advanced AFE44XX Example - Demonstrates all new library features
//    Shows configuration, diagnostics, error handling, and data acquisition
//
//    Copyright (c) 2018 ProtoCentral
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//    For information on how to use, visit https://github.com/Protocentral/protocentral-afe4490-arduino
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_afe44xx.h"

#define AFE44XX_CS_PIN   7
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_DRDY_PIN 2

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN);

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("AFE44XX Advanced Features Demo");
    Serial.println("==============================");
    
    AFE44xxConfig config = AFE44XX::getDefaultConfig();
    config.chipType = AFE44xxChipType::AFE4490;
    config.led1Current = LEDCurrent::CURRENT_75MA;
    config.led2Current = LEDCurrent::CURRENT_75MA;
    config.tiaGain = TIAGain::GAIN_500K;
    config.sampleRate = SampleRate::RATE_500HZ;
    config.averagingFactor = 4;
    config.enableDiagnostics = true;
    
    Serial.println("Initializing AFE44XX...");
    AFE44xxError error = afe44xx.begin(config);
    
    if (error != AFE44xxError::NONE) {
        Serial.print("Initialization failed: ");
        Serial.println(afe44xx.getErrorString(error));
        while (1) delay(1000);
    }
    
    Serial.println("AFE44XX initialized successfully!");
    
    Serial.println("Performing self-test...");
    error = afe44xx.performSelfTest();
    if (error != AFE44xxError::NONE) {
        Serial.print("Self-test failed: ");
        Serial.println(afe44xx.getErrorString(error));
    } else {
        Serial.println("Self-test passed!");
    }
    
    printConfiguration();
    printDiagnostics();
    
    Serial.println("\nStarting data acquisition...");
    Serial.println("Time(ms)\tIR\tRed\tValid\tError");
}

void loop() {
    AFE44xxData data;
    AFE44xxError error = afe44xx.readData(data);
    
    if (error != AFE44xxError::NONE) {
        Serial.print("Read error: ");
        Serial.println(afe44xx.getErrorString(error));
        delay(100);
        return;
    }
    
    if (data.dataValid) {
        Serial.print(data.timestamp);
        Serial.print("\t");
        Serial.print(data.irData);
        Serial.print("\t");
        Serial.print(data.redData);
        Serial.print("\t");
        Serial.print(data.dataValid ? "Y" : "N");
        Serial.print("\t");
        Serial.println(afe44xx.getErrorString(data.lastError));
        
        static unsigned long lastDiagTime = 0;
        if (millis() - lastDiagTime > 5000) {
            printDiagnostics();
            lastDiagTime = millis();
        }
    }
    
    delay(10);
}

void printConfiguration() {
    AFE44xxConfig config;
    afe44xx.getConfig(config);
    
    Serial.println("\nCurrent Configuration:");
    Serial.print("Chip Type: ");
    Serial.println(config.chipType == AFE44xxChipType::AFE4490 ? "AFE4490" : "AFE4400");
    
    Serial.print("LED1 Current: ");
    Serial.print(static_cast<int>(config.led1Current) * 25);
    Serial.println(" mA");
    
    Serial.print("LED2 Current: ");
    Serial.print(static_cast<int>(config.led2Current) * 25);
    Serial.println(" mA");
    
    Serial.print("TIA Gain: ");
    switch (config.tiaGain) {
        case TIAGain::GAIN_10K: Serial.println("10K ohm"); break;
        case TIAGain::GAIN_25K: Serial.println("25K ohm"); break;
        case TIAGain::GAIN_50K: Serial.println("50K ohm"); break;
        case TIAGain::GAIN_100K: Serial.println("100K ohm"); break;
        case TIAGain::GAIN_250K: Serial.println("250K ohm"); break;
        case TIAGain::GAIN_500K: Serial.println("500K ohm"); break;
        case TIAGain::GAIN_1M: Serial.println("1M ohm"); break;
        case TIAGain::GAIN_2M: Serial.println("2M ohm"); break;
    }
    
    Serial.print("Sample Rate: ");
    Serial.print(static_cast<int>(config.sampleRate));
    Serial.println(" Hz");
    
    Serial.print("Averaging Factor: ");
    Serial.println(config.averagingFactor);
    
    Serial.print("Diagnostics: ");
    Serial.println(config.enableDiagnostics ? "Enabled" : "Disabled");
}

void printDiagnostics() {
    AFE44xxDiagnostics diag;
    AFE44xxError error = afe44xx.getDiagnostics(diag);
    
    if (error != AFE44xxError::NONE) {
        Serial.print("Diagnostics read error: ");
        Serial.println(afe44xx.getErrorString(error));
        return;
    }
    
    Serial.println("\nDiagnostics:");
    Serial.print("LED Fault: ");
    Serial.println(diag.ledFault ? "YES" : "NO");
    
    Serial.print("PD Short: ");
    Serial.println(diag.pdShort ? "YES" : "NO");
    
    Serial.print("PD Open: ");
    Serial.println(diag.pdOpen ? "YES" : "NO");
    
    Serial.print("Gain Error: ");
    Serial.println(diag.gainError ? "YES" : "NO");
    
    Serial.print("Ambient Light: ");
    Serial.println(diag.ambientLight);
    
    Serial.print("Temperature: ");
    Serial.print(diag.temperature);
    Serial.println(" °C");
    
    if (diag.ledFault || diag.pdShort || diag.pdOpen || diag.gainError) {
        Serial.println("*** FAULT DETECTED - Consider checking connections ***");
        afe44xx.clearFaults();
    }
}

void demonstrateConfigurationChanges() {
    Serial.println("\nDemonstrating dynamic configuration changes...");
    
    Serial.println("Reducing LED currents...");
    afe44xx.setLEDCurrent(1, LEDCurrent::CURRENT_25MA);
    afe44xx.setLEDCurrent(2, LEDCurrent::CURRENT_25MA);
    delay(2000);
    
    Serial.println("Changing TIA gain...");
    afe44xx.setTIAGain(TIAGain::GAIN_1M);
    delay(2000);
    
    Serial.println("Changing sample rate...");
    afe44xx.setSampleRate(SampleRate::RATE_1000HZ);
    delay(2000);
    
    Serial.println("Restoring original settings...");
    afe44xx.setLEDCurrent(1, LEDCurrent::CURRENT_75MA);
    afe44xx.setLEDCurrent(2, LEDCurrent::CURRENT_75MA);
    afe44xx.setTIAGain(TIAGain::GAIN_500K);
    afe44xx.setSampleRate(SampleRate::RATE_500HZ);
    
    Serial.println("Configuration changes complete!");
}