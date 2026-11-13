#pragma once

// Corrected FreeCameraState layout and its fail-closed self-check for the C-FREE camera backend (PRES-D23, R3 contract
// section 1). The vendored CommonLibSSE-NG has no FreeCameraState definition. On 1.6.1170 +0x20 is the input-handler
// subobject's vtable pointer (RTTI, TDM PDB), so the RC1 candidate layout (translation 0x20, rotation 0x34) would overwrite a
// vtable; it is named here only so the static checks below can reject it. The backend stays off by default
// (runeforge_workstation.json "cameraBackend": "furniture") until SP-2 passes in game; with "free" every condition below must
// pass before the first write, and any failure falls back to the furniture camera for the rest of the session.
// Pure: no game types (the game-side reads live in src/RuneforgeWorkstation.cpp).

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace CalamityAffixes::RuneforgeWorkstation::FreeCameraLayout
{
	inline constexpr std::size_t kSecondaryVtableOffset = 0x20;  // FreeCameraState's input-handler subobject
	inline constexpr std::size_t kTranslationOffset = 0x30;      // NiPoint3 world position
	inline constexpr std::size_t kRotationOffset = 0x3C;         // NiPoint2: x = pitch, y = yaw (radians)
	inline constexpr std::size_t kMinimumSize = 0x50;
	inline constexpr std::size_t kRejectedTranslationOffset = 0x20;
	inline constexpr std::size_t kRejectedRotationOffset = 0x34;
	inline constexpr float kPositionTolerance = 96.0f;  // FreeCameraState::Begin copies the live camera position
	inline constexpr float kPi = 3.14159265358979323846f;
	inline constexpr int kPositionCheckFrames = 30;  // frames the position check may wait for the state to settle

	static_assert(kTranslationOffset >= kSecondaryVtableOffset + 8, "translation must sit past the secondary vtable pointer");
	static_assert(kRotationOffset >= kTranslationOffset + 12, "rotation must follow the NiPoint3 translation");
	static_assert(kRotationOffset + 8 <= kMinimumSize, "rotation must fit inside the minimum state size");
	static_assert(kTranslationOffset != kRejectedTranslationOffset && kRotationOffset != kRejectedRotationOffset,
		"the RC1 layout 0x20/0x34 must never be enabled");

	enum CameraCheckBit : std::uint32_t
	{
		kCamOk = 0,
		kCamNotFreeState = 1u << 0,          // camera state id is not kFree
		kCamPrimaryVtable = 1u << 1,         // vtable at +0x00 is not VTABLE_FreeCameraState[0]
		kCamSecondaryVtable = 1u << 2,       // pointer at +0x20 is not VTABLE_FreeCameraState[1]
		kCamRtti = 1u << 3,                  // the complete object locator does not name FreeCameraState
		kCamAllocation = 1u << 4,            // [base, base + kMinimumSize) is not committed read/write memory
		kCamPositionNotFinite = 1u << 5,
		kCamPositionMismatch = 1u << 6,      // +0x30 is not the live camera position
		kCamNoLiveCameraPosition = 1u << 7,  // no camera root to compare against
		kCamRotationNotFinite = 1u << 8,
		kCamRotationOutOfRange = 1u << 9
	};

	inline constexpr std::uint32_t kCamStructuralMask = kCamNotFreeState | kCamPrimaryVtable | kCamSecondaryVtable | kCamRtti | kCamAllocation;

	struct CameraCheckInput
	{
		bool stateIsFree{ false };
		std::uintptr_t primaryVtable{ 0 };
		std::uintptr_t expectedPrimaryVtable{ 0 };
		std::uintptr_t secondaryAt20{ 0 };
		std::uintptr_t expectedSecondaryVtable{ 0 };
		bool rttiMatches{ false };
		bool allocationSane{ false };
		std::array<float, 3> translationAt30{ 0.0f, 0.0f, 0.0f };
		bool liveCameraPositionValid{ false };
		std::array<float, 3> liveCameraPosition{ 0.0f, 0.0f, 0.0f };
		std::array<float, 2> rotationAt3C{ 0.0f, 0.0f };
	};

	struct CameraCheckResult
	{
		std::uint32_t failures{ 0 };
		float positionError{ -1.0f };
		[[nodiscard]] constexpr bool Pass() const noexcept { return failures == 0; }
		[[nodiscard]] constexpr bool StructuralPass() const noexcept { return (failures & kCamStructuralMask) == 0; }
	};

	[[nodiscard]] inline bool Finite(float a_value) noexcept { return std::isfinite(a_value); }

	// Every condition is evaluated (and logged by the caller) even after the first failure.
	[[nodiscard]] inline CameraCheckResult CheckFreeCamera(const CameraCheckInput& a_in) noexcept
	{
		CameraCheckResult r{};
		if (!a_in.stateIsFree) r.failures |= kCamNotFreeState;
		if (a_in.expectedPrimaryVtable == 0 || a_in.primaryVtable != a_in.expectedPrimaryVtable) r.failures |= kCamPrimaryVtable;
		if (a_in.expectedSecondaryVtable == 0 || a_in.secondaryAt20 != a_in.expectedSecondaryVtable) r.failures |= kCamSecondaryVtable;
		if (!a_in.rttiMatches) r.failures |= kCamRtti;
		if (!a_in.allocationSane) r.failures |= kCamAllocation;
		const auto& t = a_in.translationAt30;
		if (!Finite(t[0]) || !Finite(t[1]) || !Finite(t[2])) {
			r.failures |= kCamPositionNotFinite;
		} else if (!a_in.liveCameraPositionValid) {
			r.failures |= kCamNoLiveCameraPosition;
		} else {
			const float dx = t[0] - a_in.liveCameraPosition[0];
			const float dy = t[1] - a_in.liveCameraPosition[1];
			const float dz = t[2] - a_in.liveCameraPosition[2];
			r.positionError = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (!(r.positionError <= kPositionTolerance)) r.failures |= kCamPositionMismatch;
		}
		const auto& rot = a_in.rotationAt3C;
		if (!Finite(rot[0]) || !Finite(rot[1])) {
			r.failures |= kCamRotationNotFinite;
		} else if (std::fabs(rot[0]) > kPi * 0.5f + 0.01f || std::fabs(rot[1]) > kPi * 2.0f + 0.01f) {
			r.failures |= kCamRotationOutOfRange;
		}
		return r;
	}

	// The check runs at entry and then once per frame, writing nothing, until it passes or the frame budget is spent.
	enum class CameraVerdict : std::uint8_t
	{
		kVerified,   // write allowed for the rest of the session (structural checks still repeat before every write)
		kWait,       // structure is right, the position has not settled yet: no write this frame
		kFailClosed  // furniture camera for the rest of the session
	};

	[[nodiscard]] constexpr CameraVerdict Judge(const CameraCheckResult& a_result, int a_framesWaited) noexcept
	{
		if (a_result.Pass()) return CameraVerdict::kVerified;
		if (!a_result.StructuralPass()) return CameraVerdict::kFailClosed;
		if ((a_result.failures & (kCamPositionMismatch | kCamNoLiveCameraPosition)) != 0 &&
			(a_result.failures & ~(kCamPositionMismatch | kCamNoLiveCameraPosition)) == 0 && a_framesWaited < kPositionCheckFrames) {
			return CameraVerdict::kWait;
		}
		return CameraVerdict::kFailClosed;
	}

	[[nodiscard]] constexpr const char* Name(CameraVerdict a_verdict) noexcept
	{
		switch (a_verdict) {
		case CameraVerdict::kVerified: return "verified";
		case CameraVerdict::kWait: return "wait";
		case CameraVerdict::kFailClosed: return "fail-closed";
		}
		return "unknown";
	}
}
