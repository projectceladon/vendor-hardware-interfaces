
/*
 * Copyright (C) 2021 The Android Open Source Project
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
#ifndef android_hardware_gnss_common_default_SockFileReader_H_
#define android_hardware_gnss_common_default_SockFileReader_H_

#include <log/log.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include "Constants.h"
#include "GnssReplayUtils.h"

namespace android {
namespace hardware {
namespace gnss {
namespace common {

using namespace std;

class SockFileReader {
  public:
    static SockFileReader& Instance() {
        static SockFileReader reader;
        return reader;
    }
    std::string getLocationData();
    std::string getGnssRawMeasurementData();
    void getDataFromSockFile(const std::string& command, int mMinIntervalMs);

  private:
    SockFileReader();
    ~SockFileReader();
    int initializeGnssSocket();
    std::unordered_map<std::string, std::string> data_;
    std::string s_buffer_;
    std::mutex mMutex;
	int gnss_fix_fd_;
};
}  // namespace common
}  // namespace gnss
}  // namespace hardware
}  // namespace android

#endif  // android_hardware_gnss_common_default_SockFileReader_H_
