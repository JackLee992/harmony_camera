/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUDIODEC_H
#define AUDIODEC_H
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include "multimedia/player_framework/native_avcodec_audiodecoder.h"
#include "multimedia/player_framework/native_avmemory.h"
#include "multimedia/player_framework/native_avformat.h"
#include "tools.h"
#include "demuxer/DeMuxer.h"

class ADecSignal {
public:
    std::mutex inMutex_;
    std::mutex outMutex_;
    std::condition_variable inCond_;
    std::condition_variable outCond_;
    std::queue<uint32_t> inIdxQueue_;
    std::queue<uint32_t> outIdxQueue_;
    std::queue<OH_AVCodecBufferAttr> attrQueue_;
    std::queue<OH_AVMemory *> inBufferQueue_;
    std::queue<OH_AVMemory *> outBufferQueue_;
};

class AudioDec {
public:
    AudioDec() = default;
    ~AudioDec();
    int32_t RunAudioDec(std::string codeName);
    void WaitForEos();
    int32_t ConfigureAudioDecoder();
    int32_t StartAudioDecoder();
    int32_t CreateAudioDecoder(std::string codecName);
    int32_t SetAudioDecoderCallback();
    void RegisterMuxerManager(MutexManager *m);
    void RegisterDeMuxer(DeMuxer *dm);
    int32_t Release();
    void InputFunc();
    void OutPutFunc();
    void StopInLoop();
    void StopOutLoop();
    void InputEos(OH_AVCodecBufferAttr attr, int index, int frameCount);
    ADecSignal *signal_;
    int32_t audioChannelCount;
    int32_t audioSampleRate;
    int32_t audioSampleFormat;
    int32_t audioBitrate;
    int32_t aacIsADTS;
    DeMuxer *deMuxer;
    
private:
    std::atomic<bool> isRunning_ { false };
    std::unique_ptr<std::thread> inputLoop_;
    std::unique_ptr<std::thread> outputLoop_;
    OH_AVCodec *aDec_;
    OH_AVCodecAsyncCallback cb_;
    MutexManager *mutexManager;
};

#endif // AUDIODEC_H
