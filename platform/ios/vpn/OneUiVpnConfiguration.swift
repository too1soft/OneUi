import Foundation
import NetworkExtension
import Darwin

public struct OneUiVpnPrefix: Decodable {
    public let address: String
    public let prefix: Int
}

/// The same non-secret wire format as oneui_vpn_config_json_v1.
public struct OneUiVpnConfiguration: Decodable {
    public let version: Int
    public let label: String
    public let mtu: Int
    public let allowDefaultRoute: Bool
    public let addresses: [OneUiVpnPrefix]
    public let routes: [OneUiVpnPrefix]
    public let dnsServers: [String]
    public let dnsDomains: [String]
    public let excludedRoutes: [OneUiVpnPrefix]?
    public let dnsMatchDomains: [String]?

    public static func decode(_ json: String) throws -> OneUiVpnConfiguration {
        return try decode(json, dynamic: false)
    }
    public static func decodeNetworkSettings(_ data: Data) throws -> OneUiVpnConfiguration {
        guard let json = String(data: data, encoding: .utf8) else { throw CocoaError(.coderReadCorrupt) }
        return try decode(json, dynamic: true)
    }
    private static func numeric(_ value: String) -> Bool {
        var bytes = in6_addr()
        return value.withCString { inet_pton(value.contains(":") ? AF_INET6 : AF_INET, $0, &bytes) == 1 }
    }
    private static func decode(_ json: String, dynamic: Bool) throws -> OneUiVpnConfiguration {
        guard let data = json.data(using: .utf8), data.count <= 65536 else {
            throw CocoaError(.coderReadCorrupt)
        }
        let value = try JSONDecoder().decode(Self.self, from: data)
        guard value.version == 1, !value.label.isEmpty, value.label.utf8.count <= 128,
              (1280...9000).contains(value.mtu), ((dynamic ? 0 : 1)...16).contains(value.addresses.count),
              ((dynamic ? 0 : 1)...256).contains(value.routes.count), value.dnsServers.count <= 8,
              value.dnsDomains.count <= 16, (value.excludedRoutes?.count ?? 0) <= 256,
              (value.dnsMatchDomains?.count ?? 0) <= 256 else { throw CocoaError(.coderReadCorrupt) }
        for p in value.addresses + value.routes + (value.excludedRoutes ?? []) {
            guard numeric(p.address), (0...(p.address.contains(":") ? 128 : 32)).contains(p.prefix) else {
                throw CocoaError(.coderReadCorrupt)
            }
        }
        guard value.allowDefaultRoute || !value.routes.contains(where: { $0.prefix == 0 }) else {
            throw CocoaError(.coderReadCorrupt)
        }
        guard value.dnsServers.allSatisfy(numeric), value.routes.allSatisfy({ route in
            value.addresses.contains(where: { $0.address.contains(":") == route.address.contains(":") })
        }) else { throw CocoaError(.coderReadCorrupt) }
        return value
    }

    public func networkSettings() -> NEPacketTunnelNetworkSettings {
        // Mesh protocols have no single remote endpoint. This is only the OS
        // tunnel identifier; engine endpoints are supplied through its own API.
        let settings = NEPacketTunnelNetworkSettings(tunnelRemoteAddress: "127.0.0.1")
        settings.mtu = NSNumber(value: mtu)
        let v4 = addresses.filter { !$0.address.contains(":") }
        let v6 = addresses.filter { $0.address.contains(":") }
        if !v4.isEmpty {
            let ip = NEIPv4Settings(addresses: v4.map(\.address), subnetMasks: v4.map { Self.mask($0.prefix) })
            ip.includedRoutes = routes.filter { !$0.address.contains(":") }.map {
                NEIPv4Route(destinationAddress: $0.address, subnetMask: Self.mask($0.prefix))
            }
            ip.excludedRoutes = (excludedRoutes ?? []).filter { !$0.address.contains(":") }.map {
                NEIPv4Route(destinationAddress: $0.address, subnetMask: Self.mask($0.prefix))
            }
            settings.ipv4Settings = ip
        }
        if !v6.isEmpty {
            let ip = NEIPv6Settings(addresses: v6.map(\.address), networkPrefixLengths: v6.map { NSNumber(value: $0.prefix) })
            ip.includedRoutes = routes.filter { $0.address.contains(":") }.map {
                NEIPv6Route(destinationAddress: $0.address, networkPrefixLength: NSNumber(value: $0.prefix))
            }
            ip.excludedRoutes = (excludedRoutes ?? []).filter { $0.address.contains(":") }.map {
                NEIPv6Route(destinationAddress: $0.address, networkPrefixLength: NSNumber(value: $0.prefix))
            }
            settings.ipv6Settings = ip
        }
        if !dnsServers.isEmpty {
            let dns = NEDNSSettings(servers: dnsServers)
            dns.searchDomains = dnsDomains
            dns.matchDomains = dnsMatchDomains
            settings.dnsSettings = dns
        }
        return settings
    }
    private static func mask(_ prefix: Int) -> String {
        let value: UInt32 = prefix == 0 ? 0 : UInt32.max << (32 - prefix)
        return [24, 16, 8, 0].map { String((value >> $0) & 255) }.joined(separator: ".")
    }
}
