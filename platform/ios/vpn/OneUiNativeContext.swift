import Foundation

private final class OneUiPreflightServices: OneUiNativeVpnServices {
    let storage: OneUiSecureBlobStore
    init(storage: OneUiSecureBlobStore) { self.storage = storage }
    func applyNetwork(_ data: Data) throws {
        let value = try JSONSerialization.jsonObject(with: data) as? [String: Any]
        guard let addresses = value?["addresses"] as? [Any], addresses.isEmpty else { throw CocoaError(.userCancelled) }
    }
    func secureRead(_ key: String) throws -> Data? { return try storage.read(key) }
    func secureWrite(_ key: String, data: Data) throws { try storage.write(key, data: data) }
}
/// Enrollment/identity-only context before VPN authorization. Nonempty OS
/// network settings are rejected, so it cannot route application traffic.
public final class OneUiNativeContext {
    private let port: OneUiNativePacketPort
    public init(storage: OneUiSecureBlobStore, label: String, mtu: Int32) throws {
        port = try OneUiNativePacketPort(label: label, mtu: mtu, allowDefaultRoute: false, splitDNS: false,
            services: OneUiPreflightServices(storage: storage))
    }
    public var host: UInt64 { return port.handle }
    public func close() throws { try port.close() }
}
