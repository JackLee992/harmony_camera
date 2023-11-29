import abilityAccessCtrl, { Permissions } from '@ohos.abilityAccessCtrl';
import { BusinessError } from '@ohos.base';
import camera from '@ohos.multimedia.camera';
import image from '@ohos.multimedia.image';
import bundleManager from '@ohos.bundle.bundleManager';
import { GlobalContext } from '../utils/GlobalContext';
import Logger from '../utils/Logger';
import display from '@ohos.display';
import { CompressorResponseCode, CompressQuality, VideoCompressor } from '@ohos/videoCompressor';
import hilog from '@ohos.hilog';
import data from '@ohos.telephony.data';
let TAG: string = "Index";
let cameraWidth = 1920;
let cameraHeight = 1080;
let previewOutput: camera.PreviewOutput;
let previewOutput2: camera.PreviewOutput;
let cameraInput: camera.CameraInput;
let cameraSession: camera.CaptureSession;
let videoCompressor = new VideoCompressor();

@Entry
@Component
struct Index {
  private xController: XComponentController = new XComponentController();
  private surfaceId: string = "-1";
  private display = display.getDefaultDisplaySync();
  private heightPx = (this.display.width * cameraWidth / cameraHeight) + 'px';
  private receiver: image.ImageReceiver = createImageReceiver();

  async onPageShow() {
    this.startRecord();
    await grantPermission().then(res => {
      console.info(TAG, `权限申请成功  ${JSON.stringify(res)}`);
      if (res) {
        createDualChannelPreview(this.surfaceId, this.receiver);
      }
    })
  }

  private startRecord() {
    videoCompressor.startRecorder(getContext(), cameraWidth, cameraHeight)
      .then((data) => {
        if (data.code == CompressorResponseCode.SUCCESS) {
          console.log("videoCompressor message:" + data.message + "--outputPath:" + data.outputPath);
        } else {
          console.log("videoCompressor code:" + data.code + "--error message:" + data.message);
        }
      }).catch((err: Error) => {
      console.log("videoCompressor HIGH get error message" + err.message);
    });
  }

  onPageHide() {
    videoCompressor.stopRecorder()
    releaseCamera()
  }

  build() {
    Flex() {
      XComponent({
        id: '',
        type: 'surface',
        libraryname: '',
        controller: this.xController
      }).onLoad(() => {
        this.xController.setXComponentSurfaceSize({ surfaceWidth: cameraWidth, surfaceHeight: cameraHeight });
        this.surfaceId = this.xController.getXComponentSurfaceId();
        Logger.debug("getDisplayInfo --w:" + this.display.width + " h:" + this.display.height)
      }).width('100%').height(this.heightPx)
    }
  }
}


function createImageReceiver(): image.ImageReceiver {
  let receiver: image.ImageReceiver = image.createImageReceiver(cameraWidth, cameraHeight, 4, 8);
  receiver.on('imageArrival', () => {
    receiver.readNextImage((err: BusinessError, nextImage: image.Image) => {
      Logger.debug("receiveImage-- readNextImage:" + nextImage);
      if (err || nextImage === undefined) {
        Logger.error("receiveImage -error:" + err + " nextImage:" + nextImage);
        return;
      }
      nextImage.getComponent(image.ComponentType.JPEG, (err: BusinessError, imgComponent: image.Component) => {
        if (err || imgComponent === undefined) {
          Logger.error("receiveImage--getComponent -error:" + err + " imgComponent:" + imgComponent);
          return;
        }

        if (imgComponent.byteBuffer as ArrayBuffer) {
          let buffer = imgComponent.byteBuffer;
          Logger.debug("receiveImage--byteBuffer -success:" + " buffer:" + buffer);
          // TODO  process ths image
          videoCompressor.pushOneFrameData(buffer)
          nextImage.release() //需要将对应的image释放，确保Surface的BufferQueue正常轮转
        } else {
          Logger.debug("receiveImage--byteBuffer -error:" + " imgComponent.byteBuffer:" + imgComponent.byteBuffer);
          return;
        }
      });
    });
  });
  return receiver;
}

async function grantPermission(): Promise<boolean> {
  const PERMISSIONS: Array<Permissions> = [
    'ohos.permission.CAMERA',
  ];
  try {
    // 获取应用程序的accessTokenID
    let bundleInfo: bundleManager.BundleInfo =
      await bundleManager.getBundleInfoForSelf(
        bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION
      );
    let appInfo: bundleManager.ApplicationInfo = bundleInfo.appInfo;
    let tokenId = appInfo.accessTokenId;

    let atManager = abilityAccessCtrl.createAtManager();
    let pems: Array<Permissions> = [];

    for (let i = 0; i < PERMISSIONS.length; i++) {
      let state = await atManager.checkAccessToken(tokenId, PERMISSIONS[i]);
      console.info(TAG, `grantPermission  checkAccessToken ${PERMISSIONS[i]} + : ${JSON.stringify(state)}`);
      if (state !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        pems.push(PERMISSIONS[i]);
      }
    }
    if (pems.length > 0) {
      console.info(TAG, 'grantPermission  requestPermissionsFromUser :' + JSON.stringify(pems));
      let result = await atManager.requestPermissionsFromUser(GlobalContext.get().getCameraAbilityContext(), pems);

      let grantStatus: Array<number> = result.authResults;
      let length: number = grantStatus.length;
      for (let i = 0; i < length; i++) {
        console.info(TAG, `grantPermission  requestPermissionsFromUser ${result.permissions[i]} + : ${grantStatus[i]}`);

        if (grantStatus[i] === 0) {
          // 用户授权，可以继续访问目标操作
        } else {
          // 用户拒绝授权，提示用户必须授权才能访问当前页面的功能
          console.log(TAG + 'grantPermission  fail ');
          return false;
        }
      }
    }
    // 授权成功
    console.info(TAG, 'grantPermission  success ');
    return true;
  } catch (e) {
    console.info(TAG, 'grantPermission  fail ');
    return false;
  }
}

async function releaseCamera() {
  if (cameraInput) {
    cameraInput.close()
  }
  if (previewOutput) {
    previewOutput.release()
  }
  if (previewOutput2) {
    previewOutput2.release()
  }
  if (cameraSession) {
    cameraSession.release()
  }
}

function getCameraDevices(cameraManager: camera.CameraManager): Array<camera.CameraDevice> {
  let cameraArray: Array<camera.CameraDevice> = cameraManager.getSupportedCameras();
  if (cameraArray != undefined && cameraArray.length <= 0) {
    console.error("cameraManager.getSupportedCameras error");
    return [];
  }
  for (let index = 0; index < cameraArray.length; index++) {
    console.info('getCameraDevices -- cameraId : ' + cameraArray[index].cameraId); // 获取相机ID
    console.info('getCameraDevices -- cameraPosition : ' + cameraArray[index].cameraPosition); // 获取相机位置
  }
  return cameraArray;
}

async function createDualChannelPreview(XComponentSurfaceId: string, receiver: image.ImageReceiver): Promise<void> {
  releaseCamera();
  let cameraManager = getCameraManager();
  let camerasDevices: Array<camera.CameraDevice> = getCameraDevices(cameraManager); // 获取支持的相机设备对象

  // 获取profile对象
  let profiles: camera.CameraOutputCapability = cameraManager.getSupportedOutputCapability(camerasDevices[0]); // 获取对应相机设备profiles
  let previewProfiles: Array<camera.Profile> = profiles.previewProfiles;

  Logger.debug("createDualChannelPreview-- previewProfiles:" + previewProfiles.length)
  // 预览流1
  let previewProfilesObj: camera.Profile = previewProfiles[0];

  // 预览流2
  let previewProfilesObj2: camera.Profile = previewProfiles[0];

  previewProfiles.forEach(profile => {
    let supportSize = profile.size
    if (supportSize.width == cameraWidth && supportSize.height == cameraHeight) {
      // 选择1920x1080的profile
      previewProfilesObj = profile;
      previewProfilesObj2 = profile;
      Logger.debug("----choose Supported profile: size-- " + supportSize.width + " " + supportSize.height + " type:" + profile.format)
    }
    Logger.debug("Supported profile: size-- " + supportSize.width + " " + supportSize.height + " type:" + profile.format)
  });

  // 创建 预览流1 输出对象
  previewOutput = cameraManager.createPreviewOutput(previewProfilesObj, XComponentSurfaceId);

  // 创建 预览流2 输出对象
  let imageReceiverSurfaceId: string = await receiver.getReceivingSurfaceId();
  previewOutput2 = cameraManager.createPreviewOutput(previewProfilesObj2, imageReceiverSurfaceId);

  // 创建cameraInput对象
  cameraInput = cameraManager.createCameraInput(camerasDevices[1]);

  // 打开相机
  await cameraInput.open();

  // 会话流程
  cameraSession = cameraManager.createCaptureSession();

  // 开始配置会话
  cameraSession.beginConfig();

  // 把CameraInput加入到会话
  cameraSession.addInput(cameraInput);

  // 把 预览流1 加入到会话
  cameraSession.addOutput(previewOutput)

  // 把 预览流2 加入到会话
  cameraSession.addOutput(previewOutput2);

  // 提交配置信息
  await cameraSession.commitConfig();

  // 会话开始
  await cameraSession.start();

}

function getCameraManager(): camera.CameraManager {
  return camera.getCameraManager(GlobalContext.get().getCameraAbilityContext());
}
