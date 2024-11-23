#ifndef android_hardware_automotive_vehicle_aidl_impl_fake_impl_hardware_include_GpioFakeVehicleHardware_H_
#define android_hardware_automotive_vehicle_aidl_impl_fake_impl_hardware_include_GpioFakeVehicleHardware_H_

#define PWM_RANGE 255
#define FAN_PWM_PIN 0
#define DT_PIN 2
#define CLK_PIN 3

#define RED_PIN 21
#define GREEN_PIN 22
#define BLUE_PIN 23

#include <FakeVehicleHardware.h>
#include <DemonstratorJsonConfigLoader.h>

namespace android {
    namespace hardware {
        namespace automotive {
            namespace vehicle {
                namespace fake {

                    class GpioFakeVehicleHardware : public FakeVehicleHardware {
                    public:
                        GpioFakeVehicleHardware();

                        ~GpioFakeVehicleHardware();

                        void handleBatteryChange();

                        aidl::android::hardware::automotive::vehicle::StatusCode setValues(
                                std::shared_ptr<const SetValuesCallback> callback,
                                const std::vector<aidl::android::hardware::automotive::vehicle::SetValueRequest> &
                                requests) override;

                    private:
                        // Only used during initialization.
                        DemonstratorJsonConfigLoader mConfigLoader;

                        void init();

                        void initGpio();

                        void setUpAndStorePropInitialValue(const ConfigDeclaration &config);

                        VhalResult<void> setPwmHvacFanSpeed(int32_t hvacFanSpeedLevel);

                        // Load the config files in format '*.json' from the directory and parse the config files
                        // into a map from property ID to ConfigDeclarations.
                        void loadPropConfigsFromDir(const std::string &dirPath,
                                                    std::unordered_map<int32_t, ConfigDeclaration> *configs);

                        aidl::android::hardware::automotive::vehicle::SetValueResult
                        handleSetValueRequest(
                                const aidl::android::hardware::automotive::vehicle::SetValueRequest &request);

                        VhalResult<void> handleSetHvacFanSpeed(
                                const aidl::android::hardware::automotive::vehicle::VehiclePropValue &value);

                        VhalResult<void> handleSetAmbientLightMode(const aidl::android::hardware::automotive::vehicle::VehiclePropValue &value);

                        VhalResult<void> handleSetCustomAmbientLightColor(const aidl::android::hardware::automotive::vehicle::VehiclePropValue &value);

                        VhalResult<void> setPwmAmbientLightColorToBatteryLevel(float batteryLevelPercent);

                        VhalResult<void> setPwmAmbientLightColor(int32_t red, int32_t green, int32_t blue);

                        VhalResult<float> calculateCurrentBatteryLevelPercent();

                        VhalResult<void> setValue(
                                const aidl::android::hardware::automotive::vehicle::VehiclePropValue &value);

                        VhalResult<void> maybeSetSpecialDemonstratorValue(
                                const aidl::android::hardware::automotive::vehicle::VehiclePropValue &value,
                                bool *isSpecialDemonstratorValue);

                        struct SetRequestWithCallback {
                            const aidl::android::hardware::automotive::vehicle::SetValueRequest request;
                            std::shared_ptr<const SetValuesCallback> callback;
                        };

                        class PendingSetRequestHandler {
                        public:
                            PendingSetRequestHandler(GpioFakeVehicleHardware *hardware);

                            void addRequest(
                                    aidl::android::hardware::automotive::vehicle::SetValueRequest request,
                                    std::shared_ptr<const SetValuesCallback> callback);

                            void stop();

                        private:
                            GpioFakeVehicleHardware *mHardware;
                            std::thread mThread;
                            ConcurrentQueue<SetRequestWithCallback> mRequests;

                            void handleSetValueRequests();
                        };

                        mutable PendingSetRequestHandler mPendingSetValueRequests;
                    };

                }
            }
        }
    }
}

#endif //android_hardware_automotive_vehicle_aidl_impl_fake_impl_hardware_include_GpioFakeVehicleHardware_H_