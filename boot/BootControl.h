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

#pragma once

#include <aidl/android/hardware/boot/BnBootControl.h>
#include <libboot_control/libboot_control.h>
#include <hardware/boot_control.h>

using ndk::ScopedAStatus;

namespace aidl::android::hardware::boot {

class BootControl final : public BnBootControl {
  public:
    BootControl();
    ScopedAStatus getActiveBootSlot(int32_t* _aidl_return) override;
    ScopedAStatus getCurrentSlot(int32_t* _aidl_return) override;
    ScopedAStatus getNumberSlots(int32_t* _aidl_return) override;
    ScopedAStatus getSnapshotMergeStatus(
        ::aidl::android::hardware::boot::MergeStatus* _aidl_return) override;
    ScopedAStatus getSuffix(int32_t in_slot, std::string* _aidl_return) override;
    ScopedAStatus isSlotBootable(int32_t in_slot, bool* _aidl_return) override;
    ScopedAStatus isSlotMarkedSuccessful(int32_t in_slot, bool* _aidl_return) override;
    ScopedAStatus markBootSuccessful() override;
    ScopedAStatus setActiveBootSlot(int32_t in_slot) override;
    ScopedAStatus setSlotAsUnbootable(int32_t in_slot) override;
    ScopedAStatus setSnapshotMergeStatus(
        ::aidl::android::hardware::boot::MergeStatus in_status) override;

  private:
    boot_control_module_t *mModule;
};

}  // namespace aidl::android::hardware::boot
