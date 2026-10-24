import Foundation
import Network
import Observation

/// All known axis controllers: those advertised over Bonjour as
/// `_roberta-axis._tcp` plus any added by hand.
@MainActor
@Observable
final class Workspace {
    static let serviceType = "_roberta-axis._tcp"
    static let defaultPort = 7300

    private(set) var axes: [AxisModel] = []
    var selection: AxisModel.ID?
    var isShowingMoveTogether = false
    private(set) var browserState = "Starting discovery"

    private var browser: NWBrowser?

    var canMoveTogether: Bool {
        axes.filter(\.isConnected).count >= 2
    }

    var selectedAxis: AxisModel? {
        axes.first { $0.id == selection }
    }

    func startDiscovery() {
        guard browser == nil else { return }

        let browser = NWBrowser(for: .bonjour(type: Self.serviceType, domain: nil), using: NWParameters())
        browser.stateUpdateHandler = { [weak self] state in
            MainActor.assumeIsolated { self?.browserStateChanged(state) }
        }
        browser.browseResultsChangedHandler = { [weak self] results, _ in
            let names = results.compactMap { result -> String? in
                if case let .service(name, _, _, _) = result.endpoint {
                    return name
                }
                return nil
            }
            MainActor.assumeIsolated { self?.advertisedNamesChanged(Set(names)) }
        }
        browser.start(queue: .main)
        self.browser = browser
    }

    /// Accepts "tcp://host[:port]", a serial device path, or a bare host/IP.
    func addAxis(_ text: String) {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }

        let endpoint = trimmed.hasPrefix("tcp://") || trimmed.hasPrefix("/dev/") ? trimmed : "tcp://\(trimmed)"
        let axis = axis(for: endpoint, name: trimmed)
        selection = axis.id
    }

    /// Starts relative moves on several axes at the same instant: syncs every
    /// axis clock, schedules each move for one host time `lead` ahead, then
    /// reads back when each axis actually started.
    func moveTogether(_ moves: [(axis: AxisModel, steps: Int32)], lead: Duration) async -> [CoordinatedResult] {
        let jobs = moves.compactMap { move in
            move.axis.coordinationLink.map {
                CoordinationJob(id: move.axis.id, name: move.axis.name, steps: move.steps, link: $0)
            }
        }
        var results = Dictionary(uniqueKeysWithValues: jobs.map {
            ($0.id, CoordinatedResult(id: $0.id, name: $0.name, steps: $0.steps))
        })

        let syncs = await forEachJob(jobs) { job in try await job.link.syncClock() }
        for (id, outcome) in syncs {
            switch outcome {
            case let .success(roundTrip): results[id]?.roundTripUs = roundTrip
            case let .failure(error): results[id]?.error = "Clock sync: \(error.localizedDescription)"
            }
        }
        guard results.values.allSatisfy({ $0.error == nil }) else {
            return ordered(results, jobs)
        }

        let leadMicroseconds = Int64(lead.components.seconds) * 1_000_000 + lead.components.attoseconds / 1_000_000_000_000
        let target = AxisLink.hostClockMicroseconds() + leadMicroseconds
        let schedules = await forEachJob(jobs) { job in try await job.link.moveBy(steps: job.steps, atHostTime: target) }
        if schedules.contains(where: { (try? $0.1.get()) == nil }) {
            // All or nothing: cancel whatever did get armed.
            _ = await forEachJob(jobs) { job in try await job.link.stop() }
            for (id, outcome) in schedules {
                if case let .failure(error) = outcome {
                    results[id]?.error = "Schedule: \(error.localizedDescription)"
                }
            }
            return ordered(results, jobs)
        }

        try? await Task.sleep(for: lead + .milliseconds(150))
        let starts = await forEachJob(jobs) { job in try await job.link.lastStartHostTime() }
        for (id, outcome) in starts {
            switch outcome {
            case let .success(started): results[id]?.startOffsetMs = Double(started - target) / 1000
            case let .failure(error): results[id]?.error = "Read start: \(error.localizedDescription)"
            }
        }
        return ordered(results, jobs)
    }

    private func forEachJob<T: Sendable>(
        _ jobs: [CoordinationJob],
        _ body: @escaping @Sendable (CoordinationJob) async throws -> T
    ) async -> [(AxisModel.ID, Result<T, Error>)] {
        await withTaskGroup(of: (AxisModel.ID, Result<T, Error>).self) { group in
            for job in jobs {
                group.addTask {
                    do {
                        return (job.id, .success(try await body(job)))
                    } catch {
                        return (job.id, .failure(error))
                    }
                }
            }
            var outcomes: [(AxisModel.ID, Result<T, Error>)] = []
            for await outcome in group {
                outcomes.append(outcome)
            }
            return outcomes
        }
    }

    private func ordered(
        _ results: [AxisModel.ID: CoordinatedResult],
        _ jobs: [CoordinationJob]
    ) -> [CoordinatedResult] {
        jobs.compactMap { results[$0.id] }
    }

    func stopAll() {
        for axis in axes where axis.isConnected {
            axis.stop()
        }
    }

    private func axis(for endpoint: String, name: String) -> AxisModel {
        if let existing = axes.first(where: { $0.endpoint == endpoint }) {
            return existing
        }
        let axis = AxisModel(name: name, endpoint: endpoint)
        axes.append(axis)
        axes.sort { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
        return axis
    }

    private func advertisedNamesChanged(_ names: Set<String>) {
        // The firmware advertises its hostname as the service instance name.
        for name in names {
            let axis = axis(for: "tcp://\(name).local:\(Self.defaultPort)", name: name)
            axis.isAdvertised = true
            if DevHooks.autoConnect, axis.state == .disconnected {
                axis.connect()
            }
        }
        for axis in axes where axis.isAdvertised && !names.contains(axis.name) {
            axis.isAdvertised = false
        }
        if selection == nil {
            selection = axes.first?.id
        }
    }

    private func browserStateChanged(_ state: NWBrowser.State) {
        switch state {
        case .ready:
            browserState = "Searching for axes"
        case let .failed(error), let .waiting(error):
            browserState = "Discovery unavailable: \(error.localizedDescription)"
        default:
            break
        }
    }
}

private struct CoordinationJob: Sendable {
    let id: AxisModel.ID
    let name: String
    let steps: Int32
    let link: AxisLink
}

struct CoordinatedResult: Identifiable, Sendable {
    let id: AxisModel.ID
    let name: String
    let steps: Int32
    var roundTripUs: UInt32?
    /// Actual start minus the scheduled start, as measured on the axis clock.
    var startOffsetMs: Double?
    var error: String?
}
