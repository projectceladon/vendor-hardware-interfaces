/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *            http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.thermal@2.0-service.intel"

#include <errno.h>
#include <math.h>

#include <log/log.h>
#include <hidl/HidlTransportSupport.h>
#include "thread"
#include "hardware_thermal.h"
#include "Thermal.h"

using namespace android::hardware::thermal::V1_0;

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::sp;
using ::android::hardware::interfacesEqual;
using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;

float finalizeTemperature(float temperature) {
        return temperature == UNKNOWN_TEMPERATURE ? NAN : temperature;
}

Thermal::Thermal(thermal_module_t* module) : mModule(module) {
    if (mModule)
        mModule->init(mModule);

    if (mModule && mModule->getTemperatures) {
        mCheckThread = std::thread(&Thermal::CheckThermalServerity, this);
        mCheckThread.detach();
    }
}

void Thermal::CheckThermalServerity() {
    Temperature_2_0 temperature;
    std::vector<temperature_t> list;
    ssize_t size = 0;

    ALOGD("Start check temp thread.\n");
    while (1) {
        size = mModule->getTemperatures(mModule, nullptr, 0);
        if (size > 0) {
            list.resize(size);
            size = mModule->getTemperatures(mModule, list.data(), list.size());
            for (size_t i = 0; i < size; ++i) {
                if (list[i].current_value >= list[i].shutdown_threshold) {
                    temperature = (Temperature_2_0) {
                        .type = static_cast<TemperatureType>(list[i].type),
                        .name = list[i].name,
                        .value = finalizeTemperature(list[i].current_value),
                        .throttlingStatus = ThrottlingSeverity::SHUTDOWN};

                    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
                    for (auto cb : callbacks_) {
                        cb.callback->notifyThrottling(temperature);
                    }
                }
            }
        }
        sleep(1);
    }
}

// Methods from ::android::hardware::thermal::V1_0::IThermal follow.
Return<void> Thermal::getTemperatures(getTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    hidl_vec<Temperature_1_0> temperatures;
    std::vector<temperature_t> list;

    if (!mModule || !mModule->getTemperatures) {
        ALOGI("Func getTemperatures is not implemented in Thermal HAL.");
        return Void();
    }

    ssize_t size = mModule->getTemperatures(mModule, nullptr, 0);
    if (size > 0) {
        list.resize(size);
        size = mModule->getTemperatures(mModule, list.data(), list.size());
        if (size > 0) {
            temperatures.resize(list.size());
            for (size_t i = 0; i < list.size(); ++i) {
                temperatures[i] = (Temperature_1_0){
                    .type =
                     static_cast<::android::hardware::thermal::V1_0::TemperatureType>(list[i].type),
                    .name = list[i].name,
                    .currentValue =
                     finalizeTemperature(list[i].current_value),
                    .throttlingThreshold =
                     finalizeTemperature(list[i].throttling_threshold),
                    .shutdownThreshold =
                     finalizeTemperature(list[i].shutdown_threshold),
                    .vrThrottlingThreshold =
                     finalizeTemperature(list[i].vr_throttling_threshold)};
            }
        }
    }
    if (size <= 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = strerror(size);
    }
    _hidl_cb(status, temperatures);
    return Void();
}

Return<void> Thermal::getCpuUsages(getCpuUsages_cb _hidl_cb) {
    ThermalStatus status;
    hidl_vec<CpuUsage> cpuUsages;
    status.code = ThermalStatusCode::SUCCESS;

    if (!mModule || !mModule->getCpuUsages) {
        ALOGI("Func getCpuUsages is not implemented in Thermal HAL");
        _hidl_cb(status, cpuUsages);
        return Void();
    }

    ssize_t size = mModule->getCpuUsages(mModule, nullptr);
    if (size > 0) {
        std::vector<cpu_usage_t> list;
        list.resize(size);
        size = mModule->getCpuUsages(mModule, list.data());
        if (size > 0) {
            list.resize(size);
            cpuUsages.resize(size);
            for (size_t i = 0; i < list.size(); ++i) {
                cpuUsages[i] = (CpuUsage) {
                                .name = list[i].name,
                                .active = list[i].active,
                                .total = list[i].total,
                                .isOnline = list[i].is_online};
            }
        }
    }
    if (size <= 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = strerror(-size);
    }
    _hidl_cb(status, cpuUsages);
    return Void();
}

Return<void> Thermal::getCoolingDevices(getCoolingDevices_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    hidl_vec<CoolingDevice_1_0> coolingDevices;
    std::vector<cooling_device_t> list;

    if (!mModule || !mModule->getCoolingDevices) {
        ALOGI("Func getCoolingDevices is not implemented in Thermal HAL.");
        _hidl_cb(status, coolingDevices);
        return Void();
    }

    ssize_t size = mModule->getCoolingDevices(mModule, nullptr, 0);
    if (size > 0) {
        list.resize(size);
        size = mModule->getCoolingDevices(mModule, list.data(), list.size());
        if (size > 0) {
            list.resize(size);
            coolingDevices.resize(list.size());
            for (size_t i = 0; i < list.size(); ++i) {
                coolingDevices[i] = (CoolingDevice_1_0) {
                    .type =
                     static_cast<::android::hardware::thermal::V1_0::CoolingType>(list[i].type),
                    .name = list[i].name,
                    .currentValue = list[i].current_value};
            }
        }
    }
    if (size <= 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = strerror(-size);
    }
    _hidl_cb(status, coolingDevices);
    return Void();
}

// Methods from ::android::hardware::thermal::V2_0::IThermal follow.
Return<void> Thermal::getCurrentTemperatures(bool filterType, TemperatureType type,
                                             getCurrentTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    std::vector<Temperature_2_0> temperatures;
    std::vector<temperature_t> list;

    if (!mModule || !mModule->getTemperatures) {
        ALOGI("Func getTemperatures is not implemented in Thermal HAL.");
        return Void();
    }

    ssize_t size = mModule->getTemperatures(mModule, nullptr, 0);
    if (size > 0) {
        list.resize(size);
        size = mModule->getTemperatures(mModule, list.data(), list.size());
        if (size > 0) {
            if (!filterType) {
                temperatures.resize(list.size());
            }
            for (size_t i = 0; i < list.size(); ++i) {
                if (filterType) {
                    if (type == static_cast<TemperatureType>(list[i].type)) {
                        temperatures = {(Temperature_2_0) {
                            .type = static_cast<TemperatureType>(list[i].type),
                            .name = list[i].name,
                            .value = finalizeTemperature(list[i].current_value),
                            .throttlingStatus = ThrottlingSeverity::NONE}};
                        break;
                    }
                } else {
                    temperatures[i] = (Temperature_2_0) {
                        .type = static_cast<TemperatureType>(list[i].type),
                        .name = list[i].name,
                        .value = finalizeTemperature(list[i].current_value),
                        .throttlingStatus = ThrottlingSeverity::NONE};
                }
            }
        }
    }

    // Workaround for VTS Test
    if (filterType && temperatures.size() == 0) {
        temperatures = {(Temperature_2_0) {
                            .type = type,
                            .name = "FAKE_DATA",
                            .value = 0,
                            .throttlingStatus = ThrottlingSeverity::NONE}};
    }

    size = temperatures.size();
    if (size <= 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Failed to read data";
    }
    _hidl_cb(status, temperatures);
    return Void();
}

static const TemperatureThreshold kTempThreshold = {
        .type = TemperatureType::SKIN,
        .name = "test temperature sensor",
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 48.0, NAN, NAN, 60.0}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .vrThrottlingThreshold = 49.0,
};

Return<void> Thermal::getTemperatureThresholds(bool filterType, TemperatureType type,
                                               getTemperatureThresholds_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    std::vector<TemperatureThreshold> temperature_thresholds;
    if (filterType && type != kTempThreshold.type) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Failed to read data";
    } else {
        temperature_thresholds = {kTempThreshold};
    }
    _hidl_cb(status, temperature_thresholds);
    return Void();
}

Return<void> Thermal::getCurrentCoolingDevices(bool filterType, CoolingType type,
                                               getCurrentCoolingDevices_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    std::vector<CoolingDevice_2_0> cooling_devices;
    std::vector<cooling_device_t> list;

    if (!mModule || !mModule->getCoolingDevices) {
        ALOGI("Func getCoolingDevices is not implemented in Thermal HAL.");
        _hidl_cb(status, cooling_devices);
        return Void();
    }

    ssize_t size = mModule->getCoolingDevices(mModule, nullptr, 0);
    if (size > 0) {
        list.resize(size);
        size = mModule->getCoolingDevices(mModule, list.data(), list.size());
        if (size > 0) {
            if (!filterType) {
                cooling_devices.resize(list.size());
            }
            for (size_t i = 0; i < list.size(); ++i) {
                if (filterType) {
                    if (type == static_cast<CoolingType>(list[i].type)) {
                        cooling_devices = {(CoolingDevice_2_0) {
                            .type =
                             static_cast<CoolingType>(list[i].type),
                            .name = list[i].name,
                            .value = static_cast<uint64_t>(list[i].current_value)}};
                        break;
                    }
                } else {
                    cooling_devices[i] = (CoolingDevice_2_0) {
                        .type =
                         static_cast<CoolingType>(list[i].type),
                        .name = list[i].name,
                        .value = static_cast<uint64_t>(list[i].current_value)};
                }
            }
        }
    }

    // Workaround for VTS Test
    if (filterType && cooling_devices.size() == 0) {
        cooling_devices = {(CoolingDevice_2_0) {
                            .type = type,
                            .name = "FAKE_DATA",
                            .value = 0}};
    }

    size = cooling_devices.size();
    if (size <= 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Failed to read data";
    }
    _hidl_cb(status, cooling_devices);
    return Void();
}

Return<void> Thermal::registerThermalChangedCallback(const sp<IThermalChangedCallback>& callback,
                                                     bool filterType, TemperatureType type,
                                                     registerThermalChangedCallback_cb _hidl_cb) {
    ThermalStatus status;
    if (callback == nullptr) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Invalid nullptr callback";
        ALOGE("%s", status.debugMessage.c_str());
        _hidl_cb(status);
        return Void();
    } else {
        status.code = ThermalStatusCode::SUCCESS;
    }
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    if (std::any_of(callbacks_.begin(), callbacks_.end(), [&](const CallbackSetting& c) {
            return interfacesEqual(c.callback, callback);
        })) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Same callback interface registered already";
        ALOGE("%s", status.debugMessage.c_str());
    } else {
        callbacks_.emplace_back(callback, filterType, type);
        ALOGI("A callback has been registered to ThermalHAL, isFilter: %d Type: %s", filterType,
              android::hardware::thermal::V2_0::toString(type).c_str());
    }
    _hidl_cb(status);
    return Void();
}

Return<void> Thermal::unregisterThermalChangedCallback(
    const sp<IThermalChangedCallback>& callback, unregisterThermalChangedCallback_cb _hidl_cb) {
    ThermalStatus status;
    if (callback == nullptr) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Invalid nullptr callback";
        ALOGE("%s", status.debugMessage.c_str());
        _hidl_cb(status);
        return Void();
    } else {
        status.code = ThermalStatusCode::SUCCESS;
    }
    bool removed = false;
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    callbacks_.erase(
        std::remove_if(callbacks_.begin(), callbacks_.end(),
                       [&](const CallbackSetting& c) {
                           if (interfacesEqual(c.callback, callback)) {
                               ALOGI("A callback has been registered to ThermalHAL, isFilter: %d Type: %s",
                                     c.is_filter_type,
                                     android::hardware::thermal::V2_0::toString(c.type).c_str());
                               removed = true;
                               return true;
                           }
                           return false;
                       }),
        callbacks_.end());
    if (!removed) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "The callback was not registered before";
        ALOGE("%s", status.debugMessage.c_str());
    }
    _hidl_cb(status);
    return Void();
}

IThermal* HIDL_FETCH_IThermal(const char* /* name */) {
    thermal_module_t* module;
    status_t err = hw_get_module(THERMAL_HARDWARE_MODULE_ID,
                                                             const_cast<hw_module_t const**>(
                                                                     reinterpret_cast<hw_module_t**>(&module)));
    if (err || !module) {
        ALOGE("Couldn't load %s module (%s)", THERMAL_HARDWARE_MODULE_ID,
                    strerror(-err));
    }

    if (err == 0 && module->common.methods->open) {
        struct hw_device_t* device;
        err = module->common.methods->open(&module->common,
                                                                             THERMAL_HARDWARE_MODULE_ID, &device);
        if (err) {
            ALOGE("Couldn't open %s module (%s)", THERMAL_HARDWARE_MODULE_ID,
                        strerror(-err));
        } else {
            return new Thermal(reinterpret_cast<thermal_module_t*>(device));
        }
    }
    return new Thermal(module);
}

}    // namespace implementation
}    // namespace V1_1
}    // namespace thermal
}    // namespace hardware
}    // namespace android
