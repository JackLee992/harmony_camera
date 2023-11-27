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

#ifndef VIDEOENC_H
#define VIDEOENC_H
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include <multimedia/player_framework/native_avcodec_videoencoder.h>
#include <multimedia/player_framework/native_avmemory.h>
#include <multimedia/player_framework/native_avformat.h>
#include "muxer/muxer.h"
#include "tools.h"

class VEncSignal {
public:
    std::mutex outMutex_;
    std::mutex inputMutex_;
    std::condition_variable outCond_;
    std::condition_variable inputCond_;
    std::queue<uint32_t> outIdxQueue_;
    std::queue<uint32_t> inputIdxQueue_;
    std::queue<OH_AVCodecBufferAttr> outputAttrQueue;
    std::queue<OH_AVCodecBufferAttr> inputAttrQueue;
    std::queue<OH_AVMemory *> outBufferQueue_;
    std::queue<void *> inputBufferQueue_;
    uint32_t arrayBufferSize;
};

class VideoEnc {
public:
    VideoEnc() = default;
    ~VideoEnc();
    uint32_t width = 1080; // TODO
    uint32_t height = 1920; // TODO
    uint32_t bitrate = 10000000;
    double frameRate = 24;
    std::string videoMime = "";
//    int32_t CreateVideoEncoder(std::string codeName);
    int32_t CreateVideoEncoder();
    int32_t ConfigureVideoEncoder();
    int32_t SetVideoEncoderCallback();
    int32_t StartVideoEncoder();
    void RegisterMuxerManager(MutexManager *mutex);
    void RegisterMuxer(Muxer *muxer);
    uint32_t GetQualityBitrate(int quality);
    void WaitForEos();
    void StopOutLoop();
    int32_t Release();
//    void InputFunc();
    void OutputFunc();
    void SendEncEos();
//    int32_t GetSurface();
    std::unique_ptr<VEncSignal> signal_;
    void pushFrameData(void *data);
private:
    Muxer *muxer;
    std::atomic<bool> outputIsRunning_ { false };
    std::atomic<bool> inputIsRunning_ { false };
    std::unique_ptr<std::thread> outputLoop_;
    std::unique_ptr<std::thread> inputLoop_;
    OH_AVCodec *venc_;
    OH_AVCodecAsyncCallback cb_;
    MutexManager *mutexManager;
    int defaultBitrate = 959000;
    int defaultFrame = 24;
    
};

#endif // VIDEOENC_H
