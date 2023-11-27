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

#ifndef DEMUXER_H
#define DEMUXER_H
#include <multimedia/player_framework/native_avmemory.h>
#include <thread>
#include <multimedia/player_framework/native_avdemuxer.h>
#include <multimedia/player_framework/native_avsource.h>
#include <multimedia/player_framework/native_avcodec_base.h>
#include <multimedia/player_framework/native_avformat.h>
#include <multimedia/player_framework/native_avmemory.h>
#include "tools.h"
class DeMuxer {
public:
    ~DeMuxer();
    int32_t CreateDemuxer(int32_t fd, int32_t offset, int64_t size);
    void GetTrackInfo();
    int32_t DestroyDemuxer();
    void Release();
    void RegisterMuxerManager(MutexManager *m);
    OH_AVFormat *sourceFormat;
    int32_t width;
    int32_t height;
    double frameRate;
    int64_t bitrate;
    std::string videoMime;
    std::string audioMime;
    int32_t audioChannelCount;
    int32_t audioSampleRate;
    int32_t audioSampleFormat;
    int64_t audioBitrate;
    int32_t aacIsADTS;
    int32_t vTrackId;
    int32_t aTrackId;
    OH_AVDemuxer *demuxer;
private:
    OH_AVSource *source;
    int32_t trackCount;
    int32_t audioFrame = 0;
    int32_t videoFrame = 0;
    std::unique_ptr<std::thread> inputLoop_;
    bool audioIsEnd = false;
    bool videoIsEnd = false;
    std::thread *demuxThread;
    MutexManager *mutexManager;
};

#endif // DEMUXER_H
