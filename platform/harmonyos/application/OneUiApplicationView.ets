import { image } from '@kit.ImageKit';
import { KeyboardAvoidMode as UIKeyboardAvoidMode } from '@kit.ArkUI';
import { BusinessError } from '@kit.BasicServicesKit';
import native, { OneUiApplicationFrame, OneUiSystemBars } from 'liboneui_application.so';
import { OneUiApplicationController } from './OneUiApplicationController';
import { OneUiInputMethod } from './OneUiInputMethod';

/** ArkUI presents OneUI's CPU raster surface; every product control, layout,
 * hit test and edit remains in the same shared C++ Widget tree as Android/iOS.
 * ArkUI insets the raster surface once. Only the backgrounds extend under the
 * system bars, so drawing and hit testing share the same safe coordinates.
 * Set keyboardBottomPx only when the OS did not resize this content area. */
@Component
export struct OneUiApplicationView {
  controller: OneUiApplicationController | undefined = undefined;
  @Prop @Watch('activityChanged') foreground: boolean = true;
  @Prop @Watch('viewportChanged') keyboardBottomPx: number = 0;
  @Prop @Watch('appearanceChanged') appearance: number = 0;
  @Prop @Watch('appearanceChanged') fontScale: number = 1;
  onFailure: (error: Error) => void = (_error: Error): void => {};
  @State private pixels: image.PixelMap | undefined = undefined;
  @State private statusColor: string = '#00000000';
  @State private navigationColor: string = '#00000000';
  private barRevision: string = '';
  private input: OneUiInputMethod | undefined = undefined;
  private timer: number = -1;
  private viewportWidth: number = 0;
  private viewportHeight: number = 0;
  private x: number = 0;
  private y: number = 0;
  private painting: boolean = false;
  private disposed: boolean = false;
  private failureReported: boolean = false;
  private pixelScale(): number { return this.getUIContext().vp2px(1); }
  private host(): string {
    if (this.controller === undefined) { throw new Error('Application controller required'); }
    return this.controller.host;
  }
  aboutToAppear(): void {
    this.disposed = false;
    this.getUIContext().setKeyboardAvoidMode(UIKeyboardAvoidMode.RESIZE);
    this.input = new OneUiInputMethod(this.host(), (error: Error): void => this.reportFailure(error));
    this.appearanceChanged(); this.activityChanged();
    this.timer = setInterval(() => this.tick(), 16);
  }
  private activityChanged(): void {
    if (this.disposed || this.controller?.isClosed()) { return; }
    native.lifecycle(this.host(), this.foreground ? 1 : 3);
    if (this.foreground) { this.barRevision = ''; this.syncSystemBars(); }
    this.input?.sync(this.x, this.y, this.pixelScale(), true);
  }
  private syncSystemBars(): void {
    const bars: OneUiSystemBars = native.systemBars(this.host());
    if (bars.revision === this.barRevision) { return; }
    this.barRevision = bars.revision;
    this.statusColor = '#' + bars.statusColor.toString(16).padStart(8, '0');
    this.navigationColor = '#' + bars.navigationColor.toString(16).padStart(8, '0');
    this.controller?.updateSystemBars(bars).catch((error: BusinessError): void => {
      console.warn('OneUI system bar update failed', error.code);
    });
  }
  private appearanceChanged(): void {
    if (this.disposed || this.controller?.isClosed()) { return; }
    native.theme(this.host(), this.appearance, this.fontScale, 16 * this.fontScale);
  }
  private viewportChanged(): void {
    if (this.viewportWidth < 1 || this.viewportHeight < 1 || this.disposed || this.controller?.isClosed()) { return; }
    const scale: number = this.pixelScale();
    if (!native.viewport(this.host(), this.viewportWidth * scale, this.viewportHeight * scale, scale, 0, 0, 0, 0, this.keyboardBottomPx)) {
      this.reportFailure(new Error('Application viewport'));
    }
  }
  private async tick(): Promise<void> {
    if (this.disposed || this.controller?.isClosed()) { return; }
    try {
      this.controller?.drain();
      if (!this.foreground || this.painting) { return; }
      const frame: OneUiApplicationFrame | undefined = native.frame(this.host());
      if (frame === undefined) { return; }
      this.syncSystemBars();
      this.painting = true;
      const pixels: image.PixelMap = await image.createPixelMap(frame.rgba.buffer as ArrayBuffer, {
        size: { width: frame.width, height: frame.height }, srcPixelFormat: image.PixelMapFormat.RGBA_8888,
        pixelFormat: image.PixelMapFormat.RGBA_8888,
        alphaType: image.AlphaType.PREMUL, editable: false
      });
      if (this.disposed || this.controller?.isClosed()) { await pixels.release(); return; }
      const previous: image.PixelMap | undefined = this.pixels;
      this.pixels = pixels;
      // ArkUI holds its own reference after an Image source update.
      if (previous !== undefined) { await previous.release(); }
      this.input?.sync(this.x, this.y, this.pixelScale(), true);
    } catch (error) { this.reportFailure(error as Error); }
    finally { this.painting = false; }
  }
  private reportFailure(error: Error): void {
    if (!this.failureReported) {
      this.failureReported = true;
      console.error('OneUI application view failed', error.message);
    }
    this.onFailure(error);
  }
  private touch(event: TouchEvent): void {
    if (this.disposed || !this.foreground || this.controller?.isClosed()) { return; }
    if (event.type === TouchType.Cancel) { native.cancelPointers(this.host()); return; }
    const phase: number = event.type === TouchType.Down ? 0 : event.type === TouchType.Move ? 1 : event.type === TouchType.Up ? 2 : 3;
    const scale: number = this.pixelScale();
    event.changedTouches.forEach((point: TouchObject): void => {
      native.pointer(this.host(), point.id, phase, 0, point.x * scale, point.y * scale, phase === 2 || phase === 3 ? 0 : 1);
    });
    this.input?.sync(this.x, this.y, scale, true);
  }
  aboutToDisappear(): void {
    this.disposed = true;
    if (this.timer >= 0) { clearInterval(this.timer); this.timer = -1; }
    if (!this.controller?.isClosed()) { native.lifecycle(this.host(), 3); }
    const input: OneUiInputMethod | undefined = this.input; this.input = undefined;
    if (input !== undefined) { this.retireInput(input); }
    const previous: image.PixelMap | undefined = this.pixels; this.pixels = undefined;
    if (previous !== undefined) { previous.release().catch(this.onFailure); }
    // Controller lifetime belongs to the Ability. Removing a view never stops
    // an established VPN or closes business calls that still own OS resources.
  }
  private async retireInput(input: OneUiInputMethod): Promise<void> {
    try { await input.close(); }
    catch (error) { this.onFailure(error as Error); setTimeout(() => this.retireInput(input), 1000); }
  }
  build() {
    Stack() {
      Column() {
        Blank().layoutWeight(1).backgroundColor(this.statusColor)
          .expandSafeArea([SafeAreaType.SYSTEM, SafeAreaType.CUTOUT], [SafeAreaEdge.TOP, SafeAreaEdge.START, SafeAreaEdge.END])
        Blank().layoutWeight(1).backgroundColor(this.navigationColor)
          .expandSafeArea([SafeAreaType.SYSTEM, SafeAreaType.CUTOUT], [SafeAreaEdge.BOTTOM, SafeAreaEdge.START, SafeAreaEdge.END])
      }.width('100%').height('100%').hitTestBehavior(HitTestMode.None)
      if (this.pixels !== undefined) { Image(this.pixels).width('100%').height('100%').objectFit(ImageFit.Fill).draggable(false) }
    }
    .width('100%').height('100%').focusable(true).defaultFocus(true)
    .onTouch((event: TouchEvent): void => this.touch(event))
    .onAreaChange((_old: Area, area: Area): void => {
      this.viewportWidth = Number(area.width); this.viewportHeight = Number(area.height);
      this.x = this.getUIContext().vp2px(Number(area.globalPosition.x));
      this.y = this.getUIContext().vp2px(Number(area.globalPosition.y)); this.viewportChanged();
    })
  }
}
