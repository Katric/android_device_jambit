package com.android.car.carlauncher.homescreen.assistive;

import android.content.Context;

import com.android.car.carlauncher.R;
import com.android.car.carlauncher.homescreen.ui.CardContent;
import com.android.car.carlauncher.homescreen.ui.CardHeader;
import com.android.car.carlauncher.homescreen.ui.DescriptiveTextView;

public class DemoAppModel implements AssistiveModel {
    private CardHeader mCardHeader;
    private DescriptiveTextView mCardContent;
    private OnModelUpdateListener mOnModelUpdateListener;

    @Override
    public void onCreate(Context context) {
        mCardHeader = new CardHeader(context.getString(R.string.demo_app_name),
                context.getDrawable(R.drawable.ic_jambit_logo_with_claim));
        mCardContent = new DescriptiveTextView(
                /* image= */ context.getDrawable(R.drawable.ic_jambit_coffeeright),
                /* title= */ context.getString(R.string.demo_app_main_text),
                /* subtitle= */ null,
                /* footer= */ context.getString(R.string.demo_app_footer_text));
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
}