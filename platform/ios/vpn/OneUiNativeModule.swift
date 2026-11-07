import Foundation

/// Executes an application command through the same bounded C ABI as Android.
/// The caller schedules work and UI delivery; this class owns buffer copying.
public final class OneUiNativeModule {
    private let call: OneUiNativeModuleCallV1
    public init(call: @escaping OneUiNativeModuleCallV1) { self.call = call }
    public func invoke(command: Int32, request: Data) throws -> Data {
        guard request.count <= 1024 * 1024 else { throw CocoaError(.coderInvalidValue) }
        var input = request, output = Data(count: 1024 * 1024)
        defer { input.resetBytes(in: 0..<input.count) }
        let count = input.withUnsafeMutableBytes { source in
            output.withUnsafeMutableBytes { destination in
                call(command, source.baseAddress, Int32(source.count), destination.baseAddress, Int32(destination.count))
            }
        }
        guard count >= 0, count <= 1024 * 1024 else { throw NSError(domain: "OneUi.NativeModule", code: Int(count)) }
        output.count = Int(count)
        return output
    }
}
