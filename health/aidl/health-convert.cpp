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

#include "health-convert.h"

namespace aidl::android::hardware::health {

void convertHealthinfoToBattery(const HealthInfo& info, struct ::android::BatteryProperties* p) {
    p->chargerAcOnline = info.chargerAcOnline;
    p->chargerUsbOnline = info.chargerUsbOnline;
    p->chargerWirelessOnline = info.chargerWirelessOnline;
    p->chargerDockOnline = info.chargerDockOnline;
    p->maxChargingCurrent = info.maxChargingCurrentMicroamps;
    p->maxChargingVoltage = info.maxChargingVoltageMicrovolts;
    p->batteryStatus = static_cast<int>(info.batteryStatus);
    p->batteryHealth = static_cast<int>(info.batteryHealth);
    p->batteryPresent = info.batteryPresent;
    p->batteryLevel = info.batteryLevel;
    p->batteryVoltage = info.batteryVoltageMillivolts;
    p->batteryTemperature = info.batteryTemperatureTenthsCelsius;
    p->batteryCurrent = info.batteryCurrentMicroamps;
    p->batteryCycleCount = info.batteryCycleCount;
    p->batteryFullCharge = info.batteryFullChargeUah;
    p->batteryChargeCounter = info.batteryChargeCounterUah;
    p->batteryTechnology = ::android::String8(info.batteryTechnology.c_str());
}
void convertHealthinfoFromoBattery(const struct ::android::BatteryProperties* p, HealthInfo& info) {
    info.chargerAcOnline = p->chargerAcOnline;
    info.chargerUsbOnline = p->chargerUsbOnline;
    info.chargerWirelessOnline = p->chargerWirelessOnline;
    info.chargerDockOnline = p->chargerDockOnline;
    info.maxChargingCurrentMicroamps = p->maxChargingCurrent;
    info.maxChargingVoltageMicrovolts = p->maxChargingVoltage;
    info.batteryStatus = static_cast<BatteryStatus>(p->batteryStatus);
    info.batteryHealth = static_cast<BatteryHealth>(p->batteryHealth);
    info.batteryPresent = p->batteryPresent;
    info.batteryLevel = p->batteryLevel;
    info.batteryVoltageMillivolts = p->batteryVoltage;
    info.batteryTemperatureTenthsCelsius = p->batteryTemperature;
    info.batteryCurrentMicroamps = p->batteryCurrent;
    info.batteryCycleCount = p->batteryCycleCount;
    info.batteryFullChargeUah = p->batteryFullCharge;
    info.batteryChargeCounterUah = p->batteryChargeCounter;
    info.batteryTechnology = p->batteryTechnology;
}
}  // namespace aidl::android::hardware::health
