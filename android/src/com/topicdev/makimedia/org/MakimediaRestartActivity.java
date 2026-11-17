package com.topicdev.makimedia.org;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;

public class MakimediaRestartActivity extends Activity {
    private static final String TAG = "MakimediaRestart";
    private static final String EXTRA_PID = "com.topicdev.makimedia.org.RESTART_PID";

    public static void restartApp() {
        final Activity activity = MakimediaActivity.instance();
        if (activity == null) {
            Log.w(TAG, "no activity, cannot restart");
            return;
        }

        Log.i(TAG, "restarting the app to free the video decoder");
        Intent intent = new Intent(activity, MakimediaRestartActivity.class);
        intent.putExtra(EXTRA_PID, Process.myPid());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final int appPid = getIntent().getIntExtra(EXTRA_PID, -1);
        if (appPid > 0 && appPid != Process.myPid()) {
            Process.killProcess(appPid);
        }

        Intent launch = getPackageManager().getLeanbackLaunchIntentForPackage(getPackageName());
        if (launch == null) {
            launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
        }
        if (launch != null) {
            launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
            startActivity(launch);
        } else {
            Log.e(TAG, "no launch intent, the app has to be opened by hand");
        }

        finish();
        Runtime.getRuntime().exit(0);
    }
}
