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

#define LOG_TAG "VirtVehicleHardware"

#include "VirtVehicleHardware.h"
#include "VehicleVmcu.h"

#include <VehicleHalTypes.h>
#include <VehicleUtils.h>
#include <android-base/properties.h>
#include <android/binder_manager.h>
#include <utils/Log.h>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace fake {

using ::aidl::android::hardware::automotive::vehicle::StatusCode;
using ::aidl::android::hardware::automotive::vehicle::SetValueRequest;
using ::aidl::android::hardware::automotive::vehicle::SetValueResult;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropConfig;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropValue;
using ::aidl::android::hardware::automotive::vehicle::VehicleProperty;

using ::android::base::Result;
using ::android::hardware::automotive::vehicle::V2_0::impl::MessageSender;

VirtVehicleHardware::VirtVehicleHardware() {
    mInQemu = isInQemu();
    ALOGD("mInQemu=%s", mInQemu ? "true" : "false");

    mVmcu = std::make_unique<VehicleVmcu>(this);
}

VirtVehicleHardware::VirtVehicleHardware(
        bool inQemu,
        std::unique_ptr<MessageSender> socketComm,
        std::unique_ptr<MessageSender> pipeComm,
        std::unique_ptr<MessageSender> vspiComm) {
    mInQemu = inQemu;
    mVmcu = std::make_unique<VehicleVmcu>(std::move(socketComm), std::move(pipeComm), std::move(vspiComm), this);
}

VehicleVmcu* VirtVehicleHardware::getVmcu() {
    return mVmcu.get();
}

VirtVehicleHardware::~VirtVehicleHardware() {
    mVmcu.reset();
}

StatusCode VirtVehicleHardware::setValues(
            std::shared_ptr<const SetValuesCallback> callback,
            const std::vector<SetValueRequest>& requests) {
    std::vector<SetValueResult> results;

    for (const auto& request: requests) {
        const VehiclePropValue& value = request.value;
        int propId = value.prop;

        ALOGD("Set value for property ID: %d", propId);

        if (mInQemu && propId == toInt(VehicleProperty::DISPLAY_BRIGHTNESS)) {
            ALOGD("Return OKAY for DISPLAY_BRIGHTNESS in QEMU");

            // Emulator does not support remote brightness control, b/139959479
            // do not send it down so that it does not bring unnecessary property change event
            // return other error code, such NOT_AVAILABLE, causes Emulator to be freezing
            // TODO: return StatusCode::NOT_AVAILABLE once the above issue is fixed
            results.push_back({
                .requestId = request.requestId,
                .status = StatusCode::OK,
            });
            continue;
        }

        SetValueResult setValueResult;
        setValueResult.requestId = request.requestId;

        if (auto result = setValue(value); !result.ok()) {
            ALOGE("failed to set value, error: %s, code: %d", getErrorMsg(result).c_str(),
                    getIntErrorCode(result));
            setValueResult.status = getErrorCode(result);
        } else {
            setValueResult.status = StatusCode::OK;
            // Inform the emulator about a new value change.
            mVmcu->doSetValueFromClient(value);
        }

        results.push_back(std::move(setValueResult));
    }
    // In real Vehicle HAL, the values would be sent to vehicle bus. But here, we just assume
    // it is done and notify the client.
    (*callback)(std::move(results));

    return StatusCode::OK;
}

std::vector<VehiclePropValuePool::RecyclableType> VirtVehicleHardware::getAllProperties()
        const {
    return mServerSidePropStore->readAllValues();
}

VirtVehicleHardware::ConfigResultType VirtVehicleHardware::getPropConfig(int32_t propId)
        const {
    return mServerSidePropStore->getPropConfig(propId);
}

bool VirtVehicleHardware::isInQemu() {
    return android::base::GetBoolProperty("ro.boot.qemu", false);
}

}  // namespace fake
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

