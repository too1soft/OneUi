import { OneUiApplicationController } from './OneUiApplicationController';
import { OneUiApplicationView } from './OneUiApplicationView';

@Component
export struct OneUiApplicationPage {
  @LocalStorageProp('oneui.controller') private controllerId: string = '';
  @LocalStorageLink('oneui.foreground') private foreground: boolean = true;
  @LocalStorageLink('oneui.appearance') private appearance: number = 0;
  @LocalStorageLink('oneui.fontScale') private fontScale: number = 1;
  build() {
    if (this.controllerId !== '') {
      OneUiApplicationView({ controller: OneUiApplicationController.find(this.controllerId), foreground: this.foreground, appearance: this.appearance, fontScale: this.fontScale })
    }
  }
}
