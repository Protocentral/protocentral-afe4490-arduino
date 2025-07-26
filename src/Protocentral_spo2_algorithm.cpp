//////////////////////////////////////////////////////////////////////////////////////////
//
//    Optimized SpO2 Algorithm for AFE4490/AFE4400
//    Based on photoplethysmography signal processing
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
/////////////////////////////////////////////////////////////////////////////////////////

#include "Protocentral_spo2_algorithm.h"
#include <string.h> // For memset, memcpy
#include <stdlib.h> // For malloc/free
#include <math.h>   // For mathematical functions

using namespace SpO2Algorithm;

// Calibrated lookup table for SpO2 calculation (based on R ratio)
const uint8_t SpO2Calculator::spo2_lookup_table_[184] = {
    95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
    99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
    97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
    90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
    80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
    66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
    28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
    3, 2, 1};

// SpO2Calculator Implementation
SpO2Calculator::SpO2Calculator() : initialized_(false), work_buffer_ir_(nullptr), work_buffer_red_(nullptr),
                                   peak_locations_(nullptr), valley_locations_(nullptr), detected_peaks_(nullptr)
{
  config_ = getDefaultConfig();
}

SpO2Calculator::SpO2Calculator(const Config &config) : initialized_(false), work_buffer_ir_(nullptr),
                                                       work_buffer_red_(nullptr), peak_locations_(nullptr),
                                                       valley_locations_(nullptr), detected_peaks_(nullptr)
{
  initialize(config);
}

Config SpO2Calculator::getDefaultConfig()
{
  Config config;
  config.sampling_frequency = 25;
  config.buffer_size = 100;
  config.moving_avg_size = 4;
  config.min_peak_distance = 4;
  config.max_peaks = 15;
  config.min_spo2 = 70;
  config.max_spo2 = 100;
  config.perfusion_threshold = 0.02f;
  return config;
}

bool SpO2Calculator::initialize(const Config &config)
{
  config_ = config;

  // Free existing buffers
  if (work_buffer_ir_)
    free(work_buffer_ir_);
  if (work_buffer_red_)
    free(work_buffer_red_);
  if (peak_locations_)
    free(peak_locations_);
  if (valley_locations_)
    free(valley_locations_);
  if (detected_peaks_)
    free(detected_peaks_);

  // Allocate new buffers
  work_buffer_ir_ = (int32_t *)malloc(config_.buffer_size * sizeof(int32_t));
  work_buffer_red_ = (int32_t *)malloc(config_.buffer_size * sizeof(int32_t));
  peak_locations_ = (int32_t *)malloc(config_.max_peaks * sizeof(int32_t));
  valley_locations_ = (int32_t *)malloc(config_.max_peaks * sizeof(int32_t));
  detected_peaks_ = (PeakInfo *)malloc(config_.max_peaks * sizeof(PeakInfo));

  if (!work_buffer_ir_ || !work_buffer_red_ || !peak_locations_ || !valley_locations_ || !detected_peaks_)
  {
    return false;
  }

  reset();
  initialized_ = true;
  return true;
}

void SpO2Calculator::reset()
{
  if (!initialized_)
    return;

  memset(work_buffer_ir_, 0, config_.buffer_size * sizeof(int32_t));
  memset(work_buffer_red_, 0, config_.buffer_size * sizeof(int32_t));
  memset(peak_locations_, 0, config_.max_peaks * sizeof(int32_t));
  memset(valley_locations_, 0, config_.max_peaks * sizeof(int32_t));
  memset(detected_peaks_, 0, config_.max_peaks * sizeof(PeakInfo));

  peak_count_ = 0;
  valley_count_ = 0;
}

Result SpO2Calculator::calculate(const uint16_t *ir_buffer, const uint16_t *red_buffer, uint16_t buffer_length)
{
  Result result = {0, 0, Status::INITIALIZATION_ERROR, 0.0f, 0.0f, 0};

  if (!initialized_ || !ir_buffer || !red_buffer || buffer_length == 0)
  {
    return result;
  }

  // Limit buffer length to our working buffer size
  uint16_t analysis_length = (buffer_length > config_.buffer_size) ? config_.buffer_size : buffer_length;

  // Step 1: Preprocess signals (DC removal, filtering)
  SignalProcessing processed;
  if (!preprocessSignals(ir_buffer, red_buffer, analysis_length, processed))
  {
    result.status = Status::INVALID_POOR_SIGNAL;
    return result;
  }

  // Step 2: Calculate perfusion index
  float perfusion = calculatePerfusionIndex(processed, analysis_length);
  result.perfusion_index = perfusion;

  if (perfusion < config_.perfusion_threshold)
  {
    result.status = Status::INVALID_LOW_PERFUSION;
    return result;
  }

  // Step 3: Calculate signal quality
  result.signal_quality = calculateSignalQuality(processed, analysis_length);

  // Step 4: Find peaks and valleys for heart rate calculation
  if (!findPeaksAndValleys(processed, analysis_length))
  {
    result.status = Status::INVALID_INSUFFICIENT_PEAKS;
    return result;
  }

  // Step 5: Calculate heart rate
  result.heart_rate_bpm = calculateHeartRate(analysis_length);

  // Step 6: Calculate SpO2 using R-ratio method
  if (peak_count_ < 2)
  {
    result.status = Status::INVALID_INSUFFICIENT_PEAKS;
    return result;
  }

  // Calculate R ratio: (AC_red/DC_red) / (AC_ir/DC_ir)
  float ratio_sum = 0.0f;
  uint8_t valid_ratios = 0;

  for (uint8_t i = 0; i < peak_count_ - 1; i++)
  {
    PeakInfo &current_peak = detected_peaks_[i];
    PeakInfo &next_peak = detected_peaks_[i + 1];

    if (!current_peak.valid || !next_peak.valid)
      continue;

    // Find valley between peaks
    int32_t valley_ir = current_peak.ir_value;
    int32_t valley_red = current_peak.red_value;

    for (uint16_t j = current_peak.location; j < next_peak.location; j++)
    {
      if (processed.filtered_ir[j] < valley_ir)
      {
        valley_ir = processed.filtered_ir[j];
        valley_red = processed.filtered_red[j];
      }
    }

    // Calculate AC and DC components
    int32_t ac_red = next_peak.red_value - valley_red;
    int32_t ac_ir = next_peak.ir_value - valley_ir;
    int32_t dc_red = (next_peak.red_value + valley_red) / 2;
    int32_t dc_ir = (next_peak.ir_value + valley_ir) / 2;

    // Calculate R ratio with bounds checking
    if (dc_red > 0 && dc_ir > 0 && ac_ir > 0)
    {
      float red_ratio = (float)ac_red / (float)dc_red;
      float ir_ratio = (float)ac_ir / (float)dc_ir;

      if (ir_ratio > 0.001f)
      { // Avoid division by very small numbers
        float r_ratio = red_ratio / ir_ratio;

        if (r_ratio > 0.1f && r_ratio < 3.0f)
        { // Physiologically reasonable range
          ratio_sum += r_ratio;
          valid_ratios++;
        }
      }
    }
  }

  if (valid_ratios == 0)
  {
    result.status = Status::INVALID_RATIO_OUT_OF_RANGE;
    return result;
  }

  // Average the ratios and convert to SpO2
  float avg_ratio = ratio_sum / valid_ratios;
  result.spo2_percent = calculateSpO2FromRatio(avg_ratio);

  if (!isValidSpO2(result.spo2_percent))
  {
    result.status = Status::INVALID_RATIO_OUT_OF_RANGE;
    return result;
  }

  result.status = Status::VALID;
  result.confidence = (valid_ratios * 100) / (peak_count_ - 1);

  return result;
}

bool SpO2Calculator::preprocessSignals(const uint16_t *ir_buffer, const uint16_t *red_buffer, uint16_t length, SignalProcessing &processed)
{
  // Convert to int32_t and calculate DC means
  int64_t ir_sum = 0, red_sum = 0;

  for (uint16_t i = 0; i < length; i++)
  {
    work_buffer_ir_[i] = (int32_t)ir_buffer[i];
    work_buffer_red_[i] = (int32_t)red_buffer[i];
    ir_sum += work_buffer_ir_[i];
    red_sum += work_buffer_red_[i];
  }

  processed.ir_dc_mean = (int32_t)(ir_sum / length);
  processed.red_dc_mean = (int32_t)(red_sum / length);

  // Remove DC component and invert IR signal for valley detection
  for (uint16_t i = 0; i < length; i++)
  {
    work_buffer_ir_[i] = -(work_buffer_ir_[i] - processed.ir_dc_mean);
    work_buffer_red_[i] = work_buffer_red_[i] - processed.red_dc_mean;
  }

  // Apply moving average filter
  processed.filtered_ir = work_buffer_ir_;
  processed.filtered_red = work_buffer_red_;
  applyMovingAverage(processed.filtered_ir, work_buffer_ir_, length, config_.moving_avg_size);
  applyMovingAverage(processed.filtered_red, work_buffer_red_, length, config_.moving_avg_size);

  processed.valid = true;
  return true;
}

bool SpO2Calculator::findPeaksAndValleys(const SignalProcessing &signals, uint16_t length)
{
  // Calculate adaptive threshold for IR signal (inverted for valley detection)
  int32_t threshold = calculateMean(signals.filtered_ir, length);
  threshold = (threshold < 30) ? 30 : ((threshold > 60) ? 60 : threshold);

  // Find valleys in IR signal (peaks in inverted signal)
  uint16_t valley_count = 0;
  findPeaksAboveThreshold(valley_locations_, &valley_count, signals.filtered_ir, length, threshold);
  removePeaksTooClose(valley_locations_, &valley_count, signals.filtered_ir, config_.min_peak_distance);

  if (valley_count > config_.max_peaks)
  {
    valley_count = config_.max_peaks;
  }

  valley_count_ = valley_count;

  // Store peak information for SpO2 calculation
  peak_count_ = 0;
  for (uint16_t i = 0; i < valley_count && peak_count_ < config_.max_peaks; i++)
  {
    uint16_t location = valley_locations_[i];
    if (location < length)
    {
      detected_peaks_[peak_count_].location = location;
      detected_peaks_[peak_count_].ir_value = -signals.filtered_ir[location];                        // Un-invert
      detected_peaks_[peak_count_].red_value = signals.filtered_red[location] + signals.red_dc_mean; // Add DC back
      detected_peaks_[peak_count_].valid = true;
      peak_count_++;
    }
  }

  return peak_count_ >= 2;
}

void SpO2Calculator::findPeaksAboveThreshold(int32_t *locations, uint16_t *peak_count, const int32_t *signal, uint16_t length, int32_t threshold)
{
  *peak_count = 0;

  for (uint16_t i = 1; i < length - 1 && *peak_count < config_.max_peaks; i++)
  {
    if (signal[i] > threshold && signal[i] > signal[i - 1])
    {
      // Find the width of potential flat peaks
      uint16_t width = 1;
      while (i + width < length && signal[i] == signal[i + width])
      {
        width++;
      }

      // Check if this is a peak (higher than right neighbor)
      if (i + width < length && signal[i] > signal[i + width])
      {
        locations[(*peak_count)++] = i;
        i += width; // Skip past this peak
      }
      else
      {
        i += width - 1; // Continue searching
      }
    }
  }
}

void SpO2Calculator::removePeaksTooClose(int32_t *locations, uint16_t *peak_count, const int32_t *signal, uint16_t min_distance)
{
  if (*peak_count <= 1)
    return;

  // Sort peaks by height (descending)
  sortPeaksByHeight(signal, locations, *peak_count);

  // Remove peaks that are too close to higher peaks
  uint16_t kept_peaks = 0;
  bool *keep_peak = (bool *)malloc(*peak_count * sizeof(bool));

  if (!keep_peak)
    return; // Memory allocation failed

  for (uint16_t i = 0; i < *peak_count; i++)
  {
    keep_peak[i] = true;
  }

  for (uint16_t i = 0; i < *peak_count; i++)
  {
    if (!keep_peak[i])
      continue;

    for (uint16_t j = i + 1; j < *peak_count; j++)
    {
      if (!keep_peak[j])
        continue;

      int32_t distance = locations[j] - locations[i];
      if (distance < min_distance && distance > -min_distance)
      {
        keep_peak[j] = false; // Remove the lower peak
      }
    }
  }

  // Compact the array
  for (uint16_t i = 0; i < *peak_count; i++)
  {
    if (keep_peak[i])
    {
      locations[kept_peaks++] = locations[i];
    }
  }

  *peak_count = kept_peaks;

  // Sort remaining peaks by location (ascending)
  insertionSort(locations, *peak_count);

  free(keep_peak);
}

void SpO2Calculator::sortPeaksByHeight(const int32_t *signal, int32_t *locations, uint16_t peak_count)
{
  // Simple insertion sort by peak height (descending)
  for (uint16_t i = 1; i < peak_count; i++)
  {
    int32_t key = locations[i];
    int32_t key_height = signal[key];
    int16_t j = i - 1;

    while (j >= 0 && signal[locations[j]] < key_height)
    {
      locations[j + 1] = locations[j];
      j--;
    }
    locations[j + 1] = key;
  }
}

void SpO2Calculator::applyMovingAverage(int32_t *output, const int32_t *input, uint16_t length, uint8_t window_size)
{
  if (window_size == 0 || length < window_size)
    return;

  for (uint16_t i = 0; i <= length - window_size; i++)
  {
    int64_t sum = 0;
    for (uint8_t j = 0; j < window_size; j++)
    {
      sum += input[i + j];
    }
    output[i] = (int32_t)(sum / window_size);
  }
}

int32_t SpO2Calculator::calculateMean(const int32_t *data, uint16_t length)
{
  if (length == 0)
    return 0;

  int64_t sum = 0;
  for (uint16_t i = 0; i < length; i++)
  {
    sum += data[i];
  }
  return (int32_t)(sum / length);
}

void SpO2Calculator::insertionSort(int32_t *array, uint16_t length)
{
  for (uint16_t i = 1; i < length; i++)
  {
    int32_t key = array[i];
    int16_t j = i - 1;

    while (j >= 0 && array[j] > key)
    {
      array[j + 1] = array[j];
      j--;
    }
    array[j + 1] = key;
  }
}

float SpO2Calculator::calculatePerfusionIndex(const SignalProcessing &signals, uint16_t length)
{
  if (length == 0)
    return 0.0f;

  // Calculate AC component (signal variation)
  int32_t ir_min = signals.filtered_ir[0], ir_max = signals.filtered_ir[0];

  for (uint16_t i = 1; i < length; i++)
  {
    if (signals.filtered_ir[i] < ir_min)
      ir_min = signals.filtered_ir[i];
    if (signals.filtered_ir[i] > ir_max)
      ir_max = signals.filtered_ir[i];
  }

  int32_t ac_component = ir_max - ir_min;

  // Perfusion Index = (AC / DC) * 100
  if (signals.ir_dc_mean > 0)
  {
    return ((float)ac_component / (float)signals.ir_dc_mean) * 100.0f;
  }

  return 0.0f;
}

float SpO2Calculator::calculateSignalQuality(const SignalProcessing &signals, uint16_t length)
{
  if (length < 2)
    return 0.0f;

  // Calculate signal-to-noise ratio approximation
  int64_t signal_power = 0, noise_power = 0;

  for (uint16_t i = 0; i < length; i++)
  {
    int32_t deviation = signals.filtered_ir[i] - signals.ir_dc_mean;
    signal_power += (int64_t)deviation * deviation;
  }

  for (uint16_t i = 1; i < length; i++)
  {
    int32_t diff = signals.filtered_ir[i] - signals.filtered_ir[i - 1];
    noise_power += (int64_t)diff * diff;
  }

  if (noise_power == 0)
    return 1.0f;

  float snr = (float)signal_power / (float)noise_power;
  return (snr > 20.0f) ? 1.0f : (snr / 20.0f);
}

uint16_t SpO2Calculator::calculateSpO2FromRatio(float ratio)
{
  // Convert ratio to lookup table index
  uint16_t index = (uint16_t)(ratio * 100.0f);

  if (index >= 184)
  {
    index = 183;
  }

  return spo2_lookup_table_[index];
}

uint16_t SpO2Calculator::calculateHeartRate(uint16_t length)
{
  if (valley_count_ < 2)
    return 0;

  // Calculate average interval between valleys
  int32_t interval_sum = 0;
  for (uint16_t i = 1; i < valley_count_; i++)
  {
    interval_sum += valley_locations_[i] - valley_locations_[i - 1];
  }

  float avg_interval = (float)interval_sum / (float)(valley_count_ - 1);
  uint16_t heart_rate = (uint16_t)((config_.sampling_frequency * 60.0f) / avg_interval);

  return heart_rate;
}

bool SpO2Calculator::isValidSpO2(uint16_t spo2) const
{
  return (spo2 >= config_.min_spo2 && spo2 <= config_.max_spo2);
}

// Legacy compatibility implementation
spo2_algorithm::spo2_algorithm()
{
  calculator_.initialize(SpO2Calculator::getDefaultConfig());
}

void spo2_algorithm::estimate_spo2(uint16_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint16_t *pun_red_buffer,
                                   int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid)
{

  Result result = calculator_.calculate(pun_ir_buffer, pun_red_buffer, (uint16_t)n_ir_buffer_length);

  if (result.status == Status::VALID)
  {
    *pn_spo2 = result.spo2_percent;
    *pch_spo2_valid = 1;
    *pn_heart_rate = result.heart_rate_bpm;
    *pch_hr_valid = 1;
  }
  else
  {
    *pn_spo2 = -999;
    *pch_spo2_valid = 0;
    *pn_heart_rate = -999;
    *pch_hr_valid = 0;
  }
}