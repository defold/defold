# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import math, struct, wave, cStringIO

def gen_tone(name, tone_freq, sample_freq, sample_count, ramp = False):
    f = wave.open(name, "wb")
    f.setnchannels(1)
    f.setsampwidth(2)
    f.setframerate(sample_freq)
    buf = cStringIO.StringIO()

    frames = []
    for i in range(sample_count):
        a = 0.8 * 32768 * math.sin((i * 2.0 * math.pi * freq) / sample_freq)
        if ramp:
            r = ((sample_count - 1) - i) / float(sample_count)
            a = a * r
        buf.write(struct.pack('h', int(a)))
    f.writeframes(buf.getvalue())
    f.close()

length = 2
for sample_freq in [22050, 32000, 44100]:
    for freq in [440, 2000]:
        samples = length * sample_freq
        name = 'tone_%d_%d_%d.wav' % (samples, freq, sample_freq)
        gen_tone(name, freq, sample_freq, samples)

for sample_freq in [32000]:
    for freq in [440]:
        samples = length * sample_freq
        name = 'toneramp_%d_%d_%d.wav' % (samples, freq, sample_freq)
        gen_tone(name, freq, sample_freq, samples, True)
