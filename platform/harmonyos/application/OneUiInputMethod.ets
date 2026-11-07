import { inputMethod } from '@kit.IMEKit';
import { BusinessError } from '@kit.BasicServicesKit';
import native, { OneUiEditingState, OneUiCaretGeometry } from 'liboneui_application.so';

/** Public self-drawing editor API. UTF-16 commands and secret contents remain
 * in the shared C++ editor. Password snapshots sent to the IME contain bullets.
 * Attach/detach are serialized, including a late attach after backgrounding. */
export class OneUiInputMethod {
  private controller: inputMethod.InputMethodController = inputMethod.getController();
  private host: string;
  private desired: OneUiEditingState | undefined = undefined;
  private attached: string = '';
  private listening: boolean = false;
  private revision: number = 0;
  private applied: number = -1;
  private working: Promise<void> | undefined = undefined;
  private closed: boolean = false;
  private lastError: Error | undefined = undefined;
  private x: number = 0;
  private y: number = 0;
  private scale: number = 1;
  private failed: (error: Error) => void;
  private insert = (text: string): void => { this.command(0, text, 1, 0); };
  private deleteLeft = (length: number): void => { this.erase(length, 0); };
  private deleteRight = (length: number): void => { this.erase(0, length); };
  private select = (range: inputMethod.Range): void => { this.command(2, '', range.start, range.end); };
  private preview = (text: string, range: inputMethod.Range): void => {
    if (range.start >= 0 && range.end >= 0 && !this.command(3, '', range.start, range.end)) { return; }
    this.command(1, text, 1, 0);
  };
  private finish = (): void => { this.command(4, '', 0, 0); };
  private move = (direction: inputMethod.Direction): void => {
    if (direction === inputMethod.Direction.CURSOR_LEFT || direction === inputMethod.Direction.CURSOR_RIGHT) {
      this.command(7, '', direction === inputMethod.Direction.CURSOR_LEFT ? -1 : 1, 0);
    }
  };
  private functionKey = (_key: inputMethod.FunctionKey): void => {
    const state: OneUiEditingState | undefined = this.current();
    if (state?.multiline) { this.insert('\n'); }
    else { this.finish(); this.controller.hideTextInput().catch(this.failed); }
  };
  private left = (length: number): string => {
    const state: OneUiEditingState | undefined = this.current();
    if (state === undefined) { return ''; }
    return state.text.substring(Math.max(0, Math.min(state.anchor, state.caret) - Math.max(0, length)), Math.min(state.anchor, state.caret));
  };
  private right = (length: number): string => {
    const state: OneUiEditingState | undefined = this.current();
    if (state === undefined) { return ''; }
    const end: number = Math.max(state.anchor, state.caret);
    return state.text.substring(end, end + Math.max(0, length));
  };
  private index = (): number => { return this.current()?.caret ?? 0; };
  private extend = (action: inputMethod.ExtendAction): void => {
    const state: OneUiEditingState | undefined = this.current();
    if (action === inputMethod.ExtendAction.SELECT_ALL && state !== undefined) { this.command(2, '', 0, state.text.length); }
  };
  constructor(host: string, failed: (error: Error) => void) {
    this.host = host; this.failed = failed;
  }
  private addListeners(): void {
    this.listening = true;
    this.controller.on('insertText', this.insert);
    this.controller.on('deleteLeft', this.deleteLeft); this.controller.on('deleteRight', this.deleteRight);
    this.controller.on('selectByRange', this.select); this.controller.on('moveCursor', this.move);
    this.controller.on('sendFunctionKey', this.functionKey); this.controller.on('handleExtendAction', this.extend);
    this.controller.on('getLeftTextOfCursor', this.left); this.controller.on('getRightTextOfCursor', this.right);
    this.controller.on('getTextIndexAtCursor', this.index);
    this.controller.on('setPreviewText', this.preview); this.controller.on('finishTextPreview', this.finish);
  }
  private current(): OneUiEditingState | undefined {
    if (this.closed || this.attached === '') { return undefined; }
    try {
      const state: OneUiEditingState | undefined = native.editing(this.host);
      return state?.session === this.attached ? state : undefined;
    } catch (_) { return undefined; } // The owning Ability may already have destroyed its Widget host.
  }
  private command(operation: number, text: string, first: number, second: number): boolean {
    const state: OneUiEditingState | undefined = this.current();
    if (state === undefined || !native.commandEdit(this.host, state.session, operation, text, first, second)) { return false; }
    this.sync(this.x, this.y, this.scale); return true;
  }
  private erase(before: number, after: number): void {
    const state: OneUiEditingState | undefined = this.current();
    if (state === undefined || !Number.isSafeInteger(before) || !Number.isSafeInteger(after) || before < 0 || after < 0) { return; }
    // Deleting a selection first avoids deleting additional surrounding text.
    if (state.anchor !== state.caret) { this.command(0, '', 1, 0); }
    else { this.command(5, '', before, after); }
  }
  sync(xPx: number, yPx: number, scale: number, force: boolean = false): void {
    if (this.closed) { return; }
    let next: OneUiEditingState | undefined = undefined;
    try { next = native.editing(this.host); } catch (_) {}
    const old: OneUiEditingState | undefined = this.desired;
    if (!force && old?.session === next?.session && old?.text === next?.text && old?.anchor === next?.anchor && old?.caret === next?.caret &&
      this.x === xPx && this.y === yPx && this.scale === scale) { return; }
    this.desired = next; this.x = xPx; this.y = yPx; this.scale = scale; ++this.revision;
    this.schedule();
  }
  private schedule(): void {
    if (this.working !== undefined) { return; }
    this.lastError = undefined;
    this.working = this.update().catch((error: Error) => {
      this.applied = this.revision; this.lastError = error; this.failed(error);
    }).finally(() => {
      this.working = undefined;
      if (this.applied !== this.revision) { this.schedule(); }
    });
  }
  private async update(): Promise<void> {
    while (this.applied !== this.revision) {
      const revision: number = this.revision;
      const desired: OneUiEditingState | undefined = this.desired;
      if (this.attached !== (desired?.session ?? '')) {
        if (this.attached !== '') { await this.detach(); }
        if (desired !== undefined && !this.closed) {
          const config: inputMethod.TextConfig = { inputAttribute: {
            textInputType: desired.sensitive ? inputMethod.TextInputType.NEW_PASSWORD :
              desired.multiline ? inputMethod.TextInputType.MULTILINE : inputMethod.TextInputType.TEXT,
            enterKeyType: desired.multiline ? inputMethod.EnterKeyType.NEWLINE : inputMethod.EnterKeyType.DONE
          }, selection: { start: Math.min(desired.anchor, desired.caret), end: Math.max(desired.anchor, desired.caret) } };
          await this.controller.attach(false, config); this.attached = desired.session;
        }
      }
      if (revision !== this.revision) { continue; }
      if (desired !== undefined && !this.closed) {
        if (!this.listening) {
          try {
            this.addListeners();
            await this.controller.showTextInput();
          } catch (error) {
            await this.detach();
            throw error as Error;
          }
        }
        await this.controller.changeSelection(desired.text, Math.min(desired.anchor, desired.caret), Math.max(desired.anchor, desired.caret));
        if (revision !== this.revision) { continue; }
        const geometry: OneUiCaretGeometry | undefined = native.geometry(this.host);
        if (geometry?.session === desired.session) {
          const caret = geometry.caret;
          await this.controller.updateCursor({ left: Math.max(0, Math.round(this.x + caret.left * this.scale)),
            top: Math.max(0, Math.round(this.y + caret.top * this.scale)), width: Math.max(1, Math.round(caret.width * this.scale)),
            height: Math.max(1, Math.round(caret.height * this.scale)) });
        }
      }
      this.applied = revision;
    }
  }
  async close(): Promise<void> {
    this.closed = true; this.desired = undefined; ++this.revision;
    this.schedule(); await this.working;
    if (this.lastError !== undefined) { throw this.lastError; }
  }
  private async detach(): Promise<void> {
    try {
      if (this.listening) {
        this.controller.off('insertText', this.insert);
        this.controller.off('deleteLeft', this.deleteLeft); this.controller.off('deleteRight', this.deleteRight);
        this.controller.off('selectByRange', this.select); this.controller.off('moveCursor', this.move);
        this.controller.off('sendFunctionKey', this.functionKey); this.controller.off('handleExtendAction', this.extend);
        this.controller.off('getLeftTextOfCursor', this.left); this.controller.off('getRightTextOfCursor', this.right);
        this.controller.off('getTextIndexAtCursor', this.index);
        this.controller.off('setPreviewText', this.preview); this.controller.off('finishTextPreview', this.finish);
        this.listening = false;
      }
      await this.controller.detach();
    } catch (error) {
      if ((error as BusinessError).code !== 12800009) { throw error as Error; }
      this.listening = false;
    }
    this.attached = '';
  }
}
