/* Interactive documentation only. Native production code uses OneUI::vpn.
 * This single reusable simulation also drives consuming product prototypes. */
class OneUiVpnDemoSession {
  constructor(changed) {
    this.changed = changed;
    this.state = "idle";
    this.attempt = 0;
    this.cleanupPending = false;
    this.error = "";
    this.timers = new Set();
  }
  emit() {
    this.changed(this);
  }
  later(delay, callback) {
    const attempt = this.attempt;
    const id = setTimeout(() => {
      this.timers.delete(id);
      if (attempt === this.attempt) callback();
    }, delay);
    this.timers.add(id);
  }
  clear() {
    this.timers.forEach(clearTimeout);
    this.timers.clear();
  }
  start() {
    if (this.cleanupPending || !["idle", "error"].includes(this.state))
      return false;
    this.clear();
    this.attempt++;
    this.error = "";
    this.cleanupPending = true;
    this.state = "authorizing";
    this.emit();
    return true;
  }
  authorize(allowed, engineFailure = false) {
    if (this.state !== "authorizing") return;
    if (!allowed) {
      this.fail("未获得 VPN 授权，请重试并允许添加连接。");
      return;
    }
    this.state = "establishing";
    this.emit();
    this.later(250, () => {
      if (this.state !== "establishing") return;
      this.state = "tunnel-ready";
      this.emit();
      this.later(450, () => {
        if (this.state !== "tunnel-ready") return;
        if (engineFailure) this.fail("暂时无法连接网络，请检查网络后重试。");
        else {
          this.state = "connected";
          this.emit();
        }
      });
    });
  }
  fail(message) {
    this.clear();
    this.error = message;
    this.state = "error";
    this.emit();
    this.later(200, () => {
      this.cleanupPending = false;
      this.emit();
    });
  }
  stop() {
    if (!this.cleanupPending) return;
    this.clear();
    this.error = "";
    this.state = "stopping";
    this.emit();
    this.later(200, () => {
      this.cleanupPending = false;
      this.state = "idle";
      this.emit();
    });
  }
  revoke() {
    if (this.cleanupPending) this.fail("VPN 授权已被撤销，请重新连接并授权。");
  }
}
