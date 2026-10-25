import Foundation

/// The motion governor's mechanical limits for one axis, as commissioned
/// (docs/axis-controller/motion-governor.md). Every motion the controller
/// makes stays within them, whatever is asked of it.
struct MotionLimits: Equatable {
    var velocity: Int
    var accel: Int
    var decel: Int
    var jerk: Int
    var quickStopDecel: Int
    /// Saved to the controller's flash; otherwise defaults are in force.
    var commissioned = false
    /// The controller had to adjust these to make them usable.
    var adjusted = false
    /// How far motion trails requests; stops travel about speed × half this
    /// further than they would without jerk limiting.
    var smoothingMs = 0

    static let registers = [
        "LIMIT_MAX_VELOCITY_STEPS_S",
        "LIMIT_MAX_ACCEL_STEPS_S2",
        "LIMIT_MAX_DECEL_STEPS_S2",
        "LIMIT_MAX_JERK_STEPS_S3",
        "LIMIT_QUICK_STOP_DECEL_STEPS_S2",
    ]

    var values: [Int] { [velocity, accel, decel, jerk, quickStopDecel] }
}

extension AxisLink {
    func motionLimits() throws -> MotionLimits {
        let names = MotionLimits.registers + ["MOTION_LIMITS_STATUS", "MOTION_SMOOTHING_MS"]
        let reply = try command("reg readm " + names.joined(separator: " "))
        let values = reply.matches(of: /i=(-?\d+)/).compactMap { Int($0.1) }
        guard values.count == names.count else {
            throw AxisError(message: "unexpected limits reply: \(reply)")
        }
        return MotionLimits(
            velocity: values[0], accel: values[1], decel: values[2], jerk: values[3], quickStopDecel: values[4],
            commissioned: values[5] & 1 != 0, adjusted: values[5] & 2 != 0, smoothingMs: values[6])
    }

    /// Writes the limits; they take effect when the axis is next at rest.
    func setMotionLimits(_ limits: MotionLimits) throws {
        for (name, value) in zip(MotionLimits.registers, limits.values) {
            try registerCommand("reg write \(name) \(value)")
        }
    }

    /// Persists limits, soft limits and home offset in the controller's flash.
    func saveAxisConfig() throws {
        try registerCommand("reg bitset CONTROL_WORD 0x400")
    }

    private func registerCommand(_ line: String) throws {
        let reply = try command(line)
        guard reply.split(whereSeparator: \.isNewline).first?.trimmingCharacters(in: .whitespaces) == "ok" else {
            throw AxisError(message: "\(line): \(reply.trimmingCharacters(in: .whitespacesAndNewlines))")
        }
    }
}
