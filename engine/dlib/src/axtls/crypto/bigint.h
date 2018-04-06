/*
 * Copyright (c) 2007, Cameron Rich
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the axTLS project nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BIGINT_HEADER
#define BIGINT_HEADER

#include <axtls/crypto/crypto.h>

DM_BI_CTX *DM_bi_initialize(void);
void DM_bi_terminate(DM_BI_CTX *ctx);
void DM_bi_permanent(bigint *bi);
void DM_bi_depermanent(bigint *bi);
void DM_bi_clear_cache(DM_BI_CTX *ctx);
void DM_bi_free(DM_BI_CTX *ctx, bigint *bi);
bigint *DM_bi_copy(bigint *bi);
bigint *DM_bi_clone(DM_BI_CTX *ctx, const bigint *bi);
void DM_bi_export(DM_BI_CTX *ctx, bigint *bi, uint8_t *data, int size);
bigint *DM_bi_import(DM_BI_CTX *ctx, const uint8_t *data, int len);
bigint *DM_int_to_bi(DM_BI_CTX *ctx, comp i);

/* the functions that actually do something interesting */
bigint *DM_bi_add(DM_BI_CTX *ctx, bigint *bia, bigint *bib);
bigint *DM_bi_subtract(DM_BI_CTX *ctx, bigint *bia,
        bigint *bib, int *is_negative);
bigint *DM_bi_divide(DM_BI_CTX *ctx, bigint *bia, bigint *bim, int is_mod);
bigint *DM_bi_multiply(DM_BI_CTX *ctx, bigint *bia, bigint *bib);
bigint *DM_bi_mod_power(DM_BI_CTX *ctx, bigint *bi, bigint *biexp);
bigint *DM_bi_mod_power2(DM_BI_CTX *ctx, bigint *bi, bigint *bim, bigint *biexp);
int DM_bi_compare(bigint *bia, bigint *bib);
void DM_bi_set_mod(DM_BI_CTX *ctx, bigint *bim, int mod_offset);
void DM_bi_free_mod(DM_BI_CTX *ctx, int mod_offset);

#ifdef CONFIG_SSL_FULL_MODE
void DM_bi_print(const char *label, bigint *bi);
bigint *DM_bi_str_import(DM_BI_CTX *ctx, const char *data);
#endif

/**
 * @def bi_mod
 * Find the residue of B. bi_set_mod() must be called before hand.
 */
#define bi_mod(A, B)      DM_bi_divide(A, B, ctx->bi_mod[ctx->mod_offset], 1)

/**
 * bi_residue() is technically the same as bi_mod(), but it uses the
 * appropriate reduction technique (which is bi_mod() when doing classical
 * reduction).
 */
#if defined(CONFIG_BIGINT_MONTGOMERY)
#define DM_bi_residue(A, B)         DM_bi_mont(A, B)
bigint *DM_bi_mont(DM_BI_CTX *ctx, bigint *bixy);
#elif defined(CONFIG_BIGINT_BARRETT)
#define DM_bi_residue(A, B)         DM_bi_barrett(A, B)
bigint *DM_bi_barrett(DM_BI_CTX *ctx, bigint *bi);
#else /* if defined(CONFIG_BIGINT_CLASSICAL) */
#define DM_bi_residue(A, B)         DM_bi_mod(A, B)
#endif

#ifdef CONFIG_BIGINT_SQUARE
bigint *DM_bi_square(DM_BI_CTX *ctx, bigint *bi);
#else
#define DM_bi_square(A, B)     DM_bi_multiply(A, bi_copy(B), B)
#endif

#ifdef CONFIG_BIGINT_CRT
bigint *DM_bi_crt(DM_BI_CTX *ctx, bigint *bi,
        bigint *dP, bigint *dQ,
        bigint *p, bigint *q,
        bigint *qInv);
#endif

#endif
