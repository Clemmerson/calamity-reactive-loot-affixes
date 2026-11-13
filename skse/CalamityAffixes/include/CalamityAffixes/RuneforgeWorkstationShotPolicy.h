#pragma once

// Runeforge workstation camera shots (plan section 9; RC1 decision WS-D05).
// Pure math: slab-local shot poses (feasr-core/alpha/workstation/data/workstation-shots.v1.json), the slab-local ->
// world transform (Skyrim heading: positive Z angle turns clockwise from above), smoothstep easing, look angles and
// the right-stick orbit clamp with spring-back. The camera backends in RuneforgeWorkstation.cpp only consume these.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

#include "CalamityAffixes/RuneforgeWorkstationStatePolicy.h"

namespace CalamityAffixes::RuneforgeWorkstation::ShotPolicy
{
	using StatePolicy::Shot;

	struct Vec3
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float z{ 0.0f };
	};

	struct Frame
	{
		Vec3 origin{};
		float yawDegrees{ 0.0f };
	};

	struct ShotDef
	{
		Shot shot{ Shot::kNone };
		Vec3 position{};
		Vec3 lookAt{};
		float moveSeconds{ 0.4f };
	};

	struct Pose
	{
		Vec3 position{};
		Vec3 lookAt{};
	};

	inline constexpr float kPi = 3.14159265358979323846f;
	inline constexpr float kFovDegrees = 65.0f;
	inline constexpr float kOrbitYawDegrees = 20.0f;
	inline constexpr float kOrbitPitchDegrees = 10.0f;
	inline constexpr float kSpringBackSeconds = 0.35f;
	inline constexpr float kMaxMoveSeconds = 0.6f;

	// Slab REFR 000829 (FEASR_RuneforgeSites.esp): world (-170, 40, 0), yaw 0.
	inline constexpr Frame kThirdForgeSlab{ { -170.0f, 40.0f, 0.0f }, 0.0f };

	inline constexpr std::array<ShotDef, 7> kShots{ {
		{ Shot::kEnter, { 40.0f, -230.0f, 150.0f }, { 0.0f, 0.0f, 64.0f }, 0.6f },
		{ Shot::kOverview, { 0.0f, -190.0f, 175.0f }, { 0.0f, 0.0f, 64.0f }, 0.5f },
		{ Shot::kItem, { -60.0f, -150.0f, 150.0f }, { 0.0f, -40.0f, 66.0f }, 0.4f },
		{ Shot::kRow, { 0.0f, -110.0f, 170.0f }, { 0.0f, 50.0f, 66.0f }, 0.4f },
		{ Shot::kConfirm, { 0.0f, -160.0f, 185.0f }, { 0.0f, 5.0f, 64.0f }, 0.4f },
		{ Shot::kStrike, { 45.0f, -95.0f, 135.0f }, { 0.0f, 50.0f, 66.0f }, 0.25f },
		{ Shot::kResult, { 0.0f, -170.0f, 160.0f }, { 0.0f, -40.0f, 64.0f }, 0.5f },
	} };

	[[nodiscard]] constexpr const ShotDef* Find(Shot a_shot) noexcept
	{
		for (const auto& def : kShots) {
			if (def.shot == a_shot) return &def;
		}
		return nullptr;
	}

	[[nodiscard]] inline Vec3 LocalToWorld(const Frame& a_frame, const Vec3& a_local) noexcept
	{
		const double t = static_cast<double>(a_frame.yawDegrees) * 3.14159265358979323846 / 180.0;
		const double c = std::cos(t);
		const double s = std::sin(t);
		return {
			static_cast<float>(a_frame.origin.x + a_local.x * c + a_local.y * s),
			static_cast<float>(a_frame.origin.y - a_local.x * s + a_local.y * c),
			a_frame.origin.z + a_local.z
		};
	}

	[[nodiscard]] constexpr float Smoothstep(float a_t) noexcept
	{
		const float t = a_t < 0.0f ? 0.0f : (a_t > 1.0f ? 1.0f : a_t);
		return t * t * (3.0f - 2.0f * t);
	}

	[[nodiscard]] constexpr Vec3 Lerp(const Vec3& a_from, const Vec3& a_to, float a_t) noexcept
	{
		return { a_from.x + (a_to.x - a_from.x) * a_t, a_from.y + (a_to.y - a_from.y) * a_t, a_from.z + (a_to.z - a_from.z) * a_t };
	}

	// Eased pose a_elapsed seconds into a move of a_duration seconds (CR-3; reduced motion = a cut).
	[[nodiscard]] constexpr Pose Ease(const Pose& a_from, const Pose& a_to, float a_elapsed, float a_duration, bool a_reducedMotion = false) noexcept
	{
		if (a_reducedMotion || a_duration <= 0.0f) return a_to;
		const float k = Smoothstep(a_elapsed / std::min(a_duration, kMaxMoveSeconds));
		return { Lerp(a_from.position, a_to.position, k), Lerp(a_from.lookAt, a_to.lookAt, k) };
	}

	[[nodiscard]] inline Pose WorldPose(const Frame& a_frame, Shot a_shot) noexcept
	{
		const auto* def = Find(a_shot);
		if (!def) return {};
		return { LocalToWorld(a_frame, def->position), LocalToWorld(a_frame, def->lookAt) };
	}

	struct Angles
	{
		float pitch{ 0.0f };  // radians, positive looks down (Skyrim camera convention)
		float yaw{ 0.0f };    // radians, heading: 0 = +Y, pi/2 = +X
	};

	[[nodiscard]] inline Angles LookAngles(const Vec3& a_from, const Vec3& a_to) noexcept
	{
		const double dx = a_to.x - a_from.x;
		const double dy = a_to.y - a_from.y;
		const double dz = a_to.z - a_from.z;
		const double horizontal = std::sqrt(dx * dx + dy * dy);
		return { static_cast<float>(-std::atan2(dz, horizontal)), static_cast<float>(std::atan2(dx, dy)) };
	}

	struct Orbit
	{
		float yawDegrees{ 0.0f };
		float pitchDegrees{ 0.0f };
	};

	// CR-4: right-stick orbit around the current shot, clamped to +-20 yaw / +-10 pitch; it springs back to 0
	// over kSpringBackSeconds when the stick is released.
	[[nodiscard]] inline Orbit UpdateOrbit(Orbit a_orbit, float a_stickX, float a_stickY, float a_dt, float a_degreesPerSecond = 60.0f) noexcept
	{
		const bool active = std::fabs(a_stickX) > 0.2f || std::fabs(a_stickY) > 0.2f;
		if (active) {
			a_orbit.yawDegrees = std::clamp(a_orbit.yawDegrees + a_stickX * a_degreesPerSecond * a_dt, -kOrbitYawDegrees, kOrbitYawDegrees);
			a_orbit.pitchDegrees = std::clamp(a_orbit.pitchDegrees - a_stickY * a_degreesPerSecond * a_dt, -kOrbitPitchDegrees, kOrbitPitchDegrees);
			return a_orbit;
		}
		const float k = std::clamp(a_dt / kSpringBackSeconds, 0.0f, 1.0f);
		a_orbit.yawDegrees -= a_orbit.yawDegrees * k;
		a_orbit.pitchDegrees -= a_orbit.pitchDegrees * k;
		if (std::fabs(a_orbit.yawDegrees) < 0.01f) a_orbit.yawDegrees = 0.0f;
		if (std::fabs(a_orbit.pitchDegrees) < 0.01f) a_orbit.pitchDegrees = 0.0f;
		return a_orbit;
	}

	// Camera position orbited around the look-at point by the orbit offsets.
	[[nodiscard]] inline Vec3 ApplyOrbit(const Pose& a_pose, const Orbit& a_orbit) noexcept
	{
		const double yaw = static_cast<double>(a_orbit.yawDegrees) * 3.14159265358979323846 / 180.0;
		const double pitch = static_cast<double>(a_orbit.pitchDegrees) * 3.14159265358979323846 / 180.0;
		const double dx = a_pose.position.x - a_pose.lookAt.x;
		const double dy = a_pose.position.y - a_pose.lookAt.y;
		const double dz = a_pose.position.z - a_pose.lookAt.z;
		const double rx = dx * std::cos(yaw) + dy * std::sin(yaw);
		const double ry = -dx * std::sin(yaw) + dy * std::cos(yaw);
		const double horizontal = std::sqrt(rx * rx + ry * ry);
		const double radius = std::sqrt(horizontal * horizontal + dz * dz);
		const double elevation = std::atan2(dz, horizontal) + pitch;
		const double scale = horizontal > 1e-6 ? std::cos(elevation) * radius / horizontal : 0.0;
		return { static_cast<float>(a_pose.lookAt.x + rx * scale), static_cast<float>(a_pose.lookAt.y + ry * scale),
			static_cast<float>(a_pose.lookAt.z + std::sin(elevation) * radius) };
	}
}
