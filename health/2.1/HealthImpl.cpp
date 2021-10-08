#include <memory>
#include <string_view>
#include "hal_conversion.h"
#include <health/utils.h>
#include <health2impl/Health.h>
#include <hidl/Status.h>

using ::android::sp;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::health::InitHealthdConfig;
using ::android::hardware::health::V2_1::IHealth;
using ::android::hidl::base::V1_0::IBase;
using android::hardware::health::V1_0::hal_conversion::convertFromHealthInfo;
using android::hardware::health::V1_0::hal_conversion::convertToHealthInfo;

using namespace std::literals;

namespace android {
namespace hardware {
namespace health {
namespace V2_1 {
namespace implementation {

// android::hardware::health::V2_1::implementation::Health implements most
// defaults. Uncomment functions that you need to override.
class HealthImpl : public Health {
  public:
    HealthImpl(std::unique_ptr<healthd_config>&& config)
        : Health(std::move(config)) {}

    // A subclass can override this if these information should be retrieved
    // differently.
    // Return<void> getChargeCounter(getChargeCounter_cb _hidl_cb) override;
    // Return<void> getCurrentNow(getCurrentNow_cb _hidl_cb) override;
    // Return<void> getCurrentAverage(getCurrentAverage_cb _hidl_cb) override;
    // Return<void> getCapacity(getCapacity_cb _hidl_cb) override;
    // Return<void> getEnergyCounter(getEnergyCounter_cb _hidl_cb) override;
    // Return<void> getChargeStatus(getChargeStatus_cb _hidl_cb) override;
    // Return<void> getStorageInfo(getStorageInfo_cb _hidl_cb) override;
    // Return<void> getDiskStats(getDiskStats_cb _hidl_cb) override;
    // Return<void> getHealthInfo(getHealthInfo_cb _hidl_cb) override;

    // Functions introduced in Health HAL 2.1.
    // Return<void> getHealthConfig(getHealthConfig_cb _hidl_cb) override;
    // Return<void> getHealthInfo_2_1(getHealthInfo_2_1_cb _hidl_cb) override;
    // Return<void> shouldKeepScreenOn(shouldKeepScreenOn_cb _hidl_cb) override;

  protected:
    // A subclass can override this to modify any health info object before
    // returning to clients. This is similar to healthd_board_battery_update().
    // By default, it does nothing.
    void UpdateHealthInfo(HealthInfo* health_info) override;
};

void HealthImpl::UpdateHealthInfo(HealthInfo* health_info) {
    struct BatteryProperties props;
    convertFromHealthInfo(health_info->legacy.legacy, &props);
    healthd_board_battery_update(&props);
    convertToHealthInfo(&props, health_info->legacy.legacy);
    health_info->batteryCapacityLevel = android::hardware::health::V2_1::BatteryCapacityLevel::UNKNOWN;
    health_info->batteryChargeTimeToFullNowSeconds = 0;
    health_info->batteryFullChargeDesignCapacityUah = 0;
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace health
}  // namespace hardware
}  // namespace android

extern "C" IHealth* HIDL_FETCH_IHealth(const char* instance) {
    using ::android::hardware::health::V2_1::implementation::HealthImpl;
    if (instance != "default"sv) {
        return nullptr;
    }
    auto config = std::make_unique<healthd_config>();
    InitHealthdConfig(config.get());

    healthd_board_init(config.get());

    return new HealthImpl(std::move(config));
}
