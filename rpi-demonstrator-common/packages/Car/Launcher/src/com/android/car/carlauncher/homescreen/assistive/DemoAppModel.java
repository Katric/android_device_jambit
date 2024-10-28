package com.android.car.carlauncher.homescreen.assistive;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

import com.android.car.carlauncher.R;
import com.android.car.carlauncher.homescreen.ui.CardContent;
import com.android.car.carlauncher.homescreen.ui.CardHeader;
import com.android.car.carlauncher.homescreen.ui.DescriptiveTextView;

import android.util.Log;

public class DemoAppModel implements AssistiveModel {

    private static final String PACKAGE_NAME = "de.jambit.chargingstations";
    private static final String TAG = "DemoAppModel";
    private CardHeader mCardHeader;
    private DescriptiveTextView mCardContent;
    private OnModelUpdateListener mOnModelUpdateListener;
    private Intent mIntent;

    @Override
    public void onCreate(Context context) {
        mCardHeader = new CardHeader(context.getString(R.string.demo_app_name),
                context.getDrawable(R.drawable.ic_jambit_logo_with_claim));
        mCardContent = new DescriptiveTextView(
                /* image= */ context.getDrawable(R.drawable.ic_jambit_coffeeright),
                /* title= */ context.getString(R.string.demo_app_main_text),
                /* subtitle= */ null,
                /* footer= */ context.getString(R.string.demo_app_footer_text));

        PackageManager packageManager = context.getPackageManager();
        mIntent = packageManager.getLaunchIntentForPackage(PACKAGE_NAME);
        mOnModelUpdateListener.onModelUpdate(this);
    }

    @Override
    public CardHeader getCardHeader() {
        return mCardHeader;
    }

    @Override
    public CardContent getCardContent() {
        return mCardContent;
    }

    @Override
    public void setOnModelUpdateListener(OnModelUpdateListener onModelUpdateListener) {
        mOnModelUpdateListener = onModelUpdateListener;
    }

    @Override
    public Intent getIntent() {
        if (mIntent == null) {
            Log.d(TAG, "Intent no found: " + PACKAGE_NAME);
            Log.i(TAG, "Starting Intent: " + PACKAGE_NAME);
            mIntent = new Intent(Intent.ACTION_MAIN);
            mIntent.addCategory(Intent.CATEGORY_DEFAULT);
            mIntent.addCategory(Intent.CATEGORY_LAUNCHER);
            mIntent.setPackage(PACKAGE_NAME);
        }
        return mIntent;
    }
}