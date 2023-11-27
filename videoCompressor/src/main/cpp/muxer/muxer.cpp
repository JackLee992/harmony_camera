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
#include <multimedia/player_framework/native_avformat.h>
#include <multimedia/player_framework/native_averrors.h>
#include <multimedia/player_framework/native_avmemory.h>
#include <multimedia/player_framework/native_avcodec_base.h>
#include "hilog/log.h"
#include "muxer.h"
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200 // 全局domain宏，标识业务领域
#define LOG_TAG "videoCompressor" // 全局tag宏，标识模块日志tag

using namespace std;

Muxer::~Muxer()
{
    StopMuxer();
}

int32_t Muxer::CreateMuxer(int32_t fd)
{
    muxer = OH_AVMuxer_Create(fd, AV_OUTPUT_FORMAT_MPEG_4);
    if (!muxer) {
        OH_LOG_ERROR(LOG_APP, "create muxer failed");
        return AV_ERR_UNKNOWN;
    }
    OH_LOG_ERROR(LOG_APP, "muxer:audioMime:%{public}s--audioSampleRate:%{public}d"
                 "--audioChannelCount:%{public}d--audioBitrate:%{public}d",
                 audioMime.c_str(), audioSampleRate, audioChannelCount, audioBitrate);
    OH_LOG_ERROR(LOG_APP, "muxer:videoMime:%{public}s--videoWidth:%{public}d"
                 "--videoHeight:%{public}d", videoMime.c_str(), width, height);
    OH_AVFormat *formatAudio = OH_AVFormat_CreateAudioFormat(audioMime.data(), audioSampleRate, audioChannelCount);
    if (!formatAudio) {
        OH_LOG_ERROR(LOG_APP, "muxer create audioFormat error");
        return AV_ERR_UNKNOWN;
    }
    OH_AVFormat *formatVideo = OH_AVFormat_CreateVideoFormat(videoMime.data(), width, height);
    if (!formatVideo) {
        OH_LOG_ERROR(LOG_APP, "muxer create videoFormat error");
        return AV_ERR_UNKNOWN;
    }
    OH_AVFormat_SetDoubleValue(formatVideo, OH_MD_KEY_FRAME_RATE,
                               videoFrameRate > 0 ? videoFrameRate : defaultFrameDefault);
    OH_AVMuxer_AddTrack(muxer, &vTrackId, formatVideo);
    OH_AVMuxer_AddTrack(muxer, &aTrackId, formatAudio);
    OH_AVFormat_Destroy(formatAudio);
    OH_AVFormat_Destroy(formatVideo);
    OH_AVMuxer_SetRotation(muxer, 0);
    int ret = OH_AVMuxer_Start(muxer);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP, "muxer start error:%{public}d", ret);
        return AV_ERR_UNKNOWN;
    }
    return AV_ERR_OK;
}

void Muxer::StopMuxer()
{
    if (muxer != nullptr) {
        OH_AVMuxer_Stop(muxer);
        OH_AVMuxer_Destroy(muxer);
        muxer = nullptr;
    }
}