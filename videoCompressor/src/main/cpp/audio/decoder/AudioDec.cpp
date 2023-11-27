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
#include "audio/decoder/AudioDec.h"
#include <hilog/log.h>
#include <unistd.h>
#include "multimedia/player_framework/native_avcodec_base.h"
#include "tools.h"

using namespace std;

namespace {
constexpr int64_t NANOS_IN_SECOND = 1000000000L;
constexpr int64_t NANOS_IN_MICRO = 1000L;
AudioDec *g_decSample = nullptr;

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200 // 全局domain宏，标识业务领域
#define LOG_TAG "videoCompressor" // 全局tag宏，标识模块日志tag
void AdecError(OH_AVCodec *codec, int32_t errorCode, void *userData)
{
    OH_LOG_ERROR(LOG_APP, "audio decode error%{public}d", errorCode);
    g_decSample->Release();
}

void AdecFormatChanged(OH_AVCodec *codec, OH_AVFormat *format, void *userData) {}

void AdecInputDataReady(OH_AVCodec *codec, uint32_t index, OH_AVMemory *data, void *userData)
{
    ADecSignal *signal = static_cast<ADecSignal *>(userData);
    unique_lock<mutex> lock(signal->inMutex_);
    signal->inIdxQueue_.push(index);
    signal->inBufferQueue_.push(data);
    signal->inCond_.notify_all();
}

void AdecOutputDataReady(OH_AVCodec *codec, uint32_t index, OH_AVMemory *data, OH_AVCodecBufferAttr *attr,
                         void *userData)
{
    ADecSignal *signal = static_cast<ADecSignal *>(userData);
    unique_lock<mutex> lock(signal->outMutex_);
    signal->outIdxQueue_.push(index);
    signal->outBufferQueue_.push(data);
    if (!attr) {
        OH_LOG_ERROR(LOG_APP, "AdecOutputDataReady attr is null");
    } else {
        signal->attrQueue_.push(*attr);
    }
    signal->outCond_.notify_all();
}
} // namespace

AudioDec::~AudioDec()
{ Release(); }

int32_t AudioDec::ConfigureAudioDecoder()
{
    OH_AVFormat *format = OH_AVFormat_Create();
    if (format == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to create audio decoder format");
    }
    OH_LOG_ERROR(LOG_APP, "audioDecode:audioBitrate:%{public}d---aacIsADTS%{public}d---audioSampleFormat%{public}d---"
                 "audioSampleRate%{public}d---audioChannelCount%{public}d",
                 audioBitrate, aacIsADTS, audioSampleFormat, audioSampleRate, audioChannelCount);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_AUD_CHANNEL_COUNT, audioChannelCount);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_AUD_SAMPLE_RATE, audioSampleRate);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_AUDIO_SAMPLE_FORMAT, SAMPLE_F32LE);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_AAC_IS_ADTS, aacIsADTS);
    OH_AVFormat_SetLongValue(format, OH_MD_KEY_BITRATE, audioBitrate);
    int32_t ret = OH_AudioDecoder_Configure(aDec_, format);
    OH_AVFormat_Destroy(format);
    
    return ret;
}

int32_t AudioDec::RunAudioDec(string codeName)
{
    int err = CreateAudioDecoder(codeName);
    if (err != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "Failed to create audio decoder %{public}d", err);
        return err;
    }
    
    err = ConfigureAudioDecoder();
    if (err != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "Failed to configure audio decoder %{public}d", err);
        Release();
        return err;
    }
    
    err = SetAudioDecoderCallback();
        if (err != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP, "Failed to SetAudioDecoderCallback %{public}d", err);
            Release();
            return err;
        }
    
    err = StartAudioDecoder();
        if (err != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP, "Failed to StartAudioDecoder %{public}d", err);
            Release();
            return err;
        }
    return err;
}

void AudioDec::RegisterDeMuxer(DeMuxer *dm)
{
    deMuxer = dm;
}

int32_t AudioDec::SetAudioDecoderCallback()
{
    signal_ = new ADecSignal();
    if (signal_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to new audio AEncSignal AudioDecoderCallback");
        return AV_ERR_UNKNOWN;
    }
    
    cb_.onError = AdecError;
    cb_.onStreamChanged = AdecFormatChanged;
    cb_.onNeedInputData = AdecInputDataReady;
    cb_.onNeedOutputData = AdecOutputDataReady;
    return OH_AudioDecoder_SetCallback(aDec_, cb_, static_cast<void *>(signal_));
}

int32_t AudioDec::CreateAudioDecoder(string codeName)
{
    OH_LOG_ERROR(LOG_APP, "CreateAudioDecoder:%{public}s", codeName.c_str());
    aDec_ = OH_AudioDecoder_CreateByMime(codeName.data());
    if (aDec_) {
        OH_LOG_ERROR(LOG_APP, "audio decoder create success");
        g_decSample = this;
    }
    return aDec_ == nullptr ? AV_ERR_UNKNOWN : AV_ERR_OK;
}

int32_t AudioDec::StartAudioDecoder()
{
    isRunning_.store(true);
    int ret = OH_AudioDecoder_Start(aDec_);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "Failed to Start audio decode %{public}d", ret);
        isRunning_.store(false);
        Release();
        return ret;
    }
    
    inputLoop_ = make_unique<thread>(&AudioDec::InputFunc, this);
    if (inputLoop_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to create audio decode inputLoop");
        isRunning_.store(false);
        Release();
        return AV_ERR_UNKNOWN;
    }
    
    outputLoop_ = make_unique<thread>(&AudioDec::OutPutFunc, this);
    if (outputLoop_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to create audio decode outputLoop");
        isRunning_.store(false);
        Release();
        return AV_ERR_UNKNOWN;
    }
    return ret;
}

void AudioDec::RegisterMuxerManager(MutexManager *mutex)
{
    mutexManager = mutex;
}

void AudioDec::WaitForEos()
{
    if (inputLoop_ && inputLoop_->joinable()) {
        inputLoop_->join();
    }
    
    if (outputLoop_ && outputLoop_->joinable()) {
        outputLoop_->join();
    }
    inputLoop_ = nullptr;
    outputLoop_ = nullptr;
}

void AudioDec::InputFunc()
{
    uint32_t frameCount_ = 0;
    uint32_t errCount = 0;
    
    while (true) {
        if (!isRunning_.load()) { break; }
        unique_lock<mutex> lock(signal_->inMutex_);
        signal_->inCond_.wait(lock, [this]() {return signal_->inIdxQueue_.size() > 0; });
        if (!isRunning_.load()) { break; }
        uint32_t index = signal_->inIdxQueue_.front();
        auto buffer = signal_->inBufferQueue_.front();
        
        OH_AVCodecBufferAttr attr;
        OH_AVDemuxer_ReadSample(deMuxer->demuxer, mutexManager->aTrackId, buffer, &attr);
        if (attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
            InputEos(attr, index, frameCount_);
            break;
        }
        
        int32_t result = OH_AudioDecoder_PushInputData(aDec_, index, attr);
        signal_->inIdxQueue_.pop();
        signal_->inBufferQueue_.pop();
        
        if (result != AV_ERR_OK) {
            errCount++;
            OH_LOG_ERROR(LOG_APP, "audio decode push input data failed");
        }
        frameCount_++;
    }
}

void AudioDec::InputEos(OH_AVCodecBufferAttr attr, int index, int frameCount)
{
    attr.pts = 0;
    attr.size = 0;
    attr.offset = 0;
    OH_AudioDecoder_PushInputData(aDec_, index, attr);
    OH_LOG_ERROR(LOG_APP, "audio input eos %{public}d", frameCount);
}

void AudioDec::OutPutFunc()
{
    uint32_t outCount = 0;
    uint32_t errCount = 0;
    while (true) {
        if (!isRunning_.load()) {
            break;
        }
        unique_lock<mutex> lock(signal_->outMutex_);
        signal_->outCond_.wait(lock, [this]() {return signal_->outIdxQueue_.size() > 0; });
        if (!isRunning_.load()) {
            break;
        }
        uint32_t index = signal_->outIdxQueue_.front();
        OH_AVCodecBufferAttr attr = signal_->attrQueue_.front();
        OH_AVMemory *buffer = signal_->outBufferQueue_.front();
        if (attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
            OH_LOG_ERROR(LOG_APP, "audio decode output eos");
            mutexManager->AudioEncMutex.lock();
            EncodeElement *encodeElement = new EncodeElement();
            encodeElement->data = nullptr;
            encodeElement->attr = attr;
            mutexManager->ARawQueue.push_back(*encodeElement);
            mutexManager->AudioEncMutex.unlock();
            break;
        }
        uint8_t *data = new uint8_t[OH_AVMemory_GetSize(buffer)];
        Tools::ParseBuff(data, OH_AVMemory_GetSize(buffer), OH_AVMemory_GetAddr(buffer), OH_AVMemory_GetSize(buffer));
        mutexManager->AudioEncMutex.lock();
        EncodeElement *encodeElement = new EncodeElement();
        encodeElement->data = data;
        encodeElement->attr = attr;
        mutexManager->ARawQueue.push_back(*encodeElement);
        mutexManager->AudioEncMutex.unlock();
        outCount++;
        if (OH_AudioDecoder_FreeOutputData(aDec_, index) != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP, "audio decode freeOutputData fail");
            errCount++;
        }
        if (errCount > 0) {
            OH_LOG_ERROR(LOG_APP, "audio decode errCount > 0");
            break;
        }
        signal_->outIdxQueue_.pop();
        signal_->attrQueue_.pop();
        signal_->outBufferQueue_.pop();
    }
}

int32_t AudioDec::Release()
{
    StopInLoop();
    StopOutLoop();
    if (aDec_ != nullptr) {
        OH_AudioDecoder_Flush(aDec_);
        OH_AudioDecoder_Stop(aDec_);
        int ret = OH_AudioDecoder_Destroy(aDec_);
        aDec_ = nullptr;
        if (signal_ != nullptr) {
            delete signal_;
            signal_ = nullptr;
        }
        return ret;
    }
    return AV_ERR_OK;
}

void AudioDec::StopInLoop()
{
    if (inputLoop_ != nullptr && inputLoop_->joinable()) {
        unique_lock<mutex> lock(signal_->inMutex_);
        Tools::ClearIntQueue(signal_->inIdxQueue_);
        Tools::ClearMemoryBufferQueue(signal_->inBufferQueue_);
        signal_->inCond_.notify_all();
        lock.unlock();
        inputLoop_->join();
    }
}

void AudioDec::StopOutLoop()
{
    if (outputLoop_ != nullptr && outputLoop_->joinable()) {
        unique_lock<mutex> lock(signal_->outMutex_);
        Tools::ClearIntQueue(signal_->outIdxQueue_);
        Tools::ClearBufferQueue(signal_->attrQueue_);
        Tools::ClearMemoryBufferQueue(signal_->outBufferQueue_);
        signal_->outCond_.notify_all();
        lock.unlock();
        outputLoop_->join();
    }
}

