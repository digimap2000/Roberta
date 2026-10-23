// swift-tools-version: 6.0
import Foundation
import PackageDescription

// The app links the C ABI facade over software/core (libroberta_desktop_ffi),
// built by the software/ CMake "release" preset.
let packageDir = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
let repoDir = packageDir
    .deletingLastPathComponent() // apps
    .deletingLastPathComponent() // software
    .deletingLastPathComponent()
let ffiLibDir = ProcessInfo.processInfo.environment["ROBERTA_FFI_LIB_DIR"]
    ?? repoDir.appendingPathComponent("build/software/release/core").path
let infoPlist = packageDir.appendingPathComponent("Support/Info.plist").path

let package = Package(
    name: "RobertaControl",
    platforms: [.macOS("26.0")],
    targets: [
        .systemLibrary(name: "CRobertaFFI", path: "Sources/CRobertaFFI"),
        .executableTarget(
            name: "RobertaControl",
            dependencies: ["CRobertaFFI"],
            linkerSettings: [
                .unsafeFlags([
                    "-L", ffiLibDir,
                    "-Xlinker", "-rpath", "-Xlinker", ffiLibDir,
                    "-Xlinker", "-rpath", "-Xlinker", "@executable_path/../Frameworks",
                    // Embed Info.plist so Bonjour and local-network access
                    // work under `swift run` as well as in the app bundle.
                    "-Xlinker", "-sectcreate",
                    "-Xlinker", "__TEXT",
                    "-Xlinker", "__info_plist",
                    "-Xlinker", infoPlist,
                ]),
            ]
        ),
    ]
)
