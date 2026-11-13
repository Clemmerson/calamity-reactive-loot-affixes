#pragma once

// Runeforge workstation furniture-camera framing (R8.7.4.18). Pure policy; the native camera code only measures and writes.
//
// Live R8.7.4.17 evidence: with SmoothCam/TDM loaded the free-camera backend is refused, the authored shots are telemetry only,
// and the seated third-person furniture camera looks level from about the player's head (matched against the live 2560x1440
// SELECT RUNEWORD capture): the slab top fills the bottom edge while the rune row, the staged item and the optics are out of
// frame. feasr-core/alpha/workstation/r87418/framing_solve.py solves the correction on a rig fitted to that pose. Its unconstrained
// minimum (pitch 18, pull-back 95) needs a 2-degree heading change, which the native side never writes; with yaw fixed at 0 the
// smallest framing that keeps the item, all six rune slots, the slab, the eye and the lens left of the right-side panel at 16:9
// and 21:9, clear of the seated player, with a 0.075 screen-fraction margin for the approximate matched pose, is pitch 19 down
// and a 120-unit pull-back (framing-r87418.json "native").
//
// The native side applies it through the furniture camera's own controls only: the ThirdPersonState free rotation (or, if the
// furniture disallows free rotation, the player's look pitch - never the seated body's heading) and the third-person zoom. It
// measures the camera after every write (ThirdPersonState::GetRotation/GetTranslation) and fails closed: a channel with no
// measurable effect is reverted and never used again in the session; the pitch sign is learned from the measured response.
// Everything is restored at exit (zoom/FOV through the existing camera snapshot, free rotation/look pitch to their base).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

#include "CalamityAffixes/RuneforgeWorkstationStatePolicy.h"

namespace CalamityAffixes::RuneforgeWorkstation::FramingPolicy
{
	using StatePolicy::State;

	struct Target
	{
		float pitchDegrees{ 0.0f };     // positive looks down
		float pullBack{ 0.0f };         // extra camera distance from the orbit pivot, world units
	};

	inline constexpr Target kWorkstation{ 19.0f, 120.0f };   // framing-r87418.json "native": yaw 0, pitch 19, pull-back 120
	inline constexpr float kRampSeconds = 0.45f;
	inline constexpr float kMaxPitchDegrees = 30.0f;
	inline constexpr float kMaxPullBack = 160.0f;
	inline constexpr float kMaxZoomDelta = 0.6f;            // ThirdPersonState zoom offset, above the session snapshot
	inline constexpr float kZoomRatePerSecond = 0.5f;
	inline constexpr float kDistanceTolerance = 6.0f;
	inline constexpr float kProbeMinDegrees = 4.0f;         // commanded pitch before the response is judged
	inline constexpr float kNoEffectShare = 0.25f;          // measured/commanded below this = the channel does nothing
	inline constexpr std::uint32_t kZoomProbeFrames = 60;   // frames of zoom increase before "no effect" is judged (camera mods smooth)
	inline constexpr float kJudgeAfterSeconds = kRampSeconds + 0.35f;   // judge the pitch response only after the ramp has settled

	// Seated states where the slab, item, runes and optics should share the frame. ENTERING is excluded: the seat is not yet
	// acquired. EXITING/IDLE release the framing.
	[[nodiscard]] constexpr bool AppliesTo(State a_state) noexcept
	{
		switch (a_state) {
		case State::kSelectItem:
		case State::kSelectFormula:
		case State::kOperations:
		case State::kConfirm:
		case State::kCommitting:
		case State::kPerforming:
		case State::kResultSuccess:
		case State::kResultFailure:
			return true;
		default:
			return false;
		}
	}

	// Settings overrides (runeforge_workstation.json "framingPitch" / "framingPullBack") are clamped to the safe envelope.
	[[nodiscard]] constexpr Target Clamp(Target a_target) noexcept
	{
		return { std::clamp(a_target.pitchDegrees, 0.0f, kMaxPitchDegrees), std::clamp(a_target.pullBack, 0.0f, kMaxPullBack) };
	}

	[[nodiscard]] constexpr float Ramp(float a_elapsed, bool a_reducedMotion) noexcept
	{
		if (a_reducedMotion) return 1.0f;
		const float t = std::clamp(a_elapsed / kRampSeconds, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	enum class SignVerdict : std::uint8_t
	{
		kPending,   // not enough commanded yet
		kSame,      // the camera pitched the way we asked
		kFlipped,   // it pitched the other way: negate the channel sign
		kNoEffect   // it did not move: the channel is not available here
	};

	[[nodiscard]] inline SignVerdict JudgeSign(float a_commandedDegrees, float a_measuredDeltaDegrees) noexcept
	{
		if (std::fabs(a_commandedDegrees) < kProbeMinDegrees) return SignVerdict::kPending;
		if (std::fabs(a_measuredDeltaDegrees) < kNoEffectShare * std::fabs(a_commandedDegrees)) return SignVerdict::kNoEffect;
		return (a_commandedDegrees > 0.0f) == (a_measuredDeltaDegrees > 0.0f) ? SignVerdict::kSame : SignVerdict::kFlipped;
	}

	// Closed-loop zoom: step the zoom offset until the measured camera distance from the pivot reaches the target, inside
	// [base, base + kMaxZoomDelta]. Zoom only moves outward from the snapshot and back toward it.
	[[nodiscard]] inline float ZoomStep(float a_zoom, float a_baseZoom, float a_measuredDistance, float a_targetDistance, float a_dt) noexcept
	{
		const float lo = a_baseZoom;
		const float hi = a_baseZoom + kMaxZoomDelta;
		if (a_measuredDistance < a_targetDistance - kDistanceTolerance) return std::min(hi, a_zoom + kZoomRatePerSecond * a_dt);
		if (a_measuredDistance > a_targetDistance + kDistanceTolerance) return std::max(lo, a_zoom - kZoomRatePerSecond * a_dt);
		return std::clamp(a_zoom, lo, hi);
	}

	// Zoom has had kZoomProbeFrames of increase and the distance moved less than this: the camera ignores zoom here.
	[[nodiscard]] constexpr bool ZoomHasNoEffect(std::uint32_t a_framesIncreasing, float a_distanceGain) noexcept
	{
		return a_framesIncreasing >= kZoomProbeFrames && a_distanceGain < 4.0f;
	}

	struct Vec3
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float z{ 0.0f };
	};

	[[nodiscard]] inline float PitchDegreesOf(const Vec3& a_forward) noexcept
	{
		const float h = std::sqrt(a_forward.x * a_forward.x + a_forward.y * a_forward.y);
		return std::atan2(-a_forward.z, h) * 180.0f / 3.14159265358979f;   // positive looks down
	}

	// Rotation matrix of a unit quaternion (w, x, y, z); column c = image of local axis c.
	struct Mat3
	{
		float m[3][3]{};
	};

	[[nodiscard]] inline Mat3 FromQuaternion(float w, float x, float y, float z) noexcept
	{
		Mat3 r{};
		r.m[0][0] = 1 - 2 * (y * y + z * z); r.m[0][1] = 2 * (x * y - w * z);     r.m[0][2] = 2 * (x * z + w * y);
		r.m[1][0] = 2 * (x * y + w * z);     r.m[1][1] = 1 - 2 * (x * x + z * z); r.m[1][2] = 2 * (y * z - w * x);
		r.m[2][0] = 2 * (x * z - w * y);     r.m[2][1] = 2 * (y * z + w * x);     r.m[2][2] = 1 - 2 * (x * x + y * y);
		return r;
	}

	// The camera's forward axis is identified once, at framing start, as the signed matrix column (or row, for the transposed
	// convention) most aligned with the seated heading; the same axis is then read every frame. Returns the axis code:
	// 0..2 column, 3..5 row (+0 / +3), negative sign in bit 3.
	[[nodiscard]] inline std::uint8_t ForwardAxis(const Mat3& a_m, const Vec3& a_heading) noexcept
	{
		float best = -2.0f;
		std::uint8_t code = 1;
		for (std::uint8_t k = 0; k < 6; ++k) {
			const bool row = k >= 3;
			const auto i = static_cast<std::uint8_t>(k % 3);
			const Vec3 v = row ? Vec3{ a_m.m[i][0], a_m.m[i][1], a_m.m[i][2] } : Vec3{ a_m.m[0][i], a_m.m[1][i], a_m.m[2][i] };
			const float d = v.x * a_heading.x + v.y * a_heading.y + v.z * a_heading.z;
			if (d > best) { best = d; code = k; }
			if (-d > best) { best = -d; code = static_cast<std::uint8_t>(k | 0x8u); }
		}
		return code;
	}

	[[nodiscard]] inline Vec3 AxisOf(const Mat3& a_m, std::uint8_t a_code) noexcept
	{
		const bool neg = (a_code & 0x8u) != 0;
		const auto k = static_cast<std::uint8_t>(a_code & 0x7u);
		const auto i = static_cast<std::uint8_t>(k % 3);
		Vec3 v = k >= 3 ? Vec3{ a_m.m[i][0], a_m.m[i][1], a_m.m[i][2] } : Vec3{ a_m.m[0][i], a_m.m[1][i], a_m.m[2][i] };
		if (neg) v = { -v.x, -v.y, -v.z };
		return v;
	}

	enum class Channel : std::uint8_t
	{
		kNone,
		kFreeRotation,   // ThirdPersonState::freeRotation.y (the furniture camera's own orbit)
		kLookPitch       // the player's look pitch (Actor rotation X); the seated heading is never touched
	};

	// PC1.1 (2026-11-09, P1 activation wobble). The free-rotation pitch sign is an engine convention, not a per-session unknown:
	// a NEGATIVE ThirdPersonState::freeRotation.y raises the orbit camera, which then looks down at the seat. Live evidence
	// (16 framed sessions in 4 logs, 2026-11-08/09): every session whose forward axis was identified as column 1 started on +1,
	// pitched the camera UP for ~0.8 s, judged "flipped" and swung DOWN to the frame (the reported jump -> up -> down); the one
	// session that kept +1 had a misidentified axis (0x9) and in fact lowered the camera 51 units. The free-rotation channel
	// therefore starts on the engine sign (one monotonic ramp), and its safety judge reads the camera's ELEVATION above the
	// orbit pivot - a position-only measurement that does not depend on the rotation-matrix axis convention. The look-pitch
	// channel keeps its r87418 behaviour.
	inline constexpr float kFreeRotationDownSign = -1.0f;

	[[nodiscard]] constexpr float InitialSign(Channel a_channel) noexcept
	{
		return a_channel == Channel::kFreeRotation ? kFreeRotationDownSign : 1.0f;
	}

	// Degrees of the camera above the pivot as seen from the pivot; positive = the camera is above and looks down at it.
	[[nodiscard]] inline float ElevationDegrees(const Vec3& a_camera, const Vec3& a_pivot) noexcept
	{
		const float dx = a_camera.x - a_pivot.x;
		const float dy = a_camera.y - a_pivot.y;
		const float dz = a_camera.z - a_pivot.z;
		return std::atan2(dz, std::sqrt(dx * dx + dy * dy)) * 180.0f / 3.14159265358979f;
	}

	// The judged response of a channel: free rotation by elevation (axis-independent), look pitch by the forward axis (r87418).
	[[nodiscard]] inline SignVerdict JudgeChannel(Channel a_channel, float a_commandedDegrees, float a_pitchDeltaDegrees, float a_elevationDeltaDegrees) noexcept
	{
		return JudgeSign(a_commandedDegrees, a_channel == Channel::kFreeRotation ? a_elevationDeltaDegrees : a_pitchDeltaDegrees);
	}

	[[nodiscard]] constexpr std::string_view Name(Channel a_channel) noexcept
	{
		switch (a_channel) {
		case Channel::kFreeRotation: return "freeRotation";
		case Channel::kLookPitch: return "lookPitch";
		case Channel::kNone: break;
		}
		return "none";
	}
}
