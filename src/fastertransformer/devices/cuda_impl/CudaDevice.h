#pragma once

#include "src/fastertransformer/devices/DeviceBase.h"
#include "src/fastertransformer/cuda/cuda_utils.h"
#include "src/fastertransformer/cuda/cublas/cublas.h"

#include <nvml.h>

namespace fastertransformer {

// TODO(wangyin.yx): add api for query device status.
class CudaDevice : public DeviceBase {
public:
    CudaDevice();
    ~CudaDevice();

public:
    DeviceProperties getDeviceProperties() override;
    DeviceStatus getDeviceStatus() override;
    IAllocator* getAllocator() override { return allocator_.get(); }
    IAllocator* getHostAllocator() override { return host_allocator_.get(); }
    void syncAndCheck() override;

public:
    int getDeviceId() const { return device_id_; }
    cudaStream_t getStream() {return stream_;}

public:
    void copy(const CopyParams& params);
    TransposeOutput transpose(const TransposeParams& params);
    ConvertOutput convert(const ConvertParams& params);
    LayernormOutput layernorm(const LayernormParams& params);
    BufferPtr gemm(const GemmParams& params);
    GroupedGemmOutput groupedGemm(const GroupedGemmParams& params);
    BufferPtr embeddingLookup(const EmbeddingLookupParams& params);
    void activation(const ActivationParams& params);
    BufferPtr softmax(const SoftmaxParams& params);
    AttentionModuleOutput contextAttention(const AttentionModuleParams& params);
    AttentionModuleOutput decoderSelfAttention(const AttentionModuleParams& params);
    void sampleGreedy(const GreedyParams& params);
    void sampleBeamSearch(const BeamSearchParams& params);
    void broadcast(const BroadcastParams& params);
    void allReduceSum(const AllReduceParams& params);

// TODO: @xinglai delelte this
public:
    cudaStream_t stream() const {return stream_;}
    cublasMMWrapper* cublasMMWrapperPtr() const {return cublas_mm_wrapper_.get();}

private:
    std::unique_ptr<IAllocator> allocator_;
    std::unique_ptr<IAllocator> host_allocator_;
    int device_id_;
    cudaStream_t stream_;
    cublasHandle_t cublas_handle_;
    cublasLtHandle_t cublaslt_handle_;
    cudaDeviceProp device_prop_;

    std::mutex cublas_wrapper_mutex_;
    std::unique_ptr<cublasAlgoMap> cublas_algo_map_;
    std::unique_ptr<cublasMMWrapper> cublas_mm_wrapper_;

    nvmlDevice_t nvml_device_;
};

} // namespace fastertransformer

