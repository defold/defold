/*
*/

#include "SampleRateConverter/src.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int taps_per_phase = 8;             // we want 8n taps per phase
    int interpolation = 2048;           // x 2048 to give us some precision in choosing fractional read positions from the source
    int decimation = 1;                 // dummy, would only be used if we actually run a SRC here
    float cutoff_frequency = 0.32;      // cutoff to elliminate unwanted frequencies (theoretically: 0.5 - but with 8 taps we are having a fairly "soft" frequency response)
    int num_taps = taps_per_phase * interpolation;
    float* coefficients = src_generate_fir_coeffs(num_taps, cutoff_frequency / interpolation);
    FIR_Filter* filter = src_generate_fir_filter(coefficients, num_taps, interpolation, decimation);
    free(coefficients);

    // normalize tap's factors
    float w = (float)num_taps / filter->taps_per_phase;

    printf("static constexpr int32_t _pfb_num_phases = %d;\n", filter->num_phases);
    printf("static constexpr int32_t _pfb_num_taps = %d;\n", filter->taps_per_phase);
    printf("static float _pfb[]={\n");
    for(int p=0; p<filter->num_phases; ++p) {
        printf("    ");
        for(int t=0; t<filter->taps_per_phase; ++t) {
            float f = filter->pfb[filter->taps_per_phase * p + t] * w;
            printf ("%e, ", f);
        }
        printf("\n");
    }
    printf("};\n");

    return 0;
}
