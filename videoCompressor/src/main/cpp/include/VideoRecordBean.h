//
// Created on 2023/11/24.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".
#include <bits/alltypes.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <string>
#include "AsyncCallbackInfo.h"
#include "VideoCompressor.h"
#include "muxer.h"
#include "hilog/log.h"
#include "napi/native_api.h"
#include "video/decoder/VideoDec.h"
#include "video/encoder/VideoEnc.h"
#include "audio/decoder/AudioDec.h"
#include "audio/encoder/AudioEnc.h"

#ifndef ohos_videocompressor_videoRecord_H
#define ohos_videocompressor_videoRecord_H

struct VideoRecordBean {
    napi_env env;
    int32_t width;
    int32_t height;
    uint32_t bitrate;
    double frameRate;
    std::string videoMime;
    std::unique_ptr<Muxer> *muxer;
    std::unique_ptr<VideoEnc> *vEncSample;
    std::unique_ptr<MutexManager> *mutexManager;
//    std::unique_ptr<AsyncCallbackInfo> *callbackInfo;
    std::string outputPath = "";
    int32_t outFd = true;
    int32_t resultCode = 0;
    std::string resultStr = "";
};

#endif //ohos_videocompressor_videoRecord_H
