import Foundation

/// Plays Frère Jacques as a two-part round: a stepper's whine is pitched at
/// its step rate, so each note is "set the speed to the note's frequency and
/// move for its length". Commands go over the normal console links.
enum FrereJacques {
    static let tempoBPM = 120.0
    /// The second voice enters this many beats behind, an octave lower.
    static let entryBeats = 8.0
    static let repeats = 2

    /// (semitones from A4, beats)
    static let melody: [(Int, Double)] = [
        (-9, 1), (-7, 1), (-5, 1), (-9, 1),
        (-9, 1), (-7, 1), (-5, 1), (-9, 1),
        (-5, 1), (-4, 1), (-2, 2),
        (-5, 1), (-4, 1), (-2, 2),
        (-2, 0.5), (0, 0.5), (-2, 0.5), (-4, 0.5), (-5, 1), (-9, 1),
        (-2, 0.5), (0, 0.5), (-2, 0.5), (-4, 0.5), (-5, 1), (-9, 1),
        (-9, 1), (-14, 1), (-9, 2),
        (-9, 1), (-14, 1), (-9, 2),
    ]

    struct Note {
        let beat: Double
        let voice: Int
        let hertz: Double
        let seconds: Double
    }

    static func score() -> [Note] {
        let beat = 60 / tempoBPM
        var notes: [Note] = []
        for voice in 0..<2 {
            var at = Double(voice) * entryBeats
            let octave = voice == 0 ? 0 : -12
            for _ in 0..<repeats {
                for (semitones, beats) in melody {
                    let hertz = 440 * pow(2, Double(semitones + octave) / 12)
                    notes.append(Note(beat: at, voice: voice, hertz: hertz, seconds: beats * beat * 0.88))
                    at += beats
                }
            }
        }
        return notes.sorted { $0.beat < $1.beat }
    }
}

extension AxisLink {
    /// Reads the staged velocity and acceleration so they can be restored.
    func stagedProfile() throws -> (velocity: Int, accel: Int) {
        let reply = try command("reg readm STAGED_TARGET_VELOCITY_STEPS_S STAGED_TARGET_ACCEL_STEPS_S2")
        let values = reply.matches(of: /i=(-?\d+)/).compactMap { Int($0.1) }
        guard values.count == 2 else { throw AxisError(message: "unexpected profile reply: \(reply)") }
        return (values[0], values[1])
    }

    func playTone(hertz: Int, steps: Int) throws {
        _ = try command("vel \(hertz)")
        _ = try command("move \(steps)")
    }

    func restoreProfile(velocity: Int, accel: Int) throws {
        _ = try command("stop")
        _ = try command("vel \(velocity)")
        _ = try command("accel \(accel)")
    }
}

extension Workspace {
    /// The two lowest-numbered connected joints with their drives on.
    var tuneVoices: [AxisModel] {
        let ready = axes.filter { $0.isConnected && $0.isDriveEnabled }
        return Array(ready.sorted { ($0.joint ?? .max, $0.name) < ($1.joint ?? .max, $1.name) }.prefix(2))
    }

    var canPlayTune: Bool { tuneVoices.count == 2 && !isPlayingTune }

    func playFrereJacques() {
        let voices = tuneVoices
        let links = voices.compactMap(\.coordinationLink)
        guard links.count == 2, !isPlayingTune else { return }

        isPlayingTune = true
        events.record(.info, axis: voices.map(\.displayName).joined(separator: " + "), "playing Frère Jacques")
        tuneTask = Task {
            var saved: [(velocity: Int, accel: Int)] = []
            do {
                for link in links {
                    saved.append(try await link.stagedProfile())
                    _ = try await link.command("accel 200000")
                }

                let beat = 60 / FrereJacques.tempoBPM
                let start = ContinuousClock.now + .milliseconds(300)
                var directions = [1, 1]
                for note in FrereJacques.score() {
                    try await Task.sleep(until: start + .seconds(note.beat * beat), clock: .continuous)
                    // Alternate direction so the shafts stay roughly where they started.
                    directions[note.voice] = -directions[note.voice]
                    let steps = directions[note.voice] * max(1, Int((note.hertz * note.seconds).rounded()))
                    let link = links[note.voice]
                    Task { try? await link.playTone(hertz: Int(note.hertz.rounded()), steps: steps) }
                }
                try await Task.sleep(for: .seconds(2 * beat))
            } catch {
                if !(error is CancellationError) {
                    events.record(.warning, axis: "Tune", "stopped: \(error.localizedDescription)")
                }
            }

            for (link, profile) in zip(links, saved) {
                try? await link.restoreProfile(velocity: profile.velocity, accel: profile.accel)
            }
            isPlayingTune = false
            tuneTask = nil
        }
    }

    func stopTune() {
        tuneTask?.cancel()
    }
}
