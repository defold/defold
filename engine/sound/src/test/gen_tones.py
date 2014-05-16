import math, struct, wave, cStringIO

def gen_tone(name, tone_freq, sample_freq, sample_count):
    f = wave.open(name, "wb")
    f.setnchannels(1)
    f.setsampwidth(2)
    f.setframerate(sample_freq)
    buf = cStringIO.StringIO()

    frames = []
    for i in range(sample_count):
        a = 0.8 * 32768 * math.sin((i * 2.0 * math.pi * freq) / sample_freq)
        buf.write(struct.pack('h', int(a)))
    f.writeframes(buf.getvalue())
    f.close()

length = 2
for sample_freq in [22050, 32000, 44100]:
    for freq in [440, 2000]:
        samples = length * sample_freq
        name = 'tone_%d_%d_%d.wav' % (samples, freq, sample_freq)
        gen_tone(name, freq, sample_freq, samples)
