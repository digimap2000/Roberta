# Pendant Application

This directory contains the Roberta pendant application codebase. The same app
logic can be built in two modes:

- embedded Linux pendant target for the real device
- macOS development runner for local iteration in an SDL window

Both targets load the STL meshes under `software/urdf/meshes`, apply the robot
joint hierarchy, and render the assembled model through the same pendant code.
Bundled UI fonts live under `assets/fonts/` and currently use Geist Regular and
Geist Mono Regular.

## Dependencies

For the macOS dev runner, install SDL2:

```bash
brew install sdl2
```

For the embedded Linux target on Debian Trixie, install at least:

```bash
sudo apt install build-essential cmake ninja-build pkg-config libsdl2-dev libgles2-mesa-dev
```

## Build And Run On macOS

From the repository root:

```bash
cmake -S software -B build/software/pendant-macos-dev -G Ninja -DROBERTA_BUILD_PENDANT_MACOS_DEV=ON
cmake --build build/software/pendant-macos-dev --target roberta_pendant_macos_dev
./build/software/pendant-macos-dev/apps/pendant/roberta_pendant_macos_dev
```

You can also use the preset:

```bash
cd software
cmake --preset pendant-macos-dev
cmake --build --preset pendant-macos-dev
../build/software/pendant-macos-dev/apps/pendant/roberta_pendant_macos_dev
```

## Build For Embedded Linux

From the repository root:

```bash
cmake -S software -B build/software/pendant-embedded-linux -G Ninja -DROBERTA_BUILD_PENDANT_EMBEDDED_LINUX=ON
cmake --build build/software/pendant-embedded-linux --target roberta_pendant_embedded_linux
```

## Cross Compile With Docker

The repository includes a Debian Trixie cross-build image for `aarch64` Linux:

```bash
software/apps/pendant/docker/cross-build.sh
```

That script builds `software/apps/pendant/docker/Dockerfile`, mounts the
repository into the container, and runs the
`pendant-embedded-linux-aarch64-docker` CMake preset. The resulting binary lands
in:

```bash
build/software/pendant-embedded-linux-aarch64-docker/apps/pendant/roberta_pendant_embedded_linux
```

The build also copies the `urdf/` directory next to the binary, so deploy the
binary together with the sibling `urdf` and `assets` folders or set
`ROBERTA_ASSET_ROOT` and `ROBERTA_PENDANT_ASSET_ROOT` on the target device.

To run the same steps manually:

```bash
docker build --platform linux/amd64 -f software/apps/pendant/docker/Dockerfile -t roberta-pendant-cross:trixie software
docker run --rm -it --platform linux/amd64 -v "$PWD:/workspace" -w /workspace/software roberta-pendant-cross:trixie \
  bash -lc 'cmake --preset pendant-embedded-linux-aarch64-docker && cmake --build --preset pendant-embedded-linux-aarch64-docker'
```

## Run On Embedded Linux

On a desktop Linux session:

```bash
./build/software/pendant-embedded-linux/apps/pendant/roberta_pendant_embedded_linux
```

On a headless Raspberry Pi using direct KMS/DRM output, SDL often needs an
explicit video backend:

```bash
SDL_VIDEODRIVER=kmsdrm ./build/software/pendant-embedded-linux/apps/pendant/roberta_pendant_embedded_linux
```

## Controls

- `Left` / keypad `4`: orbit left
- `Right` / keypad `6`: orbit right
- `Up` / keypad `8`: orbit up
- `Down` / keypad `2`: orbit down
- `Period`: zoom in
- `Minus`: zoom out
- `Space`: pause or resume robot motion
- `R`: reset the camera
- `Esc` or `Q`: quit
