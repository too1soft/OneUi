import Foundation
import Network
import Darwin

/// Native interface enumeration and physical path observation using public APIs.
public final class OneUiNetworkState {
    private let monitor = NWPathMonitor()
    private let queue = DispatchQueue(label: "net.too1.oneui.vpn.path")
    private let lock = NSLock()
    private var defaultInterface = ""
    private var handler: (() -> Void)?
    public init() {
        monitor.pathUpdateHandler = { [weak self] path in
            guard let self = self else { return }
            let selected = path.status == .satisfied ? path.availableInterfaces.first {
                path.usesInterfaceType($0.type) && !$0.name.hasPrefix("utun")
            }?.name ?? "" : ""
            self.lock.lock(); self.defaultInterface = selected; let notify = self.handler; self.lock.unlock()
            notify?()
        }
        monitor.start(queue: queue)
    }
    public func observe(_ handler: @escaping () -> Void) { lock.lock(); self.handler = handler; lock.unlock() }
    public func close() { lock.lock(); handler = nil; lock.unlock(); monitor.cancel() }
    deinit { monitor.cancel() }
    public func snapshot() throws -> Data {
        lock.lock(); let selected = defaultInterface; lock.unlock()
        return try Self.snapshot(defaultInterface: selected)
    }
    private static func address(_ value: UnsafePointer<sockaddr>) -> String? {
        var host = [CChar](repeating: 0, count: Int(NI_MAXHOST))
        guard getnameinfo(value, socklen_t(value.pointee.sa_len), &host, socklen_t(host.count), nil, 0, NI_NUMERICHOST) == 0 else { return nil }
        return String(cString: host).components(separatedBy: "%")[0]
    }
    private static func prefix(_ mask: UnsafePointer<sockaddr>, family: Int32) -> Int? {
        let bytes: [UInt8]
        if family == AF_INET {
            var value = UnsafeRawPointer(mask).assumingMemoryBound(to: sockaddr_in.self).pointee.sin_addr
            bytes = withUnsafeBytes(of: &value) { Array($0) }
        } else {
            var value = UnsafeRawPointer(mask).assumingMemoryBound(to: sockaddr_in6.self).pointee.sin6_addr
            bytes = withUnsafeBytes(of: &value) { Array($0) }
        }
        var count = 0, ended = false
        for byte in bytes { for bit in (0..<8).reversed() {
            if byte & (1 << bit) != 0 { if ended { return nil }; count += 1 } else { ended = true }
        } }
        return count
    }
    public static func snapshot(defaultInterface: String = "") throws -> Data {
        var head: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&head) == 0, let first = head else { throw CocoaError(.fileReadUnknown) }
        defer { freeifaddrs(first) }
        var entries: [String: [String: Any]] = [:]
        var cursor: UnsafeMutablePointer<ifaddrs>? = first
        while let item = cursor {
            defer { cursor = item.pointee.ifa_next }
            let value = item.pointee, name = String(cString: value.ifa_name)
            guard !name.hasPrefix("utun"), let socket = value.ifa_addr else { continue }
            let flags = value.ifa_flags, family = Int32(socket.pointee.sa_family)
            if entries[name] == nil {
                entries[name] = ["name": name, "index": Int(if_nametoindex(value.ifa_name)), "mtu": 0,
                    "up": flags & UInt32(IFF_UP) != 0, "loopback": flags & UInt32(IFF_LOOPBACK) != 0,
                    "pointToPoint": flags & UInt32(IFF_POINTOPOINT) != 0, "multicast": flags & UInt32(IFF_MULTICAST) != 0,
                    "addresses": [String]()]
            }
            if family == AF_LINK, let data = value.ifa_data {
                entries[name]?["mtu"] = Int(data.assumingMemoryBound(to: if_data.self).pointee.ifi_mtu)
            } else if (family == AF_INET || family == AF_INET6), let mask = value.ifa_netmask,
                      let ip = address(UnsafePointer(socket)), let bits = prefix(UnsafePointer(mask), family: family) {
                var addresses = entries[name]?["addresses"] as? [String] ?? []
                addresses.append(ip + "/" + String(bits)); entries[name]?["addresses"] = addresses
            }
        }
        let interfaces = entries.values.filter { ($0["mtu"] as? Int ?? 0) > 0 && ($0["index"] as? Int ?? 0) > 0 }
            .sorted { ($0["index"] as? Int ?? 0) < ($1["index"] as? Int ?? 0) }
        let selected = interfaces.contains { $0["name"] as? String == defaultInterface } ? defaultInterface : ""
        let data = try JSONSerialization.data(withJSONObject: ["version": 1, "defaultInterface": selected, "interfaces": interfaces])
        guard data.count <= 65536 else { throw CocoaError(.coderInvalidValue) }
        return data
    }
}
