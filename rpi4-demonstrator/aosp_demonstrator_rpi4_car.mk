# override bootanimation
PRODUCT_COPY_FILES += \
	device/jambit/rpi-demonstrator-common/packages/services/Car/car_product/bootanimations/bootanimation.zip:system/media/bootanimation.zip

# inherit aosp_rpi4_car config
$(call inherit-product, device/brcm/rpi4/aosp_rpi4_car.mk)

# inherit device config
$(call inherit-product, device/jambit/rpi4-demonstrator/device-rpi4-demonstrator.mk)

PRODUCT_DEVICE := rpi4-demonstrator
PRODUCT_NAME := aosp_demonstrator_rpi4_car
PRODUCT_BRAND := Raspberry
PRODUCT_MODEL := Raspberry Pi 4
PRODUCT_MANUFACTURER := jambit
PRODUCT_RELEASE_NAME := Raspberry Pi 4 Demonstrator
