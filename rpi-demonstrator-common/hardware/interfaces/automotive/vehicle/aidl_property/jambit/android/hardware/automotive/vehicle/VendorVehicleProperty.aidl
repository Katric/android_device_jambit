package jambit.android.hardware.automotive.vehicle;
/**
* This AIDL file defines Vendor VHAL properties. It is an extension of the existing VehicleProperty.aidl
*/

import android.hardware.automotive.vehicle.VehicleArea;
import android.hardware.automotive.vehicle.VehiclePropertyGroup;
import android.hardware.automotive.vehicle.VehiclePropertyType;

@VintfStability
@Backing(type="int")
enum VendorVehicleProperty {
    /**
    * Sets the color of the ambient light inside the vehicle.
    * It expects an array of VehiclePropertyType.INT32_VEC, where 3 values must be set in this order to control the color of 
    * the ambient light:
    * red (0): 0-100
    * green (1): 0-100
    * blue (2): 0-100 
    *
    * Returns the currently set color in the above mentioned scheme.
    *
    * @change_mode VehiclePropertyChangeMode.ON_CHANGE
    * @access VehiclePropertyAccess.READ_WRITE
    */
    AMBIENT_LIGHT_COLOR = 0x4000 + VehiclePropertyGroup.VENDOR + VehicleArea.GLOBAL + VehiclePropertyType.INT32_VEC,

    /**
    * Ambient Light Mode state.
    *
    * Returns the current state of Ambient Light Mode. Valid states are defined in AmbientLightMode.
    *
    * @change_mode VehiclePropertyChangeMode.ON_CHANGE
    * @access VehiclePropertyAccess.READ_WRITE
    */
     AMBIENT_LIGHT_MODE = 0x4001 + VehiclePropertyGroup.VENDOR + VehicleArea.GLOBAL + VehiclePropertyType.INT32,
}
