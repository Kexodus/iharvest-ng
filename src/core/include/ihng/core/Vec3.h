#pragma once

#include <algorithm>
#include <cmath>

namespace ihng::core
{
	// Minimal 3D vector in Skyrim world units (+X east, +Y north, +Z up).
	struct Vec3
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float z{ 0.0f };

		constexpr Vec3 operator+(const Vec3& o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
		constexpr Vec3 operator-(const Vec3& o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
		constexpr Vec3 operator*(float s) const noexcept { return { x * s, y * s, z * s }; }
		constexpr Vec3& operator+=(const Vec3& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }

		[[nodiscard]] float Length() const noexcept { return std::sqrt(x * x + y * y + z * z); }
		[[nodiscard]] float Length2D() const noexcept { return std::sqrt(x * x + y * y); }

		[[nodiscard]] Vec3 Normalized() const noexcept
		{
			const float len = Length();
			return len > 1e-4f ? *this * (1.0f / len) : Vec3{};
		}
	};

	[[nodiscard]] inline float Distance(const Vec3& a, const Vec3& b) noexcept { return (a - b).Length(); }

	[[nodiscard]] constexpr Vec3 Lerp(const Vec3& a, const Vec3& b, float t) noexcept { return a + (b - a) * t; }

	// Skyrim heading convention: angle 0 faces +Y, increasing clockwise (towards +X). Radians.
	[[nodiscard]] inline Vec3 HeadingToDir(float a_heading) noexcept { return { std::sin(a_heading), std::cos(a_heading), 0.0f }; }

	[[nodiscard]] inline float DirToHeading(const Vec3& a_dir) noexcept { return std::atan2(a_dir.x, a_dir.y); }

	// Wraps an angle to (-pi, pi].
	[[nodiscard]] inline float WrapAngle(float a) noexcept
	{
		constexpr float kPi = 3.14159265358979f;
		constexpr float kTwoPi = 2.0f * kPi;
		a = std::fmod(a + kPi, kTwoPi);
		if (a < 0.0f) {
			a += kTwoPi;
		}
		return a - kPi;
	}

	// Quadratic Bezier point.
	[[nodiscard]] constexpr Vec3 Bezier(const Vec3& p0, const Vec3& c, const Vec3& p1, float t) noexcept
	{
		const float u = 1.0f - t;
		return p0 * (u * u) + c * (2.0f * u * t) + p1 * (t * t);
	}

	// Cheap upper-bound-ish arc length estimate of a quadratic Bezier (average of chord and control polygon).
	[[nodiscard]] inline float BezierLength(const Vec3& p0, const Vec3& c, const Vec3& p1) noexcept
	{
		const float chord = Distance(p0, p1);
		const float poly = Distance(p0, c) + Distance(c, p1);
		return std::max(1.0f, 0.5f * (chord + poly));
	}
}
