import AppKit

/// Environment switches for bench and development use:
///
/// - `ROBERTA_CONTROL_AXES=tcp://a,tcp://b` adds and connects these axes at
///   launch.
/// - `ROBERTA_CONTROL_NO_DISCOVERY=1` skips Bonjour, so tests with fake axes
///   leave the real boards alone.
/// - `ROBERTA_CONTROL_MOVE_TOGETHER=<steps>` enables every connected axis and
///   runs one coordinated move a few seconds after launch, tracing the result.
/// - `ROBERTA_CONTROL_TRACE=1` prints connection state changes and errors.
/// - `ROBERTA_CONTROL_CAPTURE=/path/shot.png` renders the main window to a PNG
///   a few seconds after launch (no screen-recording permission needed).
@MainActor
enum DevHooks {
    static let environment = ProcessInfo.processInfo.environment

    static var discoveryDisabled: Bool { environment["ROBERTA_CONTROL_NO_DISCOVERY"] == "1" }

    static func trace(_ message: @autoclosure () -> String) {
        if environment["ROBERTA_CONTROL_TRACE"] == "1" {
            FileHandle.standardError.write(Data("[trace] \(message())\n".utf8))
        }
    }

    static func addRequestedAxes(to workspace: Workspace) {
        guard let list = environment["ROBERTA_CONTROL_AXES"] else { return }
        for endpoint in list.split(separator: ",") {
            workspace.addAxis(String(endpoint))
        }
    }

    static func scheduleMoveTogetherIfRequested(_ workspace: Workspace) {
        guard let steps = Int32(environment["ROBERTA_CONTROL_MOVE_TOGETHER"] ?? "") else { return }

        Task { @MainActor in
            try? await Task.sleep(for: .seconds(2))
            let axes = workspace.axes.filter(\.isConnected)
            axes.forEach { $0.enableDrive() }
            try? await Task.sleep(for: .seconds(1))
            let results = await workspace.moveTogether(axes.map { ($0, steps) }, lead: .milliseconds(300))
            for result in results {
                trace("move together \(result.name): start \(result.startOffsetMs.map { String(format: "%+.3f ms", $0) } ?? "-")"
                      + " rtt \(result.roundTripUs.map(String.init) ?? "-") us \(result.error ?? "")")
            }
        }
    }

    static func scheduleCaptureIfRequested() {
        guard let path = environment["ROBERTA_CONTROL_CAPTURE"] else { return }
        let delay = Double(environment["ROBERTA_CONTROL_CAPTURE_DELAY"] ?? "") ?? 4

        Task { @MainActor in
            try? await Task.sleep(for: .seconds(delay))
            guard let view = NSApplication.shared.windows.first(where: \.isVisible)?.contentView?.superview,
                  let layer = view.layer else {
                return
            }
            let scale = view.window?.backingScaleFactor ?? 2
            let size = view.bounds.size
            guard let context = CGContext(
                data: nil, width: Int(size.width * scale), height: Int(size.height * scale),
                bitsPerComponent: 8, bytesPerRow: 0, space: CGColorSpaceCreateDeviceRGB(),
                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
                return
            }
            context.scaleBy(x: scale, y: scale)
            if view.isFlipped {
                context.translateBy(x: 0, y: size.height)
                context.scaleBy(x: 1, y: -1)
            }
            layer.render(in: context)
            guard let image = context.makeImage() else { return }
            let bitmap = NSBitmapImageRep(cgImage: image)
            try? bitmap.representation(using: .png, properties: [:])?.write(to: URL(fileURLWithPath: path))
        }
    }
}
