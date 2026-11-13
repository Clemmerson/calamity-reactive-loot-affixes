#pragma once

// Runeforge workstation right button and view pan (RC1-WS, 2026-10-19; run 1 fix ported from the FEASR_RF_REVIEW_ONLY r2
// build into production). Pure policy. Back / exit is Esc, Tab or gamepad B only: the right button pans the camera when
// pressed outside the GUI box and does nothing inside it; it never exits (run 1: right-click exited the GUI). The pan is
// clamped and kept until the next authored shot; with the free camera it adds to the right-stick orbit.

#include <algorithm>
#include <cstdint>

namespace CalamityAffixes::RuneforgeWorkstation::ViewPolicy
{
	enum class RightButton : std::uint8_t
	{
		kIgnore,
		kPanBegin,
		kPanEnd
	};

	[[nodiscard]] constexpr RightButton RouteRightButton(bool a_down, bool a_up, bool a_pointerInPanel, bool a_panning) noexcept
	{
		if (a_down) return a_pointerInPanel || a_panning ? RightButton::kIgnore : RightButton::kPanBegin;
		if (a_up) return a_panning ? RightButton::kPanEnd : RightButton::kIgnore;
		return RightButton::kIgnore;
	}

	struct Pan
	{
		float yawDegrees{ 0.0f };
		float pitchDegrees{ 0.0f };
	};

	inline constexpr float kPanYawDegrees = 35.0f;
	inline constexpr float kPanPitchDegrees = 15.0f;
	inline constexpr float kPanDegreesPerCount = 0.12f;  // raw mouse counts
	inline constexpr float kViewYawDegrees = 40.0f;      // pan + stick orbit together
	inline constexpr float kViewPitchDegrees = 20.0f;
	inline constexpr float kPi = 3.14159265358979323846f;

	// Drag right turns the view right; drag up (negative raw Y) tilts it up. Clamped; kept until the next shot.
	[[nodiscard]] constexpr Pan AccumulatePan(Pan a_pan, std::int32_t a_dx, std::int32_t a_dy, float a_degreesPerCount = kPanDegreesPerCount) noexcept
	{
		a_pan.yawDegrees = std::clamp(a_pan.yawDegrees + static_cast<float>(a_dx) * a_degreesPerCount, -kPanYawDegrees, kPanYawDegrees);
		a_pan.pitchDegrees = std::clamp(a_pan.pitchDegrees - static_cast<float>(a_dy) * a_degreesPerCount, -kPanPitchDegrees, kPanPitchDegrees);
		return a_pan;
	}

	// Free backend: the right-stick orbit (springs back) plus the mouse pan (kept), clamped together.
	[[nodiscard]] constexpr Pan CombineView(float a_orbitYaw, float a_orbitPitch, const Pan& a_pan) noexcept
	{
		return { std::clamp(a_orbitYaw + a_pan.yawDegrees, -kViewYawDegrees, kViewYawDegrees),
			std::clamp(a_orbitPitch + a_pan.pitchDegrees, -kViewPitchDegrees, kViewPitchDegrees) };
	}

	[[nodiscard]] constexpr bool PanIsZero(const Pan& a_pan) noexcept { return a_pan.yawDegrees == 0.0f && a_pan.pitchDegrees == 0.0f; }
}
