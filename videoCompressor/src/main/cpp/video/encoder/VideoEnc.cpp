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
#include "video/encoder/VideoEnc.h"
#include <multimedia/player_framework/native_avmemory.h>
#include <multimedia/player_framework/native_avcapability.h>
#include <queue>
#include <stack>
#include "hilog/log.h"
#include "tools.h"
using namespace std;

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200         // 全局domain宏，标识业务领域
#define LOG_TAG "videoCompressor" // 全局tag宏，标识模块日志tag

VideoEnc::~VideoEnc() { Release(); }

namespace {
    static void VencError(OH_AVCodec *codec, int32_t errorCode, void *userData) {
    
    }

    static void VencFormatChanged(OH_AVCodec *codec, OH_AVFormat *format, void *userData) {
    
    }

    static void VencOutputDataReady(OH_AVCodec *codec, uint32_t index, OH_AVMemory *data, OH_AVCodecBufferAttr *attr,
                                    void *userData) {
        VEncSignal *signal = static_cast<VEncSignal *>(userData);
        unique_lock<mutex> lock(signal->outMutex_);
        signal->outIdxQueue_.push(index);
        signal->outBufferQueue_.push(data);
        if (!attr) {
            OH_LOG_ERROR(LOG_APP, "VencOutputDataReady attr is null");
        } else {
            signal->outputAttrQueue.push(*attr);
        }
        signal->outCond_.notify_all();
    }

    static void VencNeedInputData(OH_AVCodec *codec, uint32_t index, OH_AVMemory *data, void *userData){
        VEncSignal *signal = static_cast<VEncSignal *>(userData);
        unique_lock<mutex> lock(signal->inputMutex_);
        auto arrayBuffer = signal->inputBufferQueue_.front();
        // 输入帧buffer对应的index，送入InIndexQueue队列
        // 输入帧的数据mem送入InBufferQueue队列
        memcpy(arrayBuffer, data, signal->arrayBufferSize);
        // 配置buffer info信息
        OH_AVCodecBufferAttr attr;
        attr.size = signal->arrayBufferSize;
        attr.offset = 0;
        attr.pts = 0; // TODO 
        attr.flags = AVCODEC_BUFFER_FLAGS_CODEC_DATA;
        // 送入编码输入队列进行编码，index为对应输入队列的下标
        int32_t ret = OH_VideoEncoder_PushInputData(codec, index, attr);
        if (ret != AV_ERR_OK) {
            // TODO 异常处理
        }
    };
} // namespace

int32_t VideoEnc::ConfigureVideoEncoder() {
    OH_AVFormat *format = OH_AVFormat_Create();
    if (format == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to create videoEncode format");
        return AV_ERR_UNKNOWN;
    }
    OH_LOG_ERROR(LOG_APP,
                 "get videoEncode width:%{public}d---height"
                 "---%{public}d---bitrate:%{public}d---frameRate:%{public}f",
                 width, height, bitrate, frameRate);
    int32_t rateMode = static_cast<int32_t>(OH_VideoEncodeBitrateMode::CBR);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_WIDTH, width);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_HEIGHT, height);
    OH_AVCapability *cap = OH_AVCodec_GetCapability(videoMime.data(), true);
    std::string codecName = OH_AVCapability_GetName(cap);
    OH_LOG_ERROR(LOG_APP, "ConfigureVideoEncoder g_codecName:%{public}s", codecName.data());
    if (codecName.find("rk") == string::npos) {
        OH_LOG_ERROR(LOG_APP, "ConfigureVideoEncoder g_codecName npos");
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_SURFACE_FORMAT);
    } else {
        OH_LOG_ERROR(LOG_APP, "ConfigureVideoEncoder g_codecName no npos");
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_RGBA);
    }
    if (frameRate <= 0) {
        frameRate = defaultFrame;
    }
    if (bitrate <= 0) {
        bitrate = defaultBitrate;
    }
    OH_LOG_ERROR(LOG_APP,
                 "final videoEncode width:%{public}d---height"
                 "---%{public}d---bitrate:%{public}d---frameRate:%{public}f",
                 width, height, bitrate, frameRate);
    OH_AVFormat_SetDoubleValue(format, OH_MD_KEY_FRAME_RATE, frameRate);
    OH_AVFormat_SetLongValue(format, OH_MD_KEY_BITRATE, bitrate);
    OH_AVFormat_SetIntValue(format, OH_MD_KEY_VIDEO_ENCODE_BITRATE_MODE, rateMode);
    int ret = OH_VideoEncoder_Configure(venc_, format);
    ret = OH_VideoEncoder_Prepare(venc_);
    OH_AVFormat_Destroy(format);
    return ret;
}

int32_t VideoEnc::SetVideoEncoderCallback() {
    signal_ = make_unique<VEncSignal>();
    if (signal_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to new VencSignal");
        return AV_ERR_UNKNOWN;
    }
    signal_->arrayBufferSize = width * height * 3;
    cb_.onError = VencError;
    cb_.onStreamChanged = VencFormatChanged;
    cb_.onNeedOutputData = VencOutputDataReady;
    cb_.onNeedInputData = VencNeedInputData;
    return OH_VideoEncoder_SetCallback(venc_, cb_, static_cast<void *>(signal_.get()));
}

void VideoEnc::RegisterMuxerManager(MutexManager *mutex) { mutexManager = mutex; }

int32_t VideoEnc::StartVideoEncoder() {
    outputIsRunning_.store(true);
    int ret = OH_VideoEncoder_Start(venc_);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "Failed to start video codec");
        outputIsRunning_.store(false);
        signal_->outCond_.notify_all();
        Release();
        return ret;
    }
    outputLoop_ = make_unique<thread>(&VideoEnc::OutputFunc, this);
    if (outputLoop_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "Failed to cteate output video outputLoop");
        outputIsRunning_.store(false);
        Release();
        return AV_ERR_UNKNOWN;
    }
    return AV_ERR_OK;
}

//int32_t VideoEnc::CreateVideoEncoder(std::string codeName) {
//    OH_LOG_ERROR(LOG_APP, "CreateVideoEncode start CreateVideoEncoder");
//    venc_ = OH_VideoEncoder_CreateByMime(codeName.data());
//    return venc_ == nullptr ? AV_ERR_UNKNOWN : AV_ERR_OK;
//}

/**
 * // 通过 MIME TYPE 创建编码器，系统会根据MIME创建最合适的编码器。
 * @return
 */
int32_t VideoEnc::CreateVideoEncoder(){
    venc_ = OH_VideoEncoder_CreateByMime(OH_AVCODEC_MIMETYPE_VIDEO_AVC);
    return venc_ == nullptr ? AV_ERR_UNKNOWN : AV_ERR_OK;
}

void VideoEnc::WaitForEos() {
    if (outputLoop_ && outputLoop_->joinable()) {
        outputLoop_->join();
    }
    outputLoop_ = nullptr;
    OH_LOG_ERROR(LOG_APP, "VideoEnc::WaitForEos END");
}

//int32_t VideoEnc::GetSurface() { return OH_VideoEncoder_GetSurface(venc_, &mutexManager->nativeWindow); }

void VideoEnc::SendEncEos() {
    if (venc_ == nullptr) {
        return;
    }

    int32_t ret = OH_VideoEncoder_NotifyEndOfStream(venc_);
    if (ret == 0) {
        OH_LOG_ERROR(LOG_APP, "ENC IN: input Eos notifyEndOfStream");
    } else {
        OH_LOG_ERROR(LOG_APP, "ENC IN: input Eos notifyEndOfStream error");
    }
}

void VideoEnc::pushFrameData(void* arrayBufferPtr){
    signal_.get()->inputBufferQueue_.push(arrayBufferPtr);
}

void VideoEnc::OutputFunc() {
    uint32_t errCount = 0;
    int64_t enCount = 0;
    while (true) {
        if (!outputIsRunning_.load()) {
            break;
        }
        unique_lock<mutex> lock(signal_->outMutex_);
        signal_->outCond_.wait(lock, [this]() { return (signal_->outIdxQueue_.size() > 0 || !outputIsRunning_.load()); });
        if (!outputIsRunning_.load()) {
            break;
        }
        uint32_t index = signal_->outIdxQueue_.front();
        OH_AVCodecBufferAttr attr = signal_->outputAttrQueue.front();
        if (attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
            outputIsRunning_.store(false);
            signal_->outCond_.notify_all();
            OH_LOG_ERROR(LOG_APP, "ENCODE EOS %{public}lld", enCount);
            break;
        }
        OH_AVMemory *buffer = signal_->outBufferQueue_.front();
        if (OH_AVMuxer_WriteSample(muxer->muxer, muxer->vTrackId, buffer, attr) != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP, "input video track data failed");
        }

        if (OH_VideoEncoder_FreeOutputData(venc_, index) != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP, "videoEncode FreeOutputDat error");
            errCount = errCount + 1;
        }

        if (errCount > 0) {
            OH_LOG_ERROR(LOG_APP, "videoEncode errCount > 0");
            outputIsRunning_.store(false);
            signal_->outCond_.notify_all();
            Release();
            break;
        }
        signal_->outIdxQueue_.pop();
        signal_->outputAttrQueue.pop();
        signal_->outBufferQueue_.pop();
        enCount++;
    }
}

void VideoEnc::RegisterMuxer(Muxer *m) { muxer = m; }

int32_t VideoEnc::Release() {
    StopOutLoop();
    if (venc_ != nullptr) {
        OH_VideoEncoder_Flush(venc_);
        OH_VideoEncoder_Stop(venc_);
        int ret = OH_VideoEncoder_Destroy(venc_);
        venc_ = nullptr;
        if (signal_ != nullptr) {
            signal_ = nullptr;
        }
        return ret;
    }
    return AV_ERR_OK;
}

void VideoEnc::StopOutLoop() {
    if (outputLoop_ != nullptr && outputLoop_->joinable()) {
        unique_lock<mutex> lock(signal_->outMutex_);
        Tools::ClearIntQueue(signal_->outIdxQueue_);
        Tools::ClearBufferQueue(signal_->outputAttrQueue);
        Tools::ClearMemoryBufferQueue(signal_->outBufferQueue_);
        signal_->outCond_.notify_all();
        lock.unlock();
        outputLoop_->join();
    }
}