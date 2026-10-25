import CRobertaFFI
import Foundation

/// How hard to stop. Every level but kill goes through the controller's
/// motion governor, so the arm is ramped down within its limits.
enum StopLevel: UInt8 {
    /// Ramp down at the move's own deceleration, then hold.
    case stop = 0
    /// Ramp down at the axis's quick-stop deceleration, then hold.
    case quick = 1
    /// Drive off at once. The joint is no longer held and the position is
    /// lost: for emergencies, alongside the hardware e-stop.
    case kill = 2
}

struct AxisError: LocalizedError {
    let message: String

    var errorDescription: String? { message }
}

struct AxisSnapshot: Equatable, Sendable {
    /// Soft limits are active only when min < max.
    var softLimitsActive: Bool { softLimitMin < softLimitMax }

    var axisId: UInt32
    var statusWord: UInt32
    var faultWord: UInt32
    var lastErrorCode: UInt32
    var positionSteps: Int32
    var targetSteps: Int32
    var velocityStepsPerSecond: Int32
    var stepCount: Int32
    var softLimitMin: Int32
    var softLimitMax: Int32
    var positionDegrees: Double
    var targetDegrees: Double
    var profileVelocity: Int32
    var profileAccel: Int32
    var profileHomeVelocity: Int32
    var outputsEnabled: Bool
    var motorEnabled: Bool
    var homed: Bool
    var moving: Bool
    var homing: Bool
    var homeSwitch: Bool
    var limitA: Bool
    var limitB: Bool
    var startScheduled: Bool

    init(_ raw: roberta_axis_snapshot_t) {
        axisId = raw.axis_id
        statusWord = raw.status_word
        faultWord = raw.fault_word
        lastErrorCode = raw.last_error_code
        positionSteps = raw.current_position_steps
        targetSteps = raw.target_position_steps
        velocityStepsPerSecond = raw.current_velocity_steps_per_second
        stepCount = raw.step_count
        softLimitMin = raw.min_steps
        softLimitMax = raw.max_steps
        positionDegrees = raw.current_position_deg
        targetDegrees = raw.target_position_deg
        profileVelocity = raw.velocity_steps_per_second
        profileAccel = raw.accel_steps_per_second_squared
        profileHomeVelocity = raw.home_velocity_steps_per_second
        outputsEnabled = raw.outputs_enabled != 0
        motorEnabled = raw.motor_enabled != 0
        homed = raw.homed != 0
        moving = raw.moving != 0
        homing = raw.homing != 0
        homeSwitch = raw.limit_home != 0
        limitA = raw.limit_a != 0
        limitB = raw.limit_b != 0
        startScheduled = raw.status_word & (1 << 14) != 0
    }
}

/// One connection to an axis controller through the C++ core. The core client
/// is blocking and not thread-safe, so the actor serialises every call.
actor AxisLink {
    let endpoint: String
    private var handle: ClientHandle?

    init(endpoint: String) {
        self.endpoint = endpoint
    }

    func connect() throws {
        if handle == nil {
            handle = ClientHandle(endpoint: endpoint)
        }
        let client = try requireClient()
        try call { roberta_axis_client_connect(client, $0, $1) }
    }

    func disconnect() {
        if let handle {
            roberta_axis_client_disconnect(handle.pointer)
        }
    }

    func snapshot() throws -> AxisSnapshot {
        let client = try requireClient()
        var raw = roberta_axis_snapshot_t()
        try call { roberta_axis_client_snapshot(client, &raw, $0, $1) }
        return AxisSnapshot(raw)
    }

    func enableDrive() throws {
        let client = try requireClient()
        try call { roberta_axis_client_enable_drive(client, $0, $1) }
    }

    func disableDrive() throws {
        let client = try requireClient()
        try call { roberta_axis_client_disable_drive(client, $0, $1) }
    }

    func stop(_ level: StopLevel = .stop) throws {
        let client = try requireClient()
        try call { roberta_axis_client_stop(client, level.rawValue, $0, $1) }
    }

    func home() throws {
        let client = try requireClient()
        try call { roberta_axis_client_home(client, $0, $1) }
    }

    func clearFaults() throws {
        let client = try requireClient()
        try call { roberta_axis_client_clear_faults(client, $0, $1) }
    }

    func setZero() throws {
        let client = try requireClient()
        try call { roberta_axis_client_set_zero(client, $0, $1) }
    }

    func moveBy(steps: Int32) throws {
        let client = try requireClient()
        try call { roberta_axis_client_move_by_steps(client, steps, $0, $1) }
    }

    func moveTo(steps: Int32) throws {
        let client = try requireClient()
        try call { roberta_axis_client_move_to_steps(client, steps, $0, $1) }
    }

    func setMotionProfile(velocity: Int32, accel: Int32, homeVelocity: Int32) throws {
        let client = try requireClient()
        try call {
            roberta_axis_client_set_motion_profile(client, velocity, accel, homeVelocity, $0, $1)
        }
    }

    /// The host clock that coordinated start times are expressed in.
    static func hostClockMicroseconds() -> Int64 {
        roberta_host_clock_us()
    }

    /// Estimates this axis's clock offset; returns the round trip in µs. The
    /// estimate is good to about half of that.
    func syncClock() throws -> UInt32 {
        let client = try requireClient()
        var roundTrip: UInt32 = 0
        try call { roberta_axis_client_sync_clock(client, &roundTrip, $0, $1) }
        return roundTrip
    }

    func moveBy(steps: Int32, atHostTime hostTime: Int64) throws {
        let client = try requireClient()
        try call { roberta_axis_client_move_by_steps_at(client, steps, hostTime, $0, $1) }
    }

    /// When the most recent move actually started, in host clock µs.
    func lastStartHostTime() throws -> Int64 {
        let client = try requireClient()
        var hostTime: Int64 = 0
        try call { roberta_axis_client_last_start_host_us(client, &hostTime, $0, $1) }
        return hostTime
    }

    /// Sends a raw console line, such as "net status", and returns the reply.
    func command(_ line: String) throws -> String {
        let client = try requireClient()
        var response = [CChar](repeating: 0, count: 8192)
        try response.withUnsafeMutableBufferPointer { buffer in
            try call { error, errorLength in
                line.withCString { roberta_axis_client_command(client, $0, buffer.baseAddress, buffer.count, error, errorLength) }
            }
        }
        return decodeCString(response)
    }

    private func requireClient() throws -> OpaquePointer {
        guard let handle else {
            throw AxisError(message: "cannot open \(endpoint)")
        }
        return handle.pointer
    }

    private func call(_ body: (UnsafeMutablePointer<CChar>, Int) -> UInt8) throws {
        var error = [CChar](repeating: 0, count: 512)
        let succeeded = error.withUnsafeMutableBufferPointer { body($0.baseAddress!, $0.count) }
        if succeeded == 0 {
            let message = decodeCString(error)
            throw AxisError(message: message.isEmpty ? "axis request failed" : message)
        }
    }
}

/// Owns the core client. Only ever touched from inside AxisLink.
private final class ClientHandle: @unchecked Sendable {
    let pointer: OpaquePointer

    init?(endpoint: String) {
        guard let pointer = endpoint.withCString({ roberta_axis_client_new($0, 115_200) }) else {
            return nil
        }
        self.pointer = pointer
    }

    deinit {
        roberta_axis_client_free(pointer)
    }
}

private func decodeCString(_ buffer: [CChar]) -> String {
    let bytes = buffer.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }
    return String(decoding: bytes, as: UTF8.self)
}
