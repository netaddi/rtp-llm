#define private public

#include "src/fastertransformer/devices/testing/TestBase.h"
#include "src/fastertransformer/devices/cuda_impl/CudaDevice.h"
#include "maga_transformer/cpp/models/GptModel.h"
#include "maga_transformer/cpp/test/ModelTestUtil.h"

using namespace std;
using namespace rtp_llm;

class GptModelTest: public DeviceTestBase {
};

TEST_F(GptModelTest, testSimple) {
    const auto path = test_data_path_ + "../../test/model_test/fake_test/testdata/qwen_0.5b";
    auto weights = loadWeightsFromDir(path);
    assert(weights->lm_head->kernel);
    assert(weights->embedding);
    assert(weights->layers.size() == 24);

    GptModelDescription description;
    GptModel model({device_, *weights, description});

    const auto combo_tokens = createBuffer<int32_t>({3}, {13048, 11, 220});
    const auto input_lengths = createBuffer<int32_t>({1}, {3});
    const auto sequence_lengths = createBuffer<int32_t>({0}, {});

    // TODO: fill these blokcs when BlockManager is done.
    const auto kv_cache_blocks = createBuffer<int64_t>({1, 1}, {0});

    GptModelInputs inputs = {
        *combo_tokens, *input_lengths, *sequence_lengths,
        nullopt, nullopt, *kv_cache_blocks
    };

    try {
        auto outputs = model.forward(inputs);
    } catch (const OpException& e) {
        cout << e.what() << endl;
    } catch (const exception& e) {
        cout << e.what() << endl;
    }
}

TEST_F(GptModelTest, testAttentionInputs) {
    GptModelDescription description;
    Weights weights;
    GptModel model({device_, weights, description});

    const auto kv_cache_blocks = createBuffer<int64_t>({1, 1}, {0}, AllocationType::HOST);
    const auto input_lengths = createBuffer<int32_t>({4}, {3, 5, 2, 7}, AllocationType::HOST);
    auto sequence_lengths = createBuffer<int32_t>({0}, {}, AllocationType::HOST);
    auto combo_tokens = createBuffer<int32_t>({17}, std::vector<int32_t>(17, 0), AllocationType::HOST);

    {
        auto attention_inputs = model.prepareAttentionInputs({
            *combo_tokens, *input_lengths, *sequence_lengths,
            nullopt, nullopt, *kv_cache_blocks
        });
        printBuffer<int32_t>(*attention_inputs.cu_seqlens);
        printBuffer<int32_t>(*attention_inputs.padding_offset);
        assertBufferValueEqual<int32_t>(*attention_inputs.cu_seqlens, {0, 3, 8, 10, 17});
        assertBufferValueEqual<int32_t>(*attention_inputs.padding_offset,
            {0, 0, 0, 4, 4, 4, 4, 4, 6, 6, 11, 11, 11, 11, 11, 11, 11});
    }

    sequence_lengths = createBuffer<int32_t>({3}, {4, 19, 23}, AllocationType::HOST);
    combo_tokens = createBuffer<int32_t>({7}, std::vector<int32_t>(7, 0), AllocationType::HOST);
    {
        auto attention_inputs = model.prepareAttentionInputs({
            *combo_tokens, *input_lengths, *sequence_lengths,
            nullopt, nullopt, *kv_cache_blocks
        });
        printBuffer<int32_t>(*attention_inputs.cu_seqlens);
        printBuffer<int32_t>(*attention_inputs.padding_offset);
        assertBufferValueEqual<int32_t>(*attention_inputs.cu_seqlens, {0, 7});
        assertBufferValueEqual<int32_t>(*attention_inputs.padding_offset,
            {0, 0, 0, 0, 0, 0, 0});
    }

    sequence_lengths = createBuffer<int32_t>({2}, {4, 6}, AllocationType::HOST);
    combo_tokens = createBuffer<int32_t>({9}, std::vector<int32_t>(9, 0), AllocationType::HOST);
    {
        auto attention_inputs = model.prepareAttentionInputs({
            *combo_tokens, *input_lengths, *sequence_lengths,
            nullopt, nullopt, *kv_cache_blocks
        });
        printBuffer<int32_t>(*attention_inputs.cu_seqlens);
        printBuffer<int32_t>(*attention_inputs.padding_offset);
        assertBufferValueEqual<int32_t>(*attention_inputs.cu_seqlens, {0, 2, 9});
        assertBufferValueEqual<int32_t>(*attention_inputs.padding_offset,
            {0, 0, 5, 5, 5, 5, 5, 5, 5});
    }
}
