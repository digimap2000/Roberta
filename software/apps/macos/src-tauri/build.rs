use std::env;
use std::path::PathBuf;
use std::process::Command;

fn run(command: &mut Command) {
    let status = command.status().expect("failed to spawn command");
    assert!(status.success(), "command failed with status {status}");
}

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("missing manifest dir"));
    let software_dir = manifest_dir
        .join("../../..")
        .canonicalize()
        .expect("failed to resolve software directory");
    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("missing out dir"));
    let cpp_build_dir = out_dir.join("cpp-build");

    let profile = env::var("PROFILE").unwrap_or_else(|_| "debug".to_string());
    let cmake_build_type = if profile.eq_ignore_ascii_case("release") {
        "Release"
    } else {
        "Debug"
    };

    run(
        Command::new("cmake")
            .arg("-S")
            .arg(&software_dir)
            .arg("-B")
            .arg(&cpp_build_dir)
            .arg(format!("-DCMAKE_BUILD_TYPE={cmake_build_type}"))
            .arg("-DROBERTA_BUILD_CLI=OFF")
            .arg("-DROBERTA_BUILD_IOS_APP=OFF")
            .arg("-DROBERTA_BUILD_PENDANT_EMBEDDED_LINUX=OFF")
            .arg("-DROBERTA_BUILD_PENDANT_MACOS_DEV=OFF")
            .arg("-DROBERTA_ENABLE_SANITIZERS=OFF")
            .arg("-DBUILD_TESTING=OFF"),
    );

    run(
        Command::new("cmake")
            .arg("--build")
            .arg(&cpp_build_dir)
            .arg("--target")
            .arg("roberta_desktop_ffi"),
    );

    let native_dir = cpp_build_dir.join("core");

    println!("cargo:rustc-link-search=native={}", native_dir.display());
    println!("cargo:rustc-link-lib=dylib=roberta_desktop_ffi");
    println!(
        "cargo:rustc-link-arg=-Wl,-rpath,{}",
        native_dir.display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        software_dir.join("core").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        software_dir.join("CMakeLists.txt").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        software_dir.join("cmake/RobertaDts.cmake").display()
    );

    tauri_build::build();
}
