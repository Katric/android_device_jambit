package com.android.systemui.car.statusbar;

import android.car.Car;
import android.car.VehiclePropertyIds;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyManager;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import com.android.systemui.R;
import com.android.systemui.car.CarServiceProvider;
import com.android.systemui.dagger.SysUISingleton;
import javax.inject.Inject;

/**
 * Controls TextView for displaying the car's remaining range.
 */
@SysUISingleton
public class CarRangeStatusViewController {

    private static final String TAG = CarRangeStatusViewController.class.getSimpleName();
    private final CarServiceProvider mCarServiceProvider;

    private CarPropertyManager mCarPropertyManager = null;
    private TextView mRangeTextView;

    private final CarServiceProvider.CarServiceOnConnectedListener mCarOnConnectedListener =
            new CarServiceProvider.CarServiceOnConnectedListener() {
                @Override
                public void onConnected(Car car) {
                    mCarPropertyManager = (CarPropertyManager) car.getCarManager(Car.PROPERTY_SERVICE);
                    if (mCarPropertyManager != null) {
                        mCarPropertyManager.registerCallback(mCarPropertyEventCallback,
                                VehiclePropertyIds.RANGE_REMAINING,
                                CarPropertyManager.SENSOR_RATE_UI);
                    }
                }
            };

    private final CarPropertyManager.CarPropertyEventCallback mCarPropertyEventCallback =
            new CarPropertyManager.CarPropertyEventCallback() {
                @Override
                public void onChangeEvent(CarPropertyValue value) {
                    if (value.getPropertyId() == VehiclePropertyIds.RANGE_REMAINING) {
                        float rangeRemaining = (Float) value.getValue();
                        int rangeRemainingKm = (int) (rangeRemaining / 1000.0);
                        updateRangeText(rangeRemainingKm);
                    }
                }

                @Override
                public void onErrorEvent(int propertyId, int zone) {
                    Log.e(TAG, "error occurs in event callback.");
                }
            };

    @Inject
    public CarRangeStatusViewController(CarServiceProvider carServiceProvider) {
        mCarServiceProvider = carServiceProvider;
        mCarServiceProvider.addListener(mCarOnConnectedListener);
    }

    public void addRangeTextView(View v) {
        mRangeTextView = v.findViewById(R.id.car_range_status_text);
    }

    public void removeAll() {
        mCarServiceProvider.removeListener(mCarOnConnectedListener);
        if (mCarPropertyManager != null) {
            mCarPropertyManager.unregisterCallback(mCarPropertyEventCallback);
        }
    }

    /**
     * Updates the text with the current range remaining value.
     */
    private void updateRangeText(int rangeRemaining) {
        if (mRangeTextView != null) {
            mRangeTextView.setText("~ %d km".formatted(rangeRemaining));
        }
    }
}

