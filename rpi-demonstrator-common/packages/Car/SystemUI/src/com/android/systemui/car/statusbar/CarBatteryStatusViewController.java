package com.android.systemui.car.statusbar;

import android.car.Car;
import android.car.VehiclePropertyIds;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyManager;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.Log;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import com.android.systemui.R;
import com.android.systemui.car.CarServiceProvider;
import com.android.systemui.dagger.SysUISingleton;

import javax.inject.Inject;

/**
 * Controls TextView and ImageView for displaying the car's battery level.
 */
@SysUISingleton
public class CarBatteryStatusViewController {

    private static final String TAG = CarBatteryStatusViewController.class.getSimpleName();
    private final Context mContext;
    private final CarServiceProvider mCarServiceProvider;
    private CarPropertyManager mCarPropertyManager = null;
    private TextView mBatteryTextView;
    private ImageView mBatteryImageView;
    private float mBatteryLevel;
    private float mBatteryCapacity = 150000.0f;
    private boolean mIsCharging;

    private final CarServiceProvider.CarServiceOnConnectedListener mCarOnConnectedListener =
            new CarServiceProvider.CarServiceOnConnectedListener() {
                @Override
                public void onConnected(Car car) {
                    mCarPropertyManager = (CarPropertyManager) car.getCarManager(
                            Car.PROPERTY_SERVICE);
                    if (mCarPropertyManager != null) {
                        mCarPropertyManager.registerCallback(mCarPropertyEventCallback,
                                VehiclePropertyIds.EV_BATTERY_LEVEL,
                                CarPropertyManager.SENSOR_RATE_UI);
                        mCarPropertyManager.registerCallback(mCarPropertyEventCallback,
                                VehiclePropertyIds.INFO_EV_BATTERY_CAPACITY,
                                CarPropertyManager.SENSOR_RATE_ONCHANGE);
                        mCarPropertyManager.registerCallback(mCarPropertyEventCallback,
                                VehiclePropertyIds.EV_CHARGE_PORT_CONNECTED,
                                CarPropertyManager.SENSOR_RATE_ONCHANGE);
                    }
                }
            };

    private final CarPropertyManager.CarPropertyEventCallback mCarPropertyEventCallback =
            new CarPropertyManager.CarPropertyEventCallback() {
                @Override
                public void onChangeEvent(CarPropertyValue value) {
                    int propId = value.getPropertyId();
                    if (propId == VehiclePropertyIds.EV_BATTERY_LEVEL) {
                        mBatteryLevel = (Float) value.getValue();
                    } else if (propId == VehiclePropertyIds.INFO_EV_BATTERY_CAPACITY) {
                        mBatteryCapacity = (Float) value.getValue();
                    } else if (propId == VehiclePropertyIds.EV_CHARGE_PORT_CONNECTED) {
                        mIsCharging = (Boolean) value.getValue();
                    }

                    updateBatteryStatusView();
                }

                @Override
                public void onErrorEvent(int propertyId, int zone) {
                    Log.e(TAG, "error occurs in event callback.");
                }
            };

    @Inject
    public CarBatteryStatusViewController(Context context, CarServiceProvider carServiceProvider) {
        mContext = context;
        mCarServiceProvider = carServiceProvider;
        mCarServiceProvider.addListener(mCarOnConnectedListener);
    }

    public void addBatteryStatusView(View v) {
        if (mBatteryImageView != null && mBatteryTextView != null) return;
        mBatteryTextView = v.findViewById(R.id.car_battery_status_text);
        mBatteryImageView = v.findViewById(R.id.car_battery_status_image);
    }

    public void removeAll() {
        mCarServiceProvider.removeListener(mCarOnConnectedListener);
        if (mCarPropertyManager != null) {
            mCarPropertyManager.unregisterCallback(mCarPropertyEventCallback);
        }
    }

    private void updateBatteryStatusView() {
        if (mBatteryTextView != null && mBatteryImageView != null) {
            int batteryPercentage = 0;
            if (mBatteryCapacity > 0) {
                batteryPercentage = (int) ((mBatteryLevel / mBatteryCapacity) * 100);
            }
            Drawable drawable = getBatteryDrawable(batteryPercentage);

            mBatteryImageView.setImageDrawable(drawable);

            if (mIsCharging) {
                // remove battery text in charging state
                mBatteryTextView.setVisibility(View.GONE);
            } else {
                mBatteryTextView.setText(String.valueOf(batteryPercentage));
                mBatteryTextView.setVisibility(View.VISIBLE);
            }
        }
    }

    private Drawable getBatteryDrawable(int batteryPercentage) {
        Drawable drawable = (mIsCharging) ? mContext.getDrawable(R.drawable.ic_battery_charging)
                : mContext.getDrawable(R.drawable.ic_battery);

        drawable.setLevel(batteryPercentage);
        return drawable;
    }
}

