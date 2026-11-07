import Foundation
import Security

/// App/extension sharing uses the caller's provisioned Keychain access group.
/// Blobs never enter UserDefaults or VPN providerConfiguration. Background VPN
/// access is available after first unlock and does not migrate to another device.
public final class OneUiSecureBlobStore {
    private let service: String
    private let accessGroup: String?
    public init(service: String, accessGroup: String? = nil) throws {
        guard !service.isEmpty, service.utf8.count <= 128 else { throw CocoaError(.coderInvalidValue) }
        self.service = service
        self.accessGroup = accessGroup
    }
    private func query(_ key: String) throws -> [String: Any] {
        guard !key.isEmpty, key.utf8.count <= 128 else { throw CocoaError(.coderInvalidValue) }
        var value: [String: Any] = [kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service, kSecAttrAccount as String: key,
            kSecAttrSynchronizable as String: false]
        if let group = accessGroup { value[kSecAttrAccessGroup as String] = group }
        return value
    }
    public func read(_ key: String) throws -> Data? {
        var value = try query(key)
        value[kSecReturnData as String] = true
        value[kSecMatchLimit as String] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(value as CFDictionary, &result)
        if status == errSecItemNotFound { return nil }
        guard status == errSecSuccess, let data = result as? Data, data.count <= 1024 * 1024 else {
            throw NSError(domain: NSOSStatusErrorDomain, code: Int(status == errSecSuccess ? errSecDecode : status))
        }
        return data
    }
    public func write(_ key: String, data: Data) throws {
        guard !data.isEmpty, data.count <= 1024 * 1024 else { throw CocoaError(.fileWriteOutOfSpace) }
        let value = try query(key)
        let attributes: [String: Any] = [kSecValueData as String: data,
            kSecAttrAccessible as String: kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly]
        var status = SecItemUpdate(value as CFDictionary, attributes as CFDictionary)
        if status == errSecItemNotFound {
            var insertion = value
            for (key, attribute) in attributes { insertion[key] = attribute }
            status = SecItemAdd(insertion as CFDictionary, nil)
            if status == errSecDuplicateItem { status = SecItemUpdate(value as CFDictionary, attributes as CFDictionary) }
        }
        guard status == errSecSuccess else { throw NSError(domain: NSOSStatusErrorDomain, code: Int(status)) }
    }
    public func delete(_ key: String) throws {
        let status = SecItemDelete(try query(key) as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else {
            throw NSError(domain: NSOSStatusErrorDomain, code: Int(status))
        }
    }
}
