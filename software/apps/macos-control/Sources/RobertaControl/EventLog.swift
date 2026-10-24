import Foundation
import Observation

struct RobotEvent: Identifiable {
    enum Level: String {
        case info
        case warning
        case error
    }

    let id = UUID()
    let date: Date
    let axis: String
    let level: Level
    let message: String
}

/// Everything notable that happened to the axes this session: connections,
/// reboots and why, faults, watchdog trips. Also appended to
/// ~/Library/Logs/RobertaControl/events.log so crashes can be traced later.
@MainActor
@Observable
final class EventLog {
    static let capacity = 500

    private(set) var events: [RobotEvent] = []
    private(set) var unseenWarnings = 0

    private let file: FileHandle?
    private static let timestamp: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd HH:mm:ss.SSS"
        return formatter
    }()

    init() {
        let directory = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Logs/RobertaControl", isDirectory: true)
        let url = directory.appendingPathComponent("events.log")
        try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        if !FileManager.default.fileExists(atPath: url.path) {
            FileManager.default.createFile(atPath: url.path, contents: nil)
        }
        file = try? FileHandle(forWritingTo: url)
        _ = try? file?.seekToEnd()
    }

    func record(_ level: RobotEvent.Level, axis: String, _ message: String) {
        let event = RobotEvent(date: Date(), axis: axis, level: level, message: message)
        events.append(event)
        if events.count > Self.capacity {
            events.removeFirst(events.count - Self.capacity)
        }
        if level != .info {
            unseenWarnings += 1
        }

        let line = "\(Self.timestamp.string(from: event.date)) \(level.rawValue.uppercased()) \(axis): \(message)\n"
        try? file?.write(contentsOf: Data(line.utf8))
        DevHooks.trace("event \(axis): \(message)")
    }

    func markSeen() {
        unseenWarnings = 0
    }
}
