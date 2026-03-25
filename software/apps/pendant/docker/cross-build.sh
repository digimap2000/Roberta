#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
SOFTWARE_ROOT="${REPO_ROOT}/software"

IMAGE_TAG="${ROBERTA_PENDANT_IMAGE:-roberta-pendant-cross:trixie}"
CONFIGURE_PRESET="${ROBERTA_PENDANT_PRESET:-pendant-embedded-linux-aarch64-docker}"
DOCKER_PLATFORM="${ROBERTA_PENDANT_DOCKER_PLATFORM:-linux/amd64}"

docker build \
    --platform "${DOCKER_PLATFORM}" \
    -f "${SOFTWARE_ROOT}/apps/pendant/docker/Dockerfile" \
    -t "${IMAGE_TAG}" \
    "${SOFTWARE_ROOT}"

docker run --rm -t \
    --platform "${DOCKER_PLATFORM}" \
    -v "${REPO_ROOT}:/workspace" \
    -w /workspace/software \
    "${IMAGE_TAG}" \
    bash -lc "cmake --preset ${CONFIGURE_PRESET} && cmake --build --preset ${CONFIGURE_PRESET}"
