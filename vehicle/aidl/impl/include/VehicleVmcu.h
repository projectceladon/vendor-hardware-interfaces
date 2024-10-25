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

#ifndef ANDROID_VEHICLEVMCU_VEHICLEVMCU_H
#define ANDROID_VEHICLEVMCU_VEHICLEVMCU_H

#include "VirtVehicleHardware.h"

#include <CommConn.h>
#include <VehicleHalTypes.h>
#include <VehicleHalProto.pb.h>

#include <log/log.h>

#include <memory>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace fake {
/**
 * Emulates vehicle by providing controlling interface from host side either through ADB or Pipe.
 */
class VehicleVmcu final : public V2_0::impl::MessageProcessor {
  public:
    explicit VehicleVmcu(VirtVehicleHardware* hal);
    // For testing only.
    VehicleVmcu(
        std::unique_ptr<V2_0::impl::MessageSender> socketComm,
        std::unique_ptr<V2_0::impl::MessageSender> pipeComm,
        std::unique_ptr<V2_0::impl::MessageSender> vspiComm,
        VirtVehicleHardware* mHal);
    virtual ~VehicleVmcu();

    void doSetValueFromClient(const aidl::android::hardware::automotive::vehicle::VehiclePropValue& propValue);
    void processMessage(const vhal_proto::VmcuMessage& rxMsg,
                        vhal_proto::VmcuMessage* respMsg) override;

  private:
    friend class ConnectionThread;
    using VmcuMessage = vhal_proto::VmcuMessage;

    void doGetConfig(const VmcuMessage& rxMsg, VmcuMessage* respMsg);
    void doGetConfigAll(const VmcuMessage& rxMsg, VmcuMessage* respMsg);
    void doGetProperty(const VmcuMessage& rxMsg, VmcuMessage* respMsg);
    void doGetPropertyAll(const VmcuMessage& rxMsg, VmcuMessage* respMsg);
    void doSetProperty(const VmcuMessage& rxMsg, VmcuMessage* respMsg);
    void doDebug(const VmcuMessage& rxMsg, VmcuMessage* respMsg);
    void populateProtoVehicleConfig(
          const aidl::android::hardware::automotive::vehicle::VehiclePropConfig& cfg,
          vhal_proto::VehiclePropConfig* protoCfg);
    void populateProtoVehiclePropValue(
        const aidl::android::hardware::automotive::vehicle::VehiclePropValue& val,
        vhal_proto::VehiclePropValue* protoVal);

  private:
    VirtVehicleHardware* mHal;
    std::unique_ptr<V2_0::impl::MessageSender> mSocketComm;
    std::unique_ptr<V2_0::impl::MessageSender> mPipeComm;
    std::unique_ptr<V2_0::impl::MessageSender> mVSpiComm;
};

}  // namespace fake
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_VEHICLEVMCU_VEHICLEVMCU_H
