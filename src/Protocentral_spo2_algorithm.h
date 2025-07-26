//////////////////////////////////////////////////////////////////////////////////////////
//
//    Optimized SpO2 Algorithm for AFE4490/AFE4400
//    Based on photoplethysmography signal processing
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
/////////////////////////////////////////////////////////////////////////////////////////

#ifndef _PROTOCENTRAL_SPO2_ALGORITHM_H_
#define _PROTOCENTRAL_SPO2_ALGORITHM_H_

#include <stdint.h>

namespace SpO2Algorithm {
  struct Config {
    uint16_t sampling_frequency;     // Sampling rate in Hz (default: 25)
    uint16_t buffer_size;           // Analysis buffer size (default: 100)
    uint8_t moving_avg_size;        // Moving average window (default: 4)
    uint8_t min_peak_distance;      // Minimum distance between peaks (default: 4)
    uint8_t max_peaks;              // Maximum peaks to detect (default: 15)
    uint16_t min_spo2;              // Minimum valid SpO2 value (default: 70)
    uint16_t max_spo2;              // Maximum valid SpO2 value (default: 100)
    float perfusion_threshold;       // Minimum perfusion index (default: 0.02)
  };
  
  enum class Status {
    VALID = 0,
    INVALID_LOW_PERFUSION,
    INVALID_POOR_SIGNAL,
    INVALID_INSUFFICIENT_PEAKS,
    INVALID_RATIO_OUT_OF_RANGE,
    INITIALIZATION_ERROR
  };
  
  struct Result {
    uint16_t spo2_percent;
    uint16_t heart_rate_bpm;
    Status status;
    float perfusion_index;           // Signal strength indicator
    float signal_quality;            // Overall signal quality (0.0 - 1.0)
    uint8_t confidence;              // Confidence level (0-100%)
  };
  
  struct PeakInfo {
    uint16_t location;
    int32_t ir_value;
    int32_t red_value;
    bool valid;
  };
}

class SpO2Calculator {
public:
  SpO2Calculator();
  explicit SpO2Calculator(const SpO2Algorithm::Config& config);
  
  bool initialize(const SpO2Algorithm::Config& config);
  SpO2Algorithm::Result calculate(const uint16_t* ir_buffer, const uint16_t* red_buffer, uint16_t buffer_length);
  void reset();
  
  SpO2Algorithm::Config getConfig() const { return config_; }
  static SpO2Algorithm::Config getDefaultConfig();
  
private:
  struct SignalProcessing {
    int32_t* filtered_ir;
    int32_t* filtered_red;
    int32_t ir_dc_mean;
    int32_t red_dc_mean;
    bool valid;
  };
  
  bool preprocessSignals(const uint16_t* ir_buffer, const uint16_t* red_buffer, uint16_t length, SignalProcessing& processed);
  bool findPeaksAndValleys(const SignalProcessing& signals, uint16_t length);
  float calculatePerfusionIndex(const SignalProcessing& signals, uint16_t length);
  float calculateSignalQuality(const SignalProcessing& signals, uint16_t length);
  uint16_t calculateSpO2FromRatio(float ratio);
  uint16_t calculateHeartRate(uint16_t length);
  bool isValidSpO2(uint16_t spo2) const;
  
  // Peak detection utilities
  void findPeaksAboveThreshold(int32_t* locations, uint16_t* peak_count, const int32_t* signal, 
                               uint16_t length, int32_t threshold);
  void removePeaksTooClose(int32_t* locations, uint16_t* peak_count, const int32_t* signal, 
                           uint16_t min_distance);
  void sortPeaksByHeight(const int32_t* signal, int32_t* locations, uint16_t peak_count);
  
  // Mathematical utilities
  void applyMovingAverage(int32_t* output, const int32_t* input, uint16_t length, uint8_t window_size);
  int32_t calculateMean(const int32_t* data, uint16_t length);
  void insertionSort(int32_t* array, uint16_t length);
  
  // Configuration and state
  SpO2Algorithm::Config config_;
  bool initialized_;
  
  // Working buffers
  int32_t* work_buffer_ir_;
  int32_t* work_buffer_red_;
  int32_t* peak_locations_;
  int32_t* valley_locations_;
  
  // Analysis results
  SpO2Algorithm::PeakInfo* detected_peaks_;
  uint16_t peak_count_;
  uint16_t valley_count_;
  
  // Calibration lookup table (optimized for AFE4490)
  static const uint8_t spo2_lookup_table_[184];
};

// Legacy compatibility class
class spo2_algorithm {
public:
  spo2_algorithm();
  void estimate_spo2(uint16_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint16_t *pun_red_buffer, 
                     int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);
  
private:
  SpO2Calculator calculator_;
};

#endif // _PROTOCENTRAL_SPO2_ALGORITHM_H_
