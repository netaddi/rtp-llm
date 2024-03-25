#pragma once
#include <assert.h>
#include <cstdint>
#include <mutex>
#include <string>
#include <optional>
#include <queue>
#include <condition_variable>
#include "src/fastertransformer/core/Buffer.h"
#include "maga_transformer/cpp/utils/TimeUtility.h"
#include "maga_transformer/cpp/dataclass/Query.h"
#include "maga_transformer/cpp/dataclass/StreamCacheResource.h"
#include "maga_transformer/cpp/cache/CacheManager.h"
#include "src/fastertransformer/devices/utils/BufferUtils.h"
#include "maga_transformer/cpp/utils/SynchronizedQueue.h"
#include "absl/status/statusor.h"

namespace ft = fastertransformer;

namespace rtp_llm {

class GenerateStream {
public:
    GenerateStream(const std::shared_ptr<GenerateInput>& query, int max_seq_len = 2048);
    ~GenerateStream() {
        generate_outputs_.wakeup();
    }

public:
    // Exported to python world.
    void                                cancel();

    absl::StatusOr<GenerateOutput>     nextOutput();
    // Only used in C++ world.
    bool isContextStream() const {return seqLength() == inputLength();};
    int tileNum() const {return std::max(numBeams(), numReturnSequences());}
    std::vector<int> inputTokens() const;
    std::vector<int> currentExecuteTokens() const;

    int tryReleaseKVBlock(int nums) {
        return stream_cache_resource_.tryReleaseKVBlock(nums);
    }
    bool initKVBlock() {
        return stream_cache_resource_.initKVBlock();
    }
    bool incrKVBlock() {
        return stream_cache_resource_.incrKVBlock();
    }
    int nextNeedBlockNums() const {
        return stream_cache_resource_.nextNeedBlockNums();
    }

    void setNeedReleaseResource(bool need_release_resource) {
        need_release_resource_ = need_release_resource;
    }

    // TODO(xinfei.sxf) lora resource?
    void releaseResource() {
        if (!need_release_resource_) {
            stream_cache_resource_.releaseResource();
        }
    }

    int64_t streamId() const {
        return generate_input_->request_id;
    }

    int inputLength() const {
        return generate_input_->inputLength();
    }

    int contextLength() const {
        return generate_input_->inputLength();
    }

    int initalKVCacheCount() const {
        return stream_cache_resource_.initalKVCacheCount();
    }

    size_t maxBlockSize() const {
        return stream_cache_resource_.maxBlockSize();
    }

    const ft::BufferPtr& completeTokenIds() {
        return complete_token_ids_;
    }

    // TODO(xinfei.sxf) batch?
    std::vector<int> completeTokenIdsVec() {
        return fastertransformer::buffer2vector<int>(complete_token_ids_, seq_length_);
    }

    int currentExecuteTokenSize() {
        return currentExecuteTokens().size();
    }

    int batchSize() const;

    int prefixLength() const {
        return generate_input_->prefix_length;
    }

    std::shared_ptr<GenerateConfig>& generateConfig() {
        return generate_input_->generate_config;
    }

    int numBeams() const {
        return generate_input_->generate_config->num_beams;
    }

    int numReturnSequences() const {
        return generate_input_->generate_config->num_return_sequences;
    }

    std::optional<ft::BufferPtr>& inputEmbeddings() {
        return generate_input_->input_embeddings;
    }

    std::optional<int> lora_id() {
        return generate_input_->lora_id;
    }

    int reuseLength() const {
        return reuse_length_;
    }

    int seqLength() const {
        return seq_length_;
    }

    // for test
    void setSeqLength(int seq_length) {
        seq_length_ = seq_length;
    }

    void setCacheManager(std::shared_ptr<CacheManager> cache_manager) {
        stream_cache_resource_.setCacheManager(cache_manager);
    }

    void setReuseCache(bool reuse_cache) {
        stream_cache_resource_.setReuseCache(reuse_cache);
    }

    void setStop(const std::string& err_msg) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        generate_status_.status = GenerateState::STOPPED;
        generate_status_.error_info = err_msg;
    }

    void stopAndRelease(const std::string& err_msg) {
        setStop(err_msg);
        releaseResource();
    }

    void setFinished() {
        std::lock_guard<std::mutex> lock(output_mutex_);
        generate_status_.status = GenerateState::FINISHED;
    }

    bool setRunning() {
        std::lock_guard<std::mutex> lock(output_mutex_);
        if (stoppedWithoutLock()) {
            return false;
        }
        // reportWaitTime();
        generate_status_.status = GenerateState::RUNNING;
        return true;
    }

    void setFinishedWithoutLock() {
        generate_status_.status = GenerateState::FINISHED;
    }

    bool stoppedWithoutLock() {
        return generate_status_.status == GenerateState::STOPPED;
    }

    bool stopped() {
        std::lock_guard<std::mutex> lock(output_mutex_);
        return generate_status_.status == GenerateState::STOPPED;
    }

    std::string stopReason() {
        std::lock_guard<std::mutex> lock(output_mutex_);
        return generate_status_.error_info;
    }

    bool finished() {
        std::lock_guard<std::mutex> lock(output_mutex_);
        return generate_status_.status == GenerateState::FINISHED;
    }

    void check_timeout() {
        auto running_time = TimeUtility::currentTimeInMilliSeconds() - begin_time_;
        auto timeout_ms = generate_input_->generate_config->timeout_ms;
        if (timeout_ms > 0 && timeout_ms < running_time) {
            stopAndRelease("query has been running " + std::to_string(running_time) + " ms, it's timeout");
        }
    }

    void reportWaitTime() {
        // kmonitor.report(GaugeMetrics.ASYNC_WAIT_WAIT_TIME_METRIC, TimeUtility::currentTimeInMilliSeconds() - begin_time_)
    }

    void reportFirstTokenRt() {
        // kmonitor.report(GaugeMetrics.FT_FIRST_TOKEN_RT_METRIC, TimeUtility::currentTimeInMilliSeconds() - begin_time_);
    }

    // void setLoss(th::Tensor& loss) {
    //     output_.loss = loss
    // }

    void setReuseLength(int reuse_length) {
        reuse_length_ = reuse_length;
    }

    void setKVCache(const BatchKVCacheBlockAddr &kv_cache_block_addr, int reuse_length) {
        stream_cache_resource_.setKVCache(kv_cache_block_addr);
        reuse_length_ = reuse_length;
    }

    // for test
    StreamCacheResource& streamCacheResource() {
        return stream_cache_resource_;
    }

    const BatchKVCacheBlockAddr& kvCache() const {
        return stream_cache_resource_.kvCache();
    }

    bool needFinish() {
        return seq_length_ >= std::min(max_seq_len_, generate_input_->generate_config->max_new_tokens + generate_input_->inputLength());
    }

    void update(ft::BufferPtr& new_tokens,
                int num_new_tokens,
                bool finished,
                std::optional<ft::BufferPtr>& hidden_states,
                std::optional<ft::BufferPtr>& logits,
                std::optional<ft::BufferPtr>& cum_log_probs,
                std::optional<ft::BufferPtr>& loss);

    std::string debugString() const {
        std::stringstream debug_string;
        debug_string << "GenerateStream {"
                     << "generate_input:" << generate_input_->debugString()
                     << ", max_seq_len:" << max_seq_len_
                     << ", seq_length:" << seq_length_
                     << ", reuse_length:" << reuse_length_
                     << ", batch_size:" << batch_size_
                     << "}";
        return debug_string.str();
    }

private:
    std::shared_ptr<GenerateInput>      generate_input_;
    std::shared_ptr<GenerateOutput>     generate_output_;
    SynchronizedQueue<GenerateOutput>   generate_outputs_;
    GenerateStatus                      generate_status_;
    std::vector<GenerateStatus>         sub_generate_status_;

    int                                 max_seq_len_;
    int                                 seq_length_;
    int                                 reuse_length_ = 0;
    ft::BufferPtr                       complete_token_ids_;
    int64_t                             begin_time_;
    size_t                              batch_size_ = 1;
    bool                                done_;
    bool                                cancelled_;
    std::mutex                          output_mutex_;
    std::condition_variable             update_cv_;
    bool                                released_;
    bool                                need_release_resource_;
    StreamCacheResource                 stream_cache_resource_;

    friend class StreamCacheResource;
};

typedef std::shared_ptr<GenerateStream> GenerateStreamPtr;
} // namespace rtp_llm