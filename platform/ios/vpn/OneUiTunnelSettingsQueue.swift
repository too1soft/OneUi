import Foundation
import NetworkExtension

/// Serializes asynchronous OS operations through their acknowledgements, so a
/// late start/update cannot reinstall settings after the final clear operation.
final class OneUiTunnelSettingsQueue {
    typealias Apply = (NEPacketTunnelNetworkSettings?, @escaping (Error?) -> Void) -> Void
    private struct Request {
        let settings: NEPacketTunnelNetworkSettings?
        let completion: (Error?) -> Void
    }
    private let owner = DispatchQueue(label: "net.too1.oneui.vpn.settings")
    private let apply: Apply
    private var pending: [Request] = []
    private var active = false
    init(apply: @escaping Apply) { self.apply = apply }
    func set(_ settings: NEPacketTunnelNetworkSettings?, completion: @escaping (Error?) -> Void) {
        owner.async {
            guard self.pending.count < 8 else { completion(NSError(domain: "OneUiVpn.Settings", code: 1)); return }
            self.pending.append(Request(settings: settings, completion: completion))
            self.next()
        }
    }
    private func next() {
        guard !active, !pending.isEmpty else { return }
        active = true
        let request = pending.removeFirst()
        apply(request.settings) { error in
            self.owner.async {
                self.active = false
                request.completion(error)
                self.next()
            }
        }
    }
}
