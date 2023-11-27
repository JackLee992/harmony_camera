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
#ifndef TOOLS_H
#define TOOLS_H
#include <multimedia/player_framework/native_avcodec_base.h>
#include <mutex>
#include <queue>
typedef struct EncodeElement {
    uint8_t *data;
    OH_AVCodecBufferAttr attr;
};

typedef struct DemuxElement {
    OH_AVMemory *mem;
    OH_AVCodecBufferAttr attr;
};

class Tools {
public:
    static void ClearIntQueue(std::queue<uint32_t> &q);
    static void ClearBufferQueue(std::queue<OH_AVCodecBufferAttr> &q);
    static void ClearMemoryBufferQueue(std::queue<OH_AVMemory *> &q);
    static bool ParseBuff(void *dst, size_t dstMax, const void *src, size_t count);
};

class MutexManager {
public:
    std::mutex videoMutex_;
    std::deque<std::shared_ptr<DemuxElement>> VMem;
    std::mutex videoEncMutex;
    std::deque<EncodeElement> VRawQueue;
    std::mutex audioMutex_;
    std::deque<std::shared_ptr<DemuxElement>> AMem;
    std::mutex AudioEncMutex;
    std::deque<EncodeElement> ARawQueue;
    OHNativeWindow *nativeWindow;
    int32_t vTrackId;
    int32_t aTrackId;
};
#endif // TOOLS_H
