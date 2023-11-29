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
import { newVideoCompressor } from 'libvideoCompressor.so';
import { CompressorResponse } from './CompressorResponse';
import fs from '@ohos.file.fs';

export class VideoCompressor {
  private object: newVideoCompressor;
  private outPutFilePath = "";

  constructor() {
    console.log("VideoCompressor constructor");
    this.object = new newVideoCompressor();
  }

  startRecorder(context: Context, width: number, height: number): Promise<CompressorResponse> {
    try {
      let date = new Date();
      this.outPutFilePath = context.filesDir + "/" + date.getTime() + ".mp4"; //创建输出文件
      let outputFile = fs.openSync(this.outPutFilePath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
      if (!outputFile) {
        console.info("videoCompressor outputFile create error");
        return new Promise((resolve, reject) => {
          fs.unlink(this.outPutFilePath);
          reject(new Error("videoCompressor outputFile create error"));
        });
      }
      return this.object.startRecordNative(outputFile.fd, width, height, this.outPutFilePath)
    } catch (err) {
      return new Promise((resolve, reject) => {
        fs.unlink(this.outPutFilePath);
        reject(err);
      });
    }
  }

  /**
   *
   * @param context
   * @param byteBuffer 图像数据
   */
  pushOneFrameData(byteBuffer: ArrayBuffer) {
    return this.object.pushOneFrameDataNative(byteBuffer)
  }

  stopRecorder() {
    return this.object.stopRecordNative();
  }

}