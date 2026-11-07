import Foundation

/// Network updates are synchronous acknowledgements invoked on engine workers.
/// Implementations await the native settings callback, never block the UI queue,
/// and never close the port reentrantly from these methods.
public protocol OneUiNativeVpnServices: AnyObject {
    func applyNetwork(_ data: Data) throws
    func secureRead(_ key: String) throws -> Data?
    func secureWrite(_ key: String, data: Data) throws
    func networkState() throws -> Data
}
public extension OneUiNativeVpnServices {
    func networkState() throws -> Data { return try OneUiNetworkState.snapshot() }
}

private final class OneUiNativeServicesBox {
    let services: OneUiNativeVpnServices
    init(_ services: OneUiNativeVpnServices) { self.services = services }
}

public final class OneUiNativePacketPort {
    private let condition = NSCondition()
    private var value: UInt64 = 0
    private var closing = false
    private var context: UnsafeMutableRawPointer?
    public var handle: UInt64 { condition.lock(); defer { condition.unlock() }; return value }

    public init(label: String, mtu: Int32, allowDefaultRoute: Bool, splitDNS: Bool,
                services: OneUiNativeVpnServices) throws {
        guard !label.isEmpty, label.utf8.count <= 128, !label.utf8.contains(0) else { throw CocoaError(.coderInvalidValue) }
        let pointer = Unmanaged.passRetained(OneUiNativeServicesBox(services)).toOpaque()
        var callbacks = OneUiVpnNativeCallbacks()
        callbacks.struct_size = UInt32(MemoryLayout<OneUiVpnNativeCallbacks>.size)
        callbacks.abi_version = 1
        callbacks.context = UInt(bitPattern: pointer)
        callbacks.apply_network = { context, json, length in
            guard let pointer = UnsafeRawPointer(bitPattern: context), let json = json,
                  length > 0, length <= 65536 else { return -1 }
            let box = Unmanaged<OneUiNativeServicesBox>.fromOpaque(pointer).takeUnretainedValue()
            do { try box.services.applyNetwork(Data(bytes: json, count: Int(length))); return 0 }
            catch { return -1 }
        }
        // Apple's provider transport sockets are exempt from its own tunnel.
        callbacks.protect_socket = { _, _ in return 0 }
        callbacks.read_network_state = { context, output, capacity in
            guard let pointer = UnsafeRawPointer(bitPattern: context), let output = output else { return -1 }
            let box = Unmanaged<OneUiNativeServicesBox>.fromOpaque(pointer).takeUnretainedValue()
            do {
                let data = try box.services.networkState()
                guard data.count > 0, data.count <= Int(capacity) else { return -1 }
                data.copyBytes(to: output.assumingMemoryBound(to: UInt8.self), count: data.count)
                return Int32(data.count)
            } catch { return -1 }
        }
        callbacks.secure_read = { context, key, output, capacity in
            guard let pointer = UnsafeRawPointer(bitPattern: context), let key = key, let output = output else { return -1 }
            let box = Unmanaged<OneUiNativeServicesBox>.fromOpaque(pointer).takeUnretainedValue()
            do {
                guard var data = try box.services.secureRead(String(cString: key)) else { return 0 }
                defer { data.resetBytes(in: 0..<data.count) }
                guard data.count <= Int(capacity) else { return -1 }
                data.copyBytes(to: output.assumingMemoryBound(to: UInt8.self), count: data.count)
                return Int32(data.count)
            } catch { return -1 }
        }
        callbacks.secure_write = { context, key, bytes, length in
            guard let pointer = UnsafeRawPointer(bitPattern: context), let key = key, let bytes = bytes,
                  length > 0, length <= 1024 * 1024 else { return -1 }
            let box = Unmanaged<OneUiNativeServicesBox>.fromOpaque(pointer).takeUnretainedValue()
            var data = Data(bytes: bytes, count: Int(length))
            defer { data.resetBytes(in: 0..<data.count) }
            do { try box.services.secureWrite(String(cString: key), data: data); return 0 }
            catch { return -1 }
        }
        value = label.withCString {
            oneui_vpn_go_host_create(UnsafeMutablePointer(mutating: $0), mtu,
                (allowDefaultRoute ? 1 : 0) | (splitDNS ? 2 : 0), &callbacks)
        }
        guard value != 0 else {
            Unmanaged<OneUiNativeServicesBox>.fromOpaque(pointer).release()
            throw NSError(domain: "OneUiVpn.Native", code: -4)
        }
        context = pointer
    }
    /// False denotes bounded queue backpressure; the caller retries this packet.
    public func inject(_ packet: Data, timeout: Int32 = 100) throws -> Bool {
        let result = packet.withUnsafeBytes { bytes -> Int32 in
            oneui_vpn_go_packet_inject(handle, UnsafeMutableRawPointer(mutating: bytes.baseAddress), Int32(bytes.count), timeout)
        }
        if result == -3 { return false }
        guard result == 0 else { throw NSError(domain: "OneUiVpn.Native", code: Int(result)) }
        return true
    }
    /// Nil denotes timeout. Each successful result owns its packet bytes.
    public func receive(timeout: Int32 = 100) throws -> Data? {
        var data = Data(count: 65535)
        let result = data.withUnsafeMutableBytes { bytes -> Int32 in
            oneui_vpn_go_packet_receive(handle, bytes.baseAddress, Int32(bytes.count), timeout)
        }
        if result == -3 { return nil }
        guard result > 0, result <= 65535 else { throw NSError(domain: "OneUiVpn.Native", code: Int(result)) }
        data.count = Int(result)
        return data
    }
    public func networkChanged() throws {
        let result = oneui_vpn_go_network_changed(handle)
        guard result == 0 else { throw NSError(domain: "OneUiVpn.Native", code: Int(result)) }
    }
    public func close() throws {
        condition.lock()
        while closing { condition.wait() }
        guard value != 0 else { condition.unlock(); return }
        closing = true
        let previous = value
        condition.unlock()
        let result = oneui_vpn_go_host_close(previous)
        condition.lock()
        let released = result == 0 ? context : nil
        if result == 0 { value = 0; context = nil }
        closing = false
        condition.broadcast()
        condition.unlock()
        if let pointer = released { Unmanaged<OneUiNativeServicesBox>.fromOpaque(pointer).release() }
        guard result == 0 else { throw NSError(domain: "OneUiVpn.Native", code: Int(result)) }
    }
    deinit { try? close() }
}
