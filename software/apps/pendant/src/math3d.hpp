#pragma once

#include <array>
#include <cmath>

namespace roberta::pendant {

struct Vec3 final {
    float x;
    float y;
    float z;
};

struct Vec4 final {
    float x;
    float y;
    float z;
    float w;
};

struct Mat4 final {
    std::array<float, 16> data{};

    [[nodiscard]] const float* ptr() const noexcept
    {
        return data.data();
    }
};

[[nodiscard]] inline Vec3 operator+(const Vec3& lhs, const Vec3& rhs)
{
    return Vec3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] inline Vec3 operator-(const Vec3& lhs, const Vec3& rhs)
{
    return Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] inline Vec3 operator*(const Vec3& value, float scale)
{
    return Vec3{value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] inline float dot(const Vec3& lhs, const Vec3& rhs)
{
    return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

[[nodiscard]] inline Vec3 cross(const Vec3& lhs, const Vec3& rhs)
{
    return Vec3{
        (lhs.y * rhs.z) - (lhs.z * rhs.y),
        (lhs.z * rhs.x) - (lhs.x * rhs.z),
        (lhs.x * rhs.y) - (lhs.y * rhs.x),
    };
}

[[nodiscard]] inline float length(const Vec3& value)
{
    return std::sqrt(dot(value, value));
}

[[nodiscard]] inline Vec3 normalize(const Vec3& value)
{
    const auto magnitude = length(value);
    if (magnitude <= 0.00001F) {
        return Vec3{0.0F, 0.0F, 0.0F};
    }
    return value * (1.0F / magnitude);
}

[[nodiscard]] inline Mat4 identity_matrix()
{
    return Mat4{{1.0F, 0.0F, 0.0F, 0.0F,
                 0.0F, 1.0F, 0.0F, 0.0F,
                 0.0F, 0.0F, 1.0F, 0.0F,
                 0.0F, 0.0F, 0.0F, 1.0F}};
}

[[nodiscard]] inline Vec4 transform(const Mat4& matrix, const Vec4& value)
{
    return Vec4{
        (matrix.data[0] * value.x) + (matrix.data[4] * value.y) + (matrix.data[8] * value.z) + (matrix.data[12] * value.w),
        (matrix.data[1] * value.x) + (matrix.data[5] * value.y) + (matrix.data[9] * value.z) + (matrix.data[13] * value.w),
        (matrix.data[2] * value.x) + (matrix.data[6] * value.y) + (matrix.data[10] * value.z) + (matrix.data[14] * value.w),
        (matrix.data[3] * value.x) + (matrix.data[7] * value.y) + (matrix.data[11] * value.z) + (matrix.data[15] * value.w),
    };
}

[[nodiscard]] inline Vec3 transform_point(const Mat4& matrix, const Vec3& value)
{
    const auto transformed = transform(matrix, Vec4{value.x, value.y, value.z, 1.0F});
    if (std::abs(transformed.w) <= 0.00001F) {
        return Vec3{transformed.x, transformed.y, transformed.z};
    }
    return Vec3{transformed.x / transformed.w, transformed.y / transformed.w, transformed.z / transformed.w};
}

[[nodiscard]] inline Vec3 transform_direction(const Mat4& matrix, const Vec3& value)
{
    const auto transformed = transform(matrix, Vec4{value.x, value.y, value.z, 0.0F});
    return Vec3{transformed.x, transformed.y, transformed.z};
}

[[nodiscard]] inline Mat4 multiply(const Mat4& lhs, const Mat4& rhs)
{
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0F;
            for (int k = 0; k < 4; ++k) {
                value += lhs.data[(k * 4) + row] * rhs.data[(column * 4) + k];
            }
            result.data[(column * 4) + row] = value;
        }
    }
    return result;
}

[[nodiscard]] inline Mat4 translation_matrix(const Vec3& offset)
{
    auto result = identity_matrix();
    result.data[12] = offset.x;
    result.data[13] = offset.y;
    result.data[14] = offset.z;
    return result;
}

[[nodiscard]] inline Mat4 rotation_matrix(const Vec3& axis, float radians)
{
    const auto unit_axis = normalize(axis);
    const auto cosine = std::cos(radians);
    const auto sine = std::sin(radians);
    const auto one_minus_cosine = 1.0F - cosine;

    const auto x = unit_axis.x;
    const auto y = unit_axis.y;
    const auto z = unit_axis.z;

    return Mat4{{cosine + (x * x * one_minus_cosine),
                 (y * x * one_minus_cosine) + (z * sine),
                 (z * x * one_minus_cosine) - (y * sine),
                 0.0F,
                 (x * y * one_minus_cosine) - (z * sine),
                 cosine + (y * y * one_minus_cosine),
                 (z * y * one_minus_cosine) + (x * sine),
                 0.0F,
                 (x * z * one_minus_cosine) + (y * sine),
                 (y * z * one_minus_cosine) - (x * sine),
                 cosine + (z * z * one_minus_cosine),
                 0.0F,
                 0.0F,
                 0.0F,
                 0.0F,
                 1.0F}};
}

[[nodiscard]] inline Mat4 perspective_matrix(float field_of_view_y_radians, float aspect_ratio, float near_plane,
                                             float far_plane)
{
    const auto focal_length = 1.0F / std::tan(field_of_view_y_radians * 0.5F);
    Mat4 result{};
    result.data[0] = focal_length / aspect_ratio;
    result.data[5] = focal_length;
    result.data[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.data[11] = -1.0F;
    result.data[14] = (2.0F * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

[[nodiscard]] inline Mat4 look_at_matrix(const Vec3& eye, const Vec3& target, const Vec3& up)
{
    const auto forward = normalize(target - eye);
    const auto right = normalize(cross(forward, up));
    const auto corrected_up = cross(right, forward);

    Mat4 result = identity_matrix();
    result.data[0] = right.x;
    result.data[1] = corrected_up.x;
    result.data[2] = -forward.x;

    result.data[4] = right.y;
    result.data[5] = corrected_up.y;
    result.data[6] = -forward.y;

    result.data[8] = right.z;
    result.data[9] = corrected_up.z;
    result.data[10] = -forward.z;

    result.data[12] = -dot(right, eye);
    result.data[13] = -dot(corrected_up, eye);
    result.data[14] = dot(forward, eye);
    return result;
}

} // namespace roberta::pendant
