import Foundation
import NetworkExtension
import Darwin

/// Protocol/control work is application-specific. Every native VPN, packet-flow
/// and secure-storage operation stays inside OneUI. Methods run on workers.
public protocol OneUiNativeVpnProtocol: AnyObject {
    func start(host: UInt64) throws
    func cancel()
    func close() throws
}

private final class OneUiPacketServices: OneUiNativeVpnServices {
    let apply: (Data) throws -> Void
    let storage: OneUiSecureBlobStore
    let network: OneUiNetworkState
    init(storage: OneUiSecureBlobStore, network: OneUiNetworkState, apply: @escaping (Data) throws -> Void) {
        self.storage = storage; self.network = network; self.apply = apply
    }
    func applyNetwork(_ data: Data) throws { try apply(data) }
    func secureRead(_ key: String) throws -> Data? { return try storage.read(key) }
    func secureWrite(_ key: String, data: Data) throws { try storage.write(key, data: data) }
    func networkState() throws -> Data { return try network.snapshot() }
}

public final class OneUiNativePacketEngine: OneUiPacketEngine {
    public typealias ApplySettings = (OneUiVpnConfiguration?, @escaping (Error?) -> Void) -> Void
    private let lock = NSLock()
    private let workers = DispatchGroup()
    private let input = DispatchQueue(label: "net.too1.oneui.vpn.input")
    private let output = DispatchQueue(label: "net.too1.oneui.vpn.output")
    private let control = DispatchQueue(label: "net.too1.oneui.vpn.control")
    private let cleanup = DispatchQueue(label: "net.too1.oneui.vpn.cleanup")
    private let protocolEngine: OneUiNativeVpnProtocol
    private let storage: OneUiSecureBlobStore
    private let network = OneUiNetworkState()
    private let applySettings: ApplySettings
    private var port: OneUiNativePacketPort?
    private var active = false
    private var started = false
    private var stopping = false
    private var stopped = false
    private var protocolClosed = false
    private var waiters: [(Error?) -> Void] = []
    private var failure: ((Error) -> Void)?
    private var running: Bool { lock.lock(); defer { lock.unlock() }; return active }

    public init(protocolEngine: OneUiNativeVpnProtocol, storage: OneUiSecureBlobStore,
                applySettings: @escaping ApplySettings) {
        self.protocolEngine = protocolEngine; self.storage = storage; self.applySettings = applySettings
    }
    public func start(flow: NEPacketTunnelFlow, configuration: OneUiVpnConfiguration,
                      failure: @escaping (Error) -> Void, completion: @escaping (Error?) -> Void) {
        lock.lock()
        guard !started, !stopping else { lock.unlock(); completion(CocoaError(.userCancelled)); return }
        started = true; active = true; self.failure = failure
        lock.unlock()
        do {
            let services = OneUiPacketServices(storage: storage, network: network) { [weak self] data in
                guard let engine = self else { throw CocoaError(.userCancelled) }
                try engine.reconfigure(data)
            }
            let created = try OneUiNativePacketPort(label: configuration.label, mtu: Int32(configuration.mtu),
                allowDefaultRoute: configuration.allowDefaultRoute, splitDNS: true, services: services)
            lock.lock()
            port = created
            guard active else { lock.unlock(); completion(CocoaError(.userCancelled)); return }
            network.observe { [weak created] in try? created?.networkChanged() }
            workers.enter(); workers.enter()
            lock.unlock()
            output.async {
                defer { self.workers.leave() }
                do {
                    while self.running {
                        guard let packet = try created.receive() else { continue }
                        guard self.running else { break }
                        let version = packet.first.map { $0 >> 4 } ?? 0
                        guard version == 4 || version == 6 else { throw CocoaError(.coderInvalidValue) }
                        guard flow.writePackets([packet], withProtocols: [NSNumber(value: version == 4 ? AF_INET : AF_INET6)]) else {
                            throw NSError(domain: "OneUiVpn.PacketFlow", code: 1)
                        }
                    }
                } catch { self.fail(error) }
            }
            control.async {
                defer { self.workers.leave() }
                do {
                    guard self.running else { throw CocoaError(.userCancelled) }
                    try self.protocolEngine.start(host: created.handle)
                    completion(self.running ? nil : CocoaError(.userCancelled))
                } catch { completion(error) }
            }
            readNext(flow, port: created)
        } catch { completion(error) }
    }
    private func readNext(_ flow: NEPacketTunnelFlow, port: OneUiNativePacketPort) {
        guard running else { return }
        flow.readPackets { [weak self, weak flow] packets, protocols in
            guard let engine = self, let flow = flow else { return }
            engine.lock.lock()
            guard engine.active else { engine.lock.unlock(); return }
            engine.workers.enter()
            engine.lock.unlock()
            engine.input.async {
                defer { engine.workers.leave() }
                do {
                    guard packets.count == protocols.count else { throw CocoaError(.coderInvalidValue) }
                    for (index, packet) in packets.enumerated() {
                        let version = packet.first.map { $0 >> 4 } ?? 0
                        guard (version == 4 && protocols[index].int32Value == AF_INET) ||
                              (version == 6 && protocols[index].int32Value == AF_INET6) else { throw CocoaError(.coderInvalidValue) }
                        while engine.running {
                            if try port.inject(packet) { break }
                        }
                    }
                    engine.readNext(flow, port: port)
                } catch { engine.fail(error) }
            }
        }
    }
    private func reconfigure(_ data: Data) throws {
        let settings = try OneUiVpnConfiguration.decodeNetworkSettings(data)
        guard running else {
            if settings.addresses.isEmpty { return } // Provider's final clear owns OS teardown.
            throw CocoaError(.userCancelled)
        }
        let acknowledgement = DispatchSemaphore(value: 0)
        var failure: Error?
        applySettings(settings.addresses.isEmpty ? nil : settings) { error in
            failure = error
            acknowledgement.signal()
        }
        acknowledgement.wait() // Worker only; do not outlive a late native operation.
        if let error = failure { throw error }
    }
    private func fail(_ error: Error) {
        lock.lock()
        guard active else { lock.unlock(); return }
        active = false
        let callback = failure
        lock.unlock()
        protocolEngine.cancel()
        callback?(error)
    }
    public func stop(completion: @escaping (Error?) -> Void) {
        lock.lock()
        if stopped { lock.unlock(); completion(nil); return }
        waiters.append(completion)
        if stopping { lock.unlock(); return }
        stopping = true; active = false
        lock.unlock()
        network.close()
        protocolEngine.cancel()
        cleanup.async {
            self.workers.wait()
            var error: Error?
            do {
                if !self.protocolClosed {
                    try self.protocolEngine.close()
                    self.protocolClosed = true
                }
                // The protocol may still own callbacks into the port until its
                // close succeeds. Preserve both owners when either step fails.
                try self.port?.close()
            } catch let failure { error = failure }
            self.lock.lock()
            self.stopping = false
            self.stopped = error == nil
            if error == nil { self.port = nil; self.failure = nil }
            let callbacks = self.waiters
            self.waiters.removeAll()
            self.lock.unlock()
            callbacks.forEach { $0(error) }
        }
    }
}
