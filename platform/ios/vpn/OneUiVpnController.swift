import Foundation
import NetworkExtension

/// Main-thread configuration/consent bridge. One product-owned provider bundle
/// is managed; configurations belonging to other VPN products are untouched.
public final class OneUiVpnController {
    public typealias Events = (UInt64, Int32, Int32) -> Void
    private let providerBundleID: String
    private let events: Events
    private var manager: NETunnelProviderManager?
    private var observer: NSObjectProtocol?
    private var attempt: UInt64 = 0
    private var preparing = false
    private var cancelled = false
    private var issuedStart = false
    private var reportedConnected = false

    public init(providerBundleID: String, events: @escaping Events) {
        self.providerBundleID = providerBundleID
        self.events = events
    }
    public func start(attempt: UInt64, configurationJSON: String, onlyExisting: Bool = false) {
        dispatchPrecondition(condition: .onQueue(.main))
        guard attempt > 0, !preparing, !issuedStart else { return }
        self.attempt = attempt
        cancelled = false; preparing = true; reportedConnected = false
        do { _ = try OneUiVpnConfiguration.decode(configurationJSON) }
        catch { fail(1); return }
        NETunnelProviderManager.loadAllFromPreferences { managers, error in
            DispatchQueue.main.async {
                if let error = error { self.fail(Int32((error as NSError).code)); return }
                if self.cancelled { self.finishCancelled(); return }
                let matches = (managers ?? []).filter {
                    ($0.protocolConfiguration as? NETunnelProviderProtocol)?.providerBundleIdentifier == self.providerBundleID
                }
                guard matches.count <= 1 else { self.fail(3); return }
                let manager = matches.first ?? NETunnelProviderManager()
                let status = manager.connection.status
                if (status == .connected || status == .connecting || status == .reasserting),
                   (manager.protocolConfiguration as? NETunnelProviderProtocol)?.providerConfiguration?["oneuiVpnConfiguration"] as? String == configurationJSON {
                    self.manager = manager; self.preparing = false; self.issuedStart = true
                    self.observe(manager); self.events(attempt, 1, 0); self.changed(); return
                }
                // Restoring a surviving extension must never silently restart it.
                if onlyExisting { self.fail(3); return }
                guard manager.connection.status == .disconnected || manager.connection.status == .invalid else {
                    self.fail(3); return
                }
                let proto = NETunnelProviderProtocol()
                proto.providerBundleIdentifier = self.providerBundleID
                proto.serverAddress = "OneUI VPN"
                proto.providerConfiguration = ["oneuiVpnConfiguration": configurationJSON]
                manager.protocolConfiguration = proto
                manager.localizedDescription = (try? OneUiVpnConfiguration.decode(configurationJSON).label) ?? "VPN"
                manager.isEnabled = true
                // No always-on/remote-wakeup promise is enabled implicitly.
                manager.isOnDemandEnabled = false
                self.manager = manager
                manager.saveToPreferences { error in
                    DispatchQueue.main.async {
                        if let error = error { self.fail(Int32((error as NSError).code)); return }
                        manager.loadFromPreferences { error in
                            DispatchQueue.main.async {
                                if let error = error { self.fail(Int32((error as NSError).code)); return }
                                if self.cancelled { self.finishCancelled(); return }
                                self.events(attempt, 1, 0)
                                self.preparing = false
                                if self.cancelled { self.finishCancelled(); return }
                                self.observe(manager)
                                do {
                                    self.issuedStart = true
                                    try manager.connection.startVPNTunnel()
                                } catch { self.fail(Int32((error as NSError).code)) }
                            }
                        }
                    }
                }
            }
        }
    }
    public func stop(attempt: UInt64) {
        dispatchPrecondition(condition: .onQueue(.main))
        guard attempt == self.attempt else { return }
        cancelled = true
        if preparing { return } // Wait for the configuration operation to finish.
        if issuedStart { manager?.connection.stopVPNTunnel() }
        else { finishCancelled() }
    }
    public func existingConfiguration(_ completion: @escaping (String?) -> Void) {
        dispatchPrecondition(condition: .onQueue(.main))
        NETunnelProviderManager.loadAllFromPreferences { managers, error in
            DispatchQueue.main.async {
                let matches = (managers ?? []).filter { ($0.protocolConfiguration as? NETunnelProviderProtocol)?.providerBundleIdentifier == self.providerBundleID }
                guard error == nil, matches.count == 1, let manager = matches.first,
                    manager.connection.status == .connected || manager.connection.status == .connecting || manager.connection.status == .reasserting,
                    let json = (manager.protocolConfiguration as? NETunnelProviderProtocol)?.providerConfiguration?["oneuiVpnConfiguration"] as? String,
                    (try? OneUiVpnConfiguration.decode(json)) != nil else { completion(nil); return }
                completion(json)
            }
        }
    }
    private func observe(_ manager: NETunnelProviderManager) {
        if let previous = observer { NotificationCenter.default.removeObserver(previous) }
        observer = NotificationCenter.default.addObserver(forName: .NEVPNStatusDidChange,
            object: manager.connection, queue: .main) { [weak self] _ in self?.changed() }
    }
    private func changed() {
        guard issuedStart, let manager = manager else { return }
        switch manager.connection.status {
        case .connected:
            if cancelled { manager.connection.stopVPNTunnel(); return }
            if !reportedConnected {
                reportedConnected = true
                // The shared provider completes startTunnel only after the
                // settings and packet engine both acknowledge readiness.
                events(attempt, 3, 0); events(attempt, 4, 0)
            }
        case .disconnected, .invalid:
            issuedStart = false
            events(attempt, 6, cancelled ? 0 : 1)
        default: break
        }
    }
    private func fail(_ code: Int32) {
        preparing = false; issuedStart = false; cancelled = true
        events(attempt, 5, code)
        events(attempt, 6, 0)
    }
    private func finishCancelled() {
        preparing = false; issuedStart = false
        events(attempt, 6, 0)
    }
    public func message(_ request: Data, completion: @escaping (Result<Data, Error>) -> Void) {
        dispatchPrecondition(condition: .onQueue(.main))
        guard request.count <= 1024 * 1024, let connection = manager?.connection as? NETunnelProviderSession,
              connection.status == .connected else { completion(.failure(CocoaError(.userCancelled))); return }
        var delivered = false
        let finish: (Result<Data, Error>) -> Void = { result in
            dispatchPrecondition(condition: .onQueue(.main))
            if !delivered { delivered = true; completion(result) }
        }
        do {
            try connection.sendProviderMessage(request) { response in
                DispatchQueue.main.async {
                    guard let response = response, response.count <= 1024 * 1024 else { finish(.failure(CocoaError(.coderReadCorrupt))); return }
                    finish(.success(response))
                }
            }
            DispatchQueue.main.asyncAfter(deadline: .now() + 10) { finish(.failure(CocoaError(.userCancelled))) }
        } catch { finish(.failure(error)) }
    }
    deinit { if let observer = observer { NotificationCenter.default.removeObserver(observer) } }
}
