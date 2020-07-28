/*-
 * Copyright (c) 2005 Boris Mikhaylov
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include "bs2b.h"

#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif

/* Single pole IIR filter.
 * O[n] = a0*I[n] + a1*I[n-1] + b1*O[n-1]
 */

/* Lowpass filter */
#define lo_filter(in, out_1) (bs2b->a0_lo*(in) + bs2b->b1_lo*(out_1))

/* Highboost filter */
#define hi_filter(in, in_1, out_1) (bs2b->a0_hi*(in) + bs2b->a1_hi*(in_1) + bs2b->b1_hi*(out_1))

/* Set up all data. */
static void init(struct bs2b *bs2b)
{
    double Fc_lo, Fc_hi;
    double G_lo,  G_hi;
    double x;

    if ((bs2b->srate > 192000) || (bs2b->srate < 2000))
        bs2b->srate = BS2B_DEFAULT_SRATE;

    switch(bs2b->level)
    {
    case BS2B_LOW_CLEVEL: /* Low crossfeed level */
        Fc_lo = 360.0;
        Fc_hi = 501.0;
        G_lo  = 0.398107170553497;
        G_hi  = 0.205671765275719;
        break;

    case BS2B_MIDDLE_CLEVEL: /* Middle crossfeed level */
        Fc_lo = 500.0;
        Fc_hi = 711.0;
        G_lo  = 0.459726988530872;
        G_hi  = 0.228208484414988;
        break;

    case BS2B_HIGH_CLEVEL: /* High crossfeed level (virtual speakers are closer to itself) */
        Fc_lo = 700.0;
        Fc_hi = 1021.0;
        G_lo  = 0.530884444230988;
        G_hi  = 0.250105790667544;
        break;

    case BS2B_LOW_ECLEVEL: /* Low easy crossfeed level */
        Fc_lo = 360.0;
        Fc_hi = 494.0;
        G_lo  = 0.316227766016838;
        G_hi  = 0.168236228897329;
        break;

    case BS2B_MIDDLE_ECLEVEL: /* Middle easy crossfeed level */
        Fc_lo = 500.0;
        Fc_hi = 689.0;
        G_lo  = 0.354813389233575;
        G_hi  = 0.187169483835901;
        break;

    default: /* High easy crossfeed level */
        bs2b->level = BS2B_HIGH_ECLEVEL;

        Fc_lo = 700.0;
        Fc_hi = 975.0;
        G_lo  = 0.398107170553497;
        G_hi  = 0.205671765275719;
        break;
    } /* switch */

    /* $fc = $Fc / $s;
     * $d  = 1 / 2 / pi / $fc;
     * $x  = exp(-1 / $d);
     */

    x           = exp(-2.0 * M_PI * Fc_lo / bs2b->srate);
    bs2b->b1_lo = x;
    bs2b->a0_lo = G_lo * (1.0 - x);

    x           = exp(-2.0 * M_PI * Fc_hi / bs2b->srate);
    bs2b->b1_hi = x;
    bs2b->a0_hi = 1.0 - G_hi * (1.0 - x);
    bs2b->a1_hi = -x;

    bs2b->gain  = 1.0f / (float)(1.0 - G_hi + G_lo);
} /* init */

/* Exported functions.
 * See descriptions in "bs2b.h"
 */

void bs2b_set_level(struct bs2b *bs2b, int level)
{
    if(level == bs2b->level)
        return;
    bs2b->level = level;
    init(bs2b);
} /* bs2b_set_level */

int bs2b_get_level(struct bs2b *bs2b)
{
    return bs2b->level;
} /* bs2b_get_level */

void bs2b_set_srate(struct bs2b *bs2b, int srate)
{
    if (srate == bs2b->srate)
        return;
    bs2b->srate = srate;
    init(bs2b);
} /* bs2b_set_srate */

int bs2b_get_srate(struct bs2b *bs2b)
{
    return bs2b->srate;
} /* bs2b_get_srate */

void bs2b_clear(struct bs2b *bs2b)
{
    memset(&bs2b->last_sample, 0, sizeof(bs2b->last_sample));
} /* bs2b_clear */

void bs2b_cross_feed(struct bs2b *bs2b, float *sample)
{
    /* Lowpass filter */
    bs2b->last_sample.lo[0] = lo_filter(sample[0], bs2b->last_sample.lo[0]);
    bs2b->last_sample.lo[1] = lo_filter(sample[1], bs2b->last_sample.lo[1]);

    /* Highboost filter */
    bs2b->last_sample.hi[0] = hi_filter(sample[0], bs2b->last_sample.asis[0], bs2b->last_sample.hi[0]);
    bs2b->last_sample.hi[1] = hi_filter(sample[1], bs2b->last_sample.asis[1], bs2b->last_sample.hi[1]);
    bs2b->last_sample.asis[0] = sample[0];
    bs2b->last_sample.asis[1] = sample[1];

    /* Crossfeed */
    sample[0] = (float)(bs2b->last_sample.hi[0] + bs2b->last_sample.lo[1]);
    sample[1] = (float)(bs2b->last_sample.hi[1] + bs2b->last_sample.lo[0]);

    /* Bass boost cause allpass attenuation */
    sample[0] *= bs2b->gain;
    sample[1] *= bs2b->gain;

    /* Clipping of overloaded samples */
#if 0
    if (sample[0] > 1.0)
        sample[0] = 1.0;
    if (sample[0] < -1.0)
        sample[0] = -1.0;
    if (sample[1] > 1.0)
        sample[1] = 1.0;
    if (sample[1] < -1.0)
        sample[1] = -1.0;
#endif
} /* bs2b_cross_feed */
