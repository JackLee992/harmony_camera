
export class GlobalContext {
  private static TAG: string = '[GlobalContext]:';
  private static instance: GlobalContext;
  private cameraAbilityContext;

  private constructor() {
  }

  public static get(): GlobalContext {
    if (!Boolean(GlobalContext.instance).valueOf()) {
      GlobalContext.instance = new GlobalContext();
    }
    return GlobalContext.instance;
  }

  public getCameraAbilityContext() {
    return this.cameraAbilityContext;
  }

  public setCameraAbilityContext(context): void {
    this.cameraAbilityContext = context;
  }
}