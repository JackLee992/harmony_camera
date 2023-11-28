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

#ifndef MUXER_H
#define MUXER_H

#include <multimedia/player_framework/native_avformat.h>
#include <multimedia/player_framework/native_avmuxer.h>
#include <multimedia/player_framework/native_avsource.h>
#include <string>

class Muxer {
public:
    ~Muxer();
    int32_t CreateMuxer(int32_t fd);
    void StopMuxer();
    int32_t vTrackId;
    int32_t aTrackId;
    std::string videoMime;
    std::string audioMime;
    OH_AVMuxer *muxer;
    uint32_t width;
    uint32_t height;
    double videoFrameRate;
//    int32_t audioBitrate;
//    int32_t audioSampleRate;
//    int audioChannelCount;
private:
    double defaultFrameDefault = 60.0;
    bool audioIsEnd = false;
    bool videoIsEnd = false;
};
#endif // MUXER_H
