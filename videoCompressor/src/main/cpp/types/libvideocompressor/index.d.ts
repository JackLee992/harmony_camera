declare class newVideoCompressor {
  /**
   *  视频压缩接口
   */
  compressVideoNative: (
    outputFileFd: number,
    inputFileFd: number,
    inputFileOffset: number,
    inputFileSize: number,
    quality: CompressQuality,
    outPutFilePath: string
  ) => Promise<CompressorResponse>;


  startRecordNative: (
    outputFileFd: number,
    width:number,
    height:number,
    outPutFilePath: string,
  ) => Promise<CompressorResponse>;

  pushOneFrameDataNative: (
     byteBuffer: ArrayBuffer
  )=> Promise<CompressorResponse>;

  stopRecordNative: (
  )=> Promise<CompressorResponse>;

}

export class CompressorResponse {
  code: CompressorResponseCode;
  message: string;
  outputPath: string;
}

/**
 * native 回调状态码
 */
export enum CompressorResponseCode {
  SUCCESS = 0, // 压缩成功
  OTHER_ERROR = -1, // 其他异常
}

/**
 * native 回调状态码
 */
export enum CompressQuality {
  COMPRESS_QUALITY_HIGH = 1,
  COMPRESS_QUALITY_MEDIUM = 2,
  COMPRESS_QUALITY_LOW = 3,
}