export interface OneUiApplicationFrame { width: number; height: number; rgba: Uint8Array; }
export interface OneUiSystemBars {
  revision: string;
  themeMode: number;
  statusColor: number;
  navigationColor: number;
  lightStatusIcons: boolean;
  lightNavigationIcons: boolean;
}
export interface OneUiApplicationRequest { id: string; action: string; payload: Uint8Array; }
export interface OneUiEditingState {
  session: string; text: string; anchor: number; caret: number;
  composingStart: number; composingEnd: number; multiline: boolean; sensitive: boolean;
}
export interface OneUiCaretGeometry { session: string; caret: OneUiCaretRect; }
export interface OneUiCaretRect { left: number; top: number; width: number; height: number; }
declare const native: {
  create(packagedWidgetLibrary: string): string;
  viewport(host: string, widthPx: number, heightPx: number, scale: number,
    safeLeftPx: number, safeTopPx: number, safeRightPx: number, safeBottomPx: number, keyboardBottomPx: number): boolean;
  frame(host: string): OneUiApplicationFrame | undefined;
  pointer(host: string, id: number, phase: number, kind: number, xPx: number, yPx: number, pressure: number): number;
  cancelPointers(host: string): number;
  lifecycle(host: string, state: number): number;
  theme(host: string, appearance: number, fontScale: number, bodySize: number): void;
  systemBars(host: string): OneUiSystemBars;
  acknowledgeSystemBars(host: string, revision: string): boolean;
  take(host: string): OneUiApplicationRequest | undefined;
  reply(host: string, request: string, code: number, payload: Uint8Array): boolean;
  editing(host: string): OneUiEditingState | undefined;
  edit(host: string, session: string, text: string, anchor: number, caret: number, composingStart: number, composingEnd: number): boolean;
  sensitiveEdit(host: string, session: string, command: number, text: string, first: number, second: number): boolean;
  commandEdit(host: string, session: string, command: number, text: string, first: number, second: number): boolean;
  geometry(host: string): OneUiCaretGeometry | undefined;
  back(host: string): boolean;
  destroy(host: string): void;
};
export default native;
