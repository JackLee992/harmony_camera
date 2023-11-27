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

#ifndef VIDEODEC_H
#define VIDEODEC_H
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include <multimedia/player_framework/native_avcodec_videodecoder.h>
#include <multimedia/player_framework/native_avmemory.h>
#include <multimedia/player_framework/native_avformat.h>
#include "tools.h"
#include "encoder/VideoEnc.h"
#include "demuxer/DeMuxer.h"
class VDecSignal {
public:
    std::mutex inMutex_;
    std::mutex outMutex_;
    std::condition_variable inCond_;
    std::condition_variable outCond_;
    std::queue<uint32_t> inIdxQueue;
    std::queue<uint32_t> outIdxQueue_;
    std::queue<OH_AVCodecBufferAttr> attrQueue_;
    std::queue<OH_AVMemory *> inBufferQueue_;
};

class VideoDec {
public:
    VideoDec() = default;
    ~VideoDec();
    int32_t RunVideoDec(std::string codeName);
    uint32_t width;
    uint32_t height;
    std::string videoMime;
    double defaultFrameRate = 30.0;
    void WaitForEos();
    int32_t ConfigureVideoDecoder();
    int32_t StartVideoDecoder();
    int32_t CreateVideoDecoder(std::string codeName);
    int32_t SetVideoDecoderCallback();
    void RegisterDeMuxer(DeMuxer *dm);
    int32_t Release();
    void InputFunc();
    void OutputFunc();
    void StopInLoop();
    void StopOutLoop();
    void EndInput(OH_AVCodecBufferAttr attr, int index, int frameCount);
    void RegisterMuxerManager(MutexManager *m);
    int32_t SetOutputSurface();
    std::unique_ptr<VDecSignal> signal_;
    VideoEnc *VideoEnc;
    DeMuxer *deMuxer;

private:
    std::atomic<bool> isRunning_ { false };
    std::unique_ptr<std::thread> inputLoop_;
    std::unique_ptr<std::thread> outputLoop_;
    OH_AVCodec *vdec_;
    OH_AVCodecAsyncCallback cb_;
    MutexManager *mutexManager;
};
#endif // VIDEODEC_H
