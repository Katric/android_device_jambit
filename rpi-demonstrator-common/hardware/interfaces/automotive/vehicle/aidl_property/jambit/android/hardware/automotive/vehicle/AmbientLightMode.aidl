package jambit.android.hardware.automotive.vehicle;

/**
 * Used to enumerate the mode of Ambient Light.
 */
@VintfStability
@Backing(type="int")
enum AmbientLightMode {

    /**
    * Changes ambient light colors depending on current charging level.
    */
    BATTERY_LEVEL = 0,
    /**
     * User can chose custom color for ambient light.
     */
    CUSTOM = 1,
}
