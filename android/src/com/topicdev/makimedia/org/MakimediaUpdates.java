package com.topicdev.makimedia.org;

import android.app.Activity;
import android.util.Log;

import com.google.android.play.core.appupdate.AppUpdateManager;
import com.google.android.play.core.appupdate.AppUpdateManagerFactory;
import com.google.android.play.core.appupdate.AppUpdateOptions;
import com.google.android.play.core.install.InstallStateUpdatedListener;
import com.google.android.play.core.install.model.AppUpdateType;
import com.google.android.play.core.install.model.InstallStatus;
import com.google.android.play.core.install.model.UpdateAvailability;

public class MakimediaUpdates {
    private static final String TAG = "MakimediaUpdates";
    private static final int UPDATE_REQUEST_CODE = 5101;

    private static AppUpdateManager s_manager;
    private static InstallStateUpdatedListener s_listener;

    private static native void nativeUpdateReady();

    public static void check(final Activity activity) {
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                start(activity);
            }
        });
    }

    private static void start(final Activity activity) {
        final AppUpdateManager manager = AppUpdateManagerFactory.create(activity);
        s_manager = manager;

        manager.getAppUpdateInfo().addOnSuccessListener(info -> {
            if (info.installStatus() == InstallStatus.DOWNLOADED) {
                Log.i(TAG, "an update downloaded earlier is waiting");
                ready();
                return;
            }
            if (info.updateAvailability() != UpdateAvailability.UPDATE_AVAILABLE
                || !info.isUpdateTypeAllowed(AppUpdateType.FLEXIBLE)) {
                Log.i(TAG, "no update on google play");
                return;
            }

            s_listener = state -> {
                if (state.installStatus() == InstallStatus.DOWNLOADED) {
                    Log.i(TAG, "update downloaded");
                    ready();
                }
            };
            manager.registerListener(s_listener);

            try {
                manager.startUpdateFlowForResult(
                    info, activity,
                    AppUpdateOptions.newBuilder(AppUpdateType.FLEXIBLE).build(),
                    UPDATE_REQUEST_CODE);
            } catch (Exception e) {
                Log.w(TAG, "could not start the update", e);
            }
        }).addOnFailureListener(e -> Log.w(TAG, "could not ask google play: " + e.getMessage()));
    }

    private static void ready() {
        try {
            nativeUpdateReady();
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "update ready before the app finished loading");
        }
    }

    public static void complete() {
        final AppUpdateManager manager = s_manager;
        if (manager == null) {
            return;
        }
        if (s_listener != null) {
            manager.unregisterListener(s_listener);
            s_listener = null;
        }
        manager.completeUpdate();
    }
}
