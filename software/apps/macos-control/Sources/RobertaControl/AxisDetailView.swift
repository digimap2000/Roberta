import SwiftUI

struct AxisDetailView: View {
    let axis: AxisModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                header

                if let error = axis.lastError {
                    ErrorBanner(message: error) { axis.lastError = nil }
                }

                if axis.isConnected {
                    StatusPanel(axis: axis)
                    DrivePanel(axis: axis)
                    JogPanel(axis: axis)
                    ProfilePanel(axis: axis)
                    ConsolePanel(axis: axis)
                }
            }
            .padding(20)
            .frame(maxWidth: 820, alignment: .leading)
        }
        .navigationTitle(axis.displayName)
    }

    private var header: some View {
        HStack(alignment: .firstTextBaseline) {
            VStack(alignment: .leading, spacing: 4) {
                Text(axis.displayName)
                    .font(.title2.weight(.semibold))
                Text(axis.endpoint)
                    .font(.callout.monospaced())
                    .foregroundStyle(.secondary)
                    .textSelection(.enabled)
                HStack(spacing: 6) {
                    Circle().fill(axis.stateColor).frame(width: 8, height: 8)
                    Text(axis.stateText)
                    if let roundTrip = axis.roundTripMs {
                        Text("· \(roundTrip, format: .number.precision(.fractionLength(0))) ms snapshot")
                            .foregroundStyle(.secondary)
                    }
                }
                .font(.callout)
                if axis.isConnected, !axis.netStatus.isEmpty {
                    Text(axis.netStatus)
                        .font(.caption.monospaced())
                        .foregroundStyle(.secondary)
                        .textSelection(.enabled)
                }
            }
            Spacer()
            if axis.isPaused {
                Button("Resume") { axis.resume() }
                    .help("Reconnect and keep this axis connected")
            } else {
                if !axis.isConnected {
                    ProgressView().controlSize(.small)
                }
                Button("Pause") { axis.pause() }
                    .help("Disconnect and stop reconnecting, e.g. to use the board's USB console")
            }
        }
    }
}

// MARK: - Panels

private struct StatusPanel: View {
    let axis: AxisModel

    var body: some View {
        GroupBox("Status") {
            if let snapshot = axis.snapshot {
                VStack(alignment: .leading, spacing: 12) {
                    Grid(alignment: .leading, horizontalSpacing: 24, verticalSpacing: 6) {
                        GridRow {
                            Metric(title: "Position", value: "\(snapshot.positionSteps) steps",
                                   detail: degrees(snapshot.positionDegrees))
                            Metric(title: "Target", value: "\(snapshot.targetSteps) steps",
                                   detail: degrees(snapshot.targetDegrees))
                            Metric(title: "Velocity", value: "\(snapshot.velocityStepsPerSecond) steps/s", detail: nil)
                            Metric(title: "Steps emitted", value: "\(snapshot.stepCount)", detail: nil)
                        }
                    }

                    FlowFlags(flags: [
                        ("Outputs", snapshot.outputsEnabled, .green),
                        ("Motor", snapshot.motorEnabled, .green),
                        ("Homed", snapshot.homed, .blue),
                        ("Moving", snapshot.moving, .blue),
                        ("Homing", snapshot.homing, .blue),
                        ("Home switch", snapshot.homeSwitch, .orange),
                        ("Limit A", snapshot.limitA, .orange),
                        ("Limit B", snapshot.limitB, .orange),
                        ("Armed", snapshot.startScheduled, .purple),
                    ])

                    if let fault = axis.faultDescription {
                        HStack {
                            Label(fault, systemImage: "exclamationmark.triangle.fill")
                                .foregroundStyle(.orange)
                                .help(String(format: "Fault word 0x%08X, last error %u",
                                             snapshot.faultWord, snapshot.lastErrorCode))
                            Button("Clear") { axis.clearFaults() }
                                .controlSize(.small)
                        }
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(6)
            } else {
                ProgressView().frame(maxWidth: .infinity)
            }
        }
    }

    private func degrees(_ value: Double) -> String {
        value.formatted(.number.precision(.fractionLength(2))) + "°"
    }
}

private struct DrivePanel: View {
    let axis: AxisModel

    var body: some View {
        GroupBox("Drive") {
            HStack(spacing: 10) {
                if axis.snapshot?.motorEnabled == true {
                    Button("Disable drive") { axis.disableDrive() }
                } else {
                    Button("Enable drive") { axis.enableDrive() }
                }
                Button("Home") { axis.home() }
                    .disabled(!axis.isDriveEnabled)
                    .help(axis.isDriveEnabled
                          ? "Seek the home switch in the negative direction"
                          : "Enable the drive to home")
                Button("Set zero here") { axis.setZero() }
                    .help("Call the current position zero and mark the axis homed")
                Button("Clear faults") { axis.clearFaults() }
                Spacer()
                Button(role: .destructive) {
                    axis.stop()
                } label: {
                    Label("STOP", systemImage: "stop.fill")
                }
                .buttonStyle(StopButtonStyle())
            }
            .padding(6)
        }
    }
}

private struct JogPanel: View {
    let axis: AxisModel

    @State private var stepSize: Int32 = 100
    @State private var target = ""

    private let stepSizes: [Int32] = [1, 10, 100, 1_000, 10_000]
    /// Far enough that a held jog runs until release; the controller's link
    /// watchdog stops it if this app goes away mid-hold.
    private let holdDistance: Int32 = 10_000_000

    var body: some View {
        GroupBox("Jog") {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Picker("Step", selection: $stepSize) {
                        ForEach(stepSizes, id: \.self) { Text("\($0)").tag($0) }
                    }
                    .pickerStyle(.segmented)
                    .frame(maxWidth: 320)
                    Button { axis.move(by: -stepSize) } label: { Label("−\(stepSize)", systemImage: "minus") }
                    Button { axis.move(by: stepSize) } label: { Label("+\(stepSize)", systemImage: "plus") }
                }

                HStack {
                    HoldButton(title: "Hold to jog −", systemImage: "chevron.left.2") { pressed in
                        pressed ? axis.move(by: -holdDistance) : axis.stop()
                    }
                    HoldButton(title: "Hold to jog +", systemImage: "chevron.right.2") { pressed in
                        pressed ? axis.move(by: holdDistance) : axis.stop()
                    }
                    Spacer()
                    TextField("Target steps", text: $target)
                        .textFieldStyle(.roundedBorder)
                        .frame(width: 120)
                        .onSubmit(goTo)
                    Button("Go to", action: goTo)
                        .disabled(Int32(target) == nil || axis.snapshot?.homed != true)
                        .help("Absolute moves need the axis homed or zeroed")
                }

                if !axis.isDriveEnabled {
                    Text("Enable the drive to jog.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .disabled(!axis.isDriveEnabled)
            .padding(6)
        }
    }

    private func goTo() {
        if let steps = Int32(target) {
            axis.move(to: steps)
        }
    }
}

private struct ProfilePanel: View {
    let axis: AxisModel

    @State private var velocity = ""
    @State private var accel = ""
    @State private var homeVelocity = ""

    var body: some View {
        GroupBox("Motion profile") {
            HStack(spacing: 12) {
                LabeledField(title: "Velocity (steps/s)", text: $velocity)
                LabeledField(title: "Accel (steps/s²)", text: $accel)
                LabeledField(title: "Home velocity (steps/s)", text: $homeVelocity)
                Button("Apply") {
                    if let velocity = Int32(velocity), let accel = Int32(accel), let home = Int32(homeVelocity) {
                        axis.applyProfile(velocity: velocity, accel: accel, homeVelocity: home)
                    }
                }
                .disabled(Int32(velocity) == nil || Int32(accel) == nil || Int32(homeVelocity) == nil)
            }
            .padding(6)
        }
        .onAppear(perform: fillFromSnapshot)
        .onChange(of: axis.snapshot == nil) { fillFromSnapshot() }
    }

    private func fillFromSnapshot() {
        guard let snapshot = axis.snapshot, velocity.isEmpty else { return }
        velocity = "\(snapshot.profileVelocity)"
        accel = "\(snapshot.profileAccel)"
        homeVelocity = "\(snapshot.profileHomeVelocity)"
    }
}

private struct ConsolePanel: View {
    let axis: AxisModel

    @State private var line = ""

    var body: some View {
        GroupBox("Console") {
            VStack(alignment: .leading, spacing: 8) {
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(alignment: .leading, spacing: 6) {
                            ForEach(axis.console) { entry in
                                VStack(alignment: .leading, spacing: 2) {
                                    Text("axis> \(entry.command)")
                                        .foregroundStyle(.secondary)
                                    Text(entry.response)
                                        .foregroundStyle(entry.failed ? .red : .primary)
                                }
                                .id(entry.id)
                            }
                        }
                        .font(.caption.monospaced())
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .frame(height: 160)
                    .onChange(of: axis.console.last?.id) { _, id in
                        if let id { proxy.scrollTo(id, anchor: .bottom) }
                    }
                }
                HStack {
                    TextField("help, status, net status, reg dump…", text: $line)
                        .textFieldStyle(.roundedBorder)
                        .font(.body.monospaced())
                        .onSubmit(send)
                    Button("Send", action: send)
                }
            }
            .padding(6)
        }
    }

    private func send() {
        let command = line.trimmingCharacters(in: .whitespaces)
        guard !command.isEmpty else { return }
        axis.send(command)
        line = ""
    }
}

// MARK: - Pieces

private struct Metric: View {
    let title: String
    let value: String
    let detail: String?

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            Text(value).font(.title3.monospacedDigit())
            Text(detail ?? " ").font(.caption.monospacedDigit()).foregroundStyle(.secondary)
        }
    }
}

private struct FlowFlags: View {
    let flags: [(String, Bool, Color)]

    var body: some View {
        HStack(spacing: 6) {
            ForEach(flags, id: \.0) { title, active, color in
                Text(title)
                    .font(.caption.weight(.medium))
                    .padding(.horizontal, 8)
                    .padding(.vertical, 3)
                    .background(active ? color.opacity(0.2) : Color.secondary.opacity(0.08), in: Capsule())
                    .foregroundStyle(active ? color : .secondary)
            }
        }
    }
}

private struct LabeledField: View {
    let title: String
    @Binding var text: String

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            TextField(title, text: $text)
                .textFieldStyle(.roundedBorder)
                .labelsHidden()
                .frame(width: 140)
        }
    }
}

private struct HoldButton: View {
    let title: String
    let systemImage: String
    let onChange: (Bool) -> Void

    @Environment(\.isEnabled) private var isEnabled
    @State private var pressed = false

    var body: some View {
        Label(title, systemImage: systemImage)
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .background(pressed ? Color.accentColor.opacity(0.35) : Color.secondary.opacity(0.12),
                        in: RoundedRectangle(cornerRadius: 6))
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { _ in
                        if isEnabled, !pressed {
                            pressed = true
                            onChange(true)
                        }
                    }
                    .onEnded { _ in
                        guard pressed else { return }
                        pressed = false
                        onChange(false)
                    }
            )
            .opacity(isEnabled ? 1 : 0.4)
            .help("Moves while held; stops on release")
    }
}

private struct StopButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.headline)
            .foregroundStyle(.white)
            .frame(minWidth: 110, minHeight: 36)
            .background(Color.red.opacity(configuration.isPressed ? 0.7 : 1), in: RoundedRectangle(cornerRadius: 8))
            .contentShape(RoundedRectangle(cornerRadius: 8))
    }
}

private struct ErrorBanner: View {
    let message: String
    let dismiss: () -> Void

    var body: some View {
        HStack {
            Image(systemName: "exclamationmark.octagon.fill")
            Text(message).textSelection(.enabled)
            Spacer()
            Button("Dismiss", action: dismiss).buttonStyle(.borderless)
        }
        .padding(10)
        .foregroundStyle(.red)
        .background(Color.red.opacity(0.1), in: RoundedRectangle(cornerRadius: 8))
    }
}
