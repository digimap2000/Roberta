#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path


MAGIC = b"RBTMESH1"
VERSION = 1
HEADER_FORMAT = "<8sIIf"
VERTEX_FORMAT = "<8f"
QUANTIZE_EPSILON = 1.0e-6


def read_binary_stl(path: Path, scale: float) -> list[tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]]:
    triangles: list[tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]] = []
    with path.open("rb") as handle:
        handle.seek(80)
        triangle_count = struct.unpack("<I", handle.read(4))[0]
        for _ in range(triangle_count):
            raw_normal = struct.unpack("<3f", handle.read(12))
            vertex_a = tuple(component * scale for component in struct.unpack("<3f", handle.read(12)))
            vertex_b = tuple(component * scale for component in struct.unpack("<3f", handle.read(12)))
            vertex_c = tuple(component * scale for component in struct.unpack("<3f", handle.read(12)))
            handle.read(2)

            normal = normalize(raw_normal)
            if vector_length(normal) <= 1.0e-6:
                normal = normalize(cross(subtract(vertex_b, vertex_a), subtract(vertex_c, vertex_a)))
            triangles.append((normal, vertex_a, vertex_b, vertex_c))
    return triangles


def subtract(lhs: tuple[float, float, float], rhs: tuple[float, float, float]) -> tuple[float, float, float]:
    return (lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2])


def dot(lhs: tuple[float, float, float], rhs: tuple[float, float, float]) -> float:
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2]


def cross(lhs: tuple[float, float, float], rhs: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        lhs[1] * rhs[2] - lhs[2] * rhs[1],
        lhs[2] * rhs[0] - lhs[0] * rhs[2],
        lhs[0] * rhs[1] - lhs[1] * rhs[0],
    )


def vector_length(value: tuple[float, float, float]) -> float:
    return math.sqrt(dot(value, value))


def normalize(value: tuple[float, float, float]) -> tuple[float, float, float]:
    magnitude = vector_length(value)
    if magnitude <= 1.0e-8:
        return (0.0, 0.0, 0.0)
    return (value[0] / magnitude, value[1] / magnitude, value[2] / magnitude)


def quantize_key(vertex: tuple[float, float, float]) -> tuple[int, int, int]:
    return tuple(int(round(component / QUANTIZE_EPSILON)) for component in vertex)


def surface_variation(vertex: tuple[float, float, float]) -> float:
    wave = (
        math.sin(vertex[0] * 29.0 + vertex[2] * 11.0) * 0.5
        + math.sin(vertex[1] * 21.0 - vertex[0] * 7.0) * 0.3
        + math.sin(vertex[2] * 17.0 + vertex[1] * 13.0) * 0.2
    )
    return max(0.0, min(1.0, 0.5 + (wave * 0.5)))


def build_vertices(
    triangles: list[tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]]
) -> list[tuple[float, float, float, float, float, float, float, float]]:
    normal_accumulator: dict[tuple[int, int, int], tuple[float, float, float]] = {}
    position_lookup: dict[tuple[int, int, int], tuple[float, float, float]] = {}

    for face_normal, vertex_a, vertex_b, vertex_c in triangles:
        for vertex in (vertex_a, vertex_b, vertex_c):
            key = quantize_key(vertex)
            position_lookup[key] = vertex
            previous = normal_accumulator.get(key, (0.0, 0.0, 0.0))
            normal_accumulator[key] = (
                previous[0] + face_normal[0],
                previous[1] + face_normal[1],
                previous[2] + face_normal[2],
            )

    smooth_normals = {key: normalize(accumulated) for key, accumulated in normal_accumulator.items()}

    vertices: list[tuple[float, float, float, float, float, float, float, float]] = []
    for face_normal, vertex_a, vertex_b, vertex_c in triangles:
        for vertex in (vertex_a, vertex_b, vertex_c):
            key = quantize_key(vertex)
            smooth_normal = smooth_normals[key]
            edge = max(0.0, min(1.0, (1.0 - dot(smooth_normal, face_normal)) * 2.8))
            variation = surface_variation(position_lookup[key])
            vertices.append(
                (
                    vertex[0],
                    vertex[1],
                    vertex[2],
                    smooth_normal[0],
                    smooth_normal[1],
                    smooth_normal[2],
                    variation,
                    edge,
                )
            )
    return vertices


def write_rmesh(path: Path, scale: float, vertices: list[tuple[float, float, float, float, float, float, float, float]]) -> None:
    with path.open("wb") as handle:
        handle.write(struct.pack(HEADER_FORMAT, MAGIC, VERSION, len(vertices), scale))
        for vertex in vertices:
            handle.write(struct.pack(VERTEX_FORMAT, *vertex))


def preprocess_file(path: Path, scale: float) -> Path:
    triangles = read_binary_stl(path, scale)
    vertices = build_vertices(triangles)
    output_path = path.with_suffix(".rmesh")
    write_rmesh(output_path, scale, vertices)
    return output_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate preprocessed pendant meshes from STL assets.")
    parser.add_argument("paths", nargs="+", help="STL files or directories containing STL files")
    parser.add_argument("--scale", type=float, default=0.001, help="Uniform scale baked into the output mesh")
    args = parser.parse_args()

    input_paths = [Path(value) for value in args.paths]
    stl_files: list[Path] = []
    for input_path in input_paths:
        if input_path.is_dir():
            stl_files.extend(sorted(input_path.glob("*.stl")))
        else:
            stl_files.append(input_path)

    if not stl_files:
        raise SystemExit("no STL files found")

    for stl_file in stl_files:
        output_path = preprocess_file(stl_file, args.scale)
        print(f"{stl_file} -> {output_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
