/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/operators/math/math_function.h"
#include "paddle/framework/eigen.h"
#include "paddle/memory/memcpy.h"

namespace paddle {
namespace operators {
namespace math {

template <>
void gemm<platform::CPUPlace, float>(const platform::DeviceContext& context,
                                     const CBLAS_TRANSPOSE transA,
                                     const CBLAS_TRANSPOSE transB, const int M,
                                     const int N, const int K,
                                     const float alpha, const float* A,
                                     const float* B, const float beta,
                                     float* C) {
  int lda = (transA == CblasNoTrans) ? K : M;
  int ldb = (transB == CblasNoTrans) ? N : K;
  int ldc = N;
  cblas_sgemm(CblasRowMajor, transA, transB, M, N, K, alpha, A, lda, B, ldb,
              beta, C, ldc);
}

template <>
void gemm<platform::CPUPlace, double>(const platform::DeviceContext& context,
                                      const CBLAS_TRANSPOSE transA,
                                      const CBLAS_TRANSPOSE transB, const int M,
                                      const int N, const int K,
                                      const double alpha, const double* A,
                                      const double* B, const double beta,
                                      double* C) {
  int lda = (transA == CblasNoTrans) ? K : M;
  int ldb = (transB == CblasNoTrans) ? N : K;
  int ldc = N;
  cblas_dgemm(CblasRowMajor, transA, transB, M, N, K, alpha, A, lda, B, ldb,
              beta, C, ldc);
}

template <>
void gemm<platform::CPUPlace, float>(const platform::DeviceContext& context,
                                     const bool transA, const bool transB,
                                     const int M, const int N, const int K,
                                     const float alpha, const float* A,
                                     const int lda, const float* B,
                                     const int ldb, const float beta, float* C,
                                     const int ldc) {
  cblas_sgemm(CblasRowMajor, transA == false ? CblasNoTrans : CblasTrans,
              transB == false ? CblasNoTrans : CblasTrans, M, N, K, alpha, A,
              lda, B, ldb, beta, C, ldc);
}

template <>
void gemm<platform::CPUPlace, double>(const platform::DeviceContext& context,
                                      const bool transA, const bool transB,
                                      const int M, const int N, const int K,
                                      const double alpha, const double* A,
                                      const int lda, const double* B,
                                      const int ldb, const double beta,
                                      double* C, const int ldc) {
  cblas_dgemm(CblasRowMajor, transA == false ? CblasNoTrans : CblasTrans,
              transB == false ? CblasNoTrans : CblasTrans, M, N, K, alpha, A,
              lda, B, ldb, beta, C, ldc);
}

template <>
void matmul<platform::CPUPlace, float>(
    const platform::DeviceContext& context, const framework::Tensor& matrix_a,
    bool trans_a, const framework::Tensor& matrix_b, bool trans_b, float alpha,
    framework::Tensor* matrix_out, float beta) {
  auto dim_a = matrix_a.dims();
  auto dim_b = matrix_b.dims();
  auto dim_out = matrix_out->dims();
  PADDLE_ENFORCE(dim_a.size() == 2 && dim_b.size() == 2 && dim_out.size() == 2,
                 "The input and output of matmul be matrix");

  PADDLE_ENFORCE(platform::is_cpu_place(matrix_a.place()) &&
                     platform::is_cpu_place(matrix_b.place()) &&
                     platform::is_cpu_place(matrix_out->place()),
                 "Matrix must all be in CPUPlace");

  int M = dim_out[0];
  int N = dim_out[1];
  int K = (trans_a == false) ? dim_a[1] : dim_a[0];

  CBLAS_TRANSPOSE transA = (trans_a == false) ? CblasNoTrans : CblasTrans;
  CBLAS_TRANSPOSE transB = (trans_b == false) ? CblasNoTrans : CblasTrans;

  gemm<platform::CPUPlace, float>(
      context, transA, transB, M, N, K, alpha, matrix_a.data<float>(),
      matrix_b.data<float>(), beta, matrix_out->data<float>());
}

template <>
void matmul<platform::CPUPlace, double>(
    const platform::DeviceContext& context, const framework::Tensor& matrix_a,
    bool trans_a, const framework::Tensor& matrix_b, bool trans_b, double alpha,
    framework::Tensor* matrix_out, double beta) {
  auto dim_a = matrix_a.dims();
  auto dim_b = matrix_b.dims();
  auto dim_out = matrix_out->dims();
  PADDLE_ENFORCE(dim_a.size() == 2 && dim_b.size() == 2 && dim_out.size() == 2,
                 "The input and output of matmul be matrix");

  PADDLE_ENFORCE(platform::is_cpu_place(matrix_a.place()) &&
                     platform::is_cpu_place(matrix_b.place()) &&
                     platform::is_cpu_place(matrix_out->place()),
                 "Matrix must all be in CPUPlace");

  int M = dim_out[0];
  int N = dim_out[1];
  int K = (trans_a == false) ? dim_a[1] : dim_a[0];

  CBLAS_TRANSPOSE transA = (trans_a == false) ? CblasNoTrans : CblasTrans;
  CBLAS_TRANSPOSE transB = (trans_b == false) ? CblasNoTrans : CblasTrans;

  gemm<platform::CPUPlace, double>(
      context, transA, transB, M, N, K, alpha, matrix_a.data<double>(),
      matrix_b.data<double>(), beta, matrix_out->data<double>());
}

template struct SetConstant<platform::CPUPlace, float>;

namespace detail {
size_t FindPos(const std::vector<int64_t>& rows, int64_t value) {
  for (size_t i = 0; i < rows.size(); i++) {
    if (rows[i] == value) {
      return i;
    }
  }
  return 0;
}
}  // namespace detail

template <typename T>
struct SelectedRowsAdd<platform::CPUPlace, T> {
  void operator()(const platform::DeviceContext& context,
                  const framework::SelectedRows& input1,
                  const framework::SelectedRows& input2,
                  framework::SelectedRows* output) {
    auto in1_height = input1.height();
    PADDLE_ENFORCE_EQ(in1_height, input2.height());
    output->set_height(in1_height);

    auto& in1_rows = input1.rows();
    auto& in2_rows = input2.rows();
    std::vector<int64_t> out_rows;
    out_rows.reserve(in1_rows.size() + in2_rows.size());

    // concat rows
    out_rows.insert(out_rows.end(), in1_rows.begin(), in1_rows.end());
    out_rows.insert(out_rows.end(), in2_rows.begin(), in2_rows.end());
    output->set_rows(out_rows);

    auto* out_value = output->mutable_value();
    auto& in1_value = input1.value();
    auto& in2_value = input2.value();

    auto in1_row_numel = in1_value.numel() / in1_rows.size();
    PADDLE_ENFORCE_EQ(in1_row_numel, in2_value.numel() / in2_rows.size());
    PADDLE_ENFORCE_EQ(in1_row_numel, out_value->numel() / out_rows.size());

    auto* out_data = out_value->data<T>();

    auto* in1_data = in1_value.data<T>();
    memory::Copy(platform::CPUPlace(), out_data, platform::CPUPlace(), in1_data,
                 in1_value.numel() * sizeof(T));

    auto* in2_data = in2_value.data<T>();
    memory::Copy(platform::CPUPlace(), out_data + in1_value.numel(),
                 platform::CPUPlace(), in2_data, in2_value.numel() * sizeof(T));
  }
};

template struct SelectedRowsAdd<platform::CPUPlace, float>;

template <typename T>
struct SelectedRowsAddTensor<platform::CPUPlace, T> {
  void operator()(const platform::DeviceContext& context,
                  const framework::SelectedRows& input1,
                  const framework::Tensor& input2, framework::Tensor* output) {
    auto in1_height = input1.height();
    auto in2_dims = input2.dims();
    auto out_dims = output->dims();
    PADDLE_ENFORCE_EQ(in1_height, in2_dims[0]);
    PADDLE_ENFORCE_EQ(in1_height, out_dims[0]);

    auto& in1_value = input1.value();
    auto& in1_rows = input1.rows();

    int64_t in1_row_numel = in1_value.numel() / in1_rows.size();
    PADDLE_ENFORCE_EQ(in1_row_numel, input2.numel() / in1_height);
    PADDLE_ENFORCE_EQ(in1_row_numel, output->numel() / in1_height);

    SetConstant<platform::CPUPlace, T> functor;
    functor(context, output, 0.0);

    auto* in1_data = in1_value.data<T>();
    auto* out_data = output->data<T>();

    for (size_t i = 0; i < in1_rows.size(); i++) {
      for (int64_t j = 0; j < in1_row_numel; j++) {
        out_data[in1_rows[i] * in1_row_numel + j] +=
            in1_data[i * in1_row_numel + j];
      }
    }

    auto out_eigen = framework::EigenVector<T>::Flatten(*output);
    auto in2_eigen = framework::EigenVector<T>::Flatten(input2);
    out_eigen.device(*context.GetEigenDevice<platform::CPUPlace>()) =
        out_eigen + in2_eigen;
  }
};

template struct SelectedRowsAddTensor<platform::CPUPlace, float>;

}  // namespace math
}  // namespace operators
}  // namespace paddle
