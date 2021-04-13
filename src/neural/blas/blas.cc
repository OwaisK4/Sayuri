#include "neural/blas/blas.h"



void Blas::ConvolutionSgemm(const int M, const int N, const int K,
                            const float alpha, 
                            const float *A, const int lda,
                            const float *B, const int ldb,
                            const float beta,
                            float *C, const int ldc) {
#ifndef USE_BLAS
    Sgemm<false, false>::apply(M, N, K,
                               alpha,
                               A, lda,
                               B, ldb,
                               beta,
                               C, ldc);
#else
#ifdef USE_OPENBLAS
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                M, N, K,
                alpha,
                A, lda,
                B, ldb,
                beta,
                C, ldc);

#endif
#ifdef USE_EIGEN
    (void) alpha;
    (void) beta;
    (void) lda;
    (void) ldb;
    (void) ldc;
    auto C_mat = EigenMatrixMap<float>(C, N, M);
    C_mat.noalias() = 
        ConstEigenMatrixMap<float>(B, N, K) *
        ConstEigenMatrixMap<float>(A, K, M);
#endif
#endif
}


void Blas::WinogradSgemm(const int set_U, const int set_V, const int set_M,
                         const int M, const int N, const int K,
                         const float alpha,
                         const float *A, const int lda,
                         const float *B, const int ldb,
                         const float beta,
                         float *C, const int ldc) {

#ifndef USE_BLAS
    Sgemm<true, false>::apply(M, N, K,
                              alpha,
                              A + set_U, lda,
                              B + set_V, ldb,
                              beta,
                              C + set_M, ldc);

#else
#ifdef USE_OPENBLAS
    cblas_sgemm(CblasRowMajor, CblasTrans, CblasNoTrans,
                M, N, K,
                alpha,
                A + set_U, lda,
                B + set_V, ldb,
                beta,
                C + set_M, ldc);

#endif
#ifdef USE_EIGEN
    (void) alpha;
    (void) beta;
    (void) lda;
    (void) ldb;
    (void) ldc;
    auto C_mat = EigenMatrixMap<float>(C + set_M, N, M);
    C_mat.noalias() =
        ConstEigenMatrixMap<float>(B + set_V, N, K) *
        ConstEigenMatrixMap<float>(A + set_U, M, K).transpose();

#endif
#endif
}

void Blas::DenseSgemm(const int input_size,
                      const int output_size,
                      const int batch_size,
                      const float *inputs,
                      const float *kernel,
                      float *outputs) {
#ifndef USE_EIGEN
   static constexpr float kAlpha = 1.0f;
   static constexpr float kBeta = 0.f;
#endif

#ifndef USE_BLAS
    Sgemm<false, true>::apply(batch_size, output_size, input_size,
                              kAlpha,
                              inputs, input_size,
                              kernel, input_size,
                              kBeta,
                              outputs, output_size);
#else
#ifdef USE_OPENBLAS
    if (batch_size == 1) {
        cblas_sgemv(CblasRowMajor, CblasNoTrans,
                    output_size, input_size, 1.0f, kernel,
                    input_size, inputs, 1, 0.0f, outputs, 1);
    } else {
        cblas_sgemm(CblasColMajor, CblasTrans, CblasNoTrans,
                    output_size, batch_size,  input_size,         
                    kAlpha,              
                    kernel, input_size,
                    inputs, input_size,
                    kBeta, 
                    outputs, output_size); 
  }

#endif
#ifdef USE_EIGEN
    if (batch_size == 1) {
        EigenVectorMap<float> y(outputs, output_size);
        y.noalias() =
            ConstEigenMatrixMap<float>(kernel, input_size, output_size).transpose() *
            ConstEigenVectorMap<float>(inputs, input_size);
    } else {
        auto C_mat = EigenMatrixMap<float>(outputs, output_size, batch_size);
        C_mat.noalias() =
            ConstEigenMatrixMap<float>(kernel, input_size, output_size)
                .transpose() *
                ConstEigenMatrixMap<float>(inputs, input_size, batch_size);

  }
#endif
#endif
}
