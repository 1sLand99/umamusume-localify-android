package com.kimjio.umamusumelocalify.settings.app;

import android.app.Application;

import androidx.annotation.OptIn;
import androidx.window.core.ExperimentalWindowApi;
import androidx.window.embedding.RuleController;
import androidx.window.embedding.SplitController;

import com.kimjio.umamusumelocalify.settings.R;

public class SettingsApplication extends Application {
    private static final String TAG = "SettingsApplication";

    @OptIn(markerClass = ExperimentalWindowApi.class)
    @Override
    public void onCreate() {
        super.onCreate();
        if (SplitController.getInstance(this).getSplitSupportStatus() == SplitController.SplitSupportStatus.SPLIT_AVAILABLE) {
            RuleController.getInstance(this).setRules(RuleController.parseRules(this, R.xml.split_configuration));
        }
    }
}
