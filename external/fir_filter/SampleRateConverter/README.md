# Sample Rate Converter

This is a C implementation of an audio sample rate convertor based on Polyphase FIR filter. It can be used to up or downconverting the sample rate of a raw audio stream with any fractional ratio. However this may not suitable as an arbitrary resampler as memory space consumption goes up linearly as the numerator of the ratio goes up. The stream should be in de-interleaved 32bit float format. The filter is stateful, capable of processing audio stream with arbitrary block size. The code does not depend on libraries other than standard math library (-lm). The state is decoupled from the algorithm, allowing code re-entrant and multi-instancing.

## Usage

Simple 44.1kHz to 192kHz upsampler:

```C
#include "src.h"
// 44.1kHz * 640 / 147 = 192kHz
FIR_Filter* filter = src_generate(640, 147); 
...
float* input_buffer[IN_SIZE]; // Incoming stream
float* output_buffer[IN_SIZE * 5]; // Outcoming stream
int output_length = src_filt(filter, input_buffer, input_length, output_buffer);
```

Manually pick the number of taps, cutoff frequency, and run mutiple instances:

```C
#include "src.h"
int interpolation = 2; // x 2
int decimation = 1;    // / 1
float cutoff_frequency = 0.5; // half the sampling frequency
int num_taps = 24 * interpolation;
float* coefficients = src_generate_fir_coeffs(num_taps, cutoff_frequency / interpolation);
FIR_Filter* filter_left = src_generate_fir_filter(coefficients, num_taps, interpolation, decimation);
FIR_Filter* filter_right = src_generate_fir_filter(coefficients, num_taps, interpolation, decimation);
free(coefficients);
...
float* input_buffer_left[IN_SIZE]; // Incoming stream
float* output_buffer_left[IN_SIZE * interpolation / decimation + 1]; // Outcoming stream
int output_length = src_filt(filter_left, input_buffer, input_length, output_buffer);
...
```

By default it uses 24 taps per phase and hanning window to construct the filter. This might not be the best configuration. For better filtering quality, higher taps count and kaiser window may be used.

## Performance

On ADSP-21589 SHARC+ DSP, upsampling 2 channels of audio from 44.1kHz to 48kHz uses ~24 MIPS.

## License

MIT.

Julia-DSP library source code was used as a reference, which is also licensed under MIT.