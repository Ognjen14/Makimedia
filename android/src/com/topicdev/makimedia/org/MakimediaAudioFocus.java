package com.topicdev.makimedia.org;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;

public class MakimediaAudioFocus {
    private static final String TAG = "MakimediaAudioFocus";

    private static AudioManager m_audioManager;
    private static AudioFocusRequest m_request;
    private static BroadcastReceiver m_noisyReceiver;
    private static boolean m_held;

    private static native void nativeFocusChanged(int change);
    private static native void nativeBecomingNoisy();

    private static AudioManager audioManager() {
        Context context = MakimediaActivity.applicationContext();
        if (context == null) return null;

        if (m_audioManager == null) {
            m_audioManager = (AudioManager)
                context.getSystemService(Context.AUDIO_SERVICE);
        }
        return m_audioManager;
    }

    private static AudioFocusRequest request() {
        if (m_request == null) {
            AudioAttributes attributes = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
                .build();

            m_request = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(attributes)
                .setWillPauseWhenDucked(false)
                .setOnAudioFocusChangeListener(change -> {
                    Log.i(TAG, "onAudioFocusChange " + change);
                    try {
                        nativeFocusChanged(change);
                    } catch (UnsatisfiedLinkError e) {
                        Log.e(TAG, "nativeFocusChanged is not bound", e);
                    }
                })
                .build();
        }
        return m_request;
    }

    private static void registerNoisyReceiver() {
        Context context = MakimediaActivity.applicationContext();
        if (context == null || m_noisyReceiver != null) return;

        m_noisyReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (!AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction()))
                    return;
                Log.i(TAG, "ACTION_AUDIO_BECOMING_NOISY");
                try {
                    nativeBecomingNoisy();
                } catch (UnsatisfiedLinkError e) {
                    Log.e(TAG, "nativeBecomingNoisy is not bound", e);
                }
            }
        };

        IntentFilter filter =
            new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                context.registerReceiver(m_noisyReceiver, filter,
                                         Context.RECEIVER_NOT_EXPORTED);
            } else {
                context.registerReceiver(m_noisyReceiver, filter);
            }
            Log.i(TAG, "becoming-noisy receiver registered");
        } catch (Exception e) {
            Log.e(TAG, "could not register the becoming-noisy receiver", e);
            m_noisyReceiver = null;
        }
    }

    private static void unregisterNoisyReceiver() {
        if (m_noisyReceiver == null) return;

        Context context = MakimediaActivity.applicationContext();
        if (context != null) {
            try {
                context.unregisterReceiver(m_noisyReceiver);
                Log.i(TAG, "becoming-noisy receiver unregistered");
            } catch (IllegalArgumentException e) {
                Log.w(TAG, "becoming-noisy receiver was not registered");
            }
        }
        m_noisyReceiver = null;
    }

    public static boolean requestFocus() {
        AudioManager manager = audioManager();
        if (manager == null) {
            Log.e(TAG, "no AudioManager, the app has no context yet");
            return false;
        }
        if (m_held) return true;

        int result = manager.requestAudioFocus(request());
        m_held = result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
        Log.i(TAG, "requestAudioFocus returned " + result
                   + (m_held ? " (granted)" : " (NOT granted)"));

        if (m_held) {
            registerNoisyReceiver();
        }
        return m_held;
    }

    public static boolean holdsFocus() {
        return m_held;
    }

    public static void abandonFocus() {
        AudioManager manager = audioManager();
        unregisterNoisyReceiver();

        if (manager == null || !m_held) {
            m_held = false;
            return;
        }

        Log.i(TAG, "abandonAudioFocusRequest");
        manager.abandonAudioFocusRequest(request());
        m_held = false;
    }
}
