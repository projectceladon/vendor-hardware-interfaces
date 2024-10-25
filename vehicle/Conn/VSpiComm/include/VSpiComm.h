/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ANDROID_EMULATORVSPICOMM_VSPICOMM_H
#define ANDROID_EMULATORVSPICOMM_VSPICOMM_H

#include "CommConn.h"
#include <linux/spi/spidev.h>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <mutex>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {

namespace impl {

class VSpiComm : public CommConn {
  public:
    VSpiComm(MessageProcessor* messageProcessor);
    ~VSpiComm();

    inline bool isOpen() override { return mSpiFd > 0; }
    std::vector<uint8_t> read() override;
    int write(const std::vector<uint8_t>& data) override;
    void start() override;
    void stop() override;

  private:
    int mSpiFd;
    std::string mSpiDevice;
    uint8_t mMode;
    uint8_t mBitsPerWord;
    uint32_t mSpeed;
    std::mutex mSpiMutex;
};

}  // impl

}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_EMULATORVSPICOMM_VSPICOMM_H
