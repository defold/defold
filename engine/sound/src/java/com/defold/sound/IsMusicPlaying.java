package com.defold.sound;

import android.content.Context;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;

public class IsMusicPlaying {

    private static class AudioFocusListener implements AudioManager.OnAudioFocusChangeListener {

        @Override
        public void onAudioFocusChange(int focusChange) {

        }

    };

    public static boolean isMusicPlayingFocus(Context context) {
        int result = AudioManager.AUDIOFOCUS_REQUEST_FAILED;
        boolean isPlaying = true;
        AudioManager audioManager = null;
        IsMusicPlaying.AudioFocusListener audioFocusListener = null;
        try {
            audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            if (audioManager != null) {
                Log.i("sound", "Attempting to acquire audio focus");
                audioFocusListener = new IsMusicPlaying.AudioFocusListener();
                result = audioManager.requestAudioFocus(audioFocusListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
                if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                    isPlaying = true;
                    Log.i("sound", "Succeeded to acquire audio focus!");
                } else if (result == AudioManager.AUDIOFOCUS_REQUEST_FAILED) {
                    isPlaying = false;
                    Log.i("sound", "Failed to acquire audio focus!");
                }
            }
        } catch (Exception exception) {
            Log.i("sound", "Exception occurred while querying audio focus!");
        } finally {
            if (audioManager != null && audioFocusListener != null) {
                result = audioManager.abandonAudioFocus(audioFocusListener);
                if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                    Log.i("sound", "Succeeded to abandon audio focus!");
                } else if (result == AudioManager.AUDIOFOCUS_REQUEST_FAILED) {
                    Log.i("sound", "Failed to abandon audio focus!");
                }
            }
        }

        return isPlaying;
    }

    public static boolean isMusicPlaying(Context context) {
        try {
            AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            return audioManager.isMusicActive();
        } catch (Exception exception) {
            Log.e("sound", "Exception occurred while checking audio activity!");
        }

        return true;
    }

}