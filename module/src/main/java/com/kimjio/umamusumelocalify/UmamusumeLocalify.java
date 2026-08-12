package com.kimjio.umamusumelocalify;

import static androidx.media3.session.DefaultMediaNotificationProvider.DEFAULT_CHANNEL_NAME_RESOURCE_ID;

import android.app.Activity;
import android.annotation.SuppressLint;
import android.app.Application;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.core.app.NotificationCompat;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.graphics.drawable.IconCompat;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.Player;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.session.CommandButton;
import androidx.media3.session.DefaultMediaNotificationProvider;
import androidx.media3.session.MediaNotification;
import androidx.media3.session.MediaSession;

@UnstableApi
@SuppressWarnings({"unused", "JavaJniMissingFunction"})
public final class UmamusumeLocalify {
    private static final String TAG = "UmamusumeLocalify[Java]";

    private static final String CHANNEL_ID = "media_playback_channel";
    private static final int NOTIFICATION_ID = 101;
    private static final String APK_PATH = "/system/app/UmamusumeLocalify/UmamusumeLocalify.apk";
    private static final String ACTION_MEDIA_COMMAND = "com.kimjio.umamusumelocalify.ACTION_MEDIA_COMMAND";
    private static final String EXTRA_COMMAND = "command";

    @SuppressLint("StaticFieldLeak")
    private static DefaultMediaNotificationProvider notificationProvider;
    private static MediaNotification.ActionFactory actionFactory;
    private static LocalifyMediaPlayer player;
    private static MediaSession mediaSession;
    private static final BroadcastReceiver commandReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (ACTION_MEDIA_COMMAND.equals(intent.getAction()) && mediaSession != null) {
                var command = intent.getIntExtra(EXTRA_COMMAND, -1);
                var player = mediaSession.getPlayer();
                if (command == Player.COMMAND_PLAY_PAUSE) {
                    if (player.getPlayWhenReady()) {
                        player.pause();
                    } else {
                        player.play();
                    }
                } else if (command == Player.COMMAND_SEEK_TO_NEXT) {
                    player.seekToNext();
                } else if (command == Player.COMMAND_SEEK_TO_PREVIOUS) {
                    player.seekToPrevious();
                }
            }
        }
    };

    public static native void onLayoutChange_native(Activity activity, View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom);

    public static native void handleSetPlayWhenReady_native(boolean playWhenReady);

    public static native void handleSeek_native(int mediaItemIndex, long positionMs, @Player.Command int seekCommand);

    private static boolean lifecycleRegistered = false;

    public static void load() {
        Log.i(TAG, TAG + " loaded");
    }

    private static void registerLifecycleCallbacks(Context context) {
        if (lifecycleRegistered) return;

        var app = (Application) context.getApplicationContext();
        app.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(@NonNull Activity activity, @Nullable Bundle savedInstanceState) {
            }

            @Override
            public void onActivityStarted(@NonNull Activity activity) {
            }

            @Override
            public void onActivityResumed(@NonNull Activity activity) {
                updatePlaybackState(activity, Player.STATE_READY);
            }

            @Override
            public void onActivityPaused(@NonNull Activity activity) {
                updatePlayWhenReady(activity, false);
                updatePlaybackState(activity, Player.STATE_IDLE);
            }

            @Override
            public void onActivityStopped(@NonNull Activity activity) {
            }

            @Override
            public void onActivitySaveInstanceState(@NonNull Activity activity, @NonNull Bundle outState) {
            }

            @Override
            public void onActivityDestroyed(@NonNull Activity activity) {
                Log.d(TAG, "onActivityDestroyed: " + activity.getClass().getName());
                Log.d(TAG, "onActivityDestroyed: " + activity.getClass().getSuperclass().getName());
            }
        });
        lifecycleRegistered = true;
    }

    public static void registerCallback(Activity activity) {
        activity.getWindow().getDecorView().addOnLayoutChangeListener((View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) -> v.post(() -> onLayoutChange_native(activity, v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom)));
    }

    public static Insets getDisplayCutoutInsets(Activity activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsets windowInsets = activity.getWindowManager().getCurrentWindowMetrics().getWindowInsets();
            return Insets.toCompatInsets(windowInsets.getInsets(WindowInsets.Type.displayCutout()));
        } else {
            WindowInsets windowInsets = activity.getWindow().getDecorView().getRootWindowInsets();
            if (windowInsets != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                DisplayCutout cutout = windowInsets.getDisplayCutout();
                if (cutout != null) {
                    return Insets.of(cutout.getSafeInsetLeft(), cutout.getSafeInsetTop(), cutout.getSafeInsetRight(), cutout.getSafeInsetBottom());
                }
            }
        }
        return Insets.NONE;
    }

    public static Insets getCaptionBarInsets(Activity activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!activity.isInMultiWindowMode()) {
                return Insets.NONE;
            }

            WindowInsets windowInsets = activity.getWindow().getDecorView().getRootWindowInsets();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
                if (windowInsets.getBoundingRects(WindowInsets.Type.captionBar()).isEmpty()) {
                    return Insets.NONE;
                }
            }
            return Insets.toCompatInsets(windowInsets.getInsets(WindowInsets.Type.captionBar()));
        }
        return Insets.NONE;
    }

    public static Insets getCaptionBarInsetsIgnoringVisibility(Activity activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!activity.isInMultiWindowMode()) {
                return Insets.NONE;
            }

            WindowInsets windowInsets = activity.getWindow().getDecorView().getRootWindowInsets();
            return Insets.toCompatInsets(windowInsets.getInsets(WindowInsets.Type.captionBar()));
        }
        return Insets.NONE;
    }

    public static boolean isEdgeToEdgeEnabled(Activity activity) {
        if (activity.getApplicationInfo().targetSdkVersion < Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            return false;
        }

        WindowManager.LayoutParams attributes = activity.getWindow().getAttributes();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return attributes.layoutInDisplayCutoutMode != WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
        }

        return false;
    }

    @SuppressWarnings("deprecation")
    @OptIn(markerClass = UnstableApi.class)
    public static void showMediaNotification(Context context) {
        try {
            registerLifecycleCallbacks(context);

            AssetManager assets = AssetManager.class.getDeclaredConstructor().newInstance();
            AssetManager.class.getMethod("addAssetPath", String.class).invoke(assets, APK_PATH);

            var res = new Resources(assets, context.getResources().getDisplayMetrics(), context.getResources().getConfiguration());

            var moduleContext = new ModuleContext(context.getApplicationContext(), res);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                NotificationChannel channel = new NotificationChannel(CHANNEL_ID, res.getString(R.string.default_notification_channel_name), NotificationManager.IMPORTANCE_LOW);
                NotificationManager manager = context.getSystemService(NotificationManager.class);
                if (manager != null) {
                    manager.createNotificationChannel(channel);
                }
            }

            var filter = new IntentFilter(ACTION_MEDIA_COMMAND);
            ContextCompat.registerReceiver(context, commandReceiver, filter, ContextCompat.RECEIVER_EXPORTED);

            player = new LocalifyMediaPlayer(moduleContext);

            var launchIntent = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
            PendingIntent sessionActivityPendingIntent;
            if (launchIntent != null) {
                sessionActivityPendingIntent = PendingIntent.getActivity(context, 0, launchIntent, PendingIntent.FLAG_IMMUTABLE);
            } else {
                return;
            }

            mediaSession = new MediaSession.Builder(moduleContext, player).setSessionActivity(sessionActivityPendingIntent).build();

            notificationProvider = new DefaultMediaNotificationProvider.Builder(moduleContext).setNotificationIdProvider(session -> NOTIFICATION_ID).setChannelId(CHANNEL_ID).setChannelName(DEFAULT_CHANNEL_NAME_RESOURCE_ID).build();

            notificationProvider.setSmallIcon(R.drawable.ic_carrot);

            actionFactory = new MediaNotification.ActionFactory() {
                @NonNull
                @Override
                public NotificationCompat.Action createMediaAction(@NonNull MediaSession session, @NonNull IconCompat icon, @NonNull CharSequence title, int command) {
                    return new NotificationCompat.Action.Builder(icon, title, createMediaActionPendingIntent(session, command)).build();
                }

                @NonNull
                @Override
                public NotificationCompat.Action createCustomAction(@NonNull MediaSession session, @NonNull IconCompat icon, @NonNull CharSequence title, @NonNull String customAction, @NonNull Bundle extras) {
                    return new NotificationCompat.Action.Builder(icon, title, null).build();
                }

                @NonNull
                @Override
                public NotificationCompat.Action createCustomActionFromCustomCommandButton(@NonNull MediaSession session, @NonNull CommandButton customCommandButton) {
                    return new NotificationCompat.Action.Builder(IconCompat.createWithResource(moduleContext, customCommandButton.iconResId), customCommandButton.displayName, null).build();
                }

                @NonNull
                @Override
                public PendingIntent createMediaActionPendingIntent(@NonNull MediaSession session, int command) {
                    var intent = new Intent(ACTION_MEDIA_COMMAND);
                    intent.setPackage(context.getPackageName());
                    intent.putExtra(EXTRA_COMMAND, command);
                    return PendingIntent.getBroadcast(context, command, intent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
                }
            };

            player.addListener(new Player.Listener() {
                @Override
                public void onEvents(@NonNull Player player, @NonNull Player.Events events) {
                    updateNotification(context);
                }
            });

            updateNotification(context);
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize media notification", e);
        }
    }

    @OptIn(markerClass = UnstableApi.class)
    public static void updateMetadata(Context context, String title, String artist, byte[] artworkData) {
        new Handler(Looper.getMainLooper()).post(() -> {
            if (player != null) {
                var builder = new MediaMetadata.Builder().setTitle(title).setArtist(artist);

                if (artworkData != null && artworkData.length > 0) {
                    builder.setArtworkData(artworkData, MediaMetadata.PICTURE_TYPE_FRONT_COVER);
                }

                player.updateMetadataInternal(builder.build());
            }
        });
    }

    public static boolean hasArtworkData() {
        if (player != null) {
            return player.hasArtworkData();
        }
        return false;
    }

    @OptIn(markerClass = UnstableApi.class)
    public static void updateProgress(Context context, long positionMs, long durationMs) {
        new Handler(Looper.getMainLooper()).post(() -> {
            if (player != null) {
                player.updateProgressInternal(positionMs, durationMs);
            }
        });
    }

    @OptIn(markerClass = UnstableApi.class)
    public static void updatePlaybackState(Context context, int state) {
        new Handler(Looper.getMainLooper()).post(() -> {
            if (player != null) {
                player.updatePlaybackStateInternal(state);
            }
        });
    }

    @OptIn(markerClass = UnstableApi.class)
    public static void updatePlayWhenReady(Context context, boolean playWhenReady) {
        new Handler(Looper.getMainLooper()).post(() -> {
            if (player != null) {
                player.updatePlayWhenReadyInternal(playWhenReady);
            }
        });
    }

    @OptIn(markerClass = UnstableApi.class)
    public static void updateNavigationButtons(Context context, boolean next, boolean previous) {
        new Handler(Looper.getMainLooper()).post(() -> {
            if (player != null) {
                player.updateNavigationButtonsInternal(next, previous);
            }
        });
    }

    @OptIn(markerClass = UnstableApi.class)
    public static void hideNotification(Context context) {
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                var manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
                if (manager != null) {
                    manager.cancel(NOTIFICATION_ID);
                }

                if (mediaSession != null) {
                    mediaSession.release();
                    mediaSession = null;
                }
                if (player != null) {
                    player.release();
                    player = null;
                }
                try {
                    context.unregisterReceiver(commandReceiver);
                } catch (Exception ignored) {
                }
            } catch (Exception e) {
                Log.e(TAG, "Failed to hide notification", e);
            }
        });
    }

    @OptIn(markerClass = UnstableApi.class)
    private static void updateNotification(Context context) {
        new Handler(Looper.getMainLooper()).post(() -> {
            if (mediaSession == null || notificationProvider == null || actionFactory == null || player == null) {
                return;
            }

            var mediaNotification = notificationProvider.createNotification(mediaSession, mediaSession.getMediaButtonPreferences(), actionFactory, notification -> showNotification(context, notification));

            showNotification(context, mediaNotification);
        });
    }

    private static void showNotification(Context context, MediaNotification notification) {
        var manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager != null) {
            notification.notification.flags |= Notification.FLAG_NO_CLEAR;

            if (player != null && player.getPlayWhenReady()) {
                notification.notification.flags |= Notification.FLAG_ONGOING_EVENT;
            } else {
                notification.notification.flags &= ~Notification.FLAG_ONGOING_EVENT;
            }

            manager.notify(notification.notificationId, notification.notification);
        }
    }

    private static class ModuleContext extends ContextWrapper {
        private final Resources resources;

        public ModuleContext(Context base, Resources resources) {
            super(base);
            this.resources = resources;
        }

        @Override
        public Resources getResources() {
            return resources;
        }

        @Override
        public String getPackageName() {
            return "com.kimjio.umamusumelocalify";
        }
    }
}
