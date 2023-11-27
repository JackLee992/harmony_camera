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
#include "DeMuxer.h"
#include <hilog/log.h>
#include <multimedia/player_framework/native_averrors.h>
#include <multimedia/player_framework/native_avcodec_base.h>
#include <multimedia/player_framework/native_avformat.h>
#include <multimedia/player_framework/native_avmemory.h>
#include "muxer.h"
#include "tools.h"
using namespace std;
#define LOG_DOMAIN 0x3200 // 全局domain宏，标识业务领域
#define LOG_TAG "videoCompressor" // 全局tag宏，标识模块日志tag

DeMuxer::~DeMuxer()
{
    Release();
}

int32_t DeMuxer::CreateDemuxer(int32_t fd, int32_t offset, int64_t size)
{
    source = OH_AVSource_CreateWithFD(fd, offset, size);
    if (!source) {
        OH_LOG_ERROR(LOG_APP, "create demuxer source failed");
        return AV_ERR_UNKNOWN;
    }
    
    sourceFormat = OH_AVSource_GetSourceFormat(source);
    if (!sourceFormat) {
        OH_LOG_ERROR(LOG_APP, "create demuxer getSourceFormat failed");
        return AV_ERR_UNKNOWN;
    }
    OH_AVFormat_GetIntValue(sourceFormat, OH_MD_KEY_TRACK_COUNT, &trackCount);
    demuxer = OH_AVDemuxer_CreateWithSource(source);
    GetTrackInfo();
    OH_AVFormat_Destroy(sourceFormat);
    sourceFormat = nullptr;
    return AV_ERR_OK;
}

void DeMuxer::GetTrackInfo()
{
    int trackType = 0;
    for (int32_t index = 0; index < trackCount; index++) {
            OH_AVDemuxer_SelectTrackByID(demuxer, index);
            OH_AVFormat *trackFormat = OH_AVSource_GetTrackFormat(source, index);
            OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_TRACK_TYPE, &trackType);
            if (trackType == MEDIA_TYPE_VID) {
                OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_WIDTH, &width);
                OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_HEIGHT, &height);
                OH_AVFormat_GetDoubleValue(trackFormat, OH_MD_KEY_FRAME_RATE, &frameRate);
                OH_AVFormat_GetLongValue(trackFormat, OH_MD_KEY_BITRATE, &bitrate);
                char *videoTemp;
                OH_AVFormat_GetStringValue(trackFormat, OH_MD_KEY_CODEC_MIME, const_cast<char const **>(&videoTemp));
                vTrackId = index;
                OH_LOG_ERROR(LOG_APP, "demuxer:videoMime:%{public}s--width:%{public}d--height:%{public}d"
                             "--frameRate:%{public}f--bitrate%{public}lld", videoTemp, width, height,
                             frameRate, bitrate);
                videoMime = videoTemp;
                mutexManager->vTrackId = vTrackId;
            }
            
            if (trackType == MEDIA_TYPE_AUD) {
                aTrackId = index;
                OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_AUD_CHANNEL_COUNT, &audioChannelCount);
                OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_AUD_SAMPLE_RATE, &audioSampleRate);
                OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_AUDIO_SAMPLE_FORMAT, &audioSampleFormat);
                OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_AAC_IS_ADTS, &aacIsADTS);
                char *audioTemp;
                OH_AVFormat_GetStringValue(trackFormat, OH_MD_KEY_CODEC_MIME, const_cast<char const **>(&audioTemp));
                OH_AVFormat_GetLongValue(trackFormat, OH_MD_KEY_BITRATE, &audioBitrate);
                OH_LOG_ERROR(LOG_APP, "demuxer:audioMime:%{public}s--audioBitrate:%{public}lld--aacIsADTS:%{public}d"
                             "--audioSampleFormat:%{public}d--audioSampleRate:%{public}d--audioChannelCount%{public}d",
                             audioTemp, audioBitrate, aacIsADTS, audioSampleFormat, audioSampleRate, audioChannelCount);
                audioMime = audioTemp;
                mutexManager->aTrackId = aTrackId;
            }
            OH_AVFormat_Destroy(trackFormat);
        }
}

void DeMuxer::RegisterMuxerManager(MutexManager *m)
{
    mutexManager = m;
}

void DeMuxer::Release()
{
    if (source != nullptr) {
        OH_AVSource_Destroy(source);
        source = nullptr;
    }
    if (demuxer != nullptr) {
        OH_AVDemuxer_Destroy(demuxer);
        demuxer = nullptr;
    }
}
int32_t DeMuxer::DestroyDemuxer()
{
    Release();
    return AV_ERR_OK;
}