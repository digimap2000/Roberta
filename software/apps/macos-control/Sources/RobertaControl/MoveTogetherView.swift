import SwiftUI

/// Runs one relative move on several axes, scheduled to start at the same
/// instant, and reports how closely the starts lined up.
struct MoveTogetherView: View {
    let workspace: Workspace
    @Environment(\.dismiss) private var dismiss

    @State private var included: Set<AxisModel.ID> = []
    @State private var steps: [AxisModel.ID: String] = [:]
    @State private var leadMs = 300
    @State private var running = false
    @State private var results: [CoordinatedResult] = []

    private var candidates: [AxisModel] {
        workspace.axes.filter(\.isConnected)
    }

    private var plannedMoves: [(axis: AxisModel, steps: Int32)] {
        candidates.compactMap { axis in
            guard included.contains(axis.id), let count = Int32(steps[axis.id] ?? "") else { return nil }
            return (axis, count)
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Move together")
                .font(.title2.weight(.semibold))
            Text("Each axis's clock is synced, then every move is scheduled for the same moment \(leadMs) ms ahead.")
                .font(.callout)
                .foregroundStyle(.secondary)

            if candidates.isEmpty {
                ContentUnavailableView("No connected axes", systemImage: "link",
                                       description: Text("Connect to two or more axes first."))
            } else {
                Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 8) {
                    GridRow {
                        Text("Axis").foregroundStyle(.secondary)
                        Text("Steps").foregroundStyle(.secondary)
                        Text("Drive").foregroundStyle(.secondary)
                    }
                    .font(.caption)
                    ForEach(candidates) { axis in
                        GridRow {
                            Toggle(axis.name, isOn: inclusion(for: axis))
                            TextField("Steps", text: stepsBinding(for: axis))
                                .textFieldStyle(.roundedBorder)
                                .frame(width: 100)
                            Text(axis.isDriveEnabled ? "Enabled" : "Disabled")
                                .foregroundStyle(axis.isDriveEnabled ? .green : .orange)
                        }
                    }
                }

                Stepper("Lead time: \(leadMs) ms", value: $leadMs, in: 100...2000, step: 50)
                    .frame(maxWidth: 260)
            }

            if !results.isEmpty {
                ResultsTable(results: results)
            }

            HStack {
                Button("Close") { dismiss() }
                    .keyboardShortcut(.cancelAction)
                Spacer()
                if running {
                    ProgressView().controlSize(.small)
                }
                Button("Start together", action: run)
                    .keyboardShortcut(.defaultAction)
                    .disabled(running || plannedMoves.count < 2
                              || plannedMoves.contains { !$0.axis.isDriveEnabled })
            }
        }
        .padding(24)
        .frame(width: 520)
        .onAppear {
            included = Set(candidates.map(\.id))
            for axis in candidates where steps[axis.id] == nil {
                steps[axis.id] = "200"
            }
        }
    }

    private func run() {
        let moves = plannedMoves
        running = true
        Task {
            results = await workspace.moveTogether(moves, lead: .milliseconds(leadMs))
            running = false
        }
    }

    private func inclusion(for axis: AxisModel) -> Binding<Bool> {
        Binding(
            get: { included.contains(axis.id) },
            set: { isOn in
                if isOn { included.insert(axis.id) } else { included.remove(axis.id) }
            })
    }

    private func stepsBinding(for axis: AxisModel) -> Binding<String> {
        Binding(get: { steps[axis.id] ?? "" }, set: { steps[axis.id] = $0 })
    }
}

private struct ResultsTable: View {
    let results: [CoordinatedResult]

    private var spreadMs: Double? {
        let offsets = results.compactMap(\.startOffsetMs)
        guard offsets.count == results.count, let low = offsets.min(), let high = offsets.max() else {
            return nil
        }
        return high - low
    }

    var body: some View {
        GroupBox("Last run") {
            VStack(alignment: .leading, spacing: 6) {
                ForEach(results) { result in
                    HStack {
                        Text(result.name).frame(width: 140, alignment: .leading)
                        if let error = result.error {
                            Text(error).foregroundStyle(.red)
                        } else if let offset = result.startOffsetMs {
                            Text("started \(offset, format: .number.sign(strategy: .always()).precision(.fractionLength(2))) ms")
                                .monospacedDigit()
                            if let roundTrip = result.roundTripUs {
                                Text("clock ±\(Double(roundTrip) / 2000, format: .number.precision(.fractionLength(2))) ms")
                                    .foregroundStyle(.secondary)
                                    .monospacedDigit()
                            }
                        }
                    }
                    .font(.callout)
                }
                if let spreadMs {
                    Divider()
                    Text("Spread between axes: \(spreadMs, format: .number.precision(.fractionLength(2))) ms")
                        .font(.callout.weight(.medium))
                        .monospacedDigit()
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(6)
        }
    }
}
