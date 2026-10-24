import Foundation
import Observation

struct ConsoleEntry: Identifiable {
    let id = UUID()
    let command: String
    let response: String
    let failed: Bool
}

/// UI state for one axis controller. The model keeps itself connected:
/// it reconnects with backoff whenever the link drops or the board reboots,
/// unless paused. Polling while connected doubles as the keepalive that stops
/// the controller's link watchdog from tripping.
@MainActor
@Observable
final class AxisModel: Identifiable {
    enum ConnectionState: Equatable {
        case paused
        case connecting
        case connected
        case reconnecting(attempt: Int, reason: String)
    }

    static let pollInterval: Duration = .milliseconds(200)
    static let netStatusEvery = 10
    static let reconnectDelays: [Duration] = [.milliseconds(500), .seconds(1), .seconds(2), .seconds(3), .seconds(5)]

    let id: String
    var name: String
    var endpoint: String { id }
    var isAdvertised = false

    /// Robot joint number parsed from names such as "roberta-j3".
    var joint: Int? {
        guard let match = name.lowercased().firstMatch(of: /(?:^|-)j(\d+)$/) else { return nil }
        return Int(match.1)
    }

    var displayName: String { joint.map { "J\($0)" } ?? name }

    private(set) var state: ConnectionState = .paused {
        didSet { DevHooks.trace("\(name) state \(state)") }
    }
    private(set) var snapshot: AxisSnapshot?
    private(set) var netStatus = ""
    private(set) var roundTripMs: Double?
    private(set) var console: [ConsoleEntry] = []
    private(set) var isPaused = false
    var lastError: String? {
        didSet { if let lastError { DevHooks.trace("\(name) error \(lastError)") } }
    }

    private let events: EventLog
    private var link: AxisLink?
    private var pollTask: Task<Void, Never>?
    private var reconnectTask: Task<Void, Never>?
    private var failedAttempts = 0
    private var lastBootDate: Date?
    private var lastFaultWord: UInt32 = 0
    private var lastWatchdogTrips: Int?

    init(name: String, endpoint: String, events: EventLog) {
        self.id = endpoint
        self.name = name
        self.events = events
    }

    var isConnected: Bool { state == .connected }
    var isDriveEnabled: Bool { snapshot?.motorEnabled == true }

    var faultDescription: String? {
        guard let snapshot, snapshot.faultWord != 0 else { return nil }
        return AxisFaults.describe(faultWord: snapshot.faultWord, lastErrorCode: snapshot.lastErrorCode)
    }

    /// The live connection, for operations that span several axes.
    var coordinationLink: AxisLink? { link }

    func connect() {
        guard !isPaused else { return }
        switch state {
        case .connecting, .connected:
            return
        case .paused, .reconnecting:
            break
        }

        reconnectTask?.cancel()
        reconnectTask = nil
        if case .paused = state {
            state = .connecting
        }
        let link = AxisLink(endpoint: endpoint)
        self.link = link

        Task {
            do {
                try await link.connect()
                guard self.link === link else { return }
                if failedAttempts > 0 {
                    events.record(.info, axis: displayName, "reconnected after \(failedAttempts) attempt(s)")
                }
                failedAttempts = 0
                state = .connected
                lastError = nil
                await checkResetReason(link)
                startPolling(link)
            } catch {
                guard self.link === link else { return }
                self.link = nil
                scheduleReconnect(reason: error.localizedDescription)
            }
        }
    }

    /// Stops automatic reconnection and drops the link.
    func pause() {
        isPaused = true
        reconnectTask?.cancel()
        reconnectTask = nil
        dropLink()
        state = .paused
        events.record(.info, axis: displayName, "paused")
    }

    func resume() {
        guard isPaused else { return }
        isPaused = false
        state = .paused
        connect()
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
                    self.noteSnapshot(snapshot)
                    self.roundTripMs = Double(elapsed.components.attoseconds) / 1e15
                        + Double(elapsed.components.seconds) * 1000
                    if let netStatus {
                        self.netStatus = netStatus
                        self.noteNetStatus(netStatus)
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
        events.record(.warning, axis: displayName, "link lost: \(error.localizedDescription)")
        dropLink()
        scheduleReconnect(reason: error.localizedDescription)
    }

    private func dropLink() {
        pollTask?.cancel()
        pollTask = nil
        let link = self.link
        self.link = nil
        snapshot = nil
        roundTripMs = nil
        Task { await link?.disconnect() }
    }

    private func scheduleReconnect(reason: String) {
        guard !isPaused else { return }
        failedAttempts += 1
        state = .reconnecting(attempt: failedAttempts, reason: reason)
        let delay = Self.reconnectDelays[min(failedAttempts - 1, Self.reconnectDelays.count - 1)]
        reconnectTask?.cancel()
        reconnectTask = Task { [weak self] in
            try? await Task.sleep(for: delay)
            guard let self, !Task.isCancelled else { return }
            self.connect()
        }
    }

    /// Asks the board why it last reset. An abnormal reset, or any reboot
    /// since we last saw it, means the drive was disabled and the position
    /// counter restarted at zero.
    private func checkResetReason(_ link: AxisLink) async {
        guard let reply = try? await link.command("diag"), reply.hasPrefix("diag ") else { return }
        guard let uptime = reply.firstMatch(of: /uptime_ms=(\d+)/).flatMap({ Double($0.1) }) else { return }

        let bootDate = Date().addingTimeInterval(-uptime / 1000)
        let isNewBoot = lastBootDate.map { abs($0.timeIntervalSince(bootDate)) > 3 } ?? true
        let reason = reply.dropFirst("diag ".count)
            .replacing(/\s+uptime_ms=\d+/, with: "")
        let abnormal = !reason.hasPrefix("reset=power-on")

        if isNewBoot && (abnormal || lastBootDate != nil) {
            events.record(abnormal ? .error : .warning, axis: displayName,
                          "board restarted (\(reason)); drive disabled, position reset to 0")
        } else if lastBootDate == nil {
            events.record(.info, axis: displayName, "connected")
        }
        if isNewBoot {
            lastWatchdogTrips = nil
            lastFaultWord = 0
        }
        lastBootDate = bootDate
    }

    private func noteSnapshot(_ snapshot: AxisSnapshot) {
        if snapshot.faultWord != lastFaultWord {
            if snapshot.faultWord == 0 {
                events.record(.info, axis: displayName, "faults cleared")
            } else {
                events.record(.warning, axis: displayName, "fault: \(AxisFaults.describe(faultWord: snapshot.faultWord, lastErrorCode: snapshot.lastErrorCode))")
            }
            lastFaultWord = snapshot.faultWord
        }
    }

    private func noteNetStatus(_ status: String) {
        guard let trips = status.firstMatch(of: /trips=(\d+)/).flatMap({ Int($0.1) }) else { return }
        if let previous = lastWatchdogTrips, trips > previous {
            events.record(.warning, axis: displayName, "link watchdog stopped motion (\(trips) since boot)")
        }
        lastWatchdogTrips = trips
    }

    private func appendConsole(_ entry: ConsoleEntry) {
        console.append(entry)
        if console.count > 100 {
            console.removeFirst(console.count - 100)
        }
    }
}
