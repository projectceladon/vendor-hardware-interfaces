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

#define LOG_TAG "thermal_service_intel"

#include "Thermal.h"

#include <cmath>
#include <android-base/logging.h>
#include <log/log.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <linux/vm_sockets.h>
#include <regex>
#include <dirent.h>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <map>
#include <system_error>
#include <charconv>

#define SYSFS_TEMPERATURE_CPU       "/sys/class/thermal/thermal_zone0/temp"
#define CPU_NUM_MAX                 Thermal::getNumCpu()
#define CPU_USAGE_PARAS_NUM         5
#define CPU_USAGE_FILE              "/proc/stat"
#define CPU_ONLINE_FILE             "/sys/devices/system/cpu/online"
#define TEMP_UNIT                   1000.0
#define THERMAL_PORT                14096
#define MAX_ZONES                   40

namespace aidl::android::hardware::thermal::impl::example {

using ndk::ScopedAStatus;

namespace {

bool interfacesEqual(const std::shared_ptr<::ndk::ICInterface>& left,
                     const std::shared_ptr<::ndk::ICInterface>& right) {
    if (left == nullptr || right == nullptr || !left->isRemote() || !right->isRemote()) {
        return left == right;
    }
    return left->asBinder() == right->asBinder();
}

}  // namespace

static const char *THROTTLING_SEVERITY_LABEL[] = {
                                                  "NONE",
                                                  "LIGHT",
                                                  "MODERATE",
                                                  "SEVERE",
                                                  "CRITICAL",
                                                  "EMERGENCY",
                                                  "SHUTDOWN"};

static bool is_vsock_present;

struct zone_info {
	uint32_t temperature;
	uint32_t trip_0;
	uint32_t trip_1;
	uint32_t trip_2;
	uint16_t number;
	int16_t type;
};

struct header {
	uint8_t intelipcid[9];
	uint16_t notifyid;
	uint16_t length;
};

struct temp_info {
    int16_t type;
    uint32_t temp;
};

Temperature kTemp_1_0,kTemp_2_0;
TemperatureThreshold kTempThreshold;


static void parse_temp_info(struct temp_info *t)
{
    int temp = t->temp / TEMP_UNIT;
    switch(t->type) {
        case 0:
            kTemp_2_0.value = t->temp / TEMP_UNIT;
            break;
        default:
            break;
    }
}

static void parse_zone_info(struct zone_info *zone)
{
    int temp = zone->temperature;
    kTempThreshold.hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, 99, 108}};
         switch (zone->type) {
              case 0:
                    kTempThreshold.hotThrottlingThresholds[5] = zone->trip_0 / TEMP_UNIT;
                    kTempThreshold.hotThrottlingThresholds[6] = zone->trip_1 / TEMP_UNIT;
                    kTemp_2_0.value = zone->temperature / TEMP_UNIT;;
                    break;
              default:
                    break;
         }
}

static int connect_vsock(int *vsock_fd)
{
     struct sockaddr_vm sa = {
        .svm_family = AF_VSOCK,
        .svm_cid = VMADDR_CID_HOST,
        .svm_port = THERMAL_PORT,
    };
    *vsock_fd = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (*vsock_fd < 0) {
            ALOGI("Thermal HAL socket init failed\n");
            return -1;
    }

    if (connect(*vsock_fd, (struct sockaddr*)&sa, sizeof(sa)) != 0) {
            ALOGI("Thermal HAL connect failed\n");
            close(*vsock_fd);
            return -1;
    }
    return 0;
}

static int recv_vsock(int *vsock_fd)
{
    char msgbuf[1024];
    int ret;
    struct header *head = (struct header *)malloc(sizeof(struct header));
    if (!head)
        return -ENOMEM;
    memset(msgbuf, 0, sizeof(msgbuf));
    ret = recv(*vsock_fd, msgbuf, sizeof(msgbuf), MSG_DONTWAIT);
    if (ret < 0 && errno == EBADF) {
        if (connect_vsock(vsock_fd) == 0)
            ret = recv(*vsock_fd, msgbuf, sizeof(msgbuf), MSG_DONTWAIT);
    }
    if (ret > 0) {
        memcpy(head, msgbuf, sizeof(struct header));
        int num_zones = 0;
        int ptr = 0;
        int i;
        if (head->notifyid == 1) {
            num_zones = head->length / sizeof(struct zone_info);
            num_zones = num_zones < MAX_ZONES ? num_zones : MAX_ZONES;
            ptr += sizeof(struct header);
            struct zone_info *zinfo = (struct zone_info *)malloc(sizeof(struct zone_info));
            if (!zinfo) {
                free(head);
                return -ENOMEM;
            }
            for (i = 0; i < num_zones; i++) {
                memcpy(zinfo, msgbuf + ptr, sizeof(struct zone_info));
                parse_zone_info(zinfo);
                ptr = ptr + sizeof(struct zone_info);
            }
            free(zinfo);
        } else if (head->notifyid == 2) {
            ptr = sizeof(struct header); //offset of num_zones field
            struct temp_info *tinfo = (struct temp_info *)malloc(sizeof(struct temp_info));
            if (!tinfo) {
                free(head);
                return -ENOMEM;
            }
            memcpy(&num_zones, msgbuf + ptr, sizeof(num_zones));
            num_zones = num_zones < MAX_ZONES ? num_zones : MAX_ZONES;
            ptr += 4; //offset of first zone type info
            for (i = 0; i < num_zones; i++) {
                memcpy(&tinfo->type, msgbuf + ptr, sizeof(tinfo->type));
                memcpy(&tinfo->temp, msgbuf + ptr + sizeof(tinfo->type), sizeof(tinfo->temp));
                parse_temp_info(tinfo);
                ptr += sizeof(tinfo->type) + sizeof(tinfo->temp);
            }
            free(tinfo);
        }
    }
    free(head);
    return 0;
}

static int get_soc_pkg_temperature(float* temp)
{
    float fTemp = 0;
    int len = 0;
    FILE *file = NULL;

    file = fopen(SYSFS_TEMPERATURE_CPU, "r");

    if (file == NULL) {
        ALOGE("%s: failed to open file: %s", __func__, strerror(errno));
        return -errno;
    }

    len = fscanf(file, "%f", &fTemp);
    if (len < 0) {
        ALOGE("%s: failed to read file: %s", __func__, strerror(errno));
        fclose(file);
        return -errno;
    }

    fclose(file);
    *temp = fTemp / TEMP_UNIT;

    return 0;
}

// --- Constants ---
const float TEMP_UNIT_DIVISOR = 1000.0f;

// --- Structures: CoreTemperature and AllCpuTemperatures ---
struct CoreTemperature {
    int id = -1;
    std::string label;
    float temperature = NAN;
    CoreTemperature(int i, const std::string& l, float t) : id(i), label(l), temperature(t) {}
};

struct AllCpuTemperatures {
    float package_temperature = NAN;
    std::string package_label;
    std::vector<CoreTemperature> core_temps;
    int detected_core_count = 0;
    float max_core_temp = NAN;
    float max_overall_temp = NAN;
    void update_max_temps() {
        max_core_temp = NAN;
        max_overall_temp = NAN;
        if (!isnan(package_temperature)) {
            max_overall_temp = package_temperature;
        }
        for (const auto& core_temp : core_temps) {
            if (!isnan(core_temp.temperature)) {
                if (isnan(max_core_temp) || core_temp.temperature > max_core_temp) {
                    max_core_temp = core_temp.temperature;
                }
                if (isnan(max_overall_temp) || core_temp.temperature > max_overall_temp) {
                    max_overall_temp = core_temp.temperature;
                }
            }
        }
        detected_core_count = core_temps.size();
    }
};

// --- Cached Sensor Information ---
struct DiscoveredSensor {
    int id = -1;
    std::string label;
    std::string input_file_path;
    bool is_package_sensor = false;
};

static std::vector<DiscoveredSensor> S_CACHED_SENSORS;
static std::vector<std::string> S_CACHED_HWMON_DIRS;
static bool S_CACHE_INITIALIZED = false;

// --- Function: find_hwmon_dirs_for_device  ---
std::vector<std::string> find_hwmon_dirs_for_device(const std::string& device_name, bool enable_logging) {
    std::vector<std::string> hwmon_paths;
    const std::string hwmon_base_dir = "/sys/class/hwmon/";
    DIR* dir = opendir(hwmon_base_dir.c_str());
    if (!dir) {
        if (enable_logging) ALOGE("Failed to open directory: %s - %s", hwmon_base_dir.c_str(), strerror(errno));
        return hwmon_paths;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR || entry->d_type == DT_LNK) {
            std::string dirname = entry->d_name;
            if (dirname == "." || dirname == "..") {
                continue;
            }
            std::string name_file_path = hwmon_base_dir + dirname + "/name";
            std::ifstream name_file(name_file_path);
            if (name_file.is_open()) {
                std::string current_device_name;
                if (std::getline(name_file, current_device_name) && current_device_name == device_name) {
                    hwmon_paths.push_back(hwmon_base_dir + dirname);
                }
                name_file.close();
            }
        }
    }
    closedir(dir);
    return hwmon_paths;
}

// --- Function: clear_cache ---
void clear_cache() {
    S_CACHED_SENSORS.clear();
    S_CACHED_HWMON_DIRS.clear();
    S_CACHE_INITIALIZED = false;
    ALOGW("Temperature sensor cache has been cleared.");
}


AllCpuTemperatures get_cpu_temperatures(bool enable_logging = true) {
    AllCpuTemperatures all_temps;
    bool read_from_cache_successful = true;

    if (S_CACHE_INITIALIZED) {
        for (const auto& sensor_info : S_CACHED_SENSORS) {
            std::ifstream file(sensor_info.input_file_path);
            float temp_raw;
            if (file.is_open() && (file >> temp_raw)) {
                float temp_celsius = temp_raw / TEMP_UNIT_DIVISOR;
                if (sensor_info.is_package_sensor) {
                    if (isnan(all_temps.package_temperature) || temp_celsius > all_temps.package_temperature) {
                         all_temps.package_temperature = temp_celsius;
                         all_temps.package_label = sensor_info.label;
                    }
                } else {
                    all_temps.core_temps.emplace_back(sensor_info.id, sensor_info.label, temp_celsius);
                }
            } else {
                if (enable_logging) ALOGE("Failed to read from cached path: %s. Invalidating cache.", sensor_info.input_file_path.c_str());
                clear_cache();
                read_from_cache_successful = false;
                break;
            }
            file.close();
        }
        if (read_from_cache_successful) {
             std::sort(all_temps.core_temps.begin(), all_temps.core_temps.end(),
              [](const CoreTemperature& a, const CoreTemperature& b) {
                  return a.id < b.id;
              });
            all_temps.update_max_temps();
            return all_temps;
        }
    }

    if (enable_logging && !read_from_cache_successful) {
        ALOGI("Cache miss or invalidation. Performing full sensor discovery...");
    } else if (enable_logging && !S_CACHE_INITIALIZED) { // Ensure this condition is met for initial discovery log
        ALOGI("Cache not initialized. Performing initial sensor discovery...");
    }

    S_CACHED_SENSORS.clear(); // Clear before repopulating
    S_CACHED_HWMON_DIRS.clear();

    S_CACHED_HWMON_DIRS = find_hwmon_dirs_for_device("coretemp", enable_logging);
    if (S_CACHED_HWMON_DIRS.empty()){
        if (enable_logging) ALOGE("No coretemp hwmon directories found via discovery or fallback.");
        S_CACHE_INITIALIZED = false;
        return all_temps;
   }

    std::regex temp_file_regex("^temp([0-9]+)_(input|label)$");
    std::regex core_label_regex("^Core\\s+([0-9]+)$");
    std::regex package_label_regex("^(Package id [0-9]+|Physical id [0-9]+)$");

    std::map<int, std::string> discovered_labels;
    std::map<int, std::string> discovered_input_paths;

    for (const std::string& hwmon_dir : S_CACHED_HWMON_DIRS) {
        DIR* dir = opendir(hwmon_dir.c_str());
        if (!dir) {
            if (enable_logging) ALOGE("Failed to open hwmon directory: %s - %s", hwmon_dir.c_str(), strerror(errno));
            continue;
        }
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_REG) {
                std::string filename = entry->d_name;
                std::smatch match;
                if (std::regex_match(filename, match, temp_file_regex)) {
                    std::string index_str = match[1].str();
                    int parsed_index;
                    auto fc_res_idx = std::from_chars(index_str.data(), index_str.data() + index_str.size(), parsed_index);

                    if (fc_res_idx.ec == std::errc() && fc_res_idx.ptr == index_str.data() + index_str.size()) {
                        // Successfully parsed index
                        std::string type = match[2].str();
                        std::string full_path = hwmon_dir + "/" + filename;
                        if (type == "label") {
                            std::ifstream file(full_path);
                            if (file.is_open()) {
                                std::string label_content;
                                if (std::getline(file, label_content)) {
                                    discovered_labels[parsed_index] = label_content;
                                }
                                file.close();
                            }
                        } else if (type == "input") {
                            discovered_input_paths[parsed_index] = full_path;
                        }
                    } else {
                        if (enable_logging) ALOGW("Failed to parse index from filename fragment '%s' in %s. Error: %d",
                                                  index_str.c_str(), filename.c_str(), static_cast<int>(fc_res_idx.ec));
                    }
                }
            }
        }
        closedir(dir);
    }

    for (auto const& [index_val, label_content] : discovered_labels) { // index_val is used here
        if (discovered_input_paths.count(index_val)) {
            std::string input_path = discovered_input_paths[index_val];
            std::ifstream temp_file(input_path);
            float temp_raw;
            if (temp_file.is_open() && (temp_file >> temp_raw)) {
                float temp_celsius = temp_raw / TEMP_UNIT_DIVISOR;
                DiscoveredSensor current_sensor_info;
                current_sensor_info.label = label_content;
                current_sensor_info.input_file_path = input_path;
                std::smatch core_match;
                if (std::regex_match(label_content, core_match, core_label_regex)) {
                    std::string core_id_str = core_match[1].str();
                    int parsed_core_id;
                    auto fc_res_core_id = std::from_chars(core_id_str.data(), core_id_str.data() + core_id_str.size(), parsed_core_id);

                    if (fc_res_core_id.ec == std::errc() && fc_res_core_id.ptr == core_id_str.data() + core_id_str.size()) {
                        current_sensor_info.id = parsed_core_id;
                        current_sensor_info.is_package_sensor = false;
                        all_temps.core_temps.emplace_back(current_sensor_info.id, label_content, temp_celsius);
                        S_CACHED_SENSORS.push_back(current_sensor_info);
                    } else {
                         if (enable_logging) ALOGW("Failed to parse core ID from label '%s' (value '%s'). Error: %d",
                                                  label_content.c_str(), core_id_str.c_str(), static_cast<int>(fc_res_core_id.ec));
                    }
                } else if (std::regex_match(label_content, package_label_regex)) {
                    current_sensor_info.is_package_sensor = true;
                    if (isnan(all_temps.package_temperature) || temp_celsius > all_temps.package_temperature) {
                        all_temps.package_temperature = temp_celsius;
                        all_temps.package_label = label_content;
                    }
                    S_CACHED_SENSORS.push_back(current_sensor_info);
                }
            } else {
                 if (enable_logging) ALOGW("Failed to read temp from discovered path: %s", input_path.c_str());
            }
            temp_file.close();
        }
    }

    if (!S_CACHED_SENSORS.empty()) {
        S_CACHE_INITIALIZED = true;
        if (enable_logging) ALOGI("Sensor cache populated with %zu entries.", S_CACHED_SENSORS.size());
    } else {
        S_CACHE_INITIALIZED = false;
        if (enable_logging) ALOGW("No sensors were successfully discovered to populate cache.");
    }

    std::sort(all_temps.core_temps.begin(), all_temps.core_temps.end(),
              [](const CoreTemperature& a, const CoreTemperature& b) {
                  return a.id < b.id;
              });
    all_temps.update_max_temps();
    return all_temps;
}

Thermal::Thermal() {
    mCheckThread = std::thread(&Thermal::CheckThermalServerity, this);
    mCheckThread.detach();
    mNumCpu = get_nprocs();
}
void Thermal::CheckThermalServerity() {
    float temp = NAN;
    int res = -1;
    int vsock_fd;
    bool is_pkg_temp_present = false;
    bool first_iteration_logging_enabled = true;

    kTempThreshold.hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, 99, 108}};
    ALOGI("Start check temp thread.\n");

    if (!connect_vsock(&vsock_fd))
        is_vsock_present = true;
    if (access(SYSFS_TEMPERATURE_CPU, F_OK) == 0) {
        is_pkg_temp_present = true;
        ALOGI("SOC package temperature is available\n");
    }

    while (1) {
        if (is_vsock_present) {
            res = 0;
            recv_vsock(&vsock_fd);
        } else {
            if (is_pkg_temp_present) {
                res = get_soc_pkg_temperature(&temp);
            } else {
                // If SOC package temperature is not available, get max core temperature
                AllCpuTemperatures temps = get_cpu_temperatures(first_iteration_logging_enabled);
                if (first_iteration_logging_enabled) {
                    first_iteration_logging_enabled = false; // Disable detailed logging after first successful run
                }
                temp = temps.max_core_temp;
                res = 0;
            }
            kTemp_2_0.value = temp;
        }
        if (res) {
            ALOGE("Can not get temperature of type %d", kTemp_1_0.type);
        } else {
            ALOGI("Size of kTempThreshold.hotThrottlingThresholds=%f, kTemp_2_0.value=%f",kTempThreshold.hotThrottlingThresholds[5], kTemp_2_0.value);
            for (size_t i = kTempThreshold.hotThrottlingThresholds.size() - 1; i > 0; i--) {
                if (kTemp_2_0.value >= kTempThreshold.hotThrottlingThresholds[i]) {
                    kTemp_2_0.type = TemperatureType::CPU;
                    kTemp_2_0.name = "TCPU";
                    ALOGI("CheckThermalServerity: hit ThrottlingSeverity %s, temperature is %f",
                          THROTTLING_SEVERITY_LABEL[i], kTemp_2_0.value);
                    kTemp_2_0.throttlingStatus = (ThrottlingSeverity)i;
                    {
                        std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
                        for (auto& cb : callbacks_) {
                            ::ndk::ScopedAStatus ret = cb.callback->notifyThrottling(kTemp_2_0);
                            if (!ret.isOk()) {
                                ALOGE("Thermal callback FAILURE");
                            }
                            else {
                                ALOGE("Thermal callback SUCCESS");
                            }
                        }
                    }
                }
            }
         }
        sleep(1);
    }
}


ScopedAStatus Thermal::getCoolingDevices(std::vector<CoolingDevice>* out_devices ) {
    LOG(VERBOSE) << __func__;
    fillCoolingDevices(out_devices);
    return ScopedAStatus::ok();
}

ScopedAStatus  Thermal::fillCoolingDevices(std::vector<CoolingDevice>*  out_devices) {
    std::vector<CoolingDevice> ret;

    *out_devices = ret;
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getCoolingDevicesWithType(CoolingType in_type,
                                                 std::vector<CoolingDevice>* /* out_devices */) {
    LOG(VERBOSE) << __func__ << " CoolingType: " << static_cast<int32_t>(in_type);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperatures(std::vector<Temperature>*  out_temperatures) {
    LOG(VERBOSE) << __func__;
    fillTemperatures(out_temperatures);
    return ScopedAStatus::ok();
}

ScopedAStatus  Thermal::fillTemperatures(std::vector<Temperature>*  out_temperatures) {
    *out_temperatures = {};
    std::vector<Temperature> ret;

    //CPU temperature
    kTemp_2_0.type = TemperatureType::CPU;
    kTemp_2_0.name = "TCPU";
    ret.emplace_back(kTemp_2_0);

    *out_temperatures = ret;
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperaturesWithType(TemperatureType in_type,
                                               std::vector<Temperature>* out_temperatures) {
    LOG(VERBOSE) << __func__ << " TemperatureType: " << static_cast<int32_t>(in_type);

    *out_temperatures={};

    if (in_type == TemperatureType::CPU) {
        kTemp_2_0.type = TemperatureType::CPU;
        kTemp_2_0.name = "TCPU";

        out_temperatures->emplace_back(kTemp_2_0);
    }

    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperatureThresholds(
        std::vector<TemperatureThreshold>* out_temperatureThresholds ) {
    LOG(VERBOSE) << __func__;
    *out_temperatureThresholds = {};
    std::vector<TemperatureThreshold> ret;

    kTempThreshold.type = TemperatureType::CPU;
    kTempThreshold.name = "TCPU";
    kTempThreshold.hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, 99, 108}};
    kTempThreshold.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}};
    ret.emplace_back(kTempThreshold);

    *out_temperatureThresholds = ret;
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperatureThresholdsWithType(
        TemperatureType in_type,
        std::vector<TemperatureThreshold>* out_temperatureThresholds) {
    LOG(VERBOSE) << __func__ << " TemperatureType: " << static_cast<int32_t>(in_type);

    *out_temperatureThresholds = {};

    if (in_type == TemperatureType::CPU) {
        kTempThreshold.type = TemperatureType::CPU;
        kTempThreshold.name = "TCPU";
        kTempThreshold.hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, 99, 108}};
        kTempThreshold.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}};
        out_temperatureThresholds->emplace_back(kTempThreshold);
    }

    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& in_callback) {
    LOG(VERBOSE) << __func__ << " IThermalChangedCallback: " << in_callback;
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
        if (std::any_of(thermal_callbacks_.begin(), thermal_callbacks_.end(),
                        [&](const std::shared_ptr<IThermalChangedCallback>& c) {
                            return interfacesEqual(c, in_callback);
                        })) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback already registered");
        }
        thermal_callbacks_.push_back(in_callback);
    }

    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    if (std::any_of(callbacks_.begin(), callbacks_.end(), [&](const CallbackSetting &c) {
            return interfacesEqual(c.callback, in_callback);
        })) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Callback already registered");
    }
    callbacks_.emplace_back(in_callback,false,TemperatureType::UNKNOWN);


    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::registerThermalChangedCallbackWithType(
        const std::shared_ptr<IThermalChangedCallback>& in_callback, TemperatureType in_type) {
    LOG(VERBOSE) << __func__ << " IThermalChangedCallback: " << in_callback
                 << ", TemperatureType: " << static_cast<int32_t>(in_type);
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
        if (std::any_of(thermal_callbacks_.begin(), thermal_callbacks_.end(),
                        [&](const std::shared_ptr<IThermalChangedCallback>& c) {
                            return interfacesEqual(c, in_callback);
                        })) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback already registered");
        }
        thermal_callbacks_.push_back(in_callback);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::unregisterThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& in_callback) {
    LOG(VERBOSE) << __func__ << " IThermalChangedCallback: " << in_callback;
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
        bool removed = false;
        thermal_callbacks_.erase(
                std::remove_if(thermal_callbacks_.begin(), thermal_callbacks_.end(),
                               [&](const std::shared_ptr<IThermalChangedCallback>& c) {
                                   if (interfacesEqual(c, in_callback)) {
                                       removed = true;
                                       return true;
                                   }
                                   return false;
                               }),
                thermal_callbacks_.end());
        if (!removed) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback wasn't registered");
        }
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::registerCoolingDeviceChangedCallbackWithType(
        const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback, CoolingType in_type) {
    LOG(VERBOSE) << __func__ << " ICoolingDeviceChangedCallback: " << in_callback
                 << ", CoolingType: " << static_cast<int32_t>(in_type);
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(cdev_callback_mutex_);
        if (std::any_of(cdev_callbacks_.begin(), cdev_callbacks_.end(),
                        [&](const std::shared_ptr<ICoolingDeviceChangedCallback>& c) {
                            return interfacesEqual(c, in_callback);
                        })) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback already registered");
        }
        cdev_callbacks_.push_back(in_callback);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::unregisterCoolingDeviceChangedCallback(
        const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback) {
    LOG(VERBOSE) << __func__ << " ICoolingDeviceChangedCallback: " << in_callback;
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(cdev_callback_mutex_);
        bool removed = false;
        cdev_callbacks_.erase(
                std::remove_if(cdev_callbacks_.begin(), cdev_callbacks_.end(),
                               [&](const std::shared_ptr<ICoolingDeviceChangedCallback>& c) {
                                   if (interfacesEqual(c, in_callback)) {
                                       removed = true;
                                       return true;
                                   }
                                   return false;
                               }),
                cdev_callbacks_.end());
        if (!removed) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback wasn't registered");
        }
    }
    return ScopedAStatus::ok();
}
}  // namespace aidl::android::hardware::thermal::impl::example
