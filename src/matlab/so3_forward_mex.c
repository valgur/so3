// SO3 package to perform Wigner transforms
// Copyright (C) 2013 Martin Büttner and Jason McEwen
// See LICENSE.txt for license details


#include "ssht.h"
#include <so3.h>
#include "so3_mex.h"
#include <string.h>
#include "mex.h"


/**
 * Compute forward transform.
 *
 * Usage:
 *   [flmn] = ...
 *     so3_forward_mex(f, L, N, order, storage, n_mode, dl_method, reality);
 *
 * \author Martin Büttner
 * \author Jason McEwen
 */
 void mexFunction( int nlhs, mxArray *plhs[],
                   int nrhs, const mxArray *prhs[])
 {
    int i, iin, iout, a, b, g;

    const mwSize *dims;
    int f_na, f_nb, f_ng, f_is_complex;
    double *f_real, *f_imag;
    complex double *f;
    double *fr;
    int L, N;
    int len;
    char order_str[SO3_STRING_LEN], storage_str[SO3_STRING_LEN], n_mode_str[SO3_STRING_LEN], dl_method_str[SO3_STRING_LEN];
    int nalpha, nbeta, ngamma;

    so3_storage_t storage_method;
    so3_n_mode_t n_mode;
    ssht_dl_method_t dl_method;

    int reality;

    int flmn_size;
    complex double *flmn;
    double *flmn_real, *flmn_imag;

    /* Check number of arguments. */
    if (nrhs != 8)
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:nrhs",
                          "Require eight inputs.");
    if (nlhs != 1)
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidOutput:nlhs",
                          "Require one output.");

    /* Parse reality. */
    iin = 7;
    if( !mxIsLogicalScalar(prhs[iin]) )
        mexErrMsgIdAndTxt("so3_inverse_mex:InvalidInput:reality",
                          "Reality flag must be logical.");
    reality = mxIsLogicalScalarTrue(prhs[iin]);

    /* Parse function samples f. */
    iin = 0;
    if (mxGetNumberOfDimensions(prhs[iin]) != 3)
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:fVector",
                          "Function samples must be contained in a 3d-array.");
    dims = mxGetDimensions(prhs[iin]);
    f_na = dims[2];
    f_nb = dims[1];
    f_ng = dims[0];


    f_is_complex = mxIsComplex(prhs[iin]);
    f_real = mxGetPr(prhs[iin]);
    f_imag = f_is_complex ? mxGetPi(prhs[iin]) : NULL;
    if (reality)
    {
        fr = malloc(f_ng * f_nb * f_na * sizeof(*fr));
        for(g = 0; g < f_ng; ++g)
            for(b = 0; b < f_nb; ++b)
                for(a = 0; a < f_na; ++a)
                    fr[g*f_na*f_nb + b*f_na + a] = f_real[a*f_ng*f_nb + b*f_ng + g];
    }
    else
    {
        f = malloc(f_ng * f_nb * f_na * sizeof(*f));
        for(g = 0; g < f_ng; ++g)
            for(b = 0; b < f_nb; ++b)
                for(a = 0; a < f_na; ++a)
                    f[g*f_na*f_nb + b*f_na + a] = f_real[a*f_ng*f_nb + b*f_ng + g]
                                                  + I * (f_is_complex ? f_imag[a*f_ng*f_nb + b*f_ng + g] : 0.0);
    }

    if (f_is_complex && reality)
        mexWarnMsgTxt("Running real transform but input appears to be complex (ignoring imaginary component).");
    if (!f_is_complex && !reality)
        mexWarnMsgTxt("Running complex transform on real signal (set reality flag to improve performance).");


    /* Parse harmonic band-limit L. */
    iin = 1;
    if (!mxIsDouble(prhs[iin]) ||
        mxIsComplex(prhs[iin]) ||
        mxGetNumberOfElements(prhs[iin])!=1)
    {
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:harmonicBandLimit",
                          "Harmonic band-limit must be integer.");
    }
    L = (int)mxGetScalar(prhs[iin]);
    if (mxGetScalar(prhs[iin]) > (double)L ||
        L <= 0)
    {
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:harmonicBandLimitNonInt",
                          "Harmonic band-limit must be positive integer.");
    }

    /* Parse orientational band-limit N. */
    iin = 2;
    if (!mxIsDouble(prhs[iin]) ||
        mxIsComplex(prhs[iin]) ||
        mxGetNumberOfElements(prhs[iin])!=1)
    {
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:orientationalBandLimit",
                          "Orientational band-limit must be integer.");
    }
    N = (int)mxGetScalar(prhs[iin]);
    if (mxGetScalar(prhs[iin]) > (double)N ||
        N <= 0)
    {
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:orientationalBandLimitNonInt",
                          "Orientational band-limit must be positive integer.");
    }

    /* Parse storage order. */
    iin = 3;
    if( !mxIsChar(prhs[iin]) ) {
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:orderChar",
                          "Storage order must be string.");
    }
    len = (mxGetM(prhs[iin]) * mxGetN(prhs[iin])) + 1;
    if (len >= SO3_STRING_LEN)
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:orderTooLong",
                          "Storage order exceeds string length.");
    mxGetString(prhs[iin], order_str, len);

    /* Parse storage type. */
    iin = 4;
    if( !mxIsChar(prhs[iin]) ) {
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:storageChar",
                          "Storage type must be string.");
    }
    len = (mxGetM(prhs[iin]) * mxGetN(prhs[iin])) + 1;
    if (len >= SO3_STRING_LEN)
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:storageTooLong",
                          "Storage type exceeds string length.");
    mxGetString(prhs[iin], storage_str, len);

    nalpha = so3_sampling_mw_nalpha(L);
    nbeta = so3_sampling_mw_nbeta(L);
    ngamma = so3_sampling_mw_ngamma(N);

    if (f_na != nalpha || f_nb != nbeta || f_ng != ngamma)
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:fSize",
                          "Invalid dimension sizes of function samples.");

    if (strcmp(storage_str, SO3_STORAGE_PADDED) == 0)
    {
        if (reality)
            flmn_size = N*L*L;
        else
            flmn_size = (2*N-1)*L*L;

        if (strcmp(order_str, SO3_ORDER_ZEROFIRST) == 0)
            storage_method = SO3_STORE_ZERO_FIRST_PAD;
        else if (strcmp(order_str, SO3_ORDER_NEGFIRST) == 0)
            storage_method = SO3_STORE_NEG_FIRST_PAD;
        else
            mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:order",
                              "Invalid storage order.");
    }
    else if (strcmp(storage_str, SO3_STORAGE_COMPACT) == 0)
    {
        if (reality)
            flmn_size = N*(6*L*L-(N-1)*(2*N-1))/6;
        else
            flmn_size = (2*N-1)*(3*L*L-N*(N-1))/3;

        if (strcmp(order_str, SO3_ORDER_ZEROFIRST) == 0)
            storage_method = SO3_STORE_ZERO_FIRST_COMPACT;
        else if (strcmp(order_str, SO3_ORDER_NEGFIRST) == 0)
            storage_method = SO3_STORE_NEG_FIRST_COMPACT;
        else
            mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:order",
                              "Invalid storage order.");
    }
    else
        mexErrMsgIdAndTxt("so3_forward_mex:InvalidInput:storage",
                          "Invalid storage type.");
    /* Parse n-mode. */
    iin = 5;
    if( !mxIsChar(prhs[iin]) ) {
        mexErrMsgIdAndTxt("so3_inverse_mex:InvalidInput:nModeChar",
                          "Storage type must be string.");
    }
    len = (mxGetM(prhs[iin]) * mxGetN(prhs[iin])) + 1;
    if (len >= SO3_STRING_LEN)
        mexErrMsgIdAndTxt("so3_inverse_mex:InvalidInput:nModeTooLong",
                          "n-mode exceeds string length.");
    mxGetString(prhs[iin], n_mode_str, len);

    if (strcmp(n_mode_str, SO3_N_MODE_ALL_STR) == 0)
        n_mode = SO3_N_MODE_ALL;
    else if (strcmp(n_mode_str, SO3_N_MODE_EVEN_STR) == 0)
        n_mode = SO3_N_MODE_EVEN;
    else if (strcmp(n_mode_str, SO3_N_MODE_ODD_STR) == 0)
        n_mode = SO3_N_MODE_ODD;
    else if (strcmp(n_mode_str, SO3_N_MODE_MAXIMUM_STR) == 0)
        n_mode = SO3_N_MODE_MAXIMUM;
    else
        mexErrMsgIdAndTxt("so3_inverse_mex:InvalidInput:nMode",
                          "Invalid n-mode.");

    /* Parse Wigner recursion method. */
    iin = 6;
    if( !mxIsChar(prhs[iin]) ) {
        mexErrMsgIdAndTxt("so3_inverse_mex:InvalidInput:dlMethodChar",
                          "Wigner recursion method must be string.");
    }
    len = (mxGetM(prhs[iin]) * mxGetN(prhs[iin])) + 1;
    if (len >= SO3_STRING_LEN)
        mexErrMsgIdAndTxt("so3_inverse_mex:InvalidInput:dlMethodTooLong",
                          "Wigner recursion method exceeds string length.");
    mxGetString(prhs[iin], dl_method_str, len);

    if (strcmp(dl_method_str, SSHT_RECURSION_RISBO) == 0)
        dl_method = SSHT_DL_RISBO;
    else if (strcmp(dl_method_str, SSHT_RECURSION_TRAPANI) == 0)
        dl_method = SSHT_DL_TRAPANI;
    else
        mexErrMsgIdAndTxt("so3_inverse_mex:InvalidInput:storage",
                          "Invalid Wigner recursion method.");

    /* Compute inverse transform. */

    flmn = calloc(flmn_size, sizeof(*flmn));

    if (reality)
    {
        so3_core_mw_forward_via_ssht_real(
            flmn, fr,
            0, L, N,
            storage_method,
            n_mode,
            dl_method,
            0
        );
    }
    else
    {
        so3_core_mw_forward_via_ssht(
            flmn, f,
            0, L, N,
            storage_method,
            n_mode,
            dl_method,
            0
        );
    }

    // Copy result to output argument

    iout = 0;
    plhs[iout] = mxCreateDoubleMatrix(flmn_size, 1, mxCOMPLEX);
    flmn_real = mxGetPr(plhs[iout]);
    flmn_imag = mxGetPi(plhs[iout]);
    for(i = 0; i < flmn_size; ++i)
    {
        flmn_real[i] = creal(flmn[i]);
        flmn_imag[i] = cimag(flmn[i]);
    }

    /* Free memory. */
    if (reality)
        free(fr);
    else
        free(f);
    free(flmn);
}
