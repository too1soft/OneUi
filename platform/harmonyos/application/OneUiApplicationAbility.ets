import { UIAbility, Configuration, ConfigurationConstant } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { BusinessError } from '@kit.BasicServicesKit';
import { resourceManager } from '@kit.LocalizationKit';
import { OneUiSystemBars } from 'liboneui_application.so';
import { OneUiApplicationController } from './OneUiApplicationController';

/** Generic Stage application/window lifetime. A product supplies the Widget
 * library, opaque services and its page entry; it does not implement a renderer. */
export default class OneUiApplicationAbility extends UIAbility {
  private storage: LocalStorage = new LocalStorage();
  private application: OneUiApplicationController | undefined = undefined;
  private mainWindow: window.Window | undefined = undefined;
  private barUpdates: Promise<void> = Promise.resolve();
  private abilityThemeMode: number = 0;
  private foreground: boolean = true;
  private destroyed: boolean = false;
  protected createApplication(): OneUiApplicationController { throw new Error('Application factory required'); }
  protected closeServices(): Promise<void> { return Promise.resolve(); }
  protected page(): string { return 'pages/Index'; }
  onWindowStageCreate(stage: window.WindowStage): void {
    if (this.destroyed) { return; }
    if (this.application === undefined) { this.application = this.createApplication(); }
    this.application.setUrlHandler((url: string): Promise<void> => this.context.openLink(url, { appLinkingOnly: false }));
    this.storage.setOrCreate('oneui.controller', this.application.host);
    this.storage.setOrCreate('oneui.foreground', this.foreground);
    this.configuration(this.context.config);
    this.application.setSystemBarHandler((bars: OneUiSystemBars): Promise<void> => this.updateSystemBars(bars));
    this.showPage(stage);
  }
  private async showPage(stage: window.WindowStage): Promise<void> {
    try {
      this.mainWindow = stage.getMainWindowSync();
    } catch (error) {
      console.warn('OneUI main window unavailable', (error as BusinessError).code);
    }
    if (this.destroyed) { return; }
    try {
      await stage.loadContent(this.page(), this.storage);
    } catch (error) {
      console.error('OneUI page loading failed', (error as BusinessError).code);
    }
  }
  private updateSystemBars(bars: OneUiSystemBars): Promise<void> {
    const main: window.Window | undefined = this.mainWindow;
    const pending: Promise<void> = this.barUpdates.then(async (): Promise<void> => {
      if (main === undefined || this.mainWindow !== main) { throw new Error('OneUI main window unavailable'); }
      if (bars.themeMode !== this.abilityThemeMode) {
        const mode = bars.themeMode === 2 ? ConfigurationConstant.ColorMode.COLOR_MODE_DARK :
          bars.themeMode === 1 ? ConfigurationConstant.ColorMode.COLOR_MODE_LIGHT : ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET;
        // Scope the override to this Ability. Follow-system clears it so OS
        // theme changes reach the Widget tree again.
        this.context.setColorMode(mode);
        this.abilityThemeMode = bars.themeMode;
      }
      await main.setWindowSystemBarProperties({
        statusBarColor: '#00000000', navigationBarColor: '#00000000',
        statusBarContentColor: bars.lightStatusIcons ? '#FFFFFFFF' : '#FF000000',
        navigationBarContentColor: bars.lightNavigationIcons ? '#FFFFFFFF' : '#FF000000'
      });
    });
    this.barUpdates = pending.catch(() => {});
    return pending;
  }
  private configuration(config: Configuration): void {
    let appearance: number = config.colorMode === ConfigurationConstant.ColorMode.COLOR_MODE_DARK ? 2 :
      config.colorMode === ConfigurationConstant.ColorMode.COLOR_MODE_LIGHT ? 1 : 0;
    if (this.abilityThemeMode !== 0) {
      // An Ability override must never be reported back as the system theme.
      const inherited = this.context.getApplicationContext().resourceManager.getConfigurationSync().colorMode;
      appearance = inherited === resourceManager.ColorMode.DARK ? 2 : 1;
    }
    this.storage.setOrCreate('oneui.appearance', appearance);
    this.storage.setOrCreate('oneui.fontScale', config.fontSizeScale ?? 1);
  }
  onConfigurationUpdate(config: Configuration): void { this.configuration(config); }
  onForeground(): void { this.foreground = true; this.storage.setOrCreate('oneui.foreground', true); }
  onBackground(): void { this.foreground = false; this.storage.setOrCreate('oneui.foreground', false); }
  onWindowStageDestroy(): void {
    this.mainWindow = undefined;
    this.application?.setSystemBarHandler();
    this.storage.setOrCreate('oneui.foreground', false);
  }
  onDestroy(): void {
    this.destroyed = true;
    this.application?.close(); this.application = undefined;
    // A Promise retains this owner until actual business/native cleanup ends.
    // It does not stop an established VPN owned by a different extension.
    this.releaseServices();
  }
  private async releaseServices(): Promise<void> {
    try { await this.closeServices(); }
    catch (_) { setTimeout(() => this.releaseServices(), 1000); }
  }
}
