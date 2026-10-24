import SwiftUI

struct ContentView: View {
    @Bindable var workspace: Workspace
    @State private var newAxis = ""
    @State private var showingEvents = false

    var body: some View {
        NavigationSplitView {
            VStack(spacing: 0) {
                List(selection: $workspace.selection) {
                    Section("Joints") {
                        ForEach(1...Workspace.jointCount, id: \.self) { joint in
                            if let axis = workspace.axis(forJoint: joint) {
                                AxisRow(axis: axis).tag(axis.id)
                            } else {
                                MissingJointRow(joint: joint)
                                    .selectionDisabled()
                            }
                        }
                    }
                    if !workspace.otherAxes.isEmpty {
                        Section("Other") {
                            ForEach(workspace.otherAxes) { axis in
                                AxisRow(axis: axis).tag(axis.id)
                            }
                        }
                    }
                }

                Divider()
                HStack {
                    TextField("Host, IP or tcp:// URL", text: $newAxis)
                        .textFieldStyle(.roundedBorder)
                        .onSubmit(addAxis)
                    Button("Add", action: addAxis)
                        .disabled(newAxis.trimmingCharacters(in: .whitespaces).isEmpty)
                }
                .padding(10)
            }
            .navigationSplitViewColumnWidth(min: 220, ideal: 260)
        } detail: {
            if let axis = workspace.selectedAxis {
                AxisDetailView(axis: axis)
                    .id(axis.id)
            } else {
                ContentUnavailableView(
                    "Select a joint",
                    systemImage: "gearshape.2",
                    description: Text(workspace.browserState))
            }
        }
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button {
                    workspace.isShowingMoveTogether = true
                } label: {
                    Label("Move Together", systemImage: "arrow.left.and.right.circle")
                        .labelStyle(.titleAndIcon)
                }
                .help("Start a move on several axes at the same instant (⇧⌘M)")
                .disabled(!workspace.canMoveTogether)
            }
            ToolbarItem(placement: .primaryAction) {
                Button {
                    workspace.isPlayingTune ? workspace.stopTune() : workspace.playFrereJacques()
                } label: {
                    Label(workspace.isPlayingTune ? "Stop Song" : "Frère Jacques",
                          systemImage: workspace.isPlayingTune ? "stop.circle" : "music.note")
                        .labelStyle(.titleAndIcon)
                }
                .help("Play Frère Jacques as a round on the first two joints (drives must be enabled)")
                .disabled(!workspace.isPlayingTune && !workspace.canPlayTune)
            }
            ToolbarItem(placement: .primaryAction) {
                Button {
                    showingEvents.toggle()
                } label: {
                    Label(eventsTitle, systemImage: "list.bullet.rectangle")
                        .labelStyle(.titleAndIcon)
                }
                .help("Connections, reboots, faults and watchdog trips")
            }
            ToolbarItem(placement: .primaryAction) {
                Button(role: .destructive) {
                    workspace.stopAll()
                } label: {
                    Label("Stop All", systemImage: "stop.circle.fill")
                        .labelStyle(.titleAndIcon)
                }
                .tint(.red)
                .help("Stop every connected axis (⌘.)")
            }
        }
        .inspector(isPresented: $showingEvents) {
            EventsPanel(log: workspace.events)
                .inspectorColumnWidth(min: 280, ideal: 360)
        }
        .onChange(of: showingEvents) { _, showing in
            if showing { workspace.events.markSeen() }
        }
        .sheet(isPresented: $workspace.isShowingMoveTogether) {
            MoveTogetherView(workspace: workspace)
        }
    }

    private var eventsTitle: String {
        let unseen = workspace.events.unseenWarnings
        return unseen > 0 && !showingEvents ? "Events (\(unseen))" : "Events"
    }

    private func addAxis() {
        workspace.addAxis(newAxis)
        newAxis = ""
    }
}

private struct AxisRow: View {
    let axis: AxisModel

    var body: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(axis.stateColor)
                .frame(width: 8, height: 8)
            VStack(alignment: .leading, spacing: 2) {
                Text(axis.displayName)
                    .font(.body.weight(.medium))
                Text(axis.rowDetail)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
            Spacer()
            if axis.isDriveEnabled {
                Image(systemName: "bolt.fill")
                    .foregroundStyle(.green)
                    .help("Drive enabled")
            }
        }
        .padding(.vertical, 2)
    }
}

private struct MissingJointRow: View {
    let joint: Int

    var body: some View {
        HStack(spacing: 8) {
            Circle()
                .strokeBorder(.secondary, lineWidth: 1)
                .frame(width: 8, height: 8)
            VStack(alignment: .leading, spacing: 2) {
                Text("J\(joint)")
                Text("not found on the network")
                    .font(.caption)
            }
        }
        .foregroundStyle(.tertiary)
        .padding(.vertical, 2)
    }
}

private struct EventsPanel: View {
    let log: EventLog

    var body: some View {
        List(log.events.reversed()) { event in
            VStack(alignment: .leading, spacing: 2) {
                HStack {
                    Image(systemName: event.level.symbol)
                        .foregroundStyle(event.level.color)
                    Text(event.axis).font(.callout.weight(.medium))
                    Spacer()
                    Text(event.date, format: .dateTime.hour().minute().second())
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(.secondary)
                }
                Text(event.message)
                    .font(.callout)
                    .textSelection(.enabled)
            }
            .padding(.vertical, 2)
        }
        .overlay {
            if log.events.isEmpty {
                ContentUnavailableView("No events yet", systemImage: "list.bullet.rectangle")
            }
        }
        .safeAreaInset(edge: .bottom) {
            Text("Also logged to ~/Library/Logs/RobertaControl/events.log")
                .font(.caption)
                .foregroundStyle(.secondary)
                .padding(8)
        }
    }
}

extension RobotEvent.Level {
    var symbol: String {
        switch self {
        case .info: "info.circle"
        case .warning: "exclamationmark.triangle.fill"
        case .error: "xmark.octagon.fill"
        }
    }

    var color: Color {
        switch self {
        case .info: .secondary
        case .warning: .orange
        case .error: .red
        }
    }
}

extension AxisModel {
    var stateColor: Color {
        switch state {
        case .connected:
            return snapshot?.faultWord ?? 0 != 0 ? .orange : .green
        case .connecting, .reconnecting:
            return .yellow
        case .paused:
            return .gray
        }
    }

    var stateText: String {
        if isPaused {
            return "Paused"
        }
        switch state {
        case .connected:
            return "Connected"
        case .connecting, .paused:
            return "Connecting…"
        case let .reconnecting(attempt, reason):
            return "Reconnecting (attempt \(attempt)): \(reason)"
        }
    }

    var rowDetail: String {
        switch state {
        case .connected:
            return snapshot.map { "\($0.positionSteps) steps" } ?? name
        case .reconnecting:
            return "reconnecting…"
        case .connecting:
            return "connecting…"
        case .paused:
            return isPaused ? "paused" : name
        }
    }
}
