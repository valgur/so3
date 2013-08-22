// S03 package to perform Wigner transform on the rotation group SO(3)
// Copyright (C) 2013 Martin Buettner and Jason McEwen
// See LICENSE.txt for license details

/*!
 * \file so3_core.c
 * Core algorithms to perform Wigner transform on the rotation group SO(§).
 *
 * \author <a href="mailto:m.buettner.d@gmail.com">Martin Buettner</a>
           <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>  // Must be before fftw3.h
#include <fftw3.h>

#include "ssht.h"

#include "so3_types.h"
#include "so3_error.h"
#include "so3_sampling.h"

/*!
 * Compute inverse transform for MW method via SSHT.
 *
 * \param[out] f Function on sphere. Provide a buffer of size (2*L-1)*L*(2*N-1).
 * \param[in] flmn Harmonic coefficients.
 * \param[in] L Harmonic band-limit.
 * \param[in] M Azimuthal band-limit.
 * \param[in] N Orientational band-limit.
 * \param[in] verbosity Verbosity flag in range [0,5].
 * \retval none
 *
 * \author <a href="mailto:m.buettner.d@gmail.com">Martin Buettner</a>
           <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
void so3_core_mw_inverse_via_ssht(complex double *f, const complex double *flmn,
	int L, int N,
        so3_storage_t storage,
	int verbosity)
{
    // Iterator
    int n;
    // Intermediate results
    complex double *fn, *flm;
    // Stride for several arrays
    int fn_n_stride;
    // FFTW-related variables
    int fftw_rank, fftw_howmany;
    int fftw_idist, fftw_odist;
    int fftw_istride, fftw_ostride;
    int fftw_n;
    fftw_plan plan;

    // Print messages depending on verbosity level.
    if (verbosity > 0) {
        printf("%sComputing inverse transform using MW sampling with\n", SO3_PROMPT);
        printf("%sparameters  (L, N, reality) = (%d, %d, FALSE)\n", SO3_PROMPT, L, N);
        if (verbosity > 1)
            printf("%sUsing routine so3_core_mw_inverse_via_ssht with storage method %d...\n"
                    , SO3_PROMPT
                    , storage);
    }

    // Compute fn(a,b)

    fn_n_stride = L * (2*L-1);

    fn = malloc((2*N-1)*fn_n_stride * sizeof *fn);
    SO3_ERROR_MEM_ALLOC_CHECK(fn);

    // Initialize fftw_plan first. With FFTW_ESTIMATE this is technically not
    // necessary but still good practice.
    fftw_rank = 1;
    fftw_n = 2*N-1;
    fftw_howmany = fn_n_stride;
    fftw_idist = fftw_odist = 1;
    fftw_istride = fftw_ostride = fn_n_stride;

    plan = fftw_plan_many_dft(
            fftw_rank, &fftw_n, fftw_howmany,
            fn, NULL, fftw_istride, fftw_idist,
            f, NULL, fftw_ostride, fftw_odist,
            FFTW_BACKWARD, FFTW_ESTIMATE
    );

    flm = calloc(L*L, sizeof *flm);
    SO3_ERROR_MEM_ALLOC_CHECK(flm);

    for(n = -N+1; n < N; ++n)
    {
        int ind, offset, i, el;

        switch (storage)
        {
        case SO3_STORE_ZERO_FIRST_PAD:
        case SO3_STORE_NEG_FIRST_PAD:
            so3_sampling_elmn2ind(&ind, 0, 0, n, L, N, storage);
            memcpy(flm, flmn + ind, L*L * sizeof(complex double));
            break;
        case SO3_STORE_ZERO_FIRST_COMPACT:
        case SO3_STORE_NEG_FIRST_COMPACT:
            so3_sampling_elmn2ind(&ind, abs(n), -abs(n), n, L, N, storage);
            memcpy(flm + n*n, flmn + ind, (L*L - n*n) * sizeof(complex double));
            for(i = 0; i < n*n; ++i)
                flm[i] = 0.0;
            break;
        default:
            SO3_ERROR_GENERIC("Invalid storage method.");
        }

        offset = 0;
        i = 0;
        for(el = 0; el < L; ++el)
        {
            for (; i < offset + 2*el+1; ++i)
                flm[i] *= sqrt((double)(2*el+1)/(16.*pow(SO3_PI, 3.)));

            offset = i;
        }

        // The conditional applies the spatial transform, so that we store
        // the results in n-order 0, 1, 2, -2, -1
        offset = (n < 0 ? n + 2*N-1 : n);
        ssht_core_mw_inverse_sov_sym(fn + offset*fn_n_stride, flm, L, -n, SSHT_DL_TRAPANI, verbosity);

        if(n % 2)
            for(i = 0; i < fn_n_stride; ++i)
                fn[offset*fn_n_stride + i] = -fn[offset*fn_n_stride + i];

        if (verbosity > 0)
            printf("\n");
    }

    free(flm);

    fftw_execute(plan);
    fftw_destroy_plan(plan);

    free(fn);

    if (verbosity > 0)
        printf("%sInverse transform computed!\n", SO3_PROMPT);
}

void so3_core_mw_forward_via_ssht(complex double *flmn, const complex double *f,
	int L, int N,
        so3_storage_t storage,
	int verbosity)
{
    // Iterator
    int i, n;
    // Intermediate results
    complex double *ftemp, *flm, *fn;
    // Stride for several arrays
    int fn_n_stride;
    // FFTW-related variables
    int fftw_rank, fftw_howmany;
    int fftw_idist, fftw_odist;
    int fftw_istride, fftw_ostride;
    int fftw_n;
    fftw_plan plan;

    // Print messages depending on verbosity level.
    if (verbosity > 0) {
        printf("%sComputing forward transform using MW sampling with\n", SO3_PROMPT);
        printf("%sparameters  (L, N, reality) = (%d, %d, FALSE)\n", SO3_PROMPT, L, N);
        if (verbosity > 1)
            printf("%sUsing routine so3_core_mw_forward_via_ssht with storage method %d...\n"
                    , SO3_PROMPT
                    , storage);
    }

    fn_n_stride = L * (2*L-1);

    // Make a copy of the input, because input is const
    // This could potentially be avoided by copying the input into fn and using an
    // in-place FFTW. The performance impact has to be profiled, though.
    ftemp = malloc((2*N-1)*fn_n_stride * sizeof *ftemp);
    SO3_ERROR_MEM_ALLOC_CHECK(ftemp);
    memcpy(ftemp, f, (2*N-1)*fn_n_stride * sizeof(complex double));

    fn = malloc((2*N-1)*fn_n_stride * sizeof *fn);
    SO3_ERROR_MEM_ALLOC_CHECK(fn);

    // Initialize fftw_plan first. With FFTW_ESTIMATE this is technically not
    // necessary but still good practice.
    fftw_rank = 1;
    fftw_n = 2*N-1;
    fftw_howmany = fn_n_stride;
    fftw_idist = fftw_odist = 1;
    fftw_istride = fftw_ostride = fn_n_stride;

    plan = fftw_plan_many_dft(
            fftw_rank, &fftw_n, fftw_howmany,
            ftemp, NULL, fftw_ostride, fftw_odist,
            fn, NULL, fftw_istride, fftw_idist,
            FFTW_FORWARD, FFTW_ESTIMATE
    );

    fftw_execute(plan);
    fftw_destroy_plan(plan);

    free(ftemp);

    for(i = 0; i < (2*N-1)*fn_n_stride; ++i)
        fn[i] *= 2*SO3_PI/(double)(2*N-1);

    if (storage == SO3_STORE_ZERO_FIRST_COMPACT || storage == SO3_STORE_NEG_FIRST_COMPACT)
        flm = malloc(L*L * sizeof *flm);

    for(n = -N+1; n < N; ++n)
    {
        int ind, offset, el, sign;

        if (n % 2)
            sign = -1;
        else
            sign = 1;

        // The conditional applies the spatial transform, so that we store
        // the results in n-order 0, 1, 2, -2, -1
        offset = (n < 0 ? n + 2*N-1 : n);

        switch (storage)
        {
        case SO3_STORE_ZERO_FIRST_PAD:
        case SO3_STORE_NEG_FIRST_PAD:
            so3_sampling_elmn2ind(&ind, 0, 0, n, L, N, storage);
            ssht_core_mw_forward_sov_conv_sym(flmn + ind, fn + offset*fn_n_stride, L, -n, SSHT_DL_TRAPANI, verbosity);

            i = offset = 0;
            el = 0;
            break;
        case SO3_STORE_ZERO_FIRST_COMPACT:
        case SO3_STORE_NEG_FIRST_COMPACT:
            ssht_core_mw_forward_sov_conv_sym(flm, fn + offset*fn_n_stride, L, -n, SSHT_DL_TRAPANI, verbosity);
            so3_sampling_elmn2ind(&ind, abs(n), -abs(n), n, L, N, storage);
            memcpy(flmn + ind, flm + n*n, (L*L - n*n) * sizeof(complex double));

            i = offset = 0;
            el = abs(n);
            break;
        default:
            SO3_ERROR_GENERIC("Invalid storage method.");
        }

        for(; el < L; ++el)
        {
            for (; i < offset + 2*el+1; ++i)
                flmn[ind + i] *= sign*sqrt(4.0*SO3_PI/(double)(2*el+1));

            offset = i;
        }

        if (verbosity > 0)
            printf("\n");
    }

    if (storage == SO3_STORE_ZERO_FIRST_COMPACT || storage == SO3_STORE_NEG_FIRST_COMPACT)
        free(flm);

    free(fn);

    if (verbosity > 0)
        printf("%sForward transform computed!\n", SO3_PROMPT);

}
