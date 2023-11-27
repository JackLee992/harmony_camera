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
#include "audio/encoder/AudioEnc.h"
#include <hilog/log.h>
#include <thread>
#include <mutex>
#include "video/encoder/VideoEnc.h"
#include "multimedia/player_framework/avcodec_audio_channel_layout.h"
#include "tools.h"
using namespace std;

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200 // 全局domain宏，标识业务领域
#define LOG_TAG "videoCompressor" // 全局tag宏，标识模块日志tag

namespace {
constexpr int64_t NANOS_IN_SECOND = 1000000000L;
constexpr int64_t NANOS_IN_MICRO = 1000L;
static void AencError(OH_AVCodec *codec, int32_t errorCode, void *userData) {}

static void AencFormatChanged(OH_AVCodec *codec, OH_AVFormat *format, void *userData) {}

static void AencInputDataReady(OH_AVCodec *codec, uint32_t index, OH_AVMemory *data, void *userData)
{
    AEncSignal *signal = static_cast<AEncSignal *>(userData);
    unique_lock<mutex> lock(signal->inMutex_);
    signal->inIdxQueue.push(index);
    signal->inBufferQueue_.push(data);
    signal->inCond_.notify_all();
}

static void AencOutputDataReady(OH_AVCodec *codec, uint32_t index, OH_AVMemory *data, OH_AVCodecBufferAttr *attr,
                                void *userData)
{
    AEncSignal *signal = static_cast<AEncSignal *>(userData);
    unique_lock<mutex> lock(signal->outMutex_);
    signal->outIdxQueue_.push(index);
    signal->outBufferQueue_.push(data);
    if (!attr) {
        OH_LOG_ERROR(LOG_APP, "AencOutputDataReady attr is null");
    } else {
        signal->attrQueue_.push(*attr);
    }
    signal->outCond_.notify_all();
}
} // namespace

AudioEnc::~AudioEnc()
{ Release(); }

int32_t AudioEnc::ConfigureAudioEncoder()
{
    OH_AVFormat *format = OH_AVFormat_Create();
    if (format == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to create audio encCode format");
        return AV_ERR_UNKNOWN;
    }
    
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_AUD_CHANNEL_COUNT, audioChannelCount);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_AUD_SAMPLE_RATE, audioSampleRate);
    OH_AVFormat_SetLongValue(format, OH_MD_KEY_BITRATE, audioBitrate);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_AUDIO_SAMPLE_FORMAT, SAMPLE_F32LE);
    if (audioChannelCount == 1) {
        OH_AVFormat_SetLongValue(format, OH_MD_KEY_CHANNEL_LAYOUT, MONO);
    } else {
        OH_AVFormat_SetLongValue(format, OH_MD_KEY_CHANNEL_LAYOUT, STEREO);
    }
    int ret = OH_AudioEncoder_Configure(aenc_, format);
    OH_AVFormat_Destroy(format);
    return ret;
}

int32_t AudioEnc::SetAudioEncoderCallback()
{
    signal_ = new AEncSignal();
    if (signal_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to new audio AEncSignal");
        return AV_ERR_UNKNOWN;
    }
    
    cb_.onError = AencError;
    cb_.onStreamChanged = AencFormatChanged;
    cb_.onNeedInputData = AencInputDataReady;
    cb_.onNeedOutputData = AencOutputDataReady;
    return OH_AudioEncoder_SetCallback(aenc_, cb_, static_cast<void *>(signal_));
}

int32_t AudioEnc::StartAudioEncoder()
{
    isRunning_.store(true);
    
    inputLoop_ = make_unique<thread>(&AudioEnc::InputFunc, this);
    if (inputLoop_ == nullptr) {
            OH_LOG_ERROR(LOG_APP, "Failed to create audio encode inputLoop");
            isRunning_.store(false);
            Release();
            return AV_ERR_UNKNOWN;
    }
    
    outputLoop = make_unique<thread>(&AudioEnc::OutputFunc, this);
    if (outputLoop == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to create audio encode outputLoop");
        isRunning_.store(false);
        Release();
        return AV_ERR_UNKNOWN;
    }
    
    int ret = OH_AudioEncoder_Start(aenc_);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "audio encoder start error %{public}d", ret);
        isRunning_.store(false);
        Release();
        signal_->inCond_.notify_all();
        signal_->outCond_.notify_all();
        return ret;
    }
    
    return ret;
}

void AudioEnc::RegisterMuxer(Muxer *m)
{
    muxer = m;
}

void AudioEnc::RegisterMuxerManager(MutexManager *mutex)
{
    mutexManager = mutex;
}

int32_t AudioEnc::CreateAudioEncoder(string codecName)
{
    aenc_ = OH_AudioEncoder_CreateByMime(codecName.data());
    return aenc_ == nullptr ? AV_ERR_UNKNOWN : AV_ERR_OK;
}

void AudioEnc::WaitForEos()
{
    OH_LOG_ERROR(LOG_APP, "AudioEnc WaitForEos1");
    if (inputLoop_ && inputLoop_->joinable()) {
        inputLoop_->join();
    }
    OH_LOG_ERROR(LOG_APP, "AudioEnc WaitForEos2");
    if (!outputLoop) {
        OH_LOG_ERROR(LOG_APP, "AudioEnc WaitForEos ccc");
    }
    
    if (!outputLoop->joinable()) {
        OH_LOG_ERROR(LOG_APP, "AudioEnc WaitForEos ddd");
    }
    if (outputLoop && outputLoop->joinable()) {
        outputLoop->join();
    }
    OH_LOG_ERROR(LOG_APP, "AudioEnc WaitForEos3");
    inputLoop_ = nullptr;
    outputLoop = nullptr;
    OH_LOG_ERROR(LOG_APP, "AudioEnc WaitForEos4");
}

void AudioEnc::InputFunc()
{
    uint32_t frameCount = 0;
    while (true) {
        if (!isRunning_.load()) { break; }
        uint32_t index;
        unique_lock<mutex> lock(signal_->inMutex_);
        signal_->inCond_.wait(lock, [this]() {
            if (!isRunning_.load()) { return true; }
            return signal_->inIdxQueue.size() > 0;
        });
        if (!isRunning_.load()) { break; }
        index = signal_->inIdxQueue.front();
        auto buffer = signal_->inBufferQueue_.front();
        uint8_t *fileBuffer = OH_AVMemory_GetAddr(buffer);
        if (fileBuffer == nullptr) {
            OH_LOG_ERROR(LOG_APP, "audio encode error: no memory");
            continue;
        }
        if (mutexManager->ARawQueue.size() == 0) { continue; }
        mutexManager->AudioEncMutex.lock();
        OH_AVCodecBufferAttr attr;
        EncodeElement enc = mutexManager-> ARawQueue.front();
        if (enc.data) {
            Tools::ParseBuff(fileBuffer, enc.attr.size, enc.data, enc.attr.size);
            delete[] enc.data;
        }
        Tools::ParseBuff(&attr, sizeof(attr), &enc.attr, sizeof(attr));
        mutexManager-> ARawQueue.pop_front();
        mutexManager->AudioEncMutex.unlock();
        
        if (attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
            InPutEos(attr, index, frameCount);
            break;
        }
        int32_t size = OH_AVMemory_GetSize(buffer);
        if (size < attr.size) {
            OH_LOG_ERROR(LOG_APP, "audio encode bufferSize smaller than attr size");
            continue;
        }
        OH_AudioEncoder_PushInputData(aenc_, index, attr);
        signal_->inIdxQueue.pop();
        signal_->inBufferQueue_.pop();
        frameCount++;
    }
}

void AudioEnc::InPutEos(OH_AVCodecBufferAttr attr, int index, int frameCount)
{
    attr.pts = 0;
    attr.size = 0;
    attr.offset = 0;
    attr.flags = AVCODEC_BUFFER_FLAGS_EOS;
    OH_AudioEncoder_PushInputData(aenc_, index, attr);
    OH_LOG_ERROR(LOG_APP, "audio encode eos %{public}d", frameCount);
    signal_->inIdxQueue.pop();
    signal_->inBufferQueue_.pop();
}

void AudioEnc::OutputFunc()
{
    uint32_t errCount = 0;
    uint32_t outCount = 0;
    while (true) {
        if (!isRunning_.load()) { break; }
        unique_lock<mutex> lock(signal_->outMutex_);
        signal_->outCond_.wait(lock, [this]() {
            if (!isRunning_.load()) { return true; }
            return signal_->outIdxQueue_.size() > 0;
        });
        if (!isRunning_.load()) { break; }
        uint32_t index = signal_->outIdxQueue_.front();
        OH_AVCodecBufferAttr attr = signal_->attrQueue_.front();
        OH_AVMemory *buffer = signal_->outBufferQueue_.front();
        if (attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
            OH_LOG_ERROR(LOG_APP, "audio encode output eos");
            OutPutEos();
            break;
        }
        if (OH_AVMuxer_WriteSample(muxer->muxer, muxer->aTrackId, buffer, attr) != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP, "audio track data failed");
        }
        outCount++;
        if (OH_AudioEncoder_FreeOutputData(aenc_, index) != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP, "audio encode freeOutputData error");
            errCount++;
            isRunning_.store(false);
            signal_->inCond_.notify_all();
            signal_->outCond_.notify_all();
            Release();
            break;
        }
        signal_->outIdxQueue_.pop();
        signal_->attrQueue_.pop();
        signal_->outBufferQueue_.pop();
    }
    OH_LOG_ERROR(LOG_APP, "AudioEnc::OutputFunc() end");
}

void AudioEnc::OutPutEos()
{
    signal_->outIdxQueue_.pop();
    signal_->attrQueue_.pop();
    signal_->outBufferQueue_.pop();
    isRunning_.store(false);
    signal_->inCond_.notify_all();
    signal_->outCond_.notify_all();
}
int32_t AudioEnc::Release()
{
    StopInLoop();
    StopOutLoop();
    if (aenc_ != nullptr) {
        OH_AudioEncoder_Flush(aenc_);
        OH_AudioEncoder_Stop(aenc_);
        int ret = OH_AudioEncoder_Destroy(aenc_);
        aenc_ = nullptr;
        if (signal_ != nullptr) {
            delete signal_;
            signal_ = nullptr;
        }
        return ret;
    }
    return AV_ERR_OK;
}

void AudioEnc::StopInLoop()
{
    if (inputLoop_ != nullptr && inputLoop_->joinable()) {
        unique_lock<mutex> lock(signal_->inMutex_);
        Tools::ClearIntQueue(signal_->inIdxQueue);
        Tools::ClearMemoryBufferQueue(signal_->inBufferQueue_);
        signal_->inCond_.notify_all();
        lock.unlock();
        inputLoop_->join();
        inputLoop_ = nullptr;
    }
}

void AudioEnc::StopOutLoop()
{
    if (outputLoop != nullptr && outputLoop->joinable()) {
        unique_lock<mutex> lock(signal_->outMutex_);
        Tools::ClearIntQueue(signal_->outIdxQueue_);
        Tools::ClearBufferQueue(signal_->attrQueue_);
        Tools::ClearMemoryBufferQueue(signal_->outBufferQueue_);
        signal_->outCond_.notify_all();
        lock.unlock();
        outputLoop->join();
        outputLoop = nullptr;
    }
}