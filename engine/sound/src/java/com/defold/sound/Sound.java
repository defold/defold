// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.defold.sound;

import com.defold.sound.SoundManager;

import android.content.Context;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;

public class Sound {

    public static int getSampleRate(Context context, int default_rate) {
        AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        String sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        if (sampleRate == null) {
            return default_rate;
        }
        return Integer.parseInt(sampleRate);
    }

    public static int getFramesPerBuffer(Context context) {
        final int default_frames = 1024;
        AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        String framesPerBuffer = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        if (framesPerBuffer == null) {
            return default_frames;
        }
        return Integer.parseInt(framesPerBuffer);
    }

}
