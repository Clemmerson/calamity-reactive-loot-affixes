#pragma once

// Runeforge workstation context (plan section 7 IDLE rows; RC1 decisions WS-D14, WS-I06).
// Pure policy:
//   - Resolve: workstation mode or the flat full-screen panel when an Open arrives;
//   - ResolveLegacy: the legacy Calamity/Prisma routing of docs/RUNEFORGE_LEGACY_UI_POLICY_20261019.md (feasr-core).

#include <cstdint>
#include <string_view>

namespace CalamityAffixes::RuneforgeWorkstation::ContextPolicy
{
	enum class Mode : std::uint8_t
	{
		kWorkstation,
		kFlat
	};

	enum class FlatReason : std::uint8_t
	{
		kNone,
		kFlatPreferred,
		kPresentationMissing,
		kNotInForgeCell,
		kSlabUnavailable,
		kAnchorsUnavailable,
		kStanceUnavailable,
		kStanceOccupied,
		kMenuUnavailable
	};

	struct Context
	{
		bool flatModePreferred{ false };      // accessibility setting (WS-D14)
		bool presentationLoaded{ false };     // FEASR_RuneforgePresentation.esp present
		bool playerInForgeCell{ false };      // the player's parent cell is Sites 00080B
		bool slabResolved{ false };           // REFR 000829 resolves
		bool slab3DLoaded{ false };
		bool anchorsResolved{ false };        // gear anchor 000830 and slots 00082A-F
		bool stanceResolved{ false };         // Presentation stance REFR
		bool stanceOccupiedByOther{ false };  // a follower stands in / uses the stance (RG-WS-19)
		bool workstationMenuRegistered{ false };
	};

	struct Resolution
	{
		Mode mode{ Mode::kFlat };
		FlatReason reason{ FlatReason::kNone };
	};

	[[nodiscard]] constexpr Resolution Resolve(const Context& a_context) noexcept
	{
		if (a_context.flatModePreferred) return { Mode::kFlat, FlatReason::kFlatPreferred };
		if (!a_context.workstationMenuRegistered) return { Mode::kFlat, FlatReason::kMenuUnavailable };
		if (!a_context.presentationLoaded) return { Mode::kFlat, FlatReason::kPresentationMissing };
		if (!a_context.playerInForgeCell) return { Mode::kFlat, FlatReason::kNotInForgeCell };
		if (!a_context.slabResolved || !a_context.slab3DLoaded) return { Mode::kFlat, FlatReason::kSlabUnavailable };
		if (!a_context.anchorsResolved) return { Mode::kFlat, FlatReason::kAnchorsUnavailable };
		if (!a_context.stanceResolved) return { Mode::kFlat, FlatReason::kStanceUnavailable };
		if (a_context.stanceOccupiedByOther) return { Mode::kFlat, FlatReason::kStanceOccupied };
		return { Mode::kWorkstation, FlatReason::kNone };
	}

	[[nodiscard]] constexpr std::string_view Name(FlatReason a_reason) noexcept
	{
		switch (a_reason) {
		case FlatReason::kNone: return "none";
		case FlatReason::kFlatPreferred: return "flat-mode setting";
		case FlatReason::kPresentationMissing: return "FEASR_RuneforgePresentation.esp not loaded";
		case FlatReason::kNotInForgeCell: return "player not in the Third Forge";
		case FlatReason::kSlabUnavailable: return "workslab unresolved or not loaded";
		case FlatReason::kAnchorsUnavailable: return "stage anchors unresolved";
		case FlatReason::kStanceUnavailable: return "stance unresolved";
		case FlatReason::kStanceOccupied: return "stance occupied";
		case FlatReason::kMenuUnavailable: return "workstation menu not registered";
		}
		return "?";
	}

	// ---- legacy Calamity/Prisma routing (RC1 section 13)

	enum class LegacyOperation : std::uint8_t
	{
		kRunewordInsert,  // Runeword (rune insertion / finalize)
		kIdentify,
		kReforge,
		kAddSlot,  // affix expansion
		kScour,
		kCurrencyExchange,
		kCurrencyRecover,
		kResetItem,
		kSelectBase,
		kSelectRecipe,
		kCycleSelection,
		kStatus,
		kDebugGrant
	};

	enum class LegacyVerdict : std::uint8_t
	{
		kAllow,
		kRefuseUseRuneforge
	};

	// The five Runeforge operations run only through the Runeforge (workstation, or its flat fallback) when the FEASR
	// Runeforge contract is active. Everything else stays as documented compatibility. Without FEASR (contract not
	// compatible) the upstream Calamity behaviour is unchanged.
	[[nodiscard]] constexpr bool IsRuneforgeOperation(LegacyOperation a_operation) noexcept
	{
		switch (a_operation) {
		case LegacyOperation::kRunewordInsert:
		case LegacyOperation::kIdentify:
		case LegacyOperation::kReforge:
		case LegacyOperation::kAddSlot:
		case LegacyOperation::kScour:
			return true;
		default:
			return false;
		}
	}

	[[nodiscard]] constexpr LegacyVerdict ResolveLegacy(LegacyOperation a_operation, bool a_feasrContractActive) noexcept
	{
		if (!a_feasrContractActive) return LegacyVerdict::kAllow;
		return IsRuneforgeOperation(a_operation) ? LegacyVerdict::kRefuseUseRuneforge : LegacyVerdict::kAllow;
	}

	inline constexpr std::string_view kUseRuneforgeMessage = "This action is only available at the Runeforge.";
}
