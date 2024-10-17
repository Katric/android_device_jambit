COMMON_PATH := device/jambit/rpi-demonstrator-common

# Vehicle
PRODUCT_PACKAGES += android.hardware.automotive.vehicle@V1-default-service-jambit \
# for overriding default vhal properties in FakeVehicleHardware
PRODUCT_PROPERTY_OVERRIDES += persist.vendor.vhal_init_value_override=true \

# Keyboard
PRODUCT_PACKAGES += \
    CarLatinIMECustom \

# System-Apps
PRODUCT_PACKAGES += \
    FakeGPS \
    Jambilight \
	CarAOSPHost \
	# FDroid \
    # SnappMaps \

# Privileged permissions
PRODUCT_COPY_FILES += \
    $(COMMON_PATH)/packages/Car/CarAOSPHost/permissions/com.android.car.templates.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/com.android.car.templates.host.xml \
    $(COMMON_PATH)/packages/Jambilight/permissions/privapp-permissions-com.jambit.jambilight.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/privapp-permissions-com.jambit.jambilight.xml