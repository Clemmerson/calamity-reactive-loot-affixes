#pragma once

// Runeforge workstation formula readiness and the physical rune row (RC1-WS, 2026-10-19; PRES-D31, R3 contract section 5).
// Pure policy. The list itself holds learned/discovered formulas only (RuneforgeMenu BuildSnapshot filters on
// IsFeasrRunewordRecipeKnown outside debug mode); a learned formula stays browseable without its rune pieces and shows a
// readiness state with exact required/owned counts. The physical row stages an owned rune's tablet in its formula slot and
// leaves a missing rune's slot empty (glyph 0): never a ghost stone for a rune the player does not possess.

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace CalamityAffixes::RuneforgeWorkstation::ReadinessPolicy
{
	enum class State : std::uint8_t
	{
		kReady,
		kMissingRunes,
		kItemIneligible,
		kAlreadyApplied,
		kRefused,
		kNoItem,
		kInventoryUnknown,
		kCompatibilityUnknown
	};

	// The verdict of the item/recipe preconditions for this formula, once known (a refused Accept records it).
	enum class ItemVerdict : std::uint8_t
	{
		kUnknown,
		kEligible,
		kIneligible,
		kAlreadyApplied,
		kRefused
	};

	struct InventoryEntry
	{
		std::uint64_t token{ 0 };
		std::uint32_t owned{ 0 };
	};

	struct RuneNeed
	{
		std::uint64_t token{ 0 };
		std::uint32_t required{ 0 };
		std::uint32_t owned{ 0 };
	};

	struct Readiness
	{
		State state{ State::kInventoryUnknown };
		ItemVerdict itemVerdict{ ItemVerdict::kUnknown };
		bool runesAvailable{ false };
		std::vector<std::uint8_t> slots{};  // one per formula slot: the glyph to stage, or 0 (left empty)
		std::vector<RuneNeed> needs{};      // distinct runes in first-use order with exact counts
		std::uint32_t ownedSlots{ 0 };
		std::uint32_t slotCount{ 0 };
	};

	// Do not advertise a selection as eligible unless the refreshed authoritative snapshot still
	// identifies the exact item and recipe and confirms insertion is allowed.
	[[nodiscard]] constexpr bool SelectionIdentitiesMatch(std::string_view a_requestedItem,
		std::string_view a_selectedItem, std::string_view a_snapshotItem, std::string_view a_requestedRecipe,
		std::string_view a_panelRecipe, std::string_view a_focusedRecipe, bool a_hasRecipe) noexcept
	{
		return a_hasRecipe && !a_requestedItem.empty() &&
			a_requestedItem == a_selectedItem && a_requestedItem == a_snapshotItem &&
			!a_requestedRecipe.empty() && a_requestedRecipe == a_panelRecipe &&
			a_requestedRecipe == a_focusedRecipe;
	}

	[[nodiscard]] constexpr bool MatchesSuccessfulSelection(std::string_view a_requestedItem,
		std::string_view a_selectedItem, std::string_view a_snapshotItem, std::string_view a_requestedRecipe,
		std::string_view a_panelRecipe, std::string_view a_focusedRecipe, bool a_hasRecipe,
		bool a_canInsert, bool a_operationSucceeded) noexcept
	{
		return SelectionIdentitiesMatch(a_requestedItem, a_selectedItem, a_snapshotItem, a_requestedRecipe,
			a_panelRecipe, a_focusedRecipe, a_hasRecipe) && a_canInsert && a_operationSucceeded;
	}

	[[nodiscard]] inline std::uint32_t OwnedOf(const std::vector<InventoryEntry>& a_inventory, std::uint64_t a_token) noexcept
	{
		for (const auto& e : a_inventory) {
			if (e.token == a_token) return e.owned;
		}
		return 0u;
	}

	// a_tokens / a_glyphs: the formula's rune tokens and glyph indices in slot order (same length when the presentation
	// table is consistent). A mismatch or an unknown inventory is INVENTORY UNKNOWN and stages nothing (fail closed).
	[[nodiscard]] inline Readiness Evaluate(const std::vector<std::uint64_t>& a_tokens, const std::vector<std::uint8_t>& a_glyphs,
		bool a_inventoryKnown, const std::vector<InventoryEntry>& a_inventory, bool a_hasItem, ItemVerdict a_verdict)
	{
		Readiness r{};
		r.itemVerdict = a_verdict;
		r.slotCount = static_cast<std::uint32_t>(a_glyphs.size());
		r.slots.assign(a_glyphs.size(), 0u);
		if (!a_inventoryKnown || a_tokens.empty() || a_tokens.size() != a_glyphs.size()) {
			r.state = State::kInventoryUnknown;
			return r;
		}
		std::vector<std::pair<std::uint64_t, std::uint32_t>> remaining;
		for (std::size_t i = 0; i < a_tokens.size(); ++i) {
			const auto token = a_tokens[i];
			bool counted = false;
			for (auto& need : r.needs) {
				if (need.token == token) {
					need.required += 1;
					counted = true;
				}
			}
			if (!counted) {
				const auto owned = OwnedOf(a_inventory, token);
				r.needs.push_back({ token, 1u, owned });
				remaining.emplace_back(token, owned);
			}
			for (auto& [t, left] : remaining) {
				if (t != token) continue;
				if (left > 0u) {
					left -= 1;
					r.slots[i] = a_glyphs[i];
					r.ownedSlots += 1;
				}
			}
		}
		const bool allOwned = r.ownedSlots == r.slotCount;
		r.runesAvailable = allOwned;
		if (!a_hasItem) r.state = State::kNoItem;
		else if (a_verdict == ItemVerdict::kIneligible) r.state = State::kItemIneligible;
		else if (a_verdict == ItemVerdict::kAlreadyApplied) r.state = State::kAlreadyApplied;
		else if (a_verdict == ItemVerdict::kRefused) r.state = State::kRefused;
		else if (!allOwned) r.state = State::kMissingRunes;
		else if (a_verdict == ItemVerdict::kUnknown) r.state = State::kCompatibilityUnknown;
		else r.state = State::kReady;
		return r;
	}

	[[nodiscard]] constexpr std::string_view Name(State a_state) noexcept
	{
		switch (a_state) {
		case State::kReady: return "READY";
		case State::kMissingRunes: return "MISSING RUNES";
		case State::kItemIneligible: return "ITEM INELIGIBLE";
		case State::kAlreadyApplied: return "ALREADY APPLIED";
		case State::kRefused: return "REFUSED";
		case State::kNoItem: return "NO ITEM";
		case State::kInventoryUnknown: return "INVENTORY UNKNOWN";
		case State::kCompatibilityUnknown: return "COMPATIBILITY UNKNOWN";
		}
		return "?";
	}

	// Selecting a formula is allowed to run the authoritative, stateful compatibility check only after
	// the player explicitly accepts it. Unknown compatibility never appears READY in the browse snapshot.
	[[nodiscard]] constexpr bool CanAttemptExplicitSelection(const Readiness& a_readiness) noexcept
	{
		return a_readiness.runesAvailable && a_readiness.state != State::kNoItem &&
			a_readiness.state != State::kInventoryUnknown && a_readiness.state != State::kItemIneligible &&
			a_readiness.state != State::kAlreadyApplied && a_readiness.state != State::kRefused;
	}
}
