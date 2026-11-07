import Foundation

/// Bounded native worker calls, cancellation and main-thread result delivery.
public final class OneUiNativeTaskScope {
    public final class Task {
        private let lock = NSLock()
        private var cancelled = false
        private let cancellation: (() -> Void)?
        fileprivate init(cancellation: (() -> Void)?) { self.cancellation = cancellation }
        public func cancel() {
            lock.lock(); if cancelled { lock.unlock(); return }; cancelled = true; lock.unlock(); cancellation?()
        }
        fileprivate var stopped: Bool { lock.lock(); defer { lock.unlock() }; return cancelled }
    }
    private static let workers: OperationQueue = { let queue = OperationQueue(); queue.name = "net.too1.oneui.native"; queue.maxConcurrentOperationCount = 2; return queue }()
    private static let capacityLock = NSLock()
    private static var count = 0
    private let lock = NSLock()
    private var pending: [UUID: Task] = [:]
    private var closed = false
    private var resources: [() throws -> Void] = []
    private var cleaning = false
    public func own(_ cleanup: @escaping () throws -> Void) throws {
        lock.lock(); defer { lock.unlock() }
        guard !closed else { throw CocoaError(.userCancelled) }; resources.append(cleanup)
    }
    public init() { }
    @discardableResult public func submit(work: @escaping () throws -> Data, cancellation: (() -> Void)? = nil,
                                         reply: @escaping (Int32, Data) -> Void) throws -> Task {
        Self.capacityLock.lock()
        guard Self.count < 16 else { Self.capacityLock.unlock(); throw CocoaError(.userCancelled) }
        Self.count += 1; Self.capacityLock.unlock()
        let id = UUID(), task = Task(cancellation: cancellation)
        lock.lock()
        guard !closed else { lock.unlock(); Self.releaseCapacity(); throw CocoaError(.userCancelled) }
        pending[id] = task; lock.unlock()
        Self.workers.addOperation { [self] in
            var data = Data(), status: Int32 = -1
            if !task.stopped { do { data = try work(); status = data.count <= 1024 * 1024 ? 0 : -1; if status != 0 { data.removeAll() } } catch { status = -1 } }
            Self.releaseCapacity()
            self.lock.lock(); self.pending.removeValue(forKey: id); self.lock.unlock(); self.releaseWhenIdle()
            let result = data, code = status
            DispatchQueue.main.async { [weak self] in
                guard let self = self else { return }
                self.lock.lock(); let deliver = !self.closed; self.lock.unlock()
                if deliver { reply(task.stopped ? -3 : code, result) }
            }
        }
        return task
    }
    private static func releaseCapacity() { capacityLock.lock(); count -= 1; capacityLock.unlock() }
    public func close() {
        lock.lock(); if closed { lock.unlock(); return }; closed = true; let tasks = Array(pending.values); lock.unlock()
        tasks.forEach { $0.cancel() }
        releaseWhenIdle()
    }
    private func releaseWhenIdle() {
        lock.lock()
        guard closed, pending.isEmpty, !cleaning, !resources.isEmpty else { lock.unlock(); return }
        cleaning = true; lock.unlock()
        Self.workers.addOperation { [self] in
            var retry = false
            while true {
                self.lock.lock(); let cleanup = self.resources.last; self.lock.unlock()
                guard let cleanup = cleanup else { break }
                do { try cleanup(); self.lock.lock(); self.resources.removeLast(); self.lock.unlock() }
                catch { retry = true; break }
            }
            self.lock.lock(); self.cleaning = false; self.lock.unlock()
            if retry { DispatchQueue.global().asyncAfter(deadline: .now() + 1) { self.releaseWhenIdle() } }
        }
    }
    deinit { close() }
}
