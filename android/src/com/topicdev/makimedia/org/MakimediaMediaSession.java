package com.topicdev.makimedia.org;

import android.content.Context;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.KeyEvent;

public class MakimediaMediaSession {
    private static final String TAG = "MakimediaMediaSession";

    private static final Handler s_main = new Handler(Looper.getMainLooper());

    private static MediaSession m_session;
    private static String m_title = "";
    private static String m_subtitle = "";
    private static long m_durationMs = 0;
    private static boolean m_playing;
    private static long m_positionMs;
    private static float m_speed = 1.0f;
    private static long m_updateTime;

    private static native void nativeTransportCommand(int command, long argument);

    private static final int CMD_PLAY = 1;
    private static final int CMD_PAUSE = 2;
    private static final int CMD_TOGGLE = 3;
    private static final int CMD_STOP = 4;
    private static final int CMD_NEXT = 5;
    private static final int CMD_PREVIOUS = 6;
    private static final int CMD_SEEK = 7;

    private static void dispatch(int command, long argument) {
        Log.i(TAG, "transport command " + command + " arg " + argument);
        try {
            nativeTransportCommand(command, argument);
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "nativeTransportCommand is not bound", e);
        }
    }

    public static void setActive(final boolean active) {
        s_main.post(new Runnable() {
            @Override
            public void run() {
                if (active) {
                    ensureSession();
                    if (m_session != null) {
                        m_session.setActive(true);
                        Log.i(TAG, "media session active");
                    }
                } else {
                    releaseSession("playback stopped");
                }
            }
        });
    }

    // From the activity's onDestroy: the session is made from the application
    // context and would otherwise outlive it, active and unowned.
    static void releaseForDestroyedActivity() {
        releaseSession("activity destroyed");
    }

    private static void releaseSession(String why) {
        if (m_session == null) {
            return;
        }
        m_session.setActive(false);
        m_session.release();
        m_session = null;
        Log.i(TAG, "media session released, " + why);
    }

    private static void ensureSession() {
        if (m_session != null) return;

        Context context = MakimediaActivity.applicationContext();
        if (context == null) {
            Log.w(TAG, "no application context yet, media session not created");
            return;
        }

        m_session = new MediaSession(context, "Makimedia");
        m_session.setCallback(new MediaSession.Callback() {
            @Override
            public void onPlay() {
                dispatch(CMD_PLAY, 0);
            }

            @Override
            public void onPause() {
                dispatch(CMD_PAUSE, 0);
            }

            @Override
            public void onStop() {
                dispatch(CMD_STOP, 0);
            }

            @Override
            public void onSkipToNext() {
                dispatch(CMD_NEXT, 0);
            }

            @Override
            public void onSkipToPrevious() {
                dispatch(CMD_PREVIOUS, 0);
            }

            @Override
            public void onSeekTo(long pos) {
                dispatch(CMD_SEEK, pos);
            }

            @Override
            public boolean onMediaButtonEvent(android.content.Intent intent) {
                KeyEvent event = intent.getParcelableExtra(android.content.Intent.EXTRA_KEY_EVENT);
                if (event == null || event.getAction() != KeyEvent.ACTION_DOWN) {
                    return super.onMediaButtonEvent(intent);
                }

                switch (event.getKeyCode()) {
                case KeyEvent.KEYCODE_HEADSETHOOK:
                case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                    dispatch(CMD_TOGGLE, 0);
                    return true;
                default:
                    return super.onMediaButtonEvent(intent);
                }
            }
        });

        applyMetadata();
        applyPlaybackState();
    }

    public static void updateMetadata(final String title, final String subtitle,
                                      final long durationMs) {
        s_main.post(new Runnable() {
            @Override
            public void run() {
                m_title = title == null ? "" : title;
                m_subtitle = subtitle == null ? "" : subtitle;
                m_durationMs = durationMs;
                applyMetadata();
            }
        });
    }

    public static void updatePlaybackState(final boolean playing, final long positionMs,
                                           final double speed) {
        final long capturedAt = SystemClock.elapsedRealtime();

        s_main.post(new Runnable() {
            @Override
            public void run() {
                m_playing = playing;
                m_positionMs = positionMs;
                m_speed = (float) speed;
                m_updateTime = capturedAt;
                applyPlaybackState();
            }
        });
    }

    private static void applyMetadata() {
        if (m_session == null) return;

        MediaMetadata metadata = new MediaMetadata.Builder()
            .putString(MediaMetadata.METADATA_KEY_TITLE, m_title)
            .putString(MediaMetadata.METADATA_KEY_ARTIST, m_subtitle)
            .putLong(MediaMetadata.METADATA_KEY_DURATION, m_durationMs)
            .build();

        m_session.setMetadata(metadata);
    }

    private static void applyPlaybackState() {
        if (m_session == null) return;

        long actions = PlaybackState.ACTION_PLAY
                     | PlaybackState.ACTION_PAUSE
                     | PlaybackState.ACTION_PLAY_PAUSE
                     | PlaybackState.ACTION_STOP
                     | PlaybackState.ACTION_SEEK_TO
                     | PlaybackState.ACTION_SKIP_TO_NEXT
                     | PlaybackState.ACTION_SKIP_TO_PREVIOUS;

        final long updateTime = m_updateTime > 0 ? m_updateTime
                                                 : SystemClock.elapsedRealtime();

        PlaybackState state = new PlaybackState.Builder()
            .setActions(actions)
            .setState(m_playing ? PlaybackState.STATE_PLAYING
                                : PlaybackState.STATE_PAUSED,
                      m_positionMs,
                      m_playing ? m_speed : 0.0f,
                      updateTime)
            .build();

        m_session.setPlaybackState(state);
    }
}
