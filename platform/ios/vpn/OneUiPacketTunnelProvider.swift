import Foundation
import NetworkExtension

/// Implemented by a packet engine binding. Use public packetFlow read/write
/// APIs; extracting private utun descriptors via KVC is intentionally avoided.
public protocol OneUiPacketEngine: AnyObject {
    func start(flow: NEPacketTunnelFlow, configuration: OneUiVpnConfiguration,
               failure: @escaping (Error) -> Void,
               completion: @escaping (Error?) -> Void)
    /// A nil result confirms release of callbacks, timers and connections.
    /// Retain ownership after a failure so a later stop can retry cleanup.
    func stop(completion: @escaping (Error?) -> Void)
}

open class OneUiPacketTunnelProvider: NEPacketTunnelProvider {
    private let owner = DispatchQueue(label: "net.too1.oneui.vpn.provider")
    private let messages = DispatchQueue(label: "net.too1.oneui.vpn.messages")
    private var engine: OneUiPacketEngine?
    private var generation: UInt64 = 0
    private var startCompletion: ((Error?) -> Void)?
    private var cleaning = false
    private var cleanupWaiters: [(Error?) -> Void] = []
    private var stopWaiters: [() -> Void] = []
    private var stopping = false
    private lazy var settings = OneUiTunnelSettingsQueue { [weak self] value, completion in
        guard let provider = self else { completion(CocoaError(.userCancelled)); return }
        provider.setTunnelNetworkSettings(value, completionHandler: completion)
    }

    public final func nativeEngine(protocolEngine: OneUiNativeVpnProtocol,
                                   storage: OneUiSecureBlobStore) -> OneUiNativePacketEngine {
        let attempt = generation // makeEngine is invoked on the provider owner queue.
        return OneUiNativePacketEngine(protocolEngine: protocolEngine, storage: storage) { [weak self] value, completion in
            guard let provider = self else { completion(CocoaError(.userCancelled)); return }
            provider.owner.async {
                guard provider.generation == attempt, provider.engine != nil else { completion(CocoaError(.userCancelled)); return }
                provider.settings.set(value?.networkSettings(), completion: completion)
            }
        }
    }

    /// A product subclass supplies protocol/control integration only.
    open func makeEngine() throws -> OneUiPacketEngine {
        throw NSError(domain: "OneUiVpn", code: 1,
                      userInfo: [NSLocalizedDescriptionKey: "Packet engine is required"])
    }
    /// Product message schema; the SDK owns IPC bounds and worker delivery.
    open func message(_ data: Data) throws -> Data { throw CocoaError(.coderInvalidValue) }
    override public func handleAppMessage(_ messageData: Data, completionHandler: ((Data?) -> Void)? = nil) {
        guard messageData.count <= 1024 * 1024 else { completionHandler?(nil); return }
        messages.async {
            let response = try? self.message(messageData)
            completionHandler?(response.flatMap { $0.count <= 1024 * 1024 ? $0 : nil })
        }
    }
    override public func startTunnel(options: [String: NSObject]?,
                                     completionHandler: @escaping (Error?) -> Void) {
        owner.async {
            guard self.startCompletion == nil, self.engine == nil, !self.cleaning, !self.stopping else {
                completionHandler(NSError(domain: "OneUiVpn", code: 2)); return
            }
            self.generation &+= 1
            let attempt = self.generation
            self.startCompletion = completionHandler
            do {
                guard let configuration = (self.protocolConfiguration as? NETunnelProviderProtocol)?
                        .providerConfiguration?["oneuiVpnConfiguration"] as? String else {
                    throw CocoaError(.coderReadCorrupt)
                }
                let config = try OneUiVpnConfiguration.decode(configuration)
                self.engine = try self.makeEngine()
                self.settings.set(config.networkSettings()) { error in
                    self.owner.async {
                        guard self.generation == attempt, self.startCompletion != nil else { return }
                        if let error = error { self.failStart(error); return }
                        self.engine?.start(flow: self.packetFlow, configuration: config,
                            failure: { [weak self] error in
                                self?.owner.async {
                                    guard let self = self, self.generation == attempt else { return }
                                    if self.startCompletion != nil { self.failStart(error); return }
                                    self.generation &+= 1
                                    self.cleanEngine { cleanupError in self.cancelTunnelWithError(cleanupError ?? error) }
                                }
                            }, completion: { error in
                            self.owner.async {
                                guard self.generation == attempt, self.startCompletion != nil else { return }
                                if let error = error { self.failStart(error); return }
                                let done = self.startCompletion
                                self.startCompletion = nil
                                done?(nil) // Only the engine can acknowledge readiness.
                            }
                        })
                    }
                }
            } catch { self.failStart(error) }
        }
    }
    private func failStart(_ error: Error) {
        generation &+= 1
        let done = startCompletion
        startCompletion = nil
        cleanEngine { cleanupError in
            done?(error)
            if let cleanupError = cleanupError { self.cancelTunnelWithError(cleanupError) }
        }
    }
    private func cleanEngine(completion: @escaping (Error?) -> Void) {
        cleanupWaiters.append(completion)
        guard !cleaning else { return }
        cleaning = true
        let previous = engine
        let finish: (Error?) -> Void = { engineError in
            self.owner.async {
                if let error = engineError { self.finishCleanup(error); return }
                self.settings.set(nil) { settingsError in
                    self.owner.async {
                        if settingsError == nil { self.engine = nil }
                        self.finishCleanup(settingsError)
                    }
                }
            }
        }
        if let previous = previous { previous.stop(completion: finish) }
        else { finish(nil) }
    }
    private func finishCleanup(_ error: Error?) {
        cleaning = false
        let waiters = cleanupWaiters
        cleanupWaiters.removeAll()
        waiters.forEach { $0(error) }
    }
    private func retryStop() {
        cleanEngine { error in
            if let error = error {
                self.cancelTunnelWithError(error)
                self.owner.asyncAfter(deadline: .now() + 1) { self.retryStop() }
                return
            }
            self.stopping = false
            let waiters = self.stopWaiters
            self.stopWaiters.removeAll()
            waiters.forEach { $0() }
        }
    }
    override public func stopTunnel(with reason: NEProviderStopReason,
                                    completionHandler: @escaping () -> Void) {
        owner.async {
            self.generation &+= 1
            let pending = self.startCompletion
            self.startCompletion = nil
            pending?(NSError(domain: NSCocoaErrorDomain, code: NSUserCancelledError))
            self.stopWaiters.append(completionHandler)
            guard !self.stopping else { return }
            self.stopping = true
            self.retryStop()
        }
    }
}
