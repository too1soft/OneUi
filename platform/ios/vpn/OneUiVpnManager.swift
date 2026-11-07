import Foundation

/// Application-scoped owner. Closing or recreating a page does not stop its VPN.
public final class OneUiVpnManager {
    private static var owners: [String: OneUiVpnManager] = [:]
    private let bundleID: String
    private var session: OneUiVpnSession!
    private var observers: [UUID: (OneUiVpnSession.State) -> Void] = [:]
    private var adopting = false
    private lazy var controller = OneUiVpnController(providerBundleID: bundleID) { [weak self] attempt, event, code in
        guard let self = self else { return }
        if self.session.report(attempt: attempt, event: event, code: code) { self.publish() }
    }
    public static func shared(providerBundleID: String) throws -> OneUiVpnManager {
        dispatchPrecondition(condition: .onQueue(.main))
        if let value = owners[providerBundleID] { return value }
        guard !providerBundleID.isEmpty, providerBundleID.utf8.count <= 256 else { throw CocoaError(.coderInvalidValue) }
        let value = try OneUiVpnManager(bundleID: providerBundleID); owners[providerBundleID] = value; return value
    }
    private init(bundleID: String) throws {
        self.bundleID = bundleID
        session = try OneUiVpnSession(start: { [weak self] attempt, json in
            guard let self = self else { return }
            self.controller.start(attempt: attempt, configurationJSON: json, onlyExisting: self.adopting)
        },
            stop: { [weak self] attempt in self?.controller.stop(attempt: attempt) })
        controller.existingConfiguration { [weak self] json in
            guard let self = self, let json = json, self.session.snapshot().attempt == 0 else { return }
            self.adopting = true
            defer { self.adopting = false }
            try? self.session.connect(configuration: json); self.publish()
        }
    }
    public func observe(_ callback: @escaping (OneUiVpnSession.State) -> Void) -> UUID {
        dispatchPrecondition(condition: .onQueue(.main))
        let id = UUID(); observers[id] = callback; callback(session.snapshot()); return id
    }
    public func removeObserver(_ id: UUID) { dispatchPrecondition(condition: .onQueue(.main)); observers.removeValue(forKey: id) }
    public func snapshot() -> OneUiVpnSession.State { return session.snapshot() }
    public func connect(configuration: String) throws { try session.connect(configuration: configuration); publish() }
    public func disconnect() throws { try session.disconnect(); publish() }
    public func message(_ request: Data, completion: @escaping (Result<Data, Error>) -> Void) { controller.message(request, completion: completion) }
    private func publish() { let value = session.snapshot(); Array(observers.values).forEach { $0(value) } }
}
