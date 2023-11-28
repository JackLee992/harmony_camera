//
// Created on 2023/11/22.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "VideoRecordManager.h"

void VideoRecordManager::startRecord() {
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

void VideoRecordManager::NativeRecordStart() {
    videoRecorderIsReady = false;
    if (CreateMutex() != AV_ERR_OK) {
        Release();
        SetCallBackResult( 1000, "CreateMuxer error");
        return;
    }
    if (CreateVideoEncode() != AV_ERR_OK) {
        Release();
        SetCallBackResult(1001, "CreateVideoEncode error");
        return;
    }
    // 视频编码
    if (videoRecordBean_->vEncSample->get()->StartVideoEncoder() != AV_ERR_OK) {
        Release();
        SetCallBackResult(1002,"StartVideoEncoder error");
        return;
    }
    videoRecorderIsReady  = true;
    SetCallBackResult(0, "NativeRecordStart success");
    VideoCompressorWaitEos();
}

void VideoRecordManager::SetCallBackResult(int32_t code, std::string str) {
    OH_LOG_ERROR(LOG_APP, "%{public}s", str.c_str());
    videoRecordBean_->resultCode = code;
    videoRecordBean_->resultStr = str;
}

void VideoRecordManager::DealCallback(void *data){
    AsyncCallbackInfo *asyncCallbackInfo = (AsyncCallbackInfo *)data;
    if (videoRecordBean_->resultCode != 0) {
        if (remove(asyncCallbackInfo->outputPath.data()) == 0) {
            OH_LOG_ERROR(LOG_APP, "delete outputFile");
        }
    }
    napi_env env = asyncCallbackInfo->env;
    napi_value code;
    napi_create_int32(env, videoRecordBean_->resultCode, &code);
    napi_value value;
    napi_create_string_utf8(env, asyncCallbackInfo->resultStr.data(), NAPI_AUTO_LENGTH, &value);
    napi_value outputPath;
    OH_LOG_ERROR(LOG_APP, "callback outputPath:%{public}s", asyncCallbackInfo->outputPath.c_str());
    napi_create_string_utf8(env, asyncCallbackInfo->outputPath.data(), NAPI_AUTO_LENGTH, &outputPath);
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "code", code);
    napi_set_named_property(env, obj, "message", value);
    napi_set_named_property(env, obj, "outputPath", outputPath);
    napi_resolve_deferred(env, asyncCallbackInfo->deferred, obj);
    napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
    delete asyncCallbackInfo;
}

void VideoRecordManager::pushOneFrameData(void *data){
    // 判断是否已经可以推数据了（编码器是否准备好了）
    if (!videoRecorderIsReady) {
        OH_LOG_ERROR(LOG_APP, "videoRecorderIsNotReady");
        return;
    }
    videoRecordBean_->vEncSample->get()->pushFrameData(data);
}

int32_t VideoRecordManager::CreateVideoEncode() {
    videoRecordBean_->vEncSample->get()->RegisterMuxerManager(videoRecordBean_->mutexManager->get());
    videoRecordBean_->vEncSample->get()->width = videoRecordBean_->width;
    videoRecordBean_->vEncSample->get()->height = videoRecordBean_->height;
    videoRecordBean_->vEncSample->get()->bitrate = videoRecordBean_->bitrate;
    videoRecordBean_->vEncSample->get()->frameRate = videoRecordBean_->frameRate;
    videoRecordBean_->vEncSample->get()->videoMime = videoRecordBean_->videoMime;
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
    videoRecordBean_->vEncSample->get()->RegisterMuxer(videoRecordBean_->muxer->get());
    OH_LOG_ERROR(LOG_APP, "CreateVideoEncode end");
    return ret;
}

int32_t VideoRecordManager::CreateMutex() {
    OH_LOG_ERROR(LOG_APP, "CreateMutex start");
    // 取编码器的宽高等信息
    videoRecordBean_->muxer->get()->width = videoRecordBean_->width;
    videoRecordBean_->muxer->get()->height = videoRecordBean_->height;
    videoRecordBean_->muxer->get()->videoMime = videoRecordBean_->videoMime;
    videoRecordBean_->muxer->get()->videoFrameRate = videoRecordBean_->frameRate;
    int ret = videoRecordBean_->muxer->get()->CreateMuxer(videoRecordBean_->outFd);
    if (ret !=0) {
        OH_LOG_ERROR(LOG_APP, "CreateMutex end");
    }
    return ret;
}

void VideoRecordManager::VideoCompressorWaitEos() {
    OH_LOG_ERROR(LOG_APP, "VideoCompressorDestroy");
    if (videoRecordBean_->vEncSample != nullptr) {
        videoRecordBean_->vEncSample->get()->WaitForEos();
    }
    OH_LOG_ERROR(LOG_APP, "VideoCompressorDestroy2");
    if (videoRecordBean_->muxer != nullptr) {
        videoRecordBean_->muxer->get()->StopMuxer();
    }
    Release();
    OH_LOG_ERROR(LOG_APP, "VideoCompressorDestroy end");
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