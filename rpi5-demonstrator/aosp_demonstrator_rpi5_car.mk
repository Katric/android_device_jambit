# inherit aosp_rpi5_car config
$(call inherit-product, device/brcm/rpi5/aosp_rpi5_car.mk)

# inherit device config
$(call inherit-product, device/jambit/rpi5-demonstrator/device-rpi5-demonstrator.mk)

PRODUCT_DEVICE := rpi5-demonstrator
PRODUCT_NAME := aosp_demonstrator_rpi5_car
PRODUCT_BRAND := Raspberry
PRODUCT_MODEL := Raspberry Pi 5
PRODUCT_MANUFACTURER := jambit
PRODUCT_RELEASE_NAME := Raspberry Pi 5 Demonstrator
