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
    private(set) var browserState = "Starting discovery"

    private var browser: NWBrowser?

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
