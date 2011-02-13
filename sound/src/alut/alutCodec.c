#include "alutInternal.h"

ALvoid *
_alutCodecLinear (ALvoid *data, size_t length, ALint numChannels,
                  ALint bitsPerSample, ALfloat sampleFrequency,
                  ALint blockAlign)
{
  return _alutBufferDataConstruct (data, length, numChannels, bitsPerSample,
                                   sampleFrequency);
}

ALvoid *
_alutCodecPCM8s (ALvoid *data, size_t length, ALint numChannels,
                 ALint bitsPerSample, ALfloat sampleFrequency,
                 ALint blockAlign)
{
  int8_t *d = (int8_t *) data;
  size_t i;
  for (i = 0; i < length; i++)
    {
      d[i] += (int8_t) 128;
    }
  return _alutBufferDataConstruct (data, length, numChannels, bitsPerSample,
                                   sampleFrequency);
}

ALvoid *
_alutCodecPCM16 (ALvoid *data, size_t length, ALint numChannels,
                 ALint bitsPerSample, ALfloat sampleFrequency,
                 ALint blockAlign)
{
  int16_t *d = (int16_t *) data;
  size_t i, l = length / 2;
  for (i = 0; i < l; i++)
    {
      int16_t x = d[i];
      d[i] = ((x << 8) & 0xFF00) | ((x >> 8) & 0x00FF);
    }
  return _alutBufferDataConstruct (data, length, numChannels, bitsPerSample,
                                   sampleFrequency);
}

/*
 * From: http://www.multimedia.cx/simpleaudio.html#tth_sEc6.1
 */
static int16_t
mulaw2linear (uint8_t mulawbyte)
{
  static const int16_t exp_lut[8] = {
    0, 132, 396, 924, 1980, 4092, 8316, 16764
  };
  int16_t sign, exponent, mantissa, sample;
  mulawbyte = ~mulawbyte;
  sign = (mulawbyte & 0x80);
  exponent = (mulawbyte >> 4) & 0x07;
  mantissa = mulawbyte & 0x0F;
  sample = exp_lut[exponent] + (mantissa << (exponent + 3));
  if (sign != 0)
    {
      sample = -sample;
    }
  return sample;
}

ALvoid *
_alutCodecULaw (ALvoid *data, size_t length, ALint numChannels,
                ALint bitsPerSample, ALfloat sampleFrequency,
                ALint blockAlign)
{
  uint8_t *d = (uint8_t *) data;
  int16_t *buf = (int16_t *) _alutMalloc (length * 2);
  size_t i;
  if (buf == NULL)
    {
      return NULL;
    }
  for (i = 0; i < length; i++)
    {
      buf[i] = mulaw2linear (d[i]);
    }
  free (data);
  return _alutBufferDataConstruct (buf, length * 2, numChannels,
                                   bitsPerSample, sampleFrequency);
}

/*
 * From: http://www.multimedia.cx/simpleaudio.html#tth_sEc6.2
 */
#define SIGN_BIT (0x80)         /* Sign bit for a A-law byte. */
#define QUANT_MASK (0xf)        /* Quantization field mask. */
#define SEG_SHIFT (4)           /* Left shift for segment number. */
#define SEG_MASK (0x70)         /* Segment field mask. */
static int16_t
alaw2linear (uint8_t a_val)
{
  int16_t t, seg;
  a_val ^= 0x55;
  t = (a_val & QUANT_MASK) << 4;
  seg = ((int16_t) a_val & SEG_MASK) >> SEG_SHIFT;
  switch (seg)
    {
    case 0:
      t += 8;
      break;
    case 1:
      t += 0x108;
      break;
    default:
      t += 0x108;
      t <<= seg - 1;
    }
  return (a_val & SIGN_BIT) ? t : -t;
}

ALvoid *
_alutCodecALaw (ALvoid *data, size_t length, ALint numChannels,
                ALint bitsPerSample, ALfloat sampleFrequency,
                ALint blockAlign)
{
  uint8_t *d = (uint8_t *) data;
  int16_t *buf = (int16_t *) _alutMalloc (length * 2);
  size_t i;
  if (buf == NULL)
    {
      return NULL;
    }
  for (i = 0; i < length; i++)
    {
      buf[i] = alaw2linear (d[i]);
    }
  free (data);
  return _alutBufferDataConstruct (buf, length * 2, numChannels,
                                   bitsPerSample, sampleFrequency);
}


/*
 * From: http://www.multimedia.cx/simpleaudio.html#tth_sEc4.2
 */
static int16_t
ima2linear (uint8_t nibble, int16_t *val, uint8_t *idx)
{
  static const int16_t index_table[16] =
    {
      -1, -1, -1, -1, 2, 4, 6, 8,
      -1, -1, -1, -1, 2, 4, 6, 8
    };
  static const int16_t step_table[89] =
    {
      7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
      19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
      50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
      130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
      337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
      876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
      2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
      5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
      15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
    };
  int16_t predictor, diff, step;
  int8_t delta, sign;
  int8_t index;

  index = *idx;
  step = step_table[index];
  predictor = *val;

  index += index_table[nibble];
  if (index < 0) index = 0;
  if (index > 88) index = 88;

  sign = nibble & 0x8;
  delta = nibble & 0x7;

  diff = step >> 3;
  if (delta & 4) diff += step;
  if (delta & 2) diff += (step >> 1);
  if (delta & 1) diff += (step >> 2);

  if (sign) predictor -= diff;
  else predictor += diff;

  *val = predictor;
  *idx = index;

  return predictor;
}

#define MAX_IMA_CHANNELS	2
ALvoid *
_alutCodecIMA4 (ALvoid *data, size_t length, ALint numChannels,
                ALint bitsPerSample, ALfloat sampleFrequency,
                ALint blockAlign)
{
  uint8_t *d = (uint8_t *) data;
  int16_t *ptr, *buf;
  size_t i, blocks;

  blocks = length/blockAlign;
  buf = (int16_t *) _alutMalloc ((blockAlign-numChannels)*blocks * 4);
  if ((buf == NULL) || (numChannels > MAX_IMA_CHANNELS))
    {
      return NULL;
    }

  ptr = buf;
  for (i = 0; i < blocks; i++)
    {
      int16_t predictor[MAX_IMA_CHANNELS];
      uint8_t nibble, index[MAX_IMA_CHANNELS];
      size_t j, chn;

      for (chn=0; chn < numChannels; chn++)
        {
          predictor[chn] = *d++;
          predictor[chn] |= *d++ << 8;

          index[chn] = *d++;
          d++;
        }

      for (j=numChannels*4; j < blockAlign;)
        {
          for (chn = 0; chn < numChannels; chn++)
            {
              int16_t *ptr_ch;
              size_t q;

              ptr_ch = ptr + chn;
              for (q=0; q<4; q++)
                {

                  nibble = *d & 0xf;
                  *ptr_ch = ima2linear(nibble, &predictor[chn], &index[chn]);
                  ptr_ch += numChannels;

                  nibble = *d++ >> 4;
                  *ptr_ch = ima2linear(nibble, &predictor[chn], &index[chn]);
                  ptr_ch += numChannels;
                }
            }
          j += numChannels*4;
          ptr += numChannels*8;
        }
    }
  free (data);
  return _alutBufferDataConstruct (buf, (blockAlign-numChannels)*blocks * 4,
                                   numChannels, bitsPerSample, sampleFrequency);
}
#undef MAX_IMA_CHANNELS
