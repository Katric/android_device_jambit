#define LOG_TAG "GpioFakeVehicleHardware"
#define ATRACE_TAG ATRACE_TAG_HAL

#include "GpioFakeVehicleHardware.h"
#include "DemonstratorJsonConfigLoader.h"

#include <FakeObd2Frame.h>
#include <JsonFakeValueGenerator.h>
#include <LinearFakeValueGenerator.h>
#include <PropertyUtils.h>
#include <TestPropertyUtils.h>
#include <VehicleHalTypes.h>
#include <VehicleUtils.h>

#include <softPwm.h>
#include <wiringPi.h>

#include <android-base/file.h>
#include <android-base/parsedouble.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>
#include <utils/Trace.h>

#include <dirent.h>
#include <inttypes.h>
#include <sys/types.h>
#include <fstream>
#include <regex>
#include <unordered_set>
#include <vector>

namespace android {
    namespace hardware {
        namespace automotive {
            namespace vehicle {
                namespace fake {
                    namespace {

                        using ::aidl::android::hardware::automotive::vehicle::ErrorState;
                        using ::aidl::android::hardware::automotive::vehicle::GetValueRequest;
                        using ::aidl::android::hardware::automotive::vehicle::GetValueResult;
                        using ::aidl::android::hardware::automotive::vehicle::RawPropValues;
                        using ::aidl::android::hardware::automotive::vehicle::SetValueRequest;
                        using ::aidl::android::hardware::automotive::vehicle::SetValueResult;
                        using ::aidl::android::hardware::automotive::vehicle::StatusCode;
                        using ::aidl::android::hardware::automotive::vehicle::VehicleApPowerStateReport;
                        using ::aidl::android::hardware::automotive::vehicle::VehicleApPowerStateReq;
                        using ::aidl::android::hardware::automotive::vehicle::VehicleHwKeyInputAction;
                        using ::aidl::android::hardware::automotive::vehicle::VehiclePropConfig;
                        using ::aidl::android::hardware::automotive::vehicle::VehicleProperty;
                        using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyAccess;
                        using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyGroup;
                        using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyStatus;
                        using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyType;
                        using ::aidl::android::hardware::automotive::vehicle::VehiclePropValue;
                        using ::aidl::android::hardware::automotive::vehicle::VehicleUnit;

                        using ::android::base::EqualsIgnoreCase;
                        using ::android::base::Error;
                        using ::android::base::GetIntProperty;
                        using ::android::base::ParseFloat;
                        using ::android::base::Result;
                        using ::android::base::ScopedLockAssertion;
                        using ::android::base::StartsWith;
                        using ::android::base::StringPrintf;

                        // Directory, that contains the vendor VHAL properties (DemonstratorVehicleHalProperties.json).
                        constexpr char VENDOR_PROPERTY_CONFIG_DIR[] = "/vendor/etc/automotive/vhaloverride/";
                    }

                    // a bit ugly, but it's not possible to pass a member function to wiringPiISR.
                    // This is a workaround together with gGpioFakeVehicleHardware.
                    //GpioFakeVehicleHardware* gGpioFakeVehicleHardware = nullptr;
                    //
                    //void globalBatteryChangeHandler() {
                    //    if (gGpioFakeVehicleHardware != nullptr) {
                    //        gGpioFakeVehicleHardware->handleBatteryChange();
                    //    }
                    //}

                    GpioFakeVehicleHardware::GpioFakeVehicleHardware()
                            : FakeVehicleHardware()//, mPendingSetValueRequests(this)
                            {
                        init();
                    }

                    // FakeVehicleHardware destructor is called automatically
                    GpioFakeVehicleHardware::~GpioFakeVehicleHardware() {

                    }

                    StatusCode GpioFakeVehicleHardware::setValues(
                            std::shared_ptr<const SetValuesCallback> callback,
                            const std::vector<SetValueRequest> &requests) {
                        return FakeVehicleHardware::setValues(callback, requests);
                    }

                    void GpioFakeVehicleHardware::init() {
                        ALOGI("XXXXX Called GpioFakeVehicleHardware init");
                        std::unordered_map<int32_t, ConfigDeclaration> configsByPropId;
                        loadPropConfigsFromDir(VENDOR_PROPERTY_CONFIG_DIR, &configsByPropId);

                        for (auto &[_, configDeclaration]: configsByPropId) {
                            VehiclePropConfig cfg = configDeclaration.config;
                            mServerSidePropStore->registerProperty(cfg, nullptr);
                            storePropInitialValue(configDeclaration);
                        }
                    }

                    void GpioFakeVehicleHardware::loadPropConfigsFromDir(const std::string &dirPath,
                                                                         std::unordered_map<int32_t, ConfigDeclaration> *configs) {
                        ALOGI("loading vendor properties from %s", dirPath.c_str());
                        if (auto dir = opendir(dirPath.c_str()); dir != NULL) {
                            std::regex regJson(".*[.]json", std::regex::icase);
                            while (auto f = readdir(dir)) {
                                if (!std::regex_match(f->d_name, regJson)) {
                                    continue;
                                }
                                std::string filePath = dirPath + "/" + std::string(f->d_name);
                                ALOGI("loading vendor properties from %s", filePath.c_str());
                                auto result = mConfigLoader.loadPropConfig(filePath);
                                if (!result.ok()) {
                                    ALOGE("failed to load vendor config file: %s, error: %s",
                                          filePath.c_str(),
                                          result.error().message().c_str());
                                    continue;
                                }
                                for (auto &[propId, configDeclaration]: result.value()) {
                                    (*configs)[propId] = std::move(configDeclaration);
                                }
                            }
                            closedir(dir);
                        }
                    }

                    void GpioFakeVehicleHardware::storePropInitialValue(
                            const ConfigDeclaration &config) {
                        const VehiclePropConfig &vehiclePropConfig = config.config;
                        int propId = vehiclePropConfig.prop;

                        // A global property will have only a single area
                        bool globalProp = isGlobalProp(propId);
                        size_t numAreas = globalProp ? 1 : vehiclePropConfig.areaConfigs.size();

                        for (size_t i = 0; i < numAreas; i++) {
                            int32_t curArea = globalProp ? 0
                                                         : vehiclePropConfig.areaConfigs[i].areaId;

                            // Create a separate instance for each individual zone
                            VehiclePropValue prop = {
                                    .areaId = curArea,
                                    .prop = propId,
                                    .timestamp = elapsedRealtimeNano(),
                            };

                            if (config.initialAreaValues.empty()) {
                                if (config.initialValue == RawPropValues{}) {
                                    // Skip empty initial values.
                                    continue;
                                }
                                prop.value = config.initialValue;
                            } else if (auto valueForAreaIt = config.initialAreaValues.find(curArea);
                                    valueForAreaIt != config.initialAreaValues.end()) {
                                prop.value = valueForAreaIt->second;
                            } else {
                                ALOGW("failed to get default value for prop 0x%x area 0x%x", propId,
                                      curArea);
                                continue;
                            }

                            auto result =
                                    mServerSidePropStore->writeValue(
                                            mValuePool->obtain(prop), /*updateStatus=*/true);
                            if (!result.ok()) {
                                ALOGE("failed to write default config value, error: %s, status: %d",
                                      getErrorMsg(result).c_str(), getIntErrorCode(result));
                            }
                        }
                    }
                }
            }
        }
    }
}