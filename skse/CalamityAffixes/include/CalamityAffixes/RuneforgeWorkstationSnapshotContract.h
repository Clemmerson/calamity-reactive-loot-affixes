#pragma once

// Runeforge snapshot producer/consumer contract (R8.7.4.6, 2026-11-06).
//
// RuneforgeMenu::BuildSnapshot (producer) writes one JSON document; the native workstation (consumer) reads it back to
// validate an explicit SELECT_FORMULA. R8.7.4.2 read `selectedInstanceKey` from the nested `panel` object, but the producer
// writes it at the TOP LEVEL of the snapshot, so every legitimate selection compared the requested item against "" and was
// refused as "changed during validation". This header is the single source of the field paths for both sides, and the
// formula-selection decision is a pure function of the parsed view so host tests drive the exact production decision.
//
// Generic over the JSON type (nlohmann::json in production and in the host tests); no game types.

#include "CalamityAffixes/RuneforgeWorkstationReadinessPolicy.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace CalamityAffixes::RuneforgeWorkstation::SnapshotContract
{
	// ---- field paths (producer and consumer both use these names) -------------------------------------------------
	// Top level of the snapshot document.
	inline constexpr const char* kSelectedInstanceKey = "selectedInstanceKey";  // top level ONLY; never inside `panel`
	inline constexpr const char* kPanel = "panel";
	inline constexpr const char* kBases = "bases";
	inline constexpr const char* kRecipes = "recipes";
	// Entries of `bases`.
	inline constexpr const char* kBaseInstanceKey = "instanceKey";
	inline constexpr const char* kBaseSelected = "selected";
	// Entries of `recipes`.
	inline constexpr const char* kRecipeId = "id";
	inline constexpr const char* kRecipeSelected = "selected";
	// Fields of `panel` (the selected item's runeword panel state).
	inline constexpr const char* kPanelRecipeId = "recipeId";
	inline constexpr const char* kPanelHasBase = "hasBase";
	inline constexpr const char* kPanelHasRecipe = "hasRecipe";
	inline constexpr const char* kPanelCanInsert = "canInsert";
	inline constexpr const char* kPanelComplete = "complete";
	inline constexpr const char* kPanelMissing = "missing";
	inline constexpr const char* kPanelBaseCompatibilityWarning = "baseCompatibilityWarning";
	inline constexpr const char* kPanelBaseCompatibilityMessage = "baseCompatibilityMessage";

	// The selection-relevant view of one snapshot, read through the paths above.
	struct SelectionView
	{
		bool panelPresent{ false };
		std::string selectedInstanceKey{};  // top-level snapshot field
		std::string panelRecipeId{};        // panel.recipeId
		bool hasBase{ false };
		bool hasRecipe{ false };
		bool canInsert{ false };
		bool complete{ false };
		bool baseCompatibilityWarning{ false };
		std::string baseCompatibilityMessage{};
		std::string missing{};
		bool selectedItemListed{ false };    // the selected instance key is still an entry of `bases`
		std::uint32_t selectedItemRows{ 0 };  // rows carrying that exact key (must be exactly 1)
		bool panelRecipeListed{ false };     // panel.recipeId is still an entry of `recipes`
	};

	namespace detail
	{
		template <class Json>
		[[nodiscard]] std::string Str(const Json& a_object, const char* a_key)
		{
			if (!a_object.is_object()) return {};
			const auto it = a_object.find(a_key);
			return it != a_object.end() && it->is_string() ? it->template get<std::string>() : std::string{};
		}

		template <class Json>
		[[nodiscard]] bool Flag(const Json& a_object, const char* a_key)
		{
			if (!a_object.is_object()) return false;
			const auto it = a_object.find(a_key);
			return it != a_object.end() && it->is_boolean() && it->template get<bool>();
		}
	}

	template <class Json>
	[[nodiscard]] SelectionView ReadSelection(const Json& a_snapshot)
	{
		SelectionView view{};
		if (!a_snapshot.is_object()) return view;
		view.selectedInstanceKey = detail::Str(a_snapshot, kSelectedInstanceKey);
		const auto panelIt = a_snapshot.find(kPanel);
		if (panelIt != a_snapshot.end() && panelIt->is_object()) {
			const auto& panel = *panelIt;
			view.panelPresent = true;
			view.panelRecipeId = detail::Str(panel, kPanelRecipeId);
			view.hasBase = detail::Flag(panel, kPanelHasBase);
			view.hasRecipe = detail::Flag(panel, kPanelHasRecipe);
			view.canInsert = detail::Flag(panel, kPanelCanInsert);
			view.complete = detail::Flag(panel, kPanelComplete);
			view.baseCompatibilityWarning = detail::Flag(panel, kPanelBaseCompatibilityWarning);
			view.baseCompatibilityMessage = detail::Str(panel, kPanelBaseCompatibilityMessage);
			view.missing = detail::Str(panel, kPanelMissing);
		}
		const auto basesIt = a_snapshot.find(kBases);
		if (!view.selectedInstanceKey.empty() && basesIt != a_snapshot.end() && basesIt->is_array()) {
			for (const auto& base : *basesIt) {
				if (detail::Str(base, kBaseInstanceKey) == view.selectedInstanceKey) view.selectedItemRows += 1;
			}
			view.selectedItemListed = view.selectedItemRows > 0u;
		}
		const auto recipesIt = a_snapshot.find(kRecipes);
		if (!view.panelRecipeId.empty() && recipesIt != a_snapshot.end() && recipesIt->is_array()) {
			for (const auto& recipe : *recipesIt) {
				if (detail::Str(recipe, kRecipeId) == view.panelRecipeId) {
					view.panelRecipeListed = true;
					break;
				}
			}
		}
		return view;
	}

	// ---- SELECT_FORMULA decision ---------------------------------------------------------------------------------
	enum class SelectionOutcome : std::uint8_t
	{
		kEligible,         // exact item + recipe verified, insertion allowed: may enter CONFIRM
		kIdentityDrift,    // the refreshed snapshot no longer names the requested item/recipe: refuse, allow a retry
		kOperationFailed,  // the authoritative SelectRecipe call refused
		kAlreadyApplied,
		kIneligible,
		kNotReady
	};

	struct SelectionDecision
	{
		SelectionOutcome outcome{ SelectionOutcome::kNotReady };
		bool identityMatched{ false };
		bool ready{ false };
		ReadinessPolicy::ItemVerdict verdict{ ReadinessPolicy::ItemVerdict::kUnknown };
		// A sticky verdict is remembered for this item/recipe until the item is reselected. Identity drift is never
		// sticky: it says nothing about this item/recipe pair, and its message tells the player to select again.
		bool stickyVerdict{ false };
		std::string message{};
	};

	inline constexpr std::string_view kIdentityDriftMessage =
		"The selected item or formula changed during validation. Select the formula again.";

	// a_requestedItem / a_requestedRecipe: captured BEFORE the authoritative SelectRecipe call.
	// a_workstationItem: the workstation's selected key after RefreshSnapshot. a_focusedRecipe: the focused formula row's
	// id after RefreshSnapshot (focus is restored by recipe id, not index). a_view: ReadSelection(refreshed snapshot).
	[[nodiscard]] inline SelectionDecision DecideFormulaSelection(std::string_view a_requestedItem,
		std::string_view a_workstationItem, std::string_view a_requestedRecipe, std::string_view a_focusedRecipe,
		bool a_operationSucceeded, std::string_view a_operationMessage, const SelectionView& a_view)
	{
		using ReadinessPolicy::ItemVerdict;
		SelectionDecision d{};
		d.identityMatched = a_view.panelPresent && a_view.hasBase &&
			ReadinessPolicy::SelectionIdentitiesMatch(a_requestedItem, a_workstationItem, a_view.selectedInstanceKey,
				a_requestedRecipe, a_view.panelRecipeId, a_focusedRecipe, a_view.hasRecipe) &&
			a_view.selectedItemListed && a_view.selectedItemRows == 1u && a_view.panelRecipeListed;
		d.ready = d.identityMatched && a_view.canInsert && !a_view.complete && a_operationSucceeded;
		if (d.ready) {
			d.outcome = SelectionOutcome::kEligible;
			d.verdict = ItemVerdict::kEligible;
			d.stickyVerdict = true;
			return d;
		}
		if (!a_operationSucceeded) {
			d.outcome = SelectionOutcome::kOperationFailed;
			d.verdict = ItemVerdict::kRefused;
			d.stickyVerdict = true;
			d.message = a_operationMessage.empty() ? std::string("Recipe selection failed.") : std::string(a_operationMessage);
			return d;
		}
		if (!d.identityMatched) {
			d.outcome = SelectionOutcome::kIdentityDrift;
			d.verdict = ItemVerdict::kRefused;
			d.stickyVerdict = false;
			d.message = std::string(kIdentityDriftMessage);
			return d;
		}
		if (a_view.complete) {
			d.outcome = SelectionOutcome::kAlreadyApplied;
			d.verdict = ItemVerdict::kAlreadyApplied;
			d.stickyVerdict = true;
			d.message = "This item already carries the selected runeword.";
			return d;
		}
		if (a_view.baseCompatibilityWarning) {
			d.outcome = SelectionOutcome::kIneligible;
			d.verdict = ItemVerdict::kIneligible;
			d.stickyVerdict = true;
			d.message = a_view.baseCompatibilityMessage.empty() ? std::string("This item cannot carry the selected runeword.") :
			                                                      a_view.baseCompatibilityMessage;
			return d;
		}
		d.outcome = SelectionOutcome::kNotReady;
		d.verdict = ItemVerdict::kRefused;
		d.stickyVerdict = true;
		d.message = a_view.missing.empty() ? std::string("Not ready. Check the required runes and item eligibility.") :
		                                     "Not ready. Missing or blocked: " + a_view.missing;
		return d;
	}

	[[nodiscard]] constexpr std::string_view Name(SelectionOutcome a_outcome) noexcept
	{
		switch (a_outcome) {
		case SelectionOutcome::kEligible: return "ELIGIBLE";
		case SelectionOutcome::kIdentityDrift: return "IDENTITY_DRIFT";
		case SelectionOutcome::kOperationFailed: return "OPERATION_FAILED";
		case SelectionOutcome::kAlreadyApplied: return "ALREADY_APPLIED";
		case SelectionOutcome::kIneligible: return "INELIGIBLE";
		case SelectionOutcome::kNotReady: return "NOT_READY";
		}
		return "?";
	}
}
