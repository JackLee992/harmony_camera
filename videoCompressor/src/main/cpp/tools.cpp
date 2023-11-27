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
#include "tools.h"
#include "hilog/log.h"
#include "boundscheck/third_party_bounds_checking_function/include/securec.h"
#define LOG_DOMAIN 0x3200 // 全局domain宏，标识业务领域
#define LOG_TAG "videoCompressor" // 全局tag宏，标识模块日志tag
void Tools::ClearIntQueue(std::queue<uint32_t> &q)
{
    std::queue<uint32_t> empty;
    swap(empty, q);
}

void Tools::ClearBufferQueue(std::queue<OH_AVCodecBufferAttr> &q)
{
    std::queue<OH_AVCodecBufferAttr> empty;
    swap(empty, q);
}

void Tools::ClearMemoryBufferQueue(std::queue<OH_AVMemory *> &q)
{
    std::queue<OH_AVMemory *> empty;
    swap(empty, q);
}

bool Tools::ParseBuff(void *dst, size_t dstMax, const void *src, size_t count)
{
    errno_t err = EOK;
    err = memcpy_s(dst, dstMax, src, count);
    if (err != EOK) {
        OH_LOG_ERROR(LOG_APP, "memcpy_s failed, err = %{public}d", err);
        return false;
    }
    return true;
}