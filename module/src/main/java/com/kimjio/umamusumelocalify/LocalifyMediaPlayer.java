package com.kimjio.umamusumelocalify;

import android.content.Context;
import android.os.Looper;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.media3.common.C;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.Player;
import androidx.media3.common.SimpleBasePlayer;
import androidx.media3.common.util.Clock;
import androidx.media3.common.util.UnstableApi;

import com.google.common.collect.ImmutableList;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

@UnstableApi
public class LocalifyMediaPlayer extends SimpleBasePlayer {
    private MediaMetadata playlistMetadata;
    private boolean playWhenReady = false;
    private long currentPositionMs = 0;
    private long durationUs = C.TIME_UNSET;
    private int playbackState = Player.STATE_READY;
    private boolean isNextEnabled = false;
    private boolean isPreviousEnabled = false;

    public LocalifyMediaPlayer(Context context) {
        super(Looper.getMainLooper(), Clock.DEFAULT);
        playlistMetadata = new MediaMetadata.Builder().setTitle(context.getString(R.string.default_notification_channel_name)).setArtist("").build();
    }

    @NonNull
    @Override
    protected State getState() {
        MediaItemData itemData = new MediaItemData.Builder("localify_item")
                .setMediaItem(new MediaItem.Builder().setMediaId("localify_item").build())
                .setDurationUs(durationUs)
                .setMediaMetadata(playlistMetadata)
                .build();

        Commands.Builder commandsBuilder = new Commands.Builder()
                .addAllCommands()
                .remove(Player.COMMAND_SEEK_TO_NEXT)
                .remove(Player.COMMAND_SEEK_TO_NEXT_MEDIA_ITEM)
                .remove(Player.COMMAND_SEEK_TO_PREVIOUS)
                .remove(Player.COMMAND_SEEK_TO_PREVIOUS_MEDIA_ITEM)
                .remove(Player.COMMAND_SET_REPEAT_MODE);

        if (isNextEnabled) {
            commandsBuilder.add(Player.COMMAND_SEEK_TO_NEXT);
            commandsBuilder.add(Player.COMMAND_SEEK_TO_NEXT_MEDIA_ITEM);
        }
        if (isPreviousEnabled) {
            commandsBuilder.add(Player.COMMAND_SEEK_TO_PREVIOUS);
            commandsBuilder.add(Player.COMMAND_SEEK_TO_PREVIOUS_MEDIA_ITEM);
        }

        return new State.Builder()
                .setAvailableCommands(commandsBuilder.build())
                .setPlaylist(ImmutableList.of(itemData))
                .setContentPositionMs(currentPositionMs)
                .setPlayWhenReady(playWhenReady, Player.PLAY_WHEN_READY_CHANGE_REASON_USER_REQUEST)
                .setPlaybackState(playbackState)
                .build();
    }

    public void updateMetadataInternal(MediaMetadata metadata) {
        this.playlistMetadata = metadata;
        invalidateState();
    }

    public void updateProgressInternal(long positionMs, long durationMs) {
        this.currentPositionMs = positionMs;
        if (durationMs <= 0) {
            this.durationUs = C.TIME_UNSET;
        } else {
            this.durationUs = durationMs * 1000;
        }
        invalidateState();
    }

    public void updatePlaybackStateInternal(int state) {
        this.playbackState = state;
        invalidateState();
    }

    public void updateNavigationButtonsInternal(boolean next, boolean previous) {
        this.isNextEnabled = next;
        this.isPreviousEnabled = previous;
        invalidateState();
    }

    public void updatePlayWhenReadyInternal(boolean playWhenReady) {
        this.playWhenReady = playWhenReady;
        invalidateState();
    }

    public boolean hasArtworkData() {
        return this.playlistMetadata.artworkData != null && this.playlistMetadata.artworkData.length > 0;
    }

    @NonNull
    @Override
    public ListenableFuture<?> handleSetPlayWhenReady(boolean playWhenReady) {
        UmamusumeLocalify.handleSetPlayWhenReady_native(playWhenReady);
        this.playWhenReady = playWhenReady;
        return Futures.immediateFuture(null);
    }

    @NonNull
    @Override
    protected ListenableFuture<?> handleSeek(int mediaItemIndex, long positionMs, @Command int seekCommand) {
        UmamusumeLocalify.handleSeek_native(mediaItemIndex, positionMs, seekCommand);
        return Futures.immediateFuture(null);
    }

    @NonNull
    @Override
    protected ListenableFuture<?> handleRelease() {
        return Futures.immediateFuture(null);
    }
}
