package com.defold.sound;

import com.defold.sound.SoundManager;

import android.content.Context;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;

public class Sound {

    public static int getSampleRate(Context context) {
        final int default_rate = 44100;
        if (Build.VERSION.SDK_INT >= 17) {
            AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            String sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            if (sampleRate == null) {
                return default_rate;
            }
            return Integer.parseInt(sampleRate);
        } else {
            Log.w(SoundManager.TAG, "Android version < 17. Unable to determine hardware sample rate.");
            return default_rate;
        }
    }

    public static int getFramesPerBuffer(Context context) {
        final int default_frames = 1024;
        if (Build.VERSION.SDK_INT >= 17) {
            AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            String framesPerBuffer = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            if (framesPerBuffer == null) {
                return default_frames;
            }
            return Integer.parseInt(framesPerBuffer);
        } else {
            Log.w(SoundManager.TAG, "Android version < 17. Unable to determine hardware frame count.");
            return default_frames;
        }
    }

}
