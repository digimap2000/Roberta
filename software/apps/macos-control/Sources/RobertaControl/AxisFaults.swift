/// Plain-language descriptions of the controller's fault word and last error
/// code (docs/axis-controller/registers.md).
enum AxisFaults {
    static func describe(faultWord: UInt32, lastErrorCode: UInt32) -> String {
        if let reason = errorDescriptions[lastErrorCode] {
            return reason
        }
        let names = faultNames.enumerated()
            .filter { faultWord & (1 << UInt32($0.offset)) != 0 }
            .map(\.element)
        return names.isEmpty ? "Fault (error \(lastErrorCode))" : names.joined(separator: ", ")
    }

    private static let errorDescriptions: [UInt32: String] = [
        1: "Invalid command or value",
        2: "Write to a read-only register",
        3: "Command not supported by this firmware",
        101: "Move refused: drive not enabled",
        102: "Absolute move refused: axis not homed or zeroed",
        103: "Move refused: axis busy",
        104: "Move refused: target is past a soft limit",
        105: "Move refused: limit switch active in that direction",
        106: "Scheduled start refused: start time had already passed",
        107: "Scheduled start refused: start time too far ahead",
    ]

    private static let faultNames = [
        "Invalid command",
        "Motion while disabled",
        "Move before homed",
        "Soft limit",
        "Unexpected limit switch",
        "Homing timeout",
        "Brake interlock",
        "Motion aborted",
    ]
}
