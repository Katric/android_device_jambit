#define LOG_TAG "GpioFakeVehicleHardware"
#define ATRACE_TAG ATRACE_TAG_HAL

#include "GpioFakeVehicleHardware.h"
#include "DemonstratorJsonConfigLoader.h"

#include <aidl/jambit/android/hardware/automotive/vehicle/AmbientLightMode.h>
#include <aidl/jambit/android/hardware/automotive/vehicle/VendorVehicleProperty.h>

#include <softPwm.h>
#include <wiringPi.h>

#include <android-base/parsedouble.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>
#include <utils/Trace.h>

#include <dirent.h>
#include <inttypes.h>
#include <regex>

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

                        using ::aidl::jambit::android::hardware::automotive::vehicle::VendorVehicleProperty;
                        using ::aidl::jambit::android::hardware::automotive::vehicle::AmbientLightMode;

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
                    GpioFakeVehicleHardware *gGpioFakeVehicleHardware = nullptr;

                    void globalBatteryChangeHandler() {
                        if (gGpioFakeVehicleHardware != nullptr) {
                            gGpioFakeVehicleHardware->handleBatteryChange();
                        }
                    }

                    /*
                        Global wrapper function is required to be called from the C Library WiringPi (and processed further in C++).
                    */
                    void globalRotaryPushButtonClickHandler() {
                        if (gGpioFakeVehicleHardware != nullptr) {
                            gGpioFakeVehicleHardware->handleRotaryPushButtonClick();
                        }
                    }

                    GpioFakeVehicleHardware::GpioFakeVehicleHardware()
                            : FakeVehicleHardware(), mPendingSetValueRequests(this) {
                        // initialize current battery capacity to avoid calling it multiple times, as it's not going
                        // to change in the demonstrator
                        auto batteryCapacityResult = mServerSidePropStore->readValue(toInt(VehicleProperty::EV_CURRENT_BATTERY_CAPACITY));
                        if (batteryCapacityResult.ok()) {
                            batteryCapacityWh = batteryCapacityResult.value()->value.floatValues[0];
                        } else {
                            ALOGE("Could not initialize battery capacity due to error: %s. Using default battery capacity.",
                                  getErrorMsg(batteryCapacityResult).c_str());
                        }

                        init();
                    }

                    GpioFakeVehicleHardware::~GpioFakeVehicleHardware() {
                        mPendingSetValueRequests.stop();
                        gGpioFakeVehicleHardware = nullptr;

                        // reset GPIO
                        softPwmWrite(FAN_PWM_PIN, 0);
                        softPwmStop(FAN_PWM_PIN);
                        softPwmWrite(RED_PIN, 0);
                        softPwmStop(RED_PIN);
                        softPwmWrite(GREEN_PIN, 0);
                        softPwmStop(GREEN_PIN);
                        softPwmWrite(BLUE_PIN, 0);
                        softPwmStop(BLUE_PIN);
                        pinMode(FAN_PWM_PIN, INPUT);
                    }

                    StatusCode GpioFakeVehicleHardware::setValues(
                            std::shared_ptr<const SetValuesCallback> callback,
                            const std::vector<SetValueRequest> &requests) {
                        for (auto &request: requests) {
                            ALOGD("New setValue request");
                            mPendingSetValueRequests.addRequest(request, callback);
                        }

                        return StatusCode::OK;
                    }

                    void GpioFakeVehicleHardware::init() {
                        std::unordered_map<int32_t, ConfigDeclaration> configsByPropId;
                        loadPropConfigsFromDir(VENDOR_PROPERTY_CONFIG_DIR, &configsByPropId);

                        initGpio();

                        for (auto &[_, configDeclaration]: configsByPropId) {
                            VehiclePropConfig cfg = configDeclaration.config;
                            mServerSidePropStore->registerProperty(cfg, nullptr);
                            setUpAndStorePropInitialValue(configDeclaration);
                        }

                        // for rotary encoder
                        gGpioFakeVehicleHardware = this;
                    }

                    void GpioFakeVehicleHardware::initGpio() {
                        ALOGI("Setting up wiringPi");
                        int32_t wiringPiStatus = wiringPiSetup();

                        if (wiringPiStatus != 0) {
                            ALOGE("Error while initializing wiringPi");
                        }

                        // PWM for fan
                        softPwmCreate(FAN_PWM_PIN, 0, 100);

                        // PWM for RGB LEDs
                        softPwmCreate(RED_PIN, 0, PWM_RANGE);
                        softPwmCreate(GREEN_PIN, 0, PWM_RANGE);
                        softPwmCreate(BLUE_PIN, 0, PWM_RANGE);

                        // rotary encoder for battery level setting
                        pinMode(CLK_PIN, INPUT);
                        pinMode(DT_PIN, INPUT);
                        // avoid floating state of pins, default is low (0)
                        pullUpDnControl(CLK_PIN, PUD_DOWN);
                        pullUpDnControl(DT_PIN, PUD_DOWN);
                        wiringPiISR(CLK_PIN, INT_EDGE_RISING, &globalBatteryChangeHandler);

                        pinMode(SW_PIN, INPUT); // set push button pin mode to input
                        pullUpDnControl(SW_PIN, PUD_DOWN); // set default pin state to 0
                        // wiringPiISR is a C function of WiringPi-Library
                        wiringPiISR(SW_PIN, INT_EDGE_RISING, &globalRotaryPushButtonClickHandler); // define call of global handler function, when push button is clicked (on rising edge)
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

                    void GpioFakeVehicleHardware::setUpAndStorePropInitialValue(
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

                            VehiclePropValuePool::RecyclableType updatedValue = mValuePool->obtain(
                                    prop);
                            auto result =
                                    mServerSidePropStore->writeValue(
                                            std::move(updatedValue), /*updateStatus=*/true);

                            if (!result.ok()) {
                                ALOGE("failed to write default config value, error: %s, status: %d",
                                      getErrorMsg(result).c_str(), getIntErrorCode(result));
                            }

                            // initialize special demonstrator gpio values
                            bool isSpecialDemonstratorValue = false;
                            auto maybeSetSpecialDemonstratorValueResult = maybeSetSpecialDemonstratorValue(
                                    prop, &isSpecialDemonstratorValue);

                            if (isSpecialDemonstratorValue &&
                                !maybeSetSpecialDemonstratorValueResult.ok()) {
                                ALOGE("failed to initialize GPIO for property 0x%x, error: %s",
                                      prop.prop,
                                      getErrorMsg(maybeSetSpecialDemonstratorValueResult).c_str());
                            }
                        }
                    }

                    aidl::android::hardware::automotive::vehicle::SetValueResult
                    GpioFakeVehicleHardware::handleSetValueRequest(const SetValueRequest &request) {
                        SetValueResult setValueResult;
                        setValueResult.requestId = request.requestId;

                        if (auto result = setValue(request.value); !result.ok()) {
                            ALOGE("failed to set value, error: %s, code: %d",
                                  getErrorMsg(result).c_str(),
                                  getIntErrorCode(result));
                            setValueResult.status = getErrorCode(result);
                        } else {
                            setValueResult.status = StatusCode::OK;
                        }

                        return setValueResult;
                    }

                    // if boolean is false (= not a relevant property for the demonstrator),
                    // then call FakeVehicleHardware::setValue
                    VhalResult<void>
                    GpioFakeVehicleHardware::setValue(const VehiclePropValue &value) {
                        ALOGI("New setValueRequest for property id %d", value.prop);
                        bool isSpecialDemonstratorValue = false;
                        auto setSpecialDemonstratorValue = maybeSetSpecialDemonstratorValue(value,
                                                                                            &isSpecialDemonstratorValue);

                        // not a special demonstrator value => handle with standard fake VHAL
                        if (!isSpecialDemonstratorValue) {
                            ALOGI("Value %d is not a special demonstrator value and will be handled by FakeVehicleHardware",
                                  value.prop);
                            return FakeVehicleHardware::setValue(value);
                        }

                        if (isSpecialDemonstratorValue && !setSpecialDemonstratorValue.ok()) {
                            ALOGI("Special demonstrator value not ok");
                            return StatusError(getErrorCode(setSpecialDemonstratorValue))
                                    << StringPrintf(
                                            "failed to set special demonstrator value for property ID: %d, error: %s",
                                            value.prop,
                                            getErrorMsg(setSpecialDemonstratorValue).c_str());
                        }

                        auto updatedSpecialDemonstratorValue = mValuePool->obtain(value);
                        int64_t timestamp = elapsedRealtimeNano();
                        updatedSpecialDemonstratorValue->timestamp = timestamp;

                        auto writeResult = mServerSidePropStore->writeValue(
                                std::move(updatedSpecialDemonstratorValue));
                        if (!writeResult.ok()) {
                            return StatusError(getErrorCode(writeResult))
                                    << StringPrintf(
                                            "failed to write special demonstrator value into property store, error: %s",
                                            getErrorMsg(writeResult).c_str());
                        }

                        return {};
                    }

                    VhalResult<void>
                    GpioFakeVehicleHardware::maybeSetSpecialDemonstratorValue(
                            const VehiclePropValue &value,
                            bool *isSpecialDemonstratorValue) {
                        *isSpecialDemonstratorValue = false;
                        int32_t propId = value.prop;

                        if (propId == toInt(VendorVehicleProperty::AMBIENT_LIGHT_COLOR)) {
                            ALOGD("setValue Request for AMBIENT_LIGHT_COLOR");
                            *isSpecialDemonstratorValue = true;
                            return handleSetCustomAmbientLightColor(value);
                        }

                        if (propId == toInt(VendorVehicleProperty::AMBIENT_LIGHT_MODE)) {
                            ALOGD("setValue Request for AMBIENT_LIGHT_MODE");
                            *isSpecialDemonstratorValue = true;
                            return handleSetAmbientLightMode(value);
                        }

                        if (propId == toInt(VehicleProperty::HVAC_FAN_SPEED)) {
                            ALOGD("setValue Request for HVAC_FAN_SPEED");
                            *isSpecialDemonstratorValue = true;
                            return handleSetHvacFanSpeed(value);
                        }

                        return {};
                    }

                    void GpioFakeVehicleHardware::handleRotaryPushButtonClick() {
                        // add debouncing for mechanical push button to avoid multiple calls
                        unsigned long interruptTime = millis();

                        if (interruptTime - mLastPushButtonClickInterruptTime < SW_DEBOUNCE_TIME) {
                            return;
                        }

                        mLastPushButtonClickInterruptTime = millis();

                        int32_t evChargePortConnected = 0;
                        auto evChargePortConnectedResult = mServerSidePropStore->readValue(toInt(VehicleProperty::EV_CHARGE_PORT_CONNECTED));
                        if (evChargePortConnectedResult.ok()) {
                            evChargePortConnected = evChargePortConnectedResult.value()->value.int32Values[0]; // boolean is stored as int32 (https://source.android.com/docs/automotive/vhal/property-configuration)
                        } else {
                            ALOGE("Could not read currect ev charge port connected value: %s.",
                                  getErrorMsg(evChargePortConnectedResult).c_str());
                            return;
                        } 

                        int32_t newEvChargePortConnectedState = (evChargePortConnected == 0) ? 1 : 0;

                        // write new result back 
                        auto newEvChargePortConnectedValue = mValuePool->obtain(VehiclePropertyType::BOOLEAN); // get predefined boolean config template object from value pool
                        // https://source.android.com/docs/automotive/vhal/vhal-interface#vehicle-prop
                        newEvChargePortConnectedValue->prop = toInt(VehicleProperty::EV_CHARGE_PORT_CONNECTED); 
                        newEvChargePortConnectedValue->areaId = 0; // global 
                        newEvChargePortConnectedValue->timestamp = elapsedRealtimeNano();
                        newEvChargePortConnectedValue->value.int32Values[0] = newEvChargePortConnectedState;

                        // store new value in map
                        auto newEvChargePortConnectedWriteResult = mServerSidePropStore->writeValue(std::move(newEvChargePortConnectedValue));
                        if (!newEvChargePortConnectedWriteResult.ok()) {
                            ALOGE("Could not write new charge port connected value to property store. Error: %s",
                                  getErrorMsg(newEvChargePortConnectedWriteResult).c_str());
                        }
                    }

                    void GpioFakeVehicleHardware::handleBatteryChange() {
                        unsigned long interruptTime = millis();

                        // state of clk and dt pin of rotary encoder
                        int dt = digitalRead(DT_PIN);
                        int clk = digitalRead(CLK_PIN);

                        // debouncing to avoid counting bounces (false triggers shortly after rotation of the rotary encoder)
                        if (interruptTime - mLastBatteryChangeInterruptTime <
                            ROTARY_DEBOUNCE_TIME) {
                            ALOGD("Skipped handling of rotary encoder change due to debounce time");
                            mLastBatteryChangeInterruptTime = interruptTime;
                            return;
                        }

                        auto currentBatteryLevelPercentResult = calculateCurrentBatteryLevelPercent();
                        if (!currentBatteryLevelPercentResult.ok()) {
                            ALOGE("Could not get current battery level in percent. Returning.");
                            return;
                        }

                        float_t currentBatteryLevelPercent = currentBatteryLevelPercentResult.value();
                        float_t newBatteryLevelPercent;
                        // clockwise or counterclockwise
                        // increase or decrease by BATTERY_ROTARY_ENCODER_STEP %
                        if (clk == 1 && dt == 0) {
                            ATRACE_BEGIN("Handle battery change increase");
                            newBatteryLevelPercent =
                                    std::min(currentBatteryLevelPercent +
                                             BATTERY_ROTARY_ENCODER_STEP, 100.0f);
                        } else if (clk == 1 && dt == 1) {
                            ATRACE_BEGIN("Handle battery change decrease");
                            newBatteryLevelPercent = std::max(
                                    currentBatteryLevelPercent - BATTERY_ROTARY_ENCODER_STEP, 0.0f);
                        } else {
                            ALOGD("Ambiguous combination in ct/dt state. returning.");
                            return;
                        }

                        // calculate percentage of battery capacity
                        float_t newBatteryLevel =
                                (newBatteryLevelPercent / 100.0f) * batteryCapacityWh;
                        // update current battery level property
                        auto newBatteryLevelValue = mValuePool->obtain(
                                VehiclePropertyType::FLOAT);
                        newBatteryLevelValue->prop = toInt(VehicleProperty::EV_BATTERY_LEVEL);
                        newBatteryLevelValue->areaId = 0;
                        newBatteryLevelValue->timestamp = elapsedRealtimeNano();
                        newBatteryLevelValue->value.floatValues = {newBatteryLevel};
                        auto updatedBatteryLevelWriteResult = mServerSidePropStore->writeValue(
                                std::move(newBatteryLevelValue));
                        if (!updatedBatteryLevelWriteResult.ok()) {
                            ALOGE("Could not write new battery level to property store. Error: %s",
                                  getErrorMsg(updatedBatteryLevelWriteResult).c_str());
                        }

                        // calculate remaining range
                        float_t newRangeRemaining =
                                (newBatteryLevelPercent / 100.0f) * INITIAL_RANGE;
                        auto newRangeRemainingValue = mValuePool->obtain(
                                VehiclePropertyType::FLOAT);
                        newRangeRemainingValue->prop = toInt(VehicleProperty::RANGE_REMAINING);
                        newRangeRemainingValue->areaId = 0;
                        newRangeRemainingValue->timestamp = elapsedRealtimeNano();
                        newRangeRemainingValue->value.floatValues = {newRangeRemaining};
                        auto updatedRangeRemainingWriteResult = mServerSidePropStore->writeValue(
                                std::move(newRangeRemainingValue));
                        if (!updatedRangeRemainingWriteResult.ok()) {
                            ALOGE("Could not write new remaining range to property store. Error: %s",
                                  getErrorMsg(updatedRangeRemainingWriteResult).c_str());
                        }

                        // update ambient light color if color mode is battery change
                        auto currentAmbientLightModeResult = mServerSidePropStore->readValue(
                                toInt(VendorVehicleProperty::AMBIENT_LIGHT_MODE));
                        if (currentAmbientLightModeResult.ok()) {
                            int32_t currentAmbientLightMode = currentAmbientLightModeResult.value()->value.int32Values[0];
                            if (currentAmbientLightMode == toInt(AmbientLightMode::BATTERY_LEVEL)) {
                                ALOGI("setPwmAmbientLightColorToBatteryLevel: set ambient light to battery level");
                                setAndStorePwmAmbientLightColorToBatteryLevel(
                                        newBatteryLevelPercent);
                            }
                        } else {
                            ALOGI("setPwmAmbientLightColorToBatteryLevel: did not set ambient light to battery level, because AmbientLightMode is not BATTERY_LEVEL");
                        }

                        // check if fuel level low flag has changed and store new value if yes
                        bool wasFuelLevelLow = currentBatteryLevelPercent < LOW_BATTERY_TRESHHOLD;
                        bool isFuelLevelLow = newBatteryLevelPercent < LOW_BATTERY_TRESHHOLD;
                        if (wasFuelLevelLow != isFuelLevelLow) {
                            auto fuelLevelLowValue = mValuePool->obtain(
                                    VehiclePropertyType::BOOLEAN);
                            fuelLevelLowValue->prop = toInt(VehicleProperty::FUEL_LEVEL_LOW);
                            fuelLevelLowValue->areaId = 0;
                            fuelLevelLowValue->timestamp = elapsedRealtimeNano();
                            fuelLevelLowValue->value.int32Values = {isFuelLevelLow};

                            auto fuelLevelLowWriteResult = mServerSidePropStore->writeValue(
                                    std::move(fuelLevelLowValue));
                            if (fuelLevelLowWriteResult.ok()) {
                                ALOGD("Fuel level low: %d", isFuelLevelLow);
                            } else {
                                ALOGE("Failed to write fuel level low warning: %s",
                                      fuelLevelLowWriteResult.error().message().c_str());
                            }
                        }

                        // reset time of last interrupt
                        mLastBatteryChangeInterruptTime = interruptTime;
                        ATRACE_END();
                    }

                    VhalResult<void>
                    GpioFakeVehicleHardware::handleSetHvacFanSpeed(const VehiclePropValue &value) {
                        int32_t propId = value.prop;

                        if (propId != toInt(VehicleProperty::HVAC_FAN_SPEED)) {
                            return StatusError(StatusCode::INVALID_ARG)
                                    << StringPrintf(
                                            "Invalid property ID: 0x%x, expected HVAC_FAN_SPEED",
                                            propId);
                        }

                        if (value.value.int32Values.empty()) {
                            return StatusError(StatusCode::INVALID_ARG)
                                    << "No fan speed value provided";
                        }

                        int32_t hvacFanSpeedLevel = value.value.int32Values[0];
                        return setPwmHvacFanSpeed(hvacFanSpeedLevel);
                    }

                    VhalResult<void>
                    GpioFakeVehicleHardware::setPwmHvacFanSpeed(int32_t hvacFanSpeedLevel) {
                        const int pwmDutyCycleValues[] = {0, 70, 77, 85, 92, 100};
                        const int32_t hvacFanSpeedIdx = hvacFanSpeedLevel - 1;

                        if (hvacFanSpeedIdx < 0 || hvacFanSpeedIdx > 5) {
                            ALOGE("Invalid fan speed: %d. Speed must be between 1 and 6.",
                                  hvacFanSpeedLevel);
                            return StatusError(StatusCode::INVALID_ARG)
                                    << StringPrintf(
                                            "Invalid fan speed: %d. Speed must be between 1 and 6.",
                                            hvacFanSpeedLevel);
                        }

                        softPwmWrite(FAN_PWM_PIN, pwmDutyCycleValues[hvacFanSpeedIdx]);
                        ALOGI("Fan speed set to %d. PWM: %d", hvacFanSpeedLevel,
                              pwmDutyCycleValues[hvacFanSpeedIdx]);
                        return {};
                    }

                    VhalResult<void> GpioFakeVehicleHardware::handleSetAmbientLightMode(
                            const VehiclePropValue &value) {
                        int32_t propId = value.prop;

                        if (propId != toInt(VendorVehicleProperty::AMBIENT_LIGHT_MODE)) {
                            ALOGE("handleSetAmbientLightMode: Property is not AMBIENT_LIGHT_MODE");
                            return StatusError(StatusCode::INVALID_ARG)
                                    << StringPrintf(
                                            "Invalid property ID: 0x%x, expected AMBIENT_LIGHT_MODE",
                                            propId);
                        }

                        if (value.value.int32Values.empty()) {
                            ALOGE("handleSetAmbientLightMode: invalid argument");
                            return StatusError(StatusCode::INVALID_ARG)
                                    << "No ambient light mode value provided";
                        }

                        int32_t ambientLightModeValue = value.value.int32Values[0];

                        if (ambientLightModeValue == toInt(AmbientLightMode::CUSTOM)) {
                            ALOGD("handleSetAmbientLightMode: AmbientLightMode is CUSTOM");
                            // no special action required
                            return {};
                        } else if (ambientLightModeValue ==
                                   toInt(AmbientLightMode::BATTERY_LEVEL)) {
                            ALOGD("handleSetAmbientLightMode: AmbientLightMode is BATTERY_LEVEL");
                            // set color to current battery level
                            auto batteryLevelResult = calculateCurrentBatteryLevelPercent();
                            if (!batteryLevelResult.ok()) {
                                return StatusError(getErrorCode(batteryLevelResult))
                                        << getErrorMsg(batteryLevelResult);
                            }
                            return setAndStorePwmAmbientLightColorToBatteryLevel(
                                    batteryLevelResult.value());
                        }

                        return StatusError(StatusCode::INVALID_ARG)
                                << StringPrintf("Invalid ambient light mode value: %d",
                                                ambientLightModeValue);
                    }

                    VhalResult<void>
                    GpioFakeVehicleHardware::setPwmAmbientLightColor(int32_t red, int32_t green,
                                                                     int32_t blue) {
                        if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 ||
                            blue > 255) {
                            ALOGE("setPwmAmbientLightColor: Invalid color values: red: %d green: %d blue: %d",
                                  red, green, blue);
                            return StatusError(StatusCode::INVALID_ARG)
                                    << StringPrintf(
                                            "Invalid color values: red: %d green: %d blue: %d", red,
                                            green, blue);
                        }

                        softPwmWrite(RED_PIN, red);
                        softPwmWrite(GREEN_PIN, green);
                        softPwmWrite(BLUE_PIN, blue);
                        return {};
                    }

                    VhalResult<void>
                    GpioFakeVehicleHardware::setAndStorePwmAmbientLightColorToBatteryLevel(
                            float_t batteryLevelPercent) {
                        if (batteryLevelPercent < 0 || batteryLevelPercent > 100) {
                            return StatusError(StatusCode::INVALID_ARG)
                                    << StringPrintf("Invalid battery level percent: %f%%",
                                                    batteryLevelPercent);
                        }

                        std::vector<int32_t> batteryLevelColor = getBatteryLevelColor(
                                batteryLevelPercent);
                        auto setPwmColorResult = setPwmAmbientLightColor(batteryLevelColor[0],
                                                                         batteryLevelColor[1],
                                                                         batteryLevelColor[2]);
                        if (!setPwmColorResult.ok()) {
                            return setPwmColorResult.error();
                        }

                        auto batteryLevelColorValue = mValuePool->obtain(
                                VehiclePropertyType::INT32_VEC);
                        batteryLevelColorValue->prop = toInt(
                                VendorVehicleProperty::AMBIENT_LIGHT_COLOR);
                        batteryLevelColorValue->areaId = 0;
                        batteryLevelColorValue->timestamp = elapsedRealtimeNano();
                        batteryLevelColorValue->value.int32Values = batteryLevelColor;
                        auto writeResult = mServerSidePropStore->writeValue(
                                std::move(batteryLevelColorValue));

                        if (!writeResult.ok()) {
                            return writeResult.error();
                        }

                        return {};
                    }

                    VhalResult<void> GpioFakeVehicleHardware::handleSetCustomAmbientLightColor(
                            const VehiclePropValue &value) {
                        int32_t propId = value.prop;
                        auto currentAmbientLightModeResult = mServerSidePropStore->readValue(
                                toInt(VendorVehicleProperty::AMBIENT_LIGHT_MODE));

                        if (propId != toInt(VendorVehicleProperty::AMBIENT_LIGHT_COLOR)) {
                            ALOGE("handleSetCustomAmbientLightColor: Invalid property ID: 0x%d, expected AMBIENT_LIGHT_COLOR",
                                  propId);
                            return StatusError(StatusCode::INVALID_ARG)
                                    << StringPrintf(
                                            "Invalid property ID: 0x%d, expected AMBIENT_LIGHT_COLOR",
                                            propId);
                        }

                        if (currentAmbientLightModeResult.ok()) {
                            int32_t currentAmbientLightMode = currentAmbientLightModeResult.value()->value.int32Values[0];
                            if (currentAmbientLightMode != toInt(AmbientLightMode::CUSTOM)) {
                                ALOGE("handleSetCustomAmbientLightColor: Can't set custom ambient light color, if ambient light mode is not AmbientLightMode::CUSTOM");
                                return StatusError(StatusCode::INVALID_ARG)
                                        << "Can't set custom ambient light color, if ambient light mode is not AmbientLightMode::CUSTOM";
                            }
                        } else {
                            ALOGE("Could not retrieve current ambient light mode");
                        }

                        if (value.value.int32Values.empty()) {
                            ALOGE("handleSetCustomAmbientLightColor: No ambient light mode value provided");
                            return StatusError(StatusCode::INVALID_ARG)
                                    << "No ambient light mode value provided";
                        }

                        if (value.value.int32Values.size() != 3) {
                            ALOGE("handleSetCustomAmbientLightColor: Expected 3 values for RGB color");
                            return StatusError(StatusCode::INVALID_ARG)
                                    << StringPrintf("Expected 3 values for RGB color");
                        }

                        int32_t redValue = value.value.int32Values[0];
                        int32_t greenValue = value.value.int32Values[1];
                        int32_t blueValue = value.value.int32Values[2];

                        return setPwmAmbientLightColor(redValue, greenValue, blueValue);
                    }

                    VhalResult<float_t>
                    GpioFakeVehicleHardware::calculateCurrentBatteryLevelPercent() {
                        auto currentBatteryLevelResult =
                                mServerSidePropStore->readValue(
                                        toInt(VehicleProperty::EV_BATTERY_LEVEL));

                        if (!currentBatteryLevelResult.ok()) {
                            ALOGE("calculateCurrentBatteryLevelPercent: Could not read battery level");
                            return StatusError(StatusCode::INTERNAL_ERROR)
                                    << "Could not retrieve EV_BATTERY_LEVEL value";
                        }

                        float_t currentBatteryLevel = currentBatteryLevelResult.value()->value.floatValues[0];

                        if (batteryCapacityWh == 0) {
                            return StatusError(StatusCode::INTERNAL_ERROR)
                                    << "Current battery capacity is 0";
                        }

                        float_t batteryLevel = (currentBatteryLevel / batteryCapacityWh) * 100;
                        ALOGD("calculateCurrentBatteryLevelPercent: Battery level is %f",
                              batteryLevel);
                        return batteryLevel;
                    }

                    std::vector<int32_t>
                    GpioFakeVehicleHardware::getBatteryLevelColor(float_t batteryPercentage) {
                        const std::vector<int32_t> COLOR_GOOD = {0, 255, 0};    // Green
                        const std::vector<int32_t> COLOR_WARN = {255, 255, 0};  // Yellow
                        const std::vector<int32_t> COLOR_CRITICAL = {255, 0, 0};  // Red

                        if (batteryPercentage > FUEL_WARNING_COLOR) {
                            return COLOR_GOOD;
                        } else if (batteryPercentage > FUEL_LOW_COLOR) {
                            return COLOR_WARN;
                        }
                        return COLOR_CRITICAL;
                    }

                    GpioFakeVehicleHardware::PendingSetRequestHandler::PendingSetRequestHandler(
                            GpioFakeVehicleHardware *hardware)
                            : mHardware(hardware) {
                        // waiting for incoming set requests
                        mThread = std::thread([this] {
                            while (mRequests.waitForItems()) {
                                ALOGD("Got new setValue requests in queue");
                                handleSetValueRequests();
                            }
                        });
                    }


                    void GpioFakeVehicleHardware::PendingSetRequestHandler::stop() {
                        mRequests.deactivate();
                        if (mThread.joinable()) {
                            mThread.join();
                        }
                    }

                    void
                    GpioFakeVehicleHardware::PendingSetRequestHandler::handleSetValueRequests() {
                        std::unordered_map<std::shared_ptr<const SetValuesCallback>, std::vector<SetValueResult>>
                                callbackToResults;
                        for (const auto &srwc: mRequests.flush()) {
                            ATRACE_BEGIN("GpioFakeVehicleHardware:handleSetValueRequest");
                            auto result = mHardware->handleSetValueRequest(srwc.request);
                            ATRACE_END();
                            callbackToResults[srwc.callback].push_back(std::move(result));
                        }

                        for (const auto &[callback, results]: callbackToResults) {
                            // client in DefaultVehicleHal gets notified and clears pending requests by id
                            ATRACE_BEGIN("GpioFakeVehicleHardware:call set value result callback");
                            (*callback)(std::move(results));
                            ATRACE_END();
                        }
                    }

                    void GpioFakeVehicleHardware::PendingSetRequestHandler::addRequest(
                            aidl::android::hardware::automotive::vehicle::SetValueRequest request,
                            std::shared_ptr<const SetValuesCallback> callback) {
                        mRequests.push({request, callback});
                    }
                }
            }
        }
    }
}