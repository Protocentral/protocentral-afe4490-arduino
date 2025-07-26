//////////////////////////////////////////////////////////////////////////////////////////
//
//    Optimized Heart Rate Algorithm for AFE4490/AFE4400
//    Based on TI reference implementation with improvements
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//   For information on how to use, visit https://github.com/Protocentral/protocentral-afe4490-arduino
//
/////////////////////////////////////////////////////////////////////////////////////////

#ifndef _PROTOCENTRAL_HR_ALGORITHM_H_
#define _PROTOCENTRAL_HR_ALGORITHM_H_

#include <stdint.h>

namespace HeartRateAlgorithm {
  struct Config {
    uint16_t sampling_frequency;    // Sampling rate in Hz (default: 125)
    uint8_t moving_avg_size;       // Moving average window size (default: 3)
    uint8_t min_peak_distance;     // Minimum distance between peaks (default: 4)
    uint16_t min_bpm;              // Minimum valid heart rate (default: 40)
    uint16_t max_bpm;              // Maximum valid heart rate (default: 220)
    uint8_t rate_history_size;     // Number of rate values to average (default: 8)
    uint8_t peak_window_size;      // Peak detection window size (default: 21)
  };
  
  enum class Status {
    VALID = 0,
    INVALID_LOW_SIGNAL,
    INVALID_NOISE,
    INVALID_RATE_RANGE,
    INSUFFICIENT_PEAKS,
    INITIALIZATION_ERROR
  };
  
  struct Result {
    uint16_t heart_rate_bpm;
    Status status;
    float signal_quality;        // Signal quality indicator (0.0 - 1.0)
    uint8_t confidence;          // Confidence level (0-100%)
  };
}

class HeartRateDetector {
public:
  HeartRateDetector();
  explicit HeartRateDetector(const HeartRateAlgorithm::Config& config);
  
  bool initialize(const HeartRateAlgorithm::Config& config);
  HeartRateAlgorithm::Result processSignal(int32_t ppg_sample);
  void reset();
  
  HeartRateAlgorithm::Config getConfig() const { return config_; }
  static HeartRateAlgorithm::Config getDefaultConfig();
  
private:
  struct PeakInfo {
    uint16_t location;
    int32_t amplitude;
    bool valid;
  };
  
  bool detectPeak(int32_t current_sample);
  bool detectValley(int32_t current_sample);
  float calculateSignalQuality();
  uint16_t calculateHeartRate();
  bool isValidHeartRate(uint16_t rate) const;
  void updateRateHistory(uint16_t new_rate);
  uint16_t getFilteredHeartRate() const;
  
  // Configuration
  HeartRateAlgorithm::Config config_;
  bool initialized_;
  
  // Signal processing buffers
  int32_t* signal_buffer_;
  uint16_t buffer_index_;
  int32_t moving_average_;
  int32_t moving_sum_;
  
  // Peak detection state
  PeakInfo last_peak_;
  PeakInfo last_valley_;
  uint16_t samples_since_last_peak_;
  uint16_t samples_since_last_valley_;
  bool peak_detected_this_cycle_;
  
  // Heart rate calculation
  uint16_t* rate_history_;
  uint8_t rate_history_index_;
  uint8_t valid_rates_count_;
  uint16_t peak_intervals_[10];  // Store last 10 peak intervals
  uint8_t interval_count_;
  
  // Signal quality metrics
  int32_t signal_variance_;
  int32_t noise_estimate_;
  uint16_t total_peaks_found_;
  
  // Internal counters
  uint32_t sample_count_;
};

// Legacy compatibility class
class hr_algo {
public:
  hr_algo();
  void initStatHRM();
  void statHRMAlgo(unsigned long ppgData);
  
  unsigned char HeartRate;
  
private:
  HeartRateDetector detector_;
};

#endif // _PROTOCENTRAL_HR_ALGORITHM_H_
