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
#include <bits/alltypes.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <string>
#include "VideoCompressor.h"
#include "manager/VideoRecordManager.h"
#include "muxer.h"
#include "hilog/log.h"
#include "napi/native_api.h"
#include "video/decoder/VideoDec.h"
#include "video/encoder/VideoEnc.h"
#include "audio/decoder/AudioDec.h"
#include "audio/encoder/AudioEnc.h"
#include "VideoRecordBean.h"
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200         // 全局domain宏，标识业务领域
#define LOG_TAG "videoCompressor" // 全局tag宏，标识模块日志tag

napi_value VideoCompressor::JsConstructor(napi_env env, napi_callback_info info) {
    napi_value targetObj = nullptr;
    void *data = nullptr;
    size_t argsNum = 0;
    napi_value args[2] = {nullptr};
    napi_get_cb_info(env, info, &argsNum, args, &targetObj, &data);
    auto classBind = std::make_unique<VideoCompressor>();
    napi_wrap(
        env, nullptr, classBind.get(),
        [](napi_env env, void *data, void *hint) {
            VideoCompressor *bind = (VideoCompressor *)data;
            delete bind;
            bind = nullptr;
        },
        nullptr, nullptr);
    return targetObj;
}

napi_value VideoCompressor::startRecord(napi_env env,napi_callback_info info) {
    napi_value args[4] = {nullptr};
    int32_t outFD = true;
    int32_t width = 0;
    int32_t height = 0;
    size_t charLen = 0;
    size_t argc = 4;
    int len = 1024;
    char output[1024] = {0};
    // 从js中获取传递的参数
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    napi_get_value_int32(env, args[0], &outFD); // 输出文件fd
    napi_get_value_int32(env, args[1], &width); // 输出编码宽度
    napi_get_value_int32(env, args[2], &height); //输出编码高度
    napi_get_value_string_utf8(env, args[3], output, len, &charLen); //文件路径
    auto &videoRecorder = VideoRecordManager::getInstance();
    videoRecorder.videoRecordBean_.release();
    videoRecorder.videoRecordBean_.reset();
    videoRecorder.videoRecordBean_ = std::make_unique<VideoRecordBean>();
    videoRecorder.videoRecordBean_->env = env;
    videoRecorder.videoRecordBean_->width = width;
    videoRecorder.videoRecordBean_->height = height;
    videoRecorder.videoRecordBean_->outFd = outFD;
    videoRecorder.videoRecordBean_->outputPath = output;
    int ret = videoRecorder.startRecord();
    return 0;
}

napi_value VideoCompressor::pushOneFrameDataNative(napi_env env,napi_callback_info cb) {
    napi_value args[1] = {nullptr};
    void *arrayBufferPtr = nullptr;
    size_t arrayBufferSize;
    napi_get_arraybuffer_info(env, args[0], &arrayBufferPtr, &arrayBufferSize); //获取到输入的帧数据
    auto &videoRecorder = VideoRecordManager::getInstance();
    videoRecorder.pushOneFrameData(arrayBufferPtr);
    return 0;
}

napi_value VideoCompressor::stopRecordNative(napi_env env,napi_callback_info cb) {
    VideoRecordManager::getInstance();
    return 0;
}

EXTERN_C_START static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor classProp[] = {
        {"startRecordNative", nullptr, VideoCompressor::startRecord, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"pushOneFrameDataNative", nullptr, VideoCompressor::pushOneFrameDataNative, nullptr, nullptr, nullptr,napi_default, nullptr},
        {"pushOneFrameDataNative", nullptr, VideoCompressor::stopRecordNative, nullptr, nullptr, nullptr, napi_default,nullptr},
    };
    napi_value videoCompressor = nullptr;
    const char *classBindName = "videoCompressor";
    napi_define_class(env, classBindName, strlen(classBindName), VideoCompressor::JsConstructor, nullptr, 3, classProp,
                      &videoCompressor);
    napi_set_named_property(env, exports, "newVideoCompressor", videoCompressor);
    return exports;
}
EXTERN_C_END

static napi_module VideoCompressorModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "videoCompressor",
    .nm_priv = ((void *)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) { napi_module_register(&VideoCompressorModule); }