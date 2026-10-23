import Foundation
import Observation

struct ConsoleEntry: Identifiable {
    let id = UUID()
    let command: String
    let response: String
    let failed: Bool
}

/// UI state for one axis controller. Polling while connected doubles as the
/// keepalive that stops the controller's link watchdog from tripping.
@MainActor
@Observable
final class AxisModel: Identifiable {
    enum ConnectionState: Equatable {
        case disconnected
        case connecting
        case connected
        case failed(String)
    }

    static let pollInterval: Duration = .milliseconds(200)
    static let netStatusEvery = 10

    let id: String
    var name: String
    var endpoint: String { id }
    var isAdvertised = false

    private(set) var state: ConnectionState = .disconnected {
        didSet { DevHooks.trace("\(name) state \(state)") }
    }
    private(set) var snapshot: AxisSnapshot?
    private(set) var netStatus = ""
    private(set) var roundTripMs: Double?
    private(set) var console: [ConsoleEntry] = []
    var lastError: String? {
        didSet { if let lastError { DevHooks.trace("\(name) error \(lastError)") } }
    }

    private var link: AxisLink?
    private var pollTask: Task<Void, Never>?

    init(name: String, endpoint: String) {
        self.id = endpoint
        self.name = name
    }

    var isConnected: Bool { state == .connected }

    func connect() {
        guard state != .connecting, state != .connected else { return }

        state = .connecting
        lastError = nil
        let link = AxisLink(endpoint: endpoint)
        self.link = link

        Task {
            do {
                try await link.connect()
                guard self.link === link else { return }
                state = .connected
                startPolling(link)
            } catch {
                guard self.link === link else { return }
                self.link = nil
                state = .failed(error.localizedDescription)
            }
        }
    }

    func disconnect() {
        pollTask?.cancel()
        pollTask = nil
        let link = self.link
        self.link = nil
        snapshot = nil
        roundTripMs = nil
        state = .disconnected
        Task { await link?.disconnect() }
    }

    // MARK: Commands

    func enableDrive() { perform("Enable") { try await $0.enableDrive() } }
    func disableDrive() { perform("Disable") { try await $0.disableDrive() } }
    func stop() { perform("Stop") { try await $0.stop() } }
    func home() { perform("Home") { try await $0.home() } }
    func clearFaults() { perform("Clear faults") { try await $0.clearFaults() } }
    func setZero() { perform("Set zero") { try await $0.setZero() } }
    func move(by steps: Int32) { perform("Jog") { try await $0.moveBy(steps: steps) } }
    func move(to steps: Int32) { perform("Go to") { try await $0.moveTo(steps: steps) } }

    func applyProfile(velocity: Int32, accel: Int32, homeVelocity: Int32) {
        perform("Motion profile") {
            try await $0.setMotionProfile(velocity: velocity, accel: accel, homeVelocity: homeVelocity)
        }
    }

    func send(_ line: String) {
        guard let link else { return }
        Task {
            do {
                let response = try await link.command(line)
                appendConsole(ConsoleEntry(command: line, response: response, failed: false))
            } catch {
                appendConsole(ConsoleEntry(command: line, response: error.localizedDescription, failed: true))
            }
        }
    }

    // MARK: Private

    private func perform(_ label: String, _ operation: @escaping @Sendable (AxisLink) async throws -> Void) {
        guard let link else {
            lastError = "\(label): not connected"
            return
        }
        Task {
            do {
                try await operation(link)
            } catch {
                lastError = "\(label): \(error.localizedDescription)"
            }
        }
    }

    private func startPolling(_ link: AxisLink) {
        pollTask?.cancel()
        pollTask = Task { [weak self] in
            var tick = 0
            while !Task.isCancelled {
                let started = ContinuousClock.now
                do {
                    let snapshot = try await link.snapshot()
                    let elapsed = ContinuousClock.now - started
                    var netStatus: String?
                    if tick % Self.netStatusEvery == 0 {
                        netStatus = try await link.command("net status")
                    }
                    guard let self, !Task.isCancelled else { return }
                    self.snapshot = snapshot
                    self.roundTripMs = Double(elapsed.components.attoseconds) / 1e15
                        + Double(elapsed.components.seconds) * 1000
                    if let netStatus {
                        self.netStatus = netStatus
                    }
                } catch {
                    guard let self, !Task.isCancelled else { return }
                    self.connectionLost(error)
                    return
                }
                tick += 1
                try? await Task.sleep(for: Self.pollInterval)
            }
        }
    }

    private func connectionLost(_ error: Error) {
        pollTask = nil
        let link = self.link
        self.link = nil
        snapshot = nil
        roundTripMs = nil
        state = .failed("Connection lost: \(error.localizedDescription)")
        Task { await link?.disconnect() }
    }

    private func appendConsole(_ entry: ConsoleEntry) {
        console.append(entry)
        if console.count > 100 {
            console.removeFirst(console.count - 100)
        }
    }
}
