//
// Created on 2023/11/22.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "VideoRecordManager.h"

int VideoRecordManager::startRecord() {
    auto mutexManager = std::make_unique<MutexManager>();
    // 创建muxer
    auto muxer = std::make_unique<Muxer>();
    // 创建视频编码器
    auto vEncSample = std::make_unique<VideoEnc>();
    videoRecordBean_->vEncSample = &vEncSample;
    videoRecordBean_->muxer = &muxer;
    videoRecordBean_->mutexManager = &mutexManager;
    return NativeRecordStart();
}

int VideoRecordManager::NativeRecordStart() {
    if (CreateVideoEncode() != AV_ERR_OK) {
        Release();
        return 1001;
    }
    if (CreateMutex() != AV_ERR_OK) {
        Release();
        return 1002;
    }
    // 视频编码
    if (videoRecordBean_->vEncSample->get()->StartVideoEncoder() != AV_ERR_OK) {
        Release();
        return 1003;
    }
    return 0;
}

void VideoRecordManager::pushOneFrameData(void *data){
    videoRecordBean_->vEncSample->get()->pushFrameData(data);
}

int32_t VideoRecordManager::CreateVideoEncode() {
    videoRecordBean_->vEncSample->get()->RegisterMuxerManager(videoRecordBean_->mutexManager->get());
    videoRecordBean_->vEncSample->get()->width = videoRecordBean_->vEncSample->get()->width;
    videoRecordBean_->vEncSample->get()->height = videoRecordBean_->vEncSample->get()->height;
    videoRecordBean_->vEncSample->get()->bitrate = videoRecordBean_->vEncSample->get()->bitrate;
    videoRecordBean_->vEncSample->get()->frameRate = videoRecordBean_->vEncSample->get()->frameRate;
    videoRecordBean_->vEncSample->get()->videoMime = videoRecordBean_->vEncSample->get()->videoMime;
    int ret = videoRecordBean_->vEncSample->get()->CreateVideoEncoder();
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "CreateVideoEncoder error");
        return ret;
    }
    ret = videoRecordBean_->vEncSample->get()->SetVideoEncoderCallback();
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "VideoEncoder SetVideoEncoderCallback %{public}d", ret);
        return ret;
    }
    ret = videoRecordBean_->vEncSample->get()->ConfigureVideoEncoder();
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "VideoEncoder ConfigureVideoEncoder %{public}d", ret);
        return ret;
    }
//            ret = videoRecordBean_->vEncSample->get()->GetSurface();
//            if (ret != AV_ERR_OK) {
//                OH_LOG_ERROR(LOG_APP, "VideoEncoder GetSurface %{public}d", ret);
//                return ret;
//            }
    videoRecordBean_->vEncSample->get()->RegisterMuxer(videoRecordBean_->muxer->get());
    OH_LOG_ERROR(LOG_APP, "CreateVideoEncode end");
    return ret;
}

int32_t VideoRecordManager::CreateMutex() {
    OH_LOG_ERROR(LOG_APP, "CreateMutex start");
    // 取编码器的宽高等信息
    videoRecordBean_->muxer->get()->width = videoRecordBean_->vEncSample->get()->width;
    videoRecordBean_->muxer->get()->height = videoRecordBean_->vEncSample->get()->height;
    videoRecordBean_->muxer->get()->videoMime = videoRecordBean_->vEncSample->get()->videoMime;
    videoRecordBean_->muxer->get()->videoFrameRate = videoRecordBean_->vEncSample->get()->frameRate;
    int ret = videoRecordBean_->muxer->get()->CreateMuxer(videoRecordBean_->asyncCallbackInfo->outFd); // TODO error fixme
    OH_LOG_ERROR(LOG_APP, "CreateMutex end");
    return ret;
}

void VideoRecordManager::Release() {
    if (videoRecordBean_) {
        VideoCompressorWaitEos();
        videoRecordBean_->vEncSample->get()->Release();
        videoRecordBean_->muxer->get()->StopMuxer();
        videoRecordBean_->vEncSample = nullptr;
        videoRecordBean_->muxer = nullptr;
        videoRecordBean_.reset();
    }
}

void VideoRecordManager::VideoCompressorWaitEos() {
    OH_LOG_ERROR(LOG_APP, "VideoCompressorDestroy");
    if (videoRecordBean_->vEncSample != nullptr) {
        videoRecordBean_->vEncSample->get()->WaitForEos();
    }
    OH_LOG_ERROR(LOG_APP, "VideoCompressorDestroy4");
    if (videoRecordBean_->muxer != nullptr) {
        videoRecordBean_->muxer->get()->StopMuxer();
    }
    OH_LOG_ERROR(LOG_APP, "VideoCompressorDestroy6");
    Release();
    OH_LOG_ERROR(LOG_APP, "VideoCompressorDestroy end");
}

//void VideoRecordManager::SetCallBackResult(int32_t code, std::string str) {
//    OH_LOG_ERROR(LOG_APP, "%{public}s", str.c_str());
//    videoRecordBean_->asyncCallbackInfo->resultCode = code;
//    videoRecordBean_->asyncCallbackInfo->resultStr = str;
//}




