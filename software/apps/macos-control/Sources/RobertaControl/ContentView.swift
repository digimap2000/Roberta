import SwiftUI

struct ContentView: View {
    @Bindable var workspace: Workspace
    @State private var newAxis = ""
    @State private var showingMoveTogether = false

    var body: some View {
        NavigationSplitView {
            VStack(spacing: 0) {
                List(workspace.axes, selection: $workspace.selection) { axis in
                    AxisRow(axis: axis)
                }
                .overlay {
                    if workspace.axes.isEmpty {
                        ContentUnavailableView(
                            "No axes yet",
                            systemImage: "antenna.radiowaves.left.and.right",
                            description: Text(workspace.browserState))
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
                    "Select an axis",
                    systemImage: "gearshape.2",
                    description: Text("Axes advertising _roberta-axis._tcp appear on the left."))
            }
        }
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button {
                    showingMoveTogether = true
                } label: {
                    Label("Move Together", systemImage: "arrow.left.and.right.circle")
                }
                .help("Start a move on several axes at the same instant")
                .disabled(workspace.axes.filter(\.isConnected).count < 2)
            }
            ToolbarItem(placement: .primaryAction) {
                Button(role: .destructive) {
                    workspace.stopAll()
                } label: {
                    Label("Stop All", systemImage: "stop.circle.fill")
                }
                .tint(.red)
                .help("Stop every connected axis (⌘.)")
            }
        }
        .sheet(isPresented: $showingMoveTogether) {
            MoveTogetherView(workspace: workspace)
        }
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
                Text(axis.name)
                    .font(.body.weight(.medium))
                Text(axis.endpoint)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
            Spacer()
            if axis.isAdvertised {
                Image(systemName: "dot.radiowaves.left.and.right")
                    .foregroundStyle(.secondary)
                    .help("Advertised on the network")
            }
        }
        .padding(.vertical, 2)
    }
}

extension AxisModel {
    var stateColor: Color {
        switch state {
        case .connected:
            return snapshot?.faultWord ?? 0 != 0 ? .orange : .green
        case .connecting:
            return .yellow
        case .failed:
            return .red
        case .disconnected:
            return .gray
        }
    }

    var stateText: String {
        switch state {
        case .connected:
            return "Connected"
        case .connecting:
            return "Connecting…"
        case let .failed(message):
            return message
        case .disconnected:
            return "Disconnected"
        }
    }
}
