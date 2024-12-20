#include <DemonstratorJsonConfigLoader.h>

#include <PropertyUtils.h>

#include <aidl/jambit/android/hardware/automotive/vehicle/AmbientLightMode.h>
#include <aidl/jambit/android/hardware/automotive/vehicle/VendorVehicleProperty.h>

#include <android-base/strings.h>
#include <fstream>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

namespace demonstratorjsonconfigloader_impl {

using ::aidl::android::hardware::automotive::vehicle::AutomaticEmergencyBrakingState;
using ::aidl::android::hardware::automotive::vehicle::BlindSpotWarningState;
using ::aidl::android::hardware::automotive::vehicle::CruiseControlCommand;
using ::aidl::android::hardware::automotive::vehicle::CruiseControlState;
using ::aidl::android::hardware::automotive::vehicle::CruiseControlType;
using ::aidl::android::hardware::automotive::vehicle::EmergencyLaneKeepAssistState;
using ::aidl::android::hardware::automotive::vehicle::ErrorState;
using ::aidl::android::hardware::automotive::vehicle::EvConnectorType;
using ::aidl::android::hardware::automotive::vehicle::EvsServiceState;
using ::aidl::android::hardware::automotive::vehicle::EvsServiceType;
using ::aidl::android::hardware::automotive::vehicle::ForwardCollisionWarningState;
using ::aidl::android::hardware::automotive::vehicle::FuelType;
using ::aidl::android::hardware::automotive::vehicle::GsrComplianceRequirementType;
using ::aidl::android::hardware::automotive::vehicle::HandsOnDetectionDriverState;
using ::aidl::android::hardware::automotive::vehicle::HandsOnDetectionWarning;
using ::aidl::android::hardware::automotive::vehicle::LaneCenteringAssistCommand;
using ::aidl::android::hardware::automotive::vehicle::LaneCenteringAssistState;
using ::aidl::android::hardware::automotive::vehicle::LaneDepartureWarningState;
using ::aidl::android::hardware::automotive::vehicle::LaneKeepAssistState;
using ::aidl::android::hardware::automotive::vehicle::LocationCharacterization;
using ::aidl::android::hardware::automotive::vehicle::RawPropValues;
using ::aidl::android::hardware::automotive::vehicle::VehicleApPowerStateReport;
using ::aidl::android::hardware::automotive::vehicle::VehicleApPowerStateReq;
using ::aidl::android::hardware::automotive::vehicle::VehicleAreaConfig;
using ::aidl::android::hardware::automotive::vehicle::VehicleAreaMirror;
using ::aidl::android::hardware::automotive::vehicle::VehicleAreaWindow;
using ::aidl::android::hardware::automotive::vehicle::VehicleGear;
using ::aidl::android::hardware::automotive::vehicle::VehicleHvacFanDirection;
using ::aidl::android::hardware::automotive::vehicle::VehicleIgnitionState;
using ::aidl::android::hardware::automotive::vehicle::VehicleOilLevel;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropConfig;
using ::aidl::android::hardware::automotive::vehicle::VehicleProperty;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyAccess;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyChangeMode;
using ::aidl::android::hardware::automotive::vehicle::VehicleSeatOccupancyState;
using ::aidl::android::hardware::automotive::vehicle::VehicleTurnSignal;
using ::aidl::android::hardware::automotive::vehicle::VehicleUnit;
using ::aidl::android::hardware::automotive::vehicle::VehicleVendorPermission;
using ::aidl::android::hardware::automotive::vehicle::WindshieldWipersState;
using ::aidl::android::hardware::automotive::vehicle::WindshieldWipersSwitch;

using ::aidl::jambit::android::hardware::automotive::vehicle::VendorVehicleProperty;
using ::aidl::jambit::android::hardware::automotive::vehicle::AmbientLightMode;

using ::android::base::Error;
using ::android::base::Result;

// Defines a map from constant names to constant values, the values defined here corresponds to
// the "Constants::XXXX" used in JSON config file.
const std::unordered_map<std::string, int> CONSTANTS_BY_NAME = {
        {"DOOR_1_RIGHT", DOOR_1_RIGHT},
        {"DOOR_1_LEFT", DOOR_1_LEFT},
        {"DOOR_2_RIGHT", DOOR_2_RIGHT},
        {"DOOR_2_LEFT", DOOR_2_LEFT},
        {"DOOR_REAR", DOOR_REAR},
        {"HVAC_ALL", HVAC_ALL},
        {"HVAC_LEFT", HVAC_LEFT},
        {"HVAC_RIGHT", HVAC_RIGHT},
        {"VENDOR_EXTENSION_INT_PROPERTY", VENDOR_EXTENSION_INT_PROPERTY},
        {"VENDOR_EXTENSION_BOOLEAN_PROPERTY", VENDOR_EXTENSION_BOOLEAN_PROPERTY},
        {"VENDOR_EXTENSION_STRING_PROPERTY", VENDOR_EXTENSION_STRING_PROPERTY},
        {"VENDOR_EXTENSION_FLOAT_PROPERTY", VENDOR_EXTENSION_FLOAT_PROPERTY},
        {"WINDOW_1_LEFT", WINDOW_1_LEFT},
        {"WINDOW_1_RIGHT", WINDOW_1_RIGHT},
        {"WINDOW_2_LEFT", WINDOW_2_LEFT},
        {"WINDOW_2_RIGHT", WINDOW_2_RIGHT},
        {"WINDOW_ROOF_TOP_1", WINDOW_ROOF_TOP_1},
        {"WINDOW_1_RIGHT_2_LEFT_2_RIGHT", WINDOW_1_RIGHT | WINDOW_2_LEFT | WINDOW_2_RIGHT},
        {"SEAT_1_LEFT", SEAT_1_LEFT},
        {"SEAT_1_RIGHT", SEAT_1_RIGHT},
        {"SEAT_2_LEFT", SEAT_2_LEFT},
        {"SEAT_2_RIGHT", SEAT_2_RIGHT},
        {"SEAT_2_CENTER", SEAT_2_CENTER},
        {"SEAT_2_LEFT_2_RIGHT_2_CENTER", SEAT_2_LEFT | SEAT_2_RIGHT | SEAT_2_CENTER},
        {"WHEEL_REAR_RIGHT", WHEEL_REAR_RIGHT},
        {"WHEEL_REAR_LEFT", WHEEL_REAR_LEFT},
        {"WHEEL_FRONT_RIGHT", WHEEL_FRONT_RIGHT},
        {"WHEEL_FRONT_LEFT", WHEEL_FRONT_LEFT},
        {"CHARGE_PORT_FRONT_LEFT", CHARGE_PORT_FRONT_LEFT},
        {"CHARGE_PORT_REAR_LEFT", CHARGE_PORT_REAR_LEFT},
        {"FAN_DIRECTION_UNKNOWN", toInt(VehicleHvacFanDirection::UNKNOWN)},
        {"FAN_DIRECTION_FLOOR", FAN_DIRECTION_FLOOR},
        {"FAN_DIRECTION_FACE", FAN_DIRECTION_FACE},
        {"FAN_DIRECTION_DEFROST", FAN_DIRECTION_DEFROST},
        {"FAN_DIRECTION_FACE_FLOOR", FAN_DIRECTION_FACE | FAN_DIRECTION_FLOOR},
        {"FAN_DIRECTION_FACE_DEFROST", FAN_DIRECTION_FACE | FAN_DIRECTION_DEFROST},
        {"FAN_DIRECTION_FLOOR_DEFROST", FAN_DIRECTION_FLOOR | FAN_DIRECTION_DEFROST},
        {"FAN_DIRECTION_FLOOR_DEFROST_FACE",
         FAN_DIRECTION_FLOOR | FAN_DIRECTION_DEFROST | FAN_DIRECTION_FACE},
        {"FUEL_DOOR_REAR_LEFT", FUEL_DOOR_REAR_LEFT},
        {"LIGHT_STATE_ON", LIGHT_STATE_ON},
        {"LIGHT_STATE_OFF", LIGHT_STATE_OFF},
        {"LIGHT_SWITCH_OFF", LIGHT_SWITCH_OFF},
        {"LIGHT_SWITCH_ON", LIGHT_SWITCH_ON},
        {"LIGHT_SWITCH_AUTO", LIGHT_SWITCH_AUTO},
        {"EV_STOPPING_MODE_CREEP", EV_STOPPING_MODE_CREEP},
        {"EV_STOPPING_MODE_ROLL", EV_STOPPING_MODE_ROLL},
        {"EV_STOPPING_MODE_HOLD", EV_STOPPING_MODE_HOLD},
        {"MIRROR_DRIVER_LEFT_RIGHT",
         toInt(VehicleAreaMirror::DRIVER_LEFT) | toInt(VehicleAreaMirror::DRIVER_RIGHT)},
#ifdef ENABLE_VEHICLE_HAL_TEST_PROPERTIES
        // Following are test properties:
        {"ECHO_REVERSE_BYTES", ECHO_REVERSE_BYTES},
        {"VENDOR_PROPERTY_ID", VENDOR_PROPERTY_ID},
        {"kMixedTypePropertyForTest", kMixedTypePropertyForTest},
        {"VENDOR_CLUSTER_NAVIGATION_STATE", VENDOR_CLUSTER_NAVIGATION_STATE},
        {"VENDOR_CLUSTER_REQUEST_DISPLAY", VENDOR_CLUSTER_REQUEST_DISPLAY},
        {"VENDOR_CLUSTER_SWITCH_UI", VENDOR_CLUSTER_SWITCH_UI},
        {"VENDOR_CLUSTER_DISPLAY_STATE", VENDOR_CLUSTER_DISPLAY_STATE},
        {"VENDOR_CLUSTER_REPORT_STATE", VENDOR_CLUSTER_REPORT_STATE},
        {"PLACEHOLDER_PROPERTY_INT", PLACEHOLDER_PROPERTY_INT},
        {"PLACEHOLDER_PROPERTY_FLOAT", PLACEHOLDER_PROPERTY_FLOAT},
        {"PLACEHOLDER_PROPERTY_BOOLEAN", PLACEHOLDER_PROPERTY_BOOLEAN},
        {"PLACEHOLDER_PROPERTY_STRING", PLACEHOLDER_PROPERTY_STRING}
#endif  // ENABLE_VEHICLE_HAL_TEST_PROPERTIES
};

// A class to parse vendor constant values for type T.
template <class T>
class DemonstratorConstantParser final : public ConstantParserInterface {
  public:
    DemonstratorConstantParser() {
        for (const T& v : ndk::enum_range<T>()) {
            std::string name = aidl::jambit::android::hardware::automotive::vehicle::toString(v);
            mValueByName[name] = toInt(v);
        }
    }

    ~DemonstratorConstantParser() = default;

    Result<int> parseValue(const std::string& name) const override {
        auto it = mValueByName.find(name);
        if (it == mValueByName.end()) {
            return Error() << "Constant name: " << name << " is not defined";
        }
        return it->second;
    }

  private:
    std::unordered_map<std::string, int> mValueByName;
};

// A class to parse constant values for type T.
template <class T>
class ConstantParser final : public ConstantParserInterface {
  public:
    ConstantParser() {
        for (const T& v : ndk::enum_range<T>()) {
            std::string name = aidl::android::hardware::automotive::vehicle::toString(v);
            // We use the same constant for both VehicleUnit::GALLON and VehicleUnit::US_GALLON,
            // which caused toString() not work properly for US_GALLON. So we explicitly add the
            // map here.
            if (name == "GALLON") {
                mValueByName["US_GALLON"] = toInt(v);
            }
            mValueByName[name] = toInt(v);
        }
    }

    ~ConstantParser() = default;

    Result<int> parseValue(const std::string& name) const override {
        auto it = mValueByName.find(name);
        if (it == mValueByName.end()) {
            return Error() << "Constant name: " << name << " is not defined";
        }
        return it->second;
    }

  private:
    std::unordered_map<std::string, int> mValueByName;
};

// A class to parse constant values defined in CONSTANTS_BY_NAME map.
class LocalVariableParser final : public ConstantParserInterface {
  public:
    ~LocalVariableParser() = default;

    Result<int> parseValue(const std::string& name) const override {
        auto constantsIt = CONSTANTS_BY_NAME.find(name);
        if (constantsIt == CONSTANTS_BY_NAME.end()) {
            return Error() << "Constant variable name: " << name << " is not defined";
        }
        return constantsIt->second;
    }
};

JsonValueParser::JsonValueParser() {
    mConstantParsersByType["VehiclePropertyAccess"] =
            std::make_unique<ConstantParser<VehiclePropertyAccess>>();
    mConstantParsersByType["VehiclePropertyChangeMode"] =
            std::make_unique<ConstantParser<VehiclePropertyChangeMode>>();
    mConstantParsersByType["LocationCharacterization"] =
            std::make_unique<ConstantParser<LocationCharacterization>>();
    mConstantParsersByType["VehicleGear"] = std::make_unique<ConstantParser<VehicleGear>>();
    mConstantParsersByType["VehicleAreaWindow"] =
            std::make_unique<ConstantParser<VehicleAreaWindow>>();
    mConstantParsersByType["VehicleAreaMirror"] =
            std::make_unique<ConstantParser<VehicleAreaMirror>>();
    mConstantParsersByType["VehicleOilLevel"] = std::make_unique<ConstantParser<VehicleOilLevel>>();
    mConstantParsersByType["VehicleUnit"] = std::make_unique<ConstantParser<VehicleUnit>>();
    mConstantParsersByType["VehicleSeatOccupancyState"] =
            std::make_unique<ConstantParser<VehicleSeatOccupancyState>>();
    mConstantParsersByType["VehicleHvacFanDirection"] =
            std::make_unique<ConstantParser<VehicleHvacFanDirection>>();
    mConstantParsersByType["VehicleApPowerStateReport"] =
            std::make_unique<ConstantParser<VehicleApPowerStateReport>>();
    mConstantParsersByType["VehicleTurnSignal"] =
            std::make_unique<ConstantParser<VehicleTurnSignal>>();
    mConstantParsersByType["VehicleVendorPermission"] =
            std::make_unique<ConstantParser<VehicleVendorPermission>>();
    mConstantParsersByType["EvsServiceType"] = std::make_unique<ConstantParser<EvsServiceType>>();
    mConstantParsersByType["EvsServiceState"] = std::make_unique<ConstantParser<EvsServiceState>>();
    mConstantParsersByType["EvConnectorType"] = std::make_unique<ConstantParser<EvConnectorType>>();
    mConstantParsersByType["AmbientLightMode"] = std::make_unique<DemonstratorConstantParser<AmbientLightMode>>();
    mConstantParsersByType["VehicleProperty"] = std::make_unique<ConstantParser<VehicleProperty>>();
    mConstantParsersByType["VendorVehicleProperty"] = std::make_unique<DemonstratorConstantParser<VendorVehicleProperty>>();
    mConstantParsersByType["GsrComplianceRequirementType"] =
            std::make_unique<ConstantParser<GsrComplianceRequirementType>>();
    mConstantParsersByType["VehicleIgnitionState"] =
            std::make_unique<ConstantParser<VehicleIgnitionState>>();
    mConstantParsersByType["FuelType"] = std::make_unique<ConstantParser<FuelType>>();
    mConstantParsersByType["WindshieldWipersState"] =
            std::make_unique<ConstantParser<WindshieldWipersState>>();
    mConstantParsersByType["WindshieldWipersSwitch"] =
            std::make_unique<ConstantParser<WindshieldWipersSwitch>>();
    mConstantParsersByType["EmergencyLaneKeepAssistState"] =
            std::make_unique<ConstantParser<EmergencyLaneKeepAssistState>>();
    mConstantParsersByType["CruiseControlType"] =
            std::make_unique<ConstantParser<CruiseControlType>>();
    mConstantParsersByType["CruiseControlState"] =
            std::make_unique<ConstantParser<CruiseControlState>>();
    mConstantParsersByType["CruiseControlCommand"] =
            std::make_unique<ConstantParser<CruiseControlCommand>>();
    mConstantParsersByType["HandsOnDetectionDriverState"] =
            std::make_unique<ConstantParser<HandsOnDetectionDriverState>>();
    mConstantParsersByType["HandsOnDetectionWarning"] =
            std::make_unique<ConstantParser<HandsOnDetectionWarning>>();
    mConstantParsersByType["ErrorState"] = std::make_unique<ConstantParser<ErrorState>>();
    mConstantParsersByType["AutomaticEmergencyBrakingState"] =
            std::make_unique<ConstantParser<AutomaticEmergencyBrakingState>>();
    mConstantParsersByType["ForwardCollisionWarningState"] =
            std::make_unique<ConstantParser<ForwardCollisionWarningState>>();
    mConstantParsersByType["BlindSpotWarningState"] =
            std::make_unique<ConstantParser<BlindSpotWarningState>>();
    mConstantParsersByType["LaneDepartureWarningState"] =
            std::make_unique<ConstantParser<LaneDepartureWarningState>>();
    mConstantParsersByType["LaneKeepAssistState"] =
            std::make_unique<ConstantParser<LaneKeepAssistState>>();
    mConstantParsersByType["LaneCenteringAssistCommand"] =
            std::make_unique<ConstantParser<LaneCenteringAssistCommand>>();
    mConstantParsersByType["LaneCenteringAssistState"] =
            std::make_unique<ConstantParser<LaneCenteringAssistState>>();
    mConstantParsersByType["Constants"] = std::make_unique<LocalVariableParser>();
}

template <>
Result<int32_t> JsonValueParser::convertValueToType<int32_t>(const std::string& fieldName,
                                                             const Json::Value& value) {
    if (!value.isInt()) {
        return Error() << "The value: " << value << " for field: " << fieldName
                       << " is not in correct type, expect int";
    }
    return static_cast<int32_t>(value.asInt());
}

template <>
Result<float> JsonValueParser::convertValueToType<float>(const std::string& fieldName,
                                                         const Json::Value& value) {
    // isFloat value does not exist, so we use isDouble here.
    if (!value.isDouble()) {
        return Error() << "The value: " << value << " for field: " << fieldName
                       << " is not in correct type, expect float";
    }
    return value.asFloat();
}

template <>
Result<int64_t> JsonValueParser::convertValueToType<int64_t>(const std::string& fieldName,
                                                             const Json::Value& value) {
    if (!value.isInt64()) {
        return Error() << "The value: " << value << " for field: " << fieldName
                       << " is not in correct type, expect int64";
    }
    return static_cast<int64_t>(value.asInt64());
}

template <>
Result<std::string> JsonValueParser::convertValueToType<std::string>(const std::string& fieldName,
                                                                     const Json::Value& value) {
    if (!value.isString()) {
        return Error() << "The value: " << value << " for field: " << fieldName
                       << " is not in correct type, expect string";
    }
    return value.asString();
}

Result<std::string> JsonValueParser::parseStringValue(const std::string& fieldName,
                                                      const Json::Value& value) const {
    return convertValueToType<std::string>(fieldName, value);
}

template <class T>
Result<T> JsonValueParser::parseValue(const std::string& fieldName,
                                      const Json::Value& value) const {
    if (!value.isString()) {
        return convertValueToType<T>(fieldName, value);
    }
    auto maybeTypeAndValue = maybeGetTypeAndValueName(value.asString());
    if (!maybeTypeAndValue.has_value()) {
        return Error() << "Invalid constant value: " << value << " for field: " << fieldName;
    }
    auto constantParseResult = parseConstantValue(maybeTypeAndValue.value());
    if (!constantParseResult.ok()) {
        return constantParseResult.error();
    }
    int constantValue = constantParseResult.value();
    return static_cast<T>(constantValue);
}

template <>
Result<std::string> JsonValueParser::parseValue<std::string>(const std::string& fieldName,
                                                             const Json::Value& value) const {
    return parseStringValue(fieldName, value);
}

template <class T>
Result<std::vector<T>> JsonValueParser::parseArray(const std::string& fieldName,
                                                   const Json::Value& value) const {
    if (!value.isArray()) {
        return Error() << "The value: " << value << " for field: " << fieldName
                       << " is not in correct type, expect array";
    }
    std::vector<T> parsedValues;
    for (unsigned int i = 0; i < value.size(); i++) {
        auto result = parseValue<T>(fieldName, value[i]);
        if (!result.ok()) {
            return result.error();
        }
        parsedValues.push_back(result.value());
    }
    return std::move(parsedValues);
}

std::optional<std::pair<std::string, std::string>> JsonValueParser::maybeGetTypeAndValueName(
        const std::string& jsonFieldValue) const {
    size_t pos = jsonFieldValue.find(DELIMITER);
    if (pos == std::string::npos) {
        return {};
    }
    std::string type = jsonFieldValue.substr(0, pos);
    std::string valueName = jsonFieldValue.substr(pos + DELIMITER.length(), std::string::npos);
    if (type != "Constants" && mConstantParsersByType.find(type) == mConstantParsersByType.end()) {
        return {};
    }
    return std::make_pair(type, valueName);
}

Result<int> JsonValueParser::parseConstantValue(
        const std::pair<std::string, std::string>& typeValueName) const {
    const std::string& type = typeValueName.first;
    const std::string& valueName = typeValueName.second;
    auto it = mConstantParsersByType.find(type);
    if (it == mConstantParsersByType.end()) {
        return Error() << "Unrecognized type: " << type;
    }
    auto result = it->second->parseValue(valueName);
    if (!result.ok()) {
        return Error() << type << "::" << valueName << " undefined";
    }
    return result;
}

template <class T>
bool JsonConfigParser::tryParseJsonValueToVariable(const Json::Value& parentJsonNode,
                                                   const std::string& fieldName,
                                                   bool fieldIsOptional, T* outPtr,
                                                   std::vector<std::string>* errors) {
    if (!parentJsonNode.isObject()) {
        errors->push_back("Node: " + parentJsonNode.toStyledString() + " is not an object");
        return false;
    }
    if (!parentJsonNode.isMember(fieldName)) {
        if (!fieldIsOptional) {
            errors->push_back("Missing required field: " + fieldName +
                              " in node: " + parentJsonNode.toStyledString());
            return false;
        }
        return true;
    }
    auto result = mValueParser.parseValue<T>(fieldName, parentJsonNode[fieldName]);
    if (!result.ok()) {
        errors->push_back(result.error().message());
        return false;
    }
    *outPtr = std::move(result.value());
    return true;
}

template <class T>
bool JsonConfigParser::tryParseJsonArrayToVariable(const Json::Value& parentJsonNode,
                                                   const std::string& fieldName,
                                                   bool fieldIsOptional, std::vector<T>* outPtr,
                                                   std::vector<std::string>* errors) {
    if (!parentJsonNode.isObject()) {
        errors->push_back("Node: " + parentJsonNode.toStyledString() + " is not an object");
        return false;
    }
    if (!parentJsonNode.isMember(fieldName)) {
        if (!fieldIsOptional) {
            errors->push_back("Missing required field: " + fieldName +
                              " in node: " + parentJsonNode.toStyledString());
            return false;
        }
        return true;
    }
    auto result = mValueParser.parseArray<T>(fieldName, parentJsonNode[fieldName]);
    if (!result.ok()) {
        errors->push_back(result.error().message());
        return false;
    }
    *outPtr = std::move(result.value());
    return true;
}

template <class T>
void JsonConfigParser::parseAccessChangeMode(
        const Json::Value& parentJsonNode, const std::string& fieldName,
        const std::string& propStr, T* outPtr, std::vector<std::string>* errors) {
    if (!parentJsonNode.isObject()) {
        errors->push_back("Node: " + parentJsonNode.toStyledString() + " is not an object");
        return;
    }
    if (parentJsonNode.isMember(fieldName)) {
        auto result = mValueParser.parseValue<int32_t>(fieldName, parentJsonNode[fieldName]);
        if (!result.ok()) {
            errors->push_back(result.error().message());
            return;
        }
        *outPtr = static_cast<T>(result.value());
        return;
    }
    errors->push_back("No " + fieldName + " specified for property: " + propStr);
    return;
}

bool JsonConfigParser::parsePropValues(const Json::Value& parentJsonNode,
                                       const std::string& fieldName, RawPropValues* outPtr,
                                       std::vector<std::string>* errors) {
    if (!parentJsonNode.isObject()) {
        errors->push_back("Node: " + parentJsonNode.toStyledString() + " is not an object");
        return false;
    }
    if (!parentJsonNode.isMember(fieldName)) {
        return false;
    }
    const Json::Value& jsonValue = parentJsonNode[fieldName];
    bool success = true;
    success &= tryParseJsonArrayToVariable(jsonValue, "int32Values",
                                           /*optional=*/true, &(outPtr->int32Values), errors);
    success &= tryParseJsonArrayToVariable(jsonValue, "floatValues",
                                           /*optional=*/true, &(outPtr->floatValues), errors);
    success &= tryParseJsonArrayToVariable(jsonValue, "int64Values",
                                           /*optional=*/true, &(outPtr->int64Values), errors);
    // We don't support "byteValues" yet.
    success &= tryParseJsonValueToVariable(jsonValue, "stringValue",
                                           /*optional=*/true, &(outPtr->stringValue), errors);
    return success;
}

void JsonConfigParser::parseAreas(const Json::Value& parentJsonNode, const std::string& fieldName,
                                  ConfigDeclaration* config, std::vector<std::string>* errors) {
    if (!parentJsonNode.isObject()) {
        errors->push_back("Node: " + parentJsonNode.toStyledString() + " is not an object");
        return;
    }
    if (!parentJsonNode.isMember(fieldName)) {
        return;
    }
    const Json::Value& jsonValue = parentJsonNode[fieldName];

    if (!jsonValue.isArray()) {
        errors->push_back("Field: " + fieldName + " is not an array");
        return;
    }
    for (unsigned int i = 0; i < jsonValue.size(); i++) {
        int32_t areaId;
        const Json::Value& jsonAreaConfig = jsonValue[i];
        if (!tryParseJsonValueToVariable(jsonAreaConfig, "areaId",
                                         /*optional=*/false, &areaId, errors)) {
            continue;
        }
        VehicleAreaConfig areaConfig = {};
        areaConfig.areaId = areaId;
        tryParseJsonValueToVariable(jsonAreaConfig, "minInt32Value", /*optional=*/true,
                                    &areaConfig.minInt32Value, errors);
        tryParseJsonValueToVariable(jsonAreaConfig, "maxInt32Value", /*optional=*/true,
                                    &areaConfig.maxInt32Value, errors);
        tryParseJsonValueToVariable(jsonAreaConfig, "minInt64Value", /*optional=*/true,
                                    &areaConfig.minInt64Value, errors);
        tryParseJsonValueToVariable(jsonAreaConfig, "maxInt64Value", /*optional=*/true,
                                    &areaConfig.maxInt64Value, errors);
        tryParseJsonValueToVariable(jsonAreaConfig, "minFloatValue", /*optional=*/true,
                                    &areaConfig.minFloatValue, errors);
        tryParseJsonValueToVariable(jsonAreaConfig, "maxFloatValue", /*optional=*/true,
                                    &areaConfig.maxFloatValue, errors);

        std::vector<int64_t> supportedEnumValues;
        tryParseJsonArrayToVariable(jsonAreaConfig, "supportedEnumValues", /*optional=*/true,
                                    &supportedEnumValues, errors);
        if (!supportedEnumValues.empty()) {
            areaConfig.supportedEnumValues = std::move(supportedEnumValues);
        }
        config->config.areaConfigs.push_back(std::move(areaConfig));

        RawPropValues areaValue = {};
        if (parsePropValues(jsonAreaConfig, "defaultValue", &areaValue, errors)) {
            config->initialAreaValues[areaId] = std::move(areaValue);
        }
    }
}

std::optional<ConfigDeclaration> JsonConfigParser::parseEachProperty(
        const Json::Value& propJsonValue, std::vector<std::string>* errors) {
    size_t initialErrorCount = errors->size();
    ConfigDeclaration configDecl = {};
    int32_t propId;

    // parsing the property value
    if (!tryParseJsonValueToVariable(propJsonValue, "property", /*optional=*/false, &propId,
                                     errors)) {
        return std::nullopt;
    }

    configDecl.config.prop = propId;
    std::string propStr = propJsonValue["property"].toStyledString();

    parseAccessChangeMode(propJsonValue, "access", propStr,
                          &configDecl.config.access, errors);

    parseAccessChangeMode(propJsonValue, "changeMode", propStr, &configDecl.config.changeMode, errors);

    tryParseJsonValueToVariable(propJsonValue, "configString", /*optional=*/true,
                                &configDecl.config.configString, errors);

    tryParseJsonArrayToVariable(propJsonValue, "configArray", /*optional=*/true,
                                &configDecl.config.configArray, errors);

    parsePropValues(propJsonValue, "defaultValue", &configDecl.initialValue, errors);

    tryParseJsonValueToVariable(propJsonValue, "minSampleRate", /*optional=*/true,
                                &configDecl.config.minSampleRate, errors);

    tryParseJsonValueToVariable(propJsonValue, "maxSampleRate", /*optional=*/true,
                                &configDecl.config.maxSampleRate, errors);

    parseAreas(propJsonValue, "areas", &configDecl, errors);

    if (errors->size() != initialErrorCount) {
        return std::nullopt;
    }
    return configDecl;
}

// This method parses all properties and splits them into "vendor" and "system" properties
// to handle them seperately.
Result<std::unordered_map<int32_t, ConfigDeclaration>> JsonConfigParser::parseJsonConfig(
        std::istream& is) {
    Json::CharReaderBuilder builder;
    Json::Value originalRoot;
    std::string errs;

    // Json validity checks
    if (!Json::parseFromStream(builder, is, &originalRoot, &errs)) {
        return Error() << "Failed to parse property config file as JSON, error: " << errs;
    }
    if (!originalRoot.isObject()) {
        return Error() << "root element must be an object";
    }
    if (!originalRoot.isMember("properties") || !originalRoot["properties"].isArray()) {
        return Error() << "Missing 'properties' field in root or the field is not an array";
    }

    std::vector<std::string> errors;
    Json::Value originalProperties = originalRoot["properties"];
    Json::Value systemPropertiesRoot;
    systemPropertiesRoot["properties"] = Json::Value(Json::arrayValue);
    Json::Value vendorPropertiesArray = Json::Value(Json::arrayValue);

    // iterate over all properties and decide, whether to handle as vendor or system property
    // by comparing "VendorVehicleProperty" and "VehicleProperty" (system) prefixes.
    // We build two seperate Jsons. The json of the system properties has the same structure as
    // the config Json and will be passed to JsonConfigLoader. The vendor properties will be stored
    // in an array and iterated over in the next step.
    for (const auto& prop : originalRoot["properties"]) {
        if (!prop.isMember("property")) {
            errors.push_back("Node: " + prop.toStyledString() + " does not have required \"property\" field");
            continue;
        }

        std::string propertyName = prop["property"].asString();
        if (propertyName.find("VendorVehicleProperty::", 0) == 0) {
            vendorPropertiesArray.append(prop);
        } else if (propertyName.find("VehicleProperty::", 0) == 0) {
            systemPropertiesRoot["properties"].append(prop);
        } else {
            errors.push_back("Node: " + prop.toStyledString() + " has unknown property type " + propertyName);
        }
    }

    std::stringstream systemPropertiesStream;
    systemPropertiesStream << systemPropertiesRoot;

    auto systemPropertiesResult = mSystemJsonConfigLoader.loadPropConfig(systemPropertiesStream);
    if (!systemPropertiesResult.ok()) {
        return systemPropertiesResult.error();
    }
    // get the map of system properties
    std::unordered_map<int32_t, ConfigDeclaration> systemConfigsByPropId = systemPropertiesResult.value();

    std::unordered_map<int32_t, ConfigDeclaration> vendorConfigsByPropId;
    for (unsigned int i = 0; i < vendorPropertiesArray.size(); i++) {
        if (auto maybeConfig = parseEachProperty(vendorPropertiesArray[i], &errors); maybeConfig.has_value()) {
            vendorConfigsByPropId[maybeConfig.value().config.prop] = std::move(maybeConfig.value());
        }
    }
    if (!errors.empty()) {
        return Error() << android::base::Join(errors, '\n');
    }

    vendorConfigsByPropId.insert(systemConfigsByPropId.begin(), systemConfigsByPropId.end());
    return vendorConfigsByPropId;
}

}  // namespace demonstratorjsonconfigloader_impl

DemonstratorJsonConfigLoader::DemonstratorJsonConfigLoader() {
    mParser = std::make_unique<demonstratorjsonconfigloader_impl::JsonConfigParser>();
}

android::base::Result<std::unordered_map<int32_t, ConfigDeclaration>>
DemonstratorJsonConfigLoader::loadPropConfig(std::istream& is) {
    return mParser->parseJsonConfig(is);
}

android::base::Result<std::unordered_map<int32_t, ConfigDeclaration>>
DemonstratorJsonConfigLoader::loadPropConfig(const std::string& configPath) {
    std::ifstream ifs(configPath.c_str());
    if (!ifs) {
        return android::base::Error() << "couldn't open " << configPath << " for parsing.";
    }

    return loadPropConfig(ifs);
}

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
