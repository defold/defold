/* 
 * Sample Rate Converter
 *
 * Copyright 2019 Wenting Zhang
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 */
#ifndef __SRC_H__
#define __SRC_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    FIR_LOWPASS,
    FIR_HIGHPASS,
    FIR_BANDPASS,
    FIR_BANDSTOP
} FIR_Response;

typedef struct {
    float* pfb;
    int num_phases;
    int taps_per_phase;
    int interpolation;
    int decimation;
    int phase_index;
    int input_deficit;
    float* history;
    int history_length;
} FIR_Filter;

float* src_generate_fir_coeffs(int num_taps, float cutoff);
FIR_Filter* src_generate_fir_filter(float* coefficients, int num_taps,
        int interpolation, int decimation);
FIR_Filter* src_generate(int interpolation, int decimation);
int src_filt(FIR_Filter* filter, float* samples, int count, float* p_output);

#ifdef __cplusplus
}
#endif

#endif