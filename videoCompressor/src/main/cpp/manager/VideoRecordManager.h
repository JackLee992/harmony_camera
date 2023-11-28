//
// Created on 2023/11/22.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef ohos_videocompressor_VideoRecordHandler_H
#define ohos_videocompressor_VideoRecordHandler_H

#include "VideoRecordBean.h"
class VideoRecordManager {
    bool videoRecorderIsReady = false;
    int32_t CreateVideoEncode();
    int32_t CreateMutex();
    void VideoCompressorWaitEos();
    void NativeRecordStart();
    void SetCallBackResult(int32_t code, std::string str);
    void Release();
public:
    std::unique_ptr<VideoRecordBean> videoRecordBean_;
    static VideoRecordManager &getInstance() {
        static VideoRecordManager instance;
        return instance;
    }
    void startRecord();
    void pushOneFrameData(void *data);
    void DealCallback(void *data);
    void stopRecord();
};

#endif // ohos_videocompressor_VideoRecordHandler_H
