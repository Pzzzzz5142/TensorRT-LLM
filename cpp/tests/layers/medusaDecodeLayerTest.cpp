/*
 * Copyright (c) 2022-2024, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tests/layers/medusaDecodeLayerTest.h"
#include "tensorrt_llm/kernels/decodingCommon.h"
#include "tensorrt_llm/runtime/runtimeKernels.h"
#include <algorithm>

namespace tensorrt_llm::tests::layers
{

// TODO(nkorobov):
// Add tests for
// - finished states
// - finished sum
// - max length
// - repeat n grams
// - padded vocab
// - beam search

using namespace tensorrt_llm::runtime;
using namespace tensorrt_llm::layers;
using namespace tensorrt_llm::common;

namespace tk = tensorrt_llm::kernels;
namespace tcc = tensorrt_llm::common::conversion;
namespace trk = tensorrt_llm::runtime::kernels;

constexpr float EPSILON = 1e-20f;

template <typename T>
void MedusaDecodingLayerTest<T>::SetUp()
{
    mStream = std::make_shared<tensorrt_llm::runtime::CudaStream>();
    mBufferManager = std::make_shared<tensorrt_llm::runtime::BufferManager>(mStream);

    mAllocator = std::make_shared<tensorrt_llm::common::CudaAllocator>(*mBufferManager);
}

template <typename T>
void MedusaDecodingLayerTest<T>::allocateBuffers()
{
    mMedusaDecodingLayer = std::make_shared<tensorrt_llm::layers::MedusaDecodingLayer<T>>(
        mMaxBatchSize, mVocabSize, mVocabSizePadded, mMaxTokensPerStep, mMaxNumHeads, mStream->get(), mAllocator);

    auto const dataType = TRTDataType<T>::value;

    // clang-format off

    // prob = (0.0, 0.0, 0.0, 0.0, 0.4, 0.3, 0.2, 0.1, 0.0)
    std::vector<T> targetLogitsInit = {
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 0
            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 1
            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 2
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 3
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 4
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 5
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 6
            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 7
            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 8
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 9
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 10
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX  // token 11
    };
    // Sampled tokens with K=1
    // [4, 0, 2, 1, 3, 4, 3, 0, 2, 3, 4, 1]

    std::vector<T> medusaLogitsInit = {
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 0    head=0  ids: [4, 5, 6, 7]
            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 1    head=0  ids: [0, 1, 2, 3]
            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 2    head=0  ids: [2, 3, 4, 5]
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 3    head=0  ids: [1, 2, 3, 4]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 4    head=0  ids: [3, 4, 5, 6]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 5    head=0  ids: [4, 5, 6, 7]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 6    head=0  ids: [3, 4, 5, 6]
            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 7    head=0  ids: [0, 1, 2, 3]
            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 8    head=0  ids: [2, 3, 4, 5]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 9    head=0  ids: [3, 4, 5, 6]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 10   head=0  ids: [4, 5, 6, 7]
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 11   head=0  ids: [1, 2, 3, 4]

            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 0    head=1  ids: [2, 3, 4, 5]
            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 1    head=1  ids: [0, 1, 2, 3]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 2    head=1  ids: [4, 5, 6, 7]
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 3    head=1  ids: [1, 2, 3, 4]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 4    head=1  ids: [4, 5, 6, 7]
            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 5    head=1  ids: [2, 3, 4, 5]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 6    head=1  ids: [3, 4, 5, 6]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 7    head=1  ids: [3, 4, 5, 6]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 8    head=1  ids: [3, 4, 5, 6]
            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 9    head=1  ids: [0, 1, 2, 3]
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 10   head=1  ids: [1, 2, 3, 4]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 11   head=1  ids: [4, 5, 6, 7]

            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 0    head=2  ids: [0, 1, 2, 3]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 1    head=2  ids: [4, 5, 6, 7]
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 2    head=2  ids: [1, 2, 3, 4]
            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 3    head=2  ids: [2, 3, 4, 5]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 4    head=2  ids: [4, 5, 6, 7]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 5    head=2  ids: [3, 4, 5, 6]
            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 6    head=2  ids: [0, 1, 2, 3]
            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 7    head=2  ids: [2, 3, 4, 5]
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 8    head=2  ids: [1, 2, 3, 4]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 9    head=2  ids: [3, 4, 5, 6]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 10   head=2  ids: [4, 5, 6, 7]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 11   head=2  ids: [3, 4, 5, 6]

            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 0    head=3  ids: [4, 5, 6, 7]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 1    head=3  ids: [4, 5, 6, 7]
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 2    head=3  ids: [1, 2, 3, 4]
            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 3    head=3  ids: [0, 1, 2, 3]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, // token 4    head=3  ids: [4, 5, 6, 7]
            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 5    head=3  ids: [2, 3, 4, 5]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 6    head=3  ids: [3, 4, 5, 6]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 7    head=3  ids: [3, 4, 5, 6]
            -FLT_MAX, -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, // token 8    head=3  ids: [3, 4, 5, 6]
            -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 9    head=3  ids: [1, 2, 3, 4]
            -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX, // token 10   head=3  ids: [0, 1, 2, 3]
            -FLT_MAX, -FLT_MAX, -0.9163, -1.2040, -1.6094, -2.3026, -FLT_MAX, -FLT_MAX, -FLT_MAX  // token 11   head=3  ids: [2, 3, 4, 5]
    };

    // clang-format on

    auto const targetLogitsHost
        = ITensor::wrap(targetLogitsInit.data(), dataType, ITensor::makeShape({mMaxTokensPerStep, mVocabSizePadded}));

    TensorPtr medusaLogitsHost = ITensor::wrap(
        medusaLogitsInit.data(), dataType, ITensor::makeShape({mMaxNumHeads, mMaxTokensPerStep, mVocabSizePadded}));

    mTargetLogitsDevice
        = mBufferManager->gpu(ITensor::makeShape({mBatchSize, mMaxTokensPerStep, mVocabSizePadded}), dataType);

    mFinishedDevice = mBufferManager->gpu(
        ITensor::makeShape({mMaxBatchSize}), TRTDataType<tk::FinishedState::UnderlyingType>::value);

    mOutputIdsDevice = mBufferManager->gpu(ITensor::makeShape({mMaxBatchSize, mMaxSeqLen}), nvinfer1::DataType::kINT32);

    mBatchSlots = BufferManager::pinned(ITensor::makeShape({mBatchSize}), nvinfer1::DataType::kINT32);

    mEndIdsDevice = mBufferManager->gpu(ITensor::makeShape({mMaxBatchSize}), nvinfer1::DataType::kINT32);

    mPathsDevice = mBufferManager->gpu(
        ITensor::makeShape({mMaxBatchSize, mMaxTokensPerStep, mMaxNumHeads + 1}), nvinfer1::DataType::kINT32);

    mSeqLengthsDevice = mBufferManager->gpu(ITensor::makeShape({mMaxBatchSize}), nvinfer1::DataType::kINT32);

    mAcceptedLengths = mBufferManager->gpu(ITensor::makeShape({mMaxBatchSize}), nvinfer1::DataType::kINT32);

    mMedusaLogitsDevice = mBufferManager->gpu(
        ITensor::makeShape({mMaxNumHeads, mMaxBatchSize, mMaxTokensPerStep, mVocabSizePadded}), dataType);

    mNextDraftTokensDevice
        = mBufferManager->gpu(ITensor::makeShape({mMaxBatchSize, mMaxTokensPerStep}), nvinfer1::DataType::kINT32);

    for (int32_t bi = 0; bi < mBatchSize; ++bi)
    {
        auto logitsDeviceView = ITensor::slice(mTargetLogitsDevice, bi, 1);
        mBufferManager->copy(*targetLogitsHost, *logitsDeviceView);
    }

    for (int32_t hi = 0; hi < mMaxNumHeads; ++hi)
    {
        TensorPtr logitsHeadDeviceView = ITensor::slice(mMedusaLogitsDevice, hi, 1);
        TensorPtr logitsHeadHostView = ITensor::slice(medusaLogitsHost, hi, 1);
        logitsHeadDeviceView->squeeze(0);
        for (int32_t bi = 0; bi < mBatchSize; ++bi)
        {
            TensorPtr logitsHeadBatchDeviceView = ITensor::slice(logitsHeadDeviceView, bi, 1);
            mBufferManager->copy(*logitsHeadHostView, *logitsHeadBatchDeviceView);
        }
    }
}

template <typename T>
void MedusaDecodingLayerTest<T>::setup(SamplingParams& params)
{
    auto const endId = params.endId.value_or(mEndId);
    trk::invokeFill(*mSeqLengthsDevice, SizeType{0}, *mStream);
    trk::invokeFill(*mAcceptedLengths, SizeType{0}, *mStream);
    trk::invokeFill(*mFinishedDevice, uint8_t{0}, *mStream);
    trk::invokeFill(*mOutputIdsDevice, SizeType{0}, *mStream);
    trk::invokeFill(*mEndIdsDevice, TokenIdType{endId}, *mStream);
    trk::invokeFill(*mNextDraftTokensDevice, TokenIdType{-1}, *mStream);
    trk::invokeFill(*mPathsDevice, SizeType{-1}, *mStream);

    auto batchSlotsPtr = bufferCast<SizeType>(*mBatchSlots);
    for (SizeType bi = 0; bi < mBatchSize; ++bi)
    {
        batchSlotsPtr[bi] = 2 * bi;
    }

    for (SizeType bi = 0; bi < mBatchSize; ++bi)
    {
        auto const outputIdsHost = ITensor::wrap(reinterpret_cast<TokenIdType*>(params.draftIds[bi].data()),
            nvinfer1::DataType::kINT32, ITensor::makeShape({1, mMaxTokensPerStep}));
        auto outputIdsDeviceSlice = ITensor::slice(mOutputIdsDevice, batchSlotsPtr[bi], 1);
        mBufferManager->copy(*outputIdsHost, *outputIdsDeviceSlice);
    }

    for (SizeType bi = 0; bi < mBatchSize; ++bi)
    {
        auto& path = params.paths[bi];
        auto const numPaths = static_cast<SizeType>(params.paths[bi].size() / (mMaxNumHeads + 1));
        auto const pathsHost = ITensor::wrap(reinterpret_cast<SizeType*>(path.data()), nvinfer1::DataType::kINT32,
            ITensor::makeShape({1, numPaths, mMaxNumHeads + 1}));
        TensorPtr pathsDeviceSlice = ITensor::slice(mPathsDevice, batchSlotsPtr[bi], 1);
        pathsDeviceSlice->squeeze(0);
        TensorPtr pathsNumPathsDeviceSlice = ITensor::slice(pathsDeviceSlice, 0, numPaths);
        pathsNumPathsDeviceSlice->unsqueeze(0);
        mBufferManager->copy(*pathsHost, *pathsNumPathsDeviceSlice);
    }

    typename MedusaDecodingLayer<T>::MedusaSetupParams setupParams;
    setupParams.runtimeTopK = std::make_optional<std::vector<SizeType>>(params.runtimeTopK);
    setupParams.runtimeHeadsTopK = std::make_optional<std::vector<std::vector<SizeType>>>(params.runtimeHeadsTopK);
    setupParams.tokensPerStep = std::make_optional<std::vector<SizeType>>(params.tokensPerStep);
    setupParams.randomSeed = {{0}};

    mMedusaDecodingLayer->setup(mBatchSize, batchSlotsPtr, setupParams);

    mStream->synchronize();
}

template <typename T>
typename MedusaDecodingLayer<T>::MedusaForwardParams MedusaDecodingLayerTest<T>::createInputTensors()
{
    typename MedusaDecodingLayer<T>::MedusaForwardParams forwardParams(
        tcc::toTllmTensor(*mTargetLogitsDevice), tcc::toTllmTensor(*mEndIdsDevice));

    forwardParams.finished = tcc::toTllmTensor(*mFinishedDevice);

    forwardParams.batch_slots = tcc::toTllmTensor(*mBatchSlots);

    forwardParams.paths = tcc::toTllmTensor(*mPathsDevice);

    forwardParams.medusaLogits = tcc::toTllmTensor(*mMedusaLogitsDevice);

    return forwardParams;
}

template <typename T>
DecodingOutputParams MedusaDecodingLayerTest<T>::createOutputTensors()
{
    DecodingOutputParams outputParams(tcc::toTllmTensor(*mOutputIdsDevice));

    outputParams.sequence_length = tcc::toTllmTensor(*mSeqLengthsDevice);

    outputParams.finished = tcc::toTllmTensor(*mFinishedDevice);

    outputParams.nextDraftTokens = tcc::toTllmTensor(*mNextDraftTokensDevice);

    outputParams.acceptedLengths = tcc::toTllmTensor(*mAcceptedLengths);

    return outputParams;
}

template <typename T>
void MedusaDecodingLayerTest<T>::checkResult(std::vector<std::vector<std::set<TokenIdType>>> const& expectedOutTokens,
    std::vector<std::vector<TokenIdType>> const& expectedDraftTokens, std::vector<bool> const& finished)
{
    auto const nextDraftTokensHost = mBufferManager->copyFrom(*mNextDraftTokensDevice, runtime::MemoryType::kCPU);
    auto const outputIdsHost = mBufferManager->copyFrom(*mOutputIdsDevice, runtime::MemoryType::kCPU);
    auto const seqLenHost = mBufferManager->copyFrom(*mSeqLengthsDevice, runtime::MemoryType::kCPU);
    auto const acceptedLengthsHost = mBufferManager->copyFrom(*mAcceptedLengths, runtime::MemoryType::kCPU);
    auto const finishedHost = mBufferManager->copyFrom(*mFinishedDevice, runtime::MemoryType::kCPU);

    mStream->synchronize();

    auto nextDraftTokens = BufferRange<TokenIdType>(*nextDraftTokensHost);
    auto outputIds = BufferRange<TokenIdType>(*outputIdsHost);
    auto seqLen = BufferRange<SizeType>(*seqLenHost);
    auto batchSlots = BufferRange<SizeType>(*mBatchSlots);
    auto acceptedLengths = BufferRange<SizeType>(*acceptedLengthsHost);
    auto finishedPtr
        = reinterpret_cast<tk::FinishedState*>(bufferCast<tk::FinishedState::UnderlyingType>(*finishedHost));

    for (SizeType bi = 0; bi < mBatchSize; ++bi)
    {
        auto& expectedOutTokensBatch = expectedOutTokens[bi];
        auto const slot = batchSlots[bi];
        EXPECT_EQ(expectedOutTokensBatch.size(), seqLen[slot]);
        EXPECT_EQ(expectedOutTokensBatch.size(), acceptedLengths[slot]);
        for (SizeType ti = 0; ti < expectedOutTokensBatch.size(); ++ti)
        {
            EXPECT_GE(expectedOutTokensBatch[ti].count(outputIds[slot * mMaxSeqLen + ti]), 1);
        }
    }

    for (SizeType bi = 0; bi < mBatchSize; ++bi)
    {
        auto& expectedDraftTokensBatch = expectedDraftTokens[bi];
        auto const slot = batchSlots[bi];
        for (SizeType ti = 0; ti < expectedDraftTokensBatch.size(); ++ti)
        {
            EXPECT_EQ(expectedDraftTokensBatch[ti], nextDraftTokens[slot * mMaxTokensPerStep + ti]);
        }
    }
    for (SizeType bi = 0; bi < mBatchSize; ++bi)
    {
        auto const slot = batchSlots[bi];
        EXPECT_EQ(finished[bi], finishedPtr[slot].isFinished());
    }
}

template <typename T>
void MedusaDecodingLayerTest<T>::runTest(std::vector<std::vector<std::set<TokenIdType>>> const& expectedOutTokens,
    std::vector<std::vector<TokenIdType>> const& expectedDraftTokens, std::vector<bool> const& finished,
    SamplingParams& params)
{
    mBatchSize = params.batchSize;
    mMaxBatchSize = 2 * mBatchSize;

    allocateBuffers();

    setup(params);

    auto inputTensors = createInputTensors();
    auto outputTensors = createOutputTensors();

    mMedusaDecodingLayer->forward(outputTensors, inputTensors);

    mStream->synchronize();

    checkResult(expectedOutTokens, expectedDraftTokens, finished);
}

template class MedusaDecodingLayerTest<float>;
template class MedusaDecodingLayerTest<half>;

TYPED_TEST_SUITE(MedusaDecodingLayerTest, FloatAndHalfTypes);

TYPED_TEST(MedusaDecodingLayerTest, SimpleTestBS1)
{
    SamplingParams params;
    params.runtimeTopK = {1};
    params.runtimeHeadsTopK = {{2, 3, 2, 1}};
    params.draftIds = {{0, 4, 0, 2, 1, 3, 4, 3, 0, 2, 3, 4}};
    params.paths = {{0, 1, 2, 3, -1}};
    params.tokensPerStep = {12};
    params.batchSize = 1;

    std::vector<std::vector<std::set<TokenIdType>>> expectedOutTokens = {{{4}, {0}, {2}, {1}}};
    std::vector<std::vector<TokenIdType>> expectedDraftTokens = {{1, 2, 1, 2, 3, 2, 3, 0}};
    std::vector<bool> finished = {false};
    this->runTest(expectedOutTokens, expectedDraftTokens, finished, params);
}

TYPED_TEST(MedusaDecodingLayerTest, SimpleTestBS4)
{
    // Target Ids to be sampled
    // [4, 0, 2, 1, 3, 4, 3, 0, 2, 3, 4, 1]
    SamplingParams params;
    params.runtimeTopK = {1, 1, 1, 1};
    params.runtimeHeadsTopK = {{2, 3, 2, 1}, {1, 2, 3, 4}, {3, 1, 1, 1}, {1, 1, 1, 1}};
    // clang-format off
    params.draftIds = {{0, 4, 0, 2, 1, 3, 4, 4, 0, 2, 3, 4},
                       {0, 4, 0, 2, 1, 4, 4, 4, 0, 2, 2, 4},
                       {0, 4, 0, 4, 1, 1, 4, 4, 0, 2, 0, 4},
                       {0, 4, 0, 2, 1, 3, 2, 4, 0, 2, 3, 4}};
    params.paths = {{0, 7, 2, 8, -1,
                     0, 3, -1, -1, -1},
                    {0, 5, 7, 8, 10,
                     0, 3, -1, -1, -1},
                    {0, 8, 2, 9, -1,
                     0, 3, 5, 6, -1,
                     0, 3, 5, 7, 10},
                    {0, 1, 2, 6, -1,
                     0, 3, -1, -1, -1}};

    // clang-format on
    params.tokensPerStep = {12, 11, 11, 7};
    params.batchSize = 4;

    std::vector<std::vector<std::set<TokenIdType>>> expectedOutTokens
        = {{{4}, {0}, {2}}, {{4}, {4}, {0}, {2}, {4}}, {{4}, {1}, {4}, {0}, {4}}, {{4}, {0}, {2}, {3}}};
    std::vector<std::vector<TokenIdType>> expectedDraftTokens
        = {{2, 3, 3, 4, 5, 1, 2, 3}, {4, 1, 2, 4, 5, 6, 0, 1, 2, 3}, {4, 5, 6, 1, 4, 0}, {3, 3, 0, 3}};
    std::vector<bool> finished = {false, false, false, false};
    this->runTest(expectedOutTokens, expectedDraftTokens, finished, params);
}

TYPED_TEST(MedusaDecodingLayerTest, SimpleTestEndIdNotSelected)
{
    // Target Ids to be sampled
    // [4, 0, 2, 1, 3, 4, 3, 0, 2, 3, 4, 1]
    SamplingParams params;
    params.runtimeTopK = {1};
    params.runtimeHeadsTopK = {{1, 1, 1, 1}};
    params.draftIds = {{0, 4, 0, 4, 1, 3, 2, 3, 0, 2, 3, 4}};
    // clang-format off
    params.paths = {{0, 3, 4, 5, -1,
                     0, 1, 2, 6, -1}};
    // clang-format on
    params.tokensPerStep = {12};
    params.batchSize = 1;
    params.endId = 1;

    std::vector<std::vector<std::set<TokenIdType>>> expectedOutTokens = {{{4}, {0}, {2}, {3}}};
    std::vector<std::vector<TokenIdType>> expectedDraftTokens = {{3, 3, 0, 3}};
    std::vector<bool> finished = {false};
    this->runTest(expectedOutTokens, expectedDraftTokens, finished, params);
}

TYPED_TEST(MedusaDecodingLayerTest, SimpleTestEndIdSelected)
{
    // Target Ids to be sampled
    // [4, 0, 2, 1, 3, 4, 3, 0, 2, 3, 4, 1]
    SamplingParams params;
    params.runtimeTopK = {1};
    params.runtimeHeadsTopK = {{1, 1, 1, 1}};
    params.draftIds = {{0, 4, 0, 4, 1, 3, 2, 3, 0, 2, 3, 4}};
    // clang-format off
    params.paths = {{0, 3, 4, 5, -1,
                     0, 11, 7, 9, -1}};
    // clang-format on
    params.tokensPerStep = {12};
    params.batchSize = 1;
    params.endId = 1;

    std::vector<std::vector<std::set<TokenIdType>>> expectedOutTokens = {{{4}}};
    std::vector<std::vector<TokenIdType>> expectedDraftTokens = {{3, 4, 4, 4}};
    std::vector<bool> finished = {true};
    this->runTest(expectedOutTokens, expectedDraftTokens, finished, params);
}
} // namespace tensorrt_llm::tests::layers
