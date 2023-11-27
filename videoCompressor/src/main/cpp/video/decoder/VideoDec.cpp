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
#include "decoder/VideoDec.h"
#include <hilog/log.h>
#include <multimedia/player_framework/native_avcapability.h>
#include "demuxer/DeMuxer.h"
#include "video/encoder/VideoEnc.h"
#include "tools.h"
using namespace std;

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200 // 全局domain宏，标识业务领域
#define LOG_TAG "videoCompressor" // 全局tag宏，标识模块日志tag

VideoDec::~VideoDec()
{
    Release();
}

namespace {
VideoDec *g_decSample = nullptr;
void VdecError(OH_AVCodec *codec, int32_t errorCode, void *userData)
{
    g_decSample->Release();
}

void VdecFormatChanged(OH_AVCodec *codec, OH_AVFormat *format, void *userData)
{
    int32_t current_width = 0;
    int32_t current_height = 0;
    OH_AVFormat_GetIntValue(format, OH_MD_KEY_WIDTH, &current_width);
    OH_AVFormat_GetIntValue(format, OH_MD_KEY_HEIGHT, &current_height);
    g_decSample->width = current_width;
    g_decSample->height = current_height;
}

void VdecInputDataReady(OH_AVCodec *codec, uint32_t index, OH_AVMemory *data, void *userData)
{
    VDecSignal *signal = static_cast<VDecSignal *>(userData);
    unique_lock<mutex> lock(signal->inMutex_);
    signal->inIdxQueue.push(index);
    signal->inBufferQueue_.push(data);
    signal->inCond_.notify_all();
}

void VdecOutputDataReady(OH_AVCodec *codec, uint32_t index, OH_AVMemory *data, OH_AVCodecBufferAttr *attr,
                         void *userData)
{
    VDecSignal *signal = static_cast<VDecSignal *>(userData);
    unique_lock<mutex> lock(signal->outMutex_);
    signal->outIdxQueue_.push(index);
    if (!attr) {
        OH_LOG_ERROR(LOG_APP, "VdecOutputDataReady attr is null");
    } else {
        signal->attrQueue_.push(*attr);
    }
    signal->outCond_.notify_all();
}
}

void VideoDec::RegisterDeMuxer(DeMuxer *dm)
{
    deMuxer = dm;
}


int32_t VideoDec::ConfigureVideoDecoder()
{
    OH_AVFormat *format = OH_AVFormat_Create();
    if (format == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to create videoDecode format");
        return AV_ERR_UNKNOWN;
    }
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_WIDTH, width);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_HEIGHT, height);
    OH_AVCapability *cap = OH_AVCodec_GetCapability(videoMime.data(), false);
    std::string codecName = OH_AVCapability_GetName(cap);
    OH_LOG_ERROR(LOG_APP, "ConfigureVideoDecoder g_codecName:%{public}s", codecName.data());
    if (codecName.find("rk") == string::npos) {
        OH_LOG_ERROR(LOG_APP, "ConfigureVideoDecoder g_codecName npos");
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_SURFACE_FORMAT);
    } else {
        OH_LOG_ERROR(LOG_APP, "ConfigureVideoDecoder g_codecName no npos");
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_RGBA);
    }
    OH_AVFormat_SetDoubleValue(format, OH_MD_KEY_FRAME_RATE, defaultFrameRate);
    int ret = OH_VideoDecoder_Configure(vdec_, format);
    OH_LOG_ERROR(LOG_APP,
                 "final videoDecode width:%{public}d---height"
                 "---%{public}d---frameRate:%{public}f",
                 width, height, defaultFrameRate);
    OH_AVFormat_Destroy(format);
    return ret;
}

int32_t VideoDec::SetOutputSurface()
{
    return OH_VideoDecoder_SetSurface(vdec_, mutexManager->nativeWindow);
}

int32_t VideoDec::RunVideoDec(std::string codeName)
{
    int err = CreateVideoDecoder(codeName);
    if (err != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "Failed to create video decoder");
        Release();
        return err;
    }
    
    err = ConfigureVideoDecoder();
    if (err != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "Failed to configure video decoder");
        Release();
        return err;
    }
    
    err = SetOutputSurface();
    if (err != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "Failed to video SetOutputSurface");
        Release();
        return err;
    }
    
    err = SetVideoDecoderCallback();
    if (err != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "Failed to video SetVideoDecoderCallback");
        Release();
        return err;
    }

    err = StartVideoDecoder();
    if (err != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "videoCompressor failed to start video decoder");
        Release();
        return err;
    }
    return err;
}

int32_t VideoDec::SetVideoDecoderCallback()
{
    signal_ = make_unique<VDecSignal>();
    if (signal_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to new VDecSignal");
        return AV_ERR_UNKNOWN;
    }
    
    cb_.onError = VdecError;
    cb_.onStreamChanged = VdecFormatChanged;
    cb_.onNeedInputData = VdecInputDataReady;
    cb_.onNeedOutputData = VdecOutputDataReady;
    return OH_VideoDecoder_SetCallback(vdec_, cb_, static_cast<void *>(signal_.get()));
}

int32_t VideoDec::CreateVideoDecoder(std::string codeName)
{
    OH_LOG_ERROR(LOG_APP, "CreateVideoDecoder:%{public}s", codeName.c_str());
    vdec_ = OH_VideoDecoder_CreateByMime(codeName.data());
    g_decSample = this;
    return vdec_ == nullptr ? AV_ERR_UNKNOWN : AV_ERR_OK;
}

int32_t VideoDec::StartVideoDecoder()
{
    isRunning_.store(true);
    
    int ret = OH_VideoDecoder_Start(vdec_);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "Failed to start codec");
        isRunning_.store(false);
        Release();
        return ret;
    }
    
    inputLoop_ = make_unique<thread>(&VideoDec::InputFunc, this);
    
    if (inputLoop_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to decode create input loop");
        isRunning_.store(false);
        OH_VideoDecoder_Stop(vdec_);
        return AV_ERR_UNKNOWN;
    }
    
    outputLoop_ = make_unique<thread>(&VideoDec::OutputFunc, this);
    if (outputLoop_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to decode create output loop");
        isRunning_.store(false);
        OH_VideoDecoder_Stop(vdec_);
        Release();
        return AV_ERR_UNKNOWN;
    }
    
    return AV_ERR_OK;
}

void VideoDec::WaitForEos()
{
    if (inputLoop_ && inputLoop_->joinable()) {
        inputLoop_->join();
    }
    if (outputLoop_ && outputLoop_->joinable()) {
        outputLoop_->join();
    }
    outputLoop_ = nullptr;
    inputLoop_ = nullptr;
}

void VideoDec::InputFunc()
{
    uint32_t frameCount_ = 0;
    uint32_t errCount = 0;
    while (true) {
        if (!isRunning_.load()) { break; }
        uint32_t index;
        unique_lock<mutex> lock(signal_->inMutex_);
        signal_->inCond_.wait(lock, [this]() {return signal_->inIdxQueue.size() > 0; });
        if (!isRunning_.load()) { break; }
        index = signal_->inIdxQueue.front();
        auto buffer = signal_->inBufferQueue_.front();
        OH_AVCodecBufferAttr attr;
        OH_AVDemuxer_ReadSample(deMuxer->demuxer, mutexManager->vTrackId, buffer, &attr);
        if (attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
            EndInput(attr, index, frameCount_);
            break;
        }
        if (OH_VideoDecoder_PushInputData(vdec_, index, attr) != AV_ERR_OK) {
            errCount++;
            OH_LOG_ERROR(LOG_APP, "video push input data failed error");
        }
        frameCount_++;
        signal_->inIdxQueue.pop();
        signal_->inBufferQueue_.pop();
    }
}

void VideoDec::EndInput(OH_AVCodecBufferAttr attr, int index, int frameCount)
{
    attr.pts = 0;
    attr.size = 0;
    attr.offset = 0;
    OH_VideoDecoder_PushInputData(vdec_, index, attr);
    OH_LOG_ERROR(LOG_APP, "video decode input eos %{public}d", frameCount);
}

void VideoDec::OutputFunc()
{
    uint32_t errCount = 0;
    uint32_t enCount = 0;
    while (true) {
        if (!isRunning_.load()) {
            break;
        }
        unique_lock<mutex> lock(signal_->outMutex_);
        signal_->outCond_.wait(lock, [this]() { return signal_->outIdxQueue_.size() > 0; });
        if (!isRunning_.load()) {
            break;
        }
        uint32_t index = signal_->outIdxQueue_.front();
        OH_AVCodecBufferAttr attr = signal_->attrQueue_.front();
        if (attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
            OH_LOG_ERROR(LOG_APP, "video output decode frameSize:eos%{public}d", enCount);
            VideoEnc->SendEncEos();
            break;
        }
        if (OH_VideoDecoder_RenderOutputData(vdec_, index) != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP, "videoDecode OH_VideoDecoder_RenderOutputData error");
            errCount = errCount + 1;
        }
        
        if (errCount > 0) {
            OH_LOG_ERROR(LOG_APP, "videoDecode errCount > 0");
            break;
        }
        signal_->outIdxQueue_.pop();
        signal_->attrQueue_.pop();
        enCount++;
    }
    OH_LOG_ERROR(LOG_APP, "VideoDec::OutputFunc");
}

void VideoDec::RegisterMuxerManager(MutexManager *mutex)
{
    mutexManager = mutex;
}

int32_t VideoDec::Release()
{
    StopInLoop();
    StopOutLoop();
    if (vdec_ != nullptr) {
        OH_VideoDecoder_Flush(vdec_);
        OH_VideoDecoder_Stop(vdec_);
        int ret = OH_VideoDecoder_Destroy(vdec_);
        vdec_ = nullptr;
        if (signal_ != nullptr) {
            signal_ = nullptr;
        }
        return ret;
    }
    return AV_ERR_OK;
}

void VideoDec::StopInLoop()
{
    if (inputLoop_ != nullptr && inputLoop_->joinable()) {
        unique_lock<mutex> lock(signal_->inMutex_);
        Tools::ClearIntQueue(signal_->inIdxQueue);
        Tools::ClearMemoryBufferQueue(signal_->inBufferQueue_);
        signal_->inCond_.notify_all();
        lock.unlock();
        inputLoop_->join();
    }
}

void VideoDec::StopOutLoop()
{
    if (outputLoop_ != nullptr && outputLoop_->joinable()) {
        unique_lock<mutex> lock(signal_->outMutex_);
        Tools::ClearIntQueue(signal_->outIdxQueue_);
        Tools::ClearBufferQueue(signal_->attrQueue_);
        signal_->outCond_.notify_all();
        lock.unlock();
        outputLoop_->join();
    }
}