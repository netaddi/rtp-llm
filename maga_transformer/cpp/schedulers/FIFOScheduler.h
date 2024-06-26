#pragma once

#include <queue>
#include "maga_transformer/cpp/cache/CacheManager.h"
#include "maga_transformer/cpp/dataclass/MagaInitParameter.h"
#include "maga_transformer/cpp/dataclass/Query.h"
#include "maga_transformer/cpp/schedulers/SchedulerBase.h"

namespace rtp_llm {

class FIFOScheduler: public SchedulerBase {
public:
    explicit FIFOScheduler(const MagaInitParams& config, const std::shared_ptr<CacheManager>& cache_manager);

    ~FIFOScheduler();

    absl::Status enqueue(const GenerateStreamPtr& stream) override;
    absl::StatusOr<std::list<GenerateStreamPtr>> schedule() override;

    absl::Status stop() override;

public:
    // for test
    int waitingStreamsSize();
    int runningStreamsSize();

private:
    void evictDoneStreams(std::list<GenerateStreamPtr>& streams) const;
    bool evaluateNewStream(const std::list<GenerateStreamPtr>& streams, const GenerateStreamPtr& new_stream) const;
    void evaluateRunningNext();
    int  runningNextBlockNum() const;
    bool evaluateRunningMemory(int total_seq_size) const;
    bool evaluateKVCacheMemory(int block_num) const;
    std::list<GenerateStreamPtr> scheduleNew();

private:
    std::list<GenerateStreamPtr>        waiting_streams_;
    std::list<GenerateStreamPtr>        running_streams_;
    std::shared_ptr<CacheManager>       cache_manager_;
    int                                 max_seq_len_        = 0;
    int                                 reserve_block_num_  = 0;
    bool                                enable_fallback     = false;
    std::atomic<bool>                   stop_               = false;
    std::mutex                          lock_;
    std::condition_variable             cond_;

    // TODO @wangyin support different beams run togather
};

}  // namespace rtp_llm
