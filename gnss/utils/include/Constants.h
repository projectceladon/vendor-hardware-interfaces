/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef android_hardware_gnss_common_Constants_H_
#define android_hardware_gnss_common_Constants_H_

#include <cstdint>

namespace android {
namespace hardware {
namespace gnss {
namespace common {

const float kMockHorizontalAccuracyMeters = 5;
const float kMockVerticalAccuracyMeters = 5;
const float kMockSpeedAccuracyMetersPerSecond = 1;
const float kMockBearingAccuracyDegrees = 90;
const int64_t kMockTimestamp = 1519930775453L;
const float kGpsL1FreqHz = 1575.42 * 1e6;
const float kGpsL5FreqHz = 1176.45 * 1e6;
const float kGloG1FreqHz = 1602.0 * 1e6;
const float kIrnssL5FreqHz = 1176.45 * 1e6;

// Location replay constants
constexpr char GNSS_PATH[] = "/dev/gnss0";
constexpr char FIXED_LOCATION_PATH[] = "/dev/gnss1";
constexpr int INPUT_BUFFER_SIZE = 1024;
constexpr char CMD_GET_LOCATION[] = "CMD_GET_LOCATION";
constexpr char CMD_GET_RAWMEASUREMENT[] = "CMD_GET_RAWMEASUREMENT";
constexpr char CMD_SET_CC_2HZ[] = "$PCAS02,500*1A\r\n";
constexpr char CMD_SET_CC_1HZ[] = "$PCAS02,1000*2E\r\n";
constexpr char CMD_SET_CC_GGARMC[] = "$PCAS03,1,0,0,0,1,0,0,0,0,0,,,0,0*02\r\n";
constexpr char CMD_SET_CC_BR115200[] = "$PCAS01,5*19\r\n";
constexpr char CMD_SET_CC_BR9600[] = "$PCAS01,1*1D\r\n";
constexpr char LINE_SEPARATOR = '\n';
constexpr char COMMA_SEPARATOR = ',';
constexpr char GPGA_RECORD_TAG[] = "$GPGGA";
constexpr char GNGA_RECORD_TAG[] = "$GNGGA";
constexpr char GPRMC_RECORD_TAG[] = "$GPRMC";
constexpr char GNRMC_RECORD_TAG[] = "$GNRMC";
constexpr double TIMESTAMP_EPSILON = 0.001;
constexpr int MIN_COL_NUM = 13;
constexpr int BYTES_PER_SES = 1024;
constexpr int DEVICE_COUNT = 20;

}  // namespace common
}  // namespace gnss
}  // namespace hardware
}  // namespace android

#endif  // android_hardware_gnss_common_Constants_H_
