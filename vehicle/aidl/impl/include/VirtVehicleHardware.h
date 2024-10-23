/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef ANDROID_VEHICLEVMCU_VIRTVEHICLEHARDWARE_H
#define ANDROID_VEHICLEVMCU_VIRTVEHICLEHARDWARE_H

#include <CommConn.h>
#include <FakeVehicleHardware.h>
#include <VehicleUtils.h>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace fake {

class VehicleVmcu;
class VehicleVmcuTest;

class VirtVehicleHardware final : public FakeVehicleHardware {
  public:
    using AidlVehiclePropValue = aidl::android::hardware::automotive::vehicle::VehiclePropValue;
    using ConfigResultType = android::base::Result<const aidl::android::hardware::automotive::vehicle::VehiclePropConfig*, VhalError>;

    VirtVehicleHardware();

    ~VirtVehicleHardware();

    aidl::android::hardware::automotive::vehicle::StatusCode setValues(
            std::shared_ptr<const SetValuesCallback> callback,
            const std::vector<aidl::android::hardware::automotive::vehicle::SetValueRequest>&
                    requests) override;

    std::vector<VehiclePropValuePool::RecyclableType> getAllProperties() const;

    ConfigResultType getPropConfig(int32_t propId) const;

  private:
    friend class VehicleVmcu;
    friend class VehicleVmcuTest;

    bool mInQemu;

    std::unique_ptr<VehicleVmcu> mVmcu;

    // determine if it's running inside Android Emulator
    static bool isInQemu();

    // For testing only.
    VirtVehicleHardware(
        bool inQEMU,
        std::unique_ptr<V2_0::impl::MessageSender> socketComm,
        std::unique_ptr<V2_0::impl::MessageSender> pipeComm);

    // For testing only.
    VehicleVmcu* getVmcu();
};

}  // namespace fake
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_VEHICLEVMCU_VIRTVEHICLEHARDWARE_H
