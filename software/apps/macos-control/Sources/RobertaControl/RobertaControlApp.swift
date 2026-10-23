import AppKit
import SwiftUI

@main
struct RobertaControlApp: App {
    @State private var workspace = Workspace()

    init() {
        // Run as a regular foreground app under `swift run` too.
        NSApplication.shared.setActivationPolicy(.regular)
    }

    var body: some Scene {
        WindowGroup("Roberta Control") {
            ContentView(workspace: workspace)
                .frame(minWidth: 900, minHeight: 640)
                .onAppear {
                    workspace.startDiscovery()
                    NSApplication.shared.activate()
                    DevHooks.scheduleCaptureIfRequested()
                }
        }
        .commands {
            CommandMenu("Motion") {
                Button("Stop All Axes") { workspace.stopAll() }
                    .keyboardShortcut(".", modifiers: .command)
            }
        }
    }
}
