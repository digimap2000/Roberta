import AppKit

/// Environment switches for bench and development use:
///
/// - `ROBERTA_CONTROL_AUTOCONNECT=1` connects to every advertised axis.
/// - `ROBERTA_CONTROL_TRACE=1` prints connection state changes and errors.
/// - `ROBERTA_CONTROL_CAPTURE=/path/shot.png` renders the main window to a PNG
///   a few seconds after launch (no screen-recording permission needed).
@MainActor
enum DevHooks {
    static let environment = ProcessInfo.processInfo.environment

    static var autoConnect: Bool { environment["ROBERTA_CONTROL_AUTOCONNECT"] == "1" }

    static func trace(_ message: @autoclosure () -> String) {
        if environment["ROBERTA_CONTROL_TRACE"] == "1" {
            FileHandle.standardError.write(Data("[trace] \(message())\n".utf8))
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
