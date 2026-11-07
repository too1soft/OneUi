export interface OneUiTunnelDriver {
  create(configuration: string): Promise<number>;
  destroy(): Promise<void>;
}
export interface OneUiTunnelUpdate {
  accepted: boolean;
  fd: number;
  fatal: boolean;
}

/** Serial OS ownership and rollback. A failed replacement either restores the
 * acknowledged configuration or explicitly reports a fatal loss of the TUN.
 * A create which completes after cancellation is destroyed before close ends. */
export class OneUiPacketTunnel {
  private driver: OneUiTunnelDriver;
  private current: string | undefined = undefined;
  private fd: number = -1;
  private serial: Promise<void> = Promise.resolve();
  private pending: number = 0;
  constructor(driver: OneUiTunnelDriver) { this.driver = driver; }
  change(configuration: string | undefined, beforeReplace: () => void, active: () => boolean): Promise<OneUiTunnelUpdate> {
    if (this.pending >= 16 || (configuration !== undefined && configuration.length > 65536)) {
      return Promise.reject(new Error('VPN settings queue capacity'));
    }
    ++this.pending;
    const result: Promise<OneUiTunnelUpdate> = this.serial.then(() => this.apply(configuration, beforeReplace, active));
    this.serial = result.then(() => {}, () => {});
    return result.finally(() => { --this.pending; });
  }
  private async apply(configuration: string | undefined, beforeReplace: () => void, active: () => boolean): Promise<OneUiTunnelUpdate> {
    if (configuration !== undefined && !active()) { return { accepted: false, fd: this.fd, fatal: false }; }
    const previous: string | undefined = this.current;
    beforeReplace();
    if (this.fd >= 0) {
      try { await this.driver.destroy(); }
      catch (_) { return { accepted: false, fd: this.fd, fatal: false }; }
      this.fd = -1; this.current = undefined;
    }
    if (configuration === undefined) { return { accepted: true, fd: -1, fatal: false }; }
    try {
      const fd: number = await this.driver.create(configuration);
      if (!Number.isInteger(fd) || fd < 0) { throw new Error('VPN packet descriptor'); }
      this.fd = fd; this.current = configuration;
    } catch (_) {
      if (previous !== undefined && active()) {
        try {
          const restored: number = await this.driver.create(previous);
          if (!Number.isInteger(restored) || restored < 0) { throw new Error('VPN rollback descriptor'); }
          this.fd = restored; this.current = previous;
          return { accepted: false, fd: restored, fatal: false };
        } catch (_) { return { accepted: false, fd: -1, fatal: true }; }
      }
      return { accepted: false, fd: -1, fatal: active() };
    }
    if (!active()) {
      try { await this.driver.destroy(); this.fd = -1; this.current = undefined; }
      catch (_) { return { accepted: false, fd: this.fd, fatal: true }; }
      return { accepted: false, fd: -1, fatal: false };
    }
    return { accepted: true, fd: this.fd, fatal: false };
  }
  async close(): Promise<void> {
    const result: OneUiTunnelUpdate = await this.change(undefined, () => {}, () => true);
    if (!result.accepted) { throw new Error('VPN system cleanup pending'); }
  }
}
