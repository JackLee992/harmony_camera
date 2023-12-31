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

#ifndef AUDIOENC_H
#define AUDIOENC_H
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include "multimedia/player_framework/native_avcodec_audioencoder.h"
#include "multimedia/player_framework/native_avmemory.h"
#include "multimedia/player_framework/native_avformat.h"
#include "muxer.h"
#include "tools.h"

class AEncSignal {
public:
    std::mutex inMutex_;
    std::mutex outMutex_;
    std::condition_variable inCond_;
    std::condition_variable outCond_;
    std::queue<uint32_t> inIdxQueue;
    std::queue<uint32_t> outIdxQueue_;
    std::queue<OH_AVCodecBufferAttr> attrQueue_;
    std::queue<OH_AVMemory *> inBufferQueue_;
    std::queue<OH_AVMemory *> outBufferQueue_;
};

class AudioEnc {
public:
    AudioEnc() = default;
    ~AudioEnc();
    int32_t audioChannelCount;
    int32_t audioSampleRate;
    int32_t audioBitrate;
    int32_t CreateAudioEncoder(std::string codecName);
    void RegisterMuxerManager(MutexManager *m);
    int32_t ConfigureAudioEncoder();
    int32_t SetAudioEncoderCallback();
    int32_t StartAudioEncoder();
    void RegisterMuxer(Muxer *muxer);
    void WaitForEos();
    int32_t Release();
    void InputFunc();
    void OutputFunc();
    void StopInLoop();
    void StopOutLoop();
    void InPutEos(OH_AVCodecBufferAttr attr, int index, int frameCount);
    void OutPutEos();
    AEncSignal *signal_;
private:
    Muxer *muxer;
    std::atomic<bool> isRunning_ { false };
    std::unique_ptr<std::thread> inputLoop_;
    std::unique_ptr<std::thread> outputLoop;
    OH_AVCodec *aenc_;
    OH_AVCodecAsyncCallback cb_;
    MutexManager *mutexManager;
};
#endif // AUDIOENC_H
