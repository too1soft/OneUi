import Foundation

private final class OneUiVpnHostBox {
    let start: (UInt64, String) -> Void
    let stop: (UInt64) -> Void
    init(start: @escaping (UInt64, String) -> Void, stop: @escaping (UInt64) -> Void) { self.start = start; self.stop = stop }
}
/// Main-thread binding of the portable C++ state machine; no duplicate Swift states.
public final class OneUiVpnSession {
    public struct State { public let state: Int32, error: Int32; public let attempt: UInt64, revision: UInt64 }
    private var session: OpaquePointer?
    private let context: UnsafeMutableRawPointer
    public init(start: @escaping (UInt64, String) -> Void, stop: @escaping (UInt64) -> Void) throws {
        dispatchPrecondition(condition: .onQueue(.main))
        context = Unmanaged.passRetained(OneUiVpnHostBox(start: start, stop: stop)).toOpaque()
        var host = OneUiVpnHostV1()
        host.struct_size = UInt32(MemoryLayout<OneUiVpnHostV1>.size); host.context = context
        host.start = { context, attempt, json in
            guard let context = context, let json = json else { return }
            Unmanaged<OneUiVpnHostBox>.fromOpaque(context).takeUnretainedValue().start(attempt, String(cString: json))
        }
        host.stop = { context, attempt in
            guard let context = context else { return }
            Unmanaged<OneUiVpnHostBox>.fromOpaque(context).takeUnretainedValue().stop(attempt)
        }
        guard oneui_vpn_create_v1(&host, &session) == ONEUI_VPN_OK else {
            Unmanaged<OneUiVpnHostBox>.fromOpaque(context).release(); throw CocoaError(.coderInvalidValue)
        }
    }
    public func connect(configuration: String) throws {
        dispatchPrecondition(condition: .onQueue(.main))
        let result = configuration.utf8CString.withUnsafeBufferPointer { oneui_vpn_connect_json_v1(session, $0.baseAddress, $0.count - 1) }
        guard result == ONEUI_VPN_OK else { throw NSError(domain: "OneUiVpn.Session", code: Int(result.rawValue)) }
    }
    public func disconnect() throws {
        dispatchPrecondition(condition: .onQueue(.main))
        let result = oneui_vpn_disconnect(session)
        guard result == ONEUI_VPN_OK else { throw NSError(domain: "OneUiVpn.Session", code: Int(result.rawValue)) }
    }
    @discardableResult public func report(attempt: UInt64, event: Int32, code: Int32) -> Bool {
        dispatchPrecondition(condition: .onQueue(.main))
        return oneui_vpn_report_v1(session, attempt, event, code) == ONEUI_VPN_OK
    }
    public func snapshot() -> State {
        dispatchPrecondition(condition: .onQueue(.main))
        var value = OneUiVpnSnapshotV1(); value.struct_size = UInt32(MemoryLayout<OneUiVpnSnapshotV1>.size)
        guard oneui_vpn_snapshot_v1(session, &value) == ONEUI_VPN_OK else { return State(state: 6, error: 3, attempt: 0, revision: 0) }
        return State(state: value.state, error: value.error_code, attempt: value.attempt, revision: value.revision)
    }
    /// A manager owns this object for the application's lifetime. Explicit close
    /// succeeds only after the OS cleanup acknowledgement, on the main thread.
    public func close() throws {
        dispatchPrecondition(condition: .onQueue(.main))
        guard let value = session else { return }
        let result = oneui_vpn_destroy(value)
        guard result == ONEUI_VPN_OK else { throw NSError(domain: "OneUiVpn.Session", code: Int(result.rawValue)) }
        session = nil; Unmanaged<OneUiVpnHostBox>.fromOpaque(context).release()
    }
    deinit {
        if let value = session, Thread.isMainThread, oneui_vpn_destroy(value) == ONEUI_VPN_OK {
            Unmanaged<OneUiVpnHostBox>.fromOpaque(context).release()
        }
    }
}
