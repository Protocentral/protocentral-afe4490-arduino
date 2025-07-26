//////////////////////////////////////////////////////////////////////////////////////////
//
//    Optimized Heart Rate Algorithm for AFE4490/AFE4400
//    Based on TI reference implementation with improvements
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_hr_algorithm.h"
#include <string.h>  // For memset
#include <stdlib.h>  // For malloc/free

using namespace HeartRateAlgorithm;

// HeartRateDetector Implementation
HeartRateDetector::HeartRateDetector() : initialized_(false), signal_buffer_(nullptr), rate_history_(nullptr) {
  config_ = getDefaultConfig();
}

HeartRateDetector::HeartRateDetector(const Config& config) : initialized_(false), signal_buffer_(nullptr), rate_history_(nullptr) {
  initialize(config);
}

HeartRateAlgorithm::Config HeartRateDetector::getDefaultConfig() {
  Config config;
  config.sampling_frequency = 125;
  config.moving_avg_size = 3;
  config.min_peak_distance = 4;
  config.min_bpm = 40;
  config.max_bpm = 220;
  config.rate_history_size = 8;
  config.peak_window_size = 21;
  return config;
}

bool HeartRateDetector::initialize(const Config& config) {
  config_ = config;
  
  // Allocate buffers
  if (signal_buffer_) {
    free(signal_buffer_);
  }
  if (rate_history_) {
    free(rate_history_);
  }
  
  signal_buffer_ = (int32_t*)malloc(config_.peak_window_size * sizeof(int32_t));
  rate_history_ = (uint16_t*)malloc(config_.rate_history_size * sizeof(uint16_t));
  
  if (!signal_buffer_ || !rate_history_) {
    return false;
  }
  
  reset();
  initialized_ = true;
  return true;
}

void HeartRateDetector::reset() {
  if (!signal_buffer_ || !rate_history_) return;
  
  memset(signal_buffer_, 0, config_.peak_window_size * sizeof(int32_t));
  memset(rate_history_, 0, config_.rate_history_size * sizeof(uint16_t));
  memset(peak_intervals_, 0, sizeof(peak_intervals_));
  
  buffer_index_ = 0;
  moving_average_ = 0;
  moving_sum_ = 0;
  
  last_peak_ = {0, 0, false};
  last_valley_ = {0, 0, false};
  samples_since_last_peak_ = 0;
  samples_since_last_valley_ = 0;
  peak_detected_this_cycle_ = false;
  
  rate_history_index_ = 0;
  valid_rates_count_ = 0;
  interval_count_ = 0;
  
  signal_variance_ = 0;
  noise_estimate_ = 0;
  total_peaks_found_ = 0;
  sample_count_ = 0;
}


Result HeartRateDetector::processSignal(int32_t ppg_sample) {
  Result result = {0, Status::INITIALIZATION_ERROR, 0.0f, 0};
  
  if (!initialized_) {
    return result;
  }
  
  sample_count_++;
  
  // Update moving average
  moving_sum_ += ppg_sample;
  if (sample_count_ > config_.moving_avg_size) {
    moving_sum_ -= signal_buffer_[(buffer_index_ + config_.peak_window_size - config_.moving_avg_size) % config_.peak_window_size];
  }
  
  if (sample_count_ >= config_.moving_avg_size) {
    moving_average_ = moving_sum_ / config_.moving_avg_size;
  }
  
  // Store sample in circular buffer
  signal_buffer_[buffer_index_] = ppg_sample;
  buffer_index_ = (buffer_index_ + 1) % config_.peak_window_size;
  
  // Update counters
  samples_since_last_peak_++;
  samples_since_last_valley_++;
  peak_detected_this_cycle_ = false;
  
  // Only start processing after buffer is full
  if (sample_count_ < config_.peak_window_size) {
    result.status = Status::INSUFFICIENT_PEAKS;
    return result;
  }
  
  // Detect peaks and valleys
  bool peak_found = detectPeak(ppg_sample);
  bool valley_found = detectValley(ppg_sample);
  
  if (peak_found || valley_found) {
    total_peaks_found_++;
    
    // Calculate heart rate if we have enough peaks
    if (total_peaks_found_ >= 3) {
      uint16_t calculated_rate = calculateHeartRate();
      
      if (isValidHeartRate(calculated_rate)) {
        updateRateHistory(calculated_rate);
        result.heart_rate_bpm = getFilteredHeartRate();
        result.status = Status::VALID;
        result.signal_quality = calculateSignalQuality();
        result.confidence = (valid_rates_count_ * 100) / config_.rate_history_size;
      } else {
        result.status = Status::INVALID_RATE_RANGE;
      }
    } else {
      result.status = Status::INSUFFICIENT_PEAKS;
    }
  } else {
    // Return last valid rate if available
    if (valid_rates_count_ > 0) {
      result.heart_rate_bpm = getFilteredHeartRate();
      result.status = Status::VALID;
      result.signal_quality = calculateSignalQuality();
      result.confidence = (valid_rates_count_ * 100) / config_.rate_history_size;
    } else {
      result.status = Status::INSUFFICIENT_PEAKS;
    }
  }
  
  return result;
}

bool HeartRateDetector::detectPeak(int32_t current_sample) {
  if (samples_since_last_peak_ < config_.min_peak_distance) {
    return false;
  }
  
  uint16_t center_idx = (buffer_index_ + config_.peak_window_size/2) % config_.peak_window_size;
  int32_t center_value = signal_buffer_[center_idx];
  
  // Check if center is local maximum
  bool is_peak = true;
  for (uint8_t i = 1; i <= config_.peak_window_size/2; i++) {
    uint16_t left_idx = (center_idx + config_.peak_window_size - i) % config_.peak_window_size;
    uint16_t right_idx = (center_idx + i) % config_.peak_window_size;
    
    if (center_value <= signal_buffer_[left_idx] || center_value <= signal_buffer_[right_idx]) {
      is_peak = false;
      break;
    }
  }
  
  if (is_peak && center_value > moving_average_ + (moving_average_ >> 3)) {  // 12.5% above average
    if (last_peak_.valid && interval_count_ < 10) {
      uint16_t interval = samples_since_last_peak_;
      peak_intervals_[interval_count_] = interval;
      interval_count_++;
    }
    
    last_peak_ = {(uint16_t)sample_count_, center_value, true};
    samples_since_last_peak_ = 0;
    peak_detected_this_cycle_ = true;
    return true;
  }
  
  return false;
}

bool HeartRateDetector::detectValley(int32_t current_sample) {
  if (samples_since_last_valley_ < config_.min_peak_distance || peak_detected_this_cycle_) {
    return false;
  }
  
  uint16_t center_idx = (buffer_index_ + config_.peak_window_size/2) % config_.peak_window_size;
  int32_t center_value = signal_buffer_[center_idx];
  
  // Check if center is local minimum
  bool is_valley = true;
  for (uint8_t i = 1; i <= config_.peak_window_size/2; i++) {
    uint16_t left_idx = (center_idx + config_.peak_window_size - i) % config_.peak_window_size;
    uint16_t right_idx = (center_idx + i) % config_.peak_window_size;
    
    if (center_value >= signal_buffer_[left_idx] || center_value >= signal_buffer_[right_idx]) {
      is_valley = false;
      break;
    }
  }
  
  if (is_valley && center_value < moving_average_ - (moving_average_ >> 3)) {  // 12.5% below average
    last_valley_ = {(uint16_t)sample_count_, center_value, true};
    samples_since_last_valley_ = 0;
    return true;
  }
  
  return false;
}

uint16_t HeartRateDetector::calculateHeartRate() {
  if (interval_count_ < 2) {
    return 0;
  }
  
  // Calculate average interval
  uint32_t sum = 0;
  for (uint8_t i = 0; i < interval_count_; i++) {
    sum += peak_intervals_[i];
  }
  
  uint16_t avg_interval = sum / interval_count_;
  uint16_t heart_rate = (config_.sampling_frequency * 60) / avg_interval;
  
  return heart_rate;
}

bool HeartRateDetector::isValidHeartRate(uint16_t rate) const {
  return (rate >= config_.min_bpm && rate <= config_.max_bpm);
}

void HeartRateDetector::updateRateHistory(uint16_t new_rate) {
  rate_history_[rate_history_index_] = new_rate;
  rate_history_index_ = (rate_history_index_ + 1) % config_.rate_history_size;
  
  if (valid_rates_count_ < config_.rate_history_size) {
    valid_rates_count_++;
  }
}

uint16_t HeartRateDetector::getFilteredHeartRate() const {
  if (valid_rates_count_ == 0) {
    return 0;
  }
  
  // Calculate median for better noise rejection
  uint16_t sorted_rates[16];  // Max rate_history_size
  memcpy(sorted_rates, rate_history_, valid_rates_count_ * sizeof(uint16_t));
  
  // Simple insertion sort
  for (uint8_t i = 1; i < valid_rates_count_; i++) {
    uint16_t key = sorted_rates[i];
    int8_t j = i - 1;
    
    while (j >= 0 && sorted_rates[j] > key) {
      sorted_rates[j + 1] = sorted_rates[j];
      j--;
    }
    sorted_rates[j + 1] = key;
  }
  
  // Return median
  if (valid_rates_count_ % 2 == 0) {
    return (sorted_rates[valid_rates_count_/2 - 1] + sorted_rates[valid_rates_count_/2]) / 2;
  } else {
    return sorted_rates[valid_rates_count_/2];
  }
}

float HeartRateDetector::calculateSignalQuality() {
  if (sample_count_ < config_.peak_window_size) {
    return 0.0f;
  }
  
  // Calculate signal-to-noise ratio approximation
  int32_t signal_power = 0;
  int32_t noise_power = 0;
  
  for (uint8_t i = 0; i < config_.peak_window_size; i++) {
    int32_t diff = signal_buffer_[i] - moving_average_;
    signal_power += (diff * diff) >> 8;  // Scaled to prevent overflow
  }
  
  // Estimate noise from high-frequency components
  for (uint8_t i = 1; i < config_.peak_window_size; i++) {
    int32_t derivative = signal_buffer_[i] - signal_buffer_[i-1];
    noise_power += (derivative * derivative) >> 8;
  }
  
  if (noise_power == 0) {
    return 1.0f;
  }
  
  float snr = (float)signal_power / (float)noise_power;
  return (snr > 10.0f) ? 1.0f : (snr / 10.0f);
}

// Legacy compatibility implementation
hr_algo::hr_algo() : HeartRate(0) {
  detector_.initialize(HeartRateDetector::getDefaultConfig());
}

void hr_algo::initStatHRM() {
  detector_.reset();
  HeartRate = 0;
}

void hr_algo::statHRMAlgo(unsigned long ppgData) {
  Result result = detector_.processSignal((int32_t)ppgData);
  
  if (result.status == Status::VALID) {
    HeartRate = (unsigned char)result.heart_rate_bpm;
  }
}
