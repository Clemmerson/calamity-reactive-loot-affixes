#pragma once
#include "CalamityAffixes/InstanceAffixSlots.h"
#include <cstdint>
#include <string_view>

namespace CalamityAffixes
{
	enum class AffixCraftAction : std::uint8_t { kIdentify, kReforge, kScour };
	enum class CurrencyExchange : std::uint8_t { kReforgeOrb, kScouringOrb };
	namespace detail
	{
		inline constexpr std::uint32_t kSelectedReforgeCost = 2u;
		inline constexpr std::uint32_t kIdentifyScrollCost = 1u;
		inline constexpr std::uint32_t kScouringOrbCost = 1u;
		// Selected reforge may be used 6 times per item; identifying or scouring the
		// item starts the count over. Without a limit, keeping every other affix
		// always beat rerolling all of them, so the rarer Scouring Orb had no job.
		inline constexpr std::uint8_t kSelectedReforgesPerItem = 6u;

		[[nodiscard]] constexpr std::uint8_t RemainingSelectedReforges(std::uint8_t a_reforgesDone) noexcept
		{
			return a_reforgesDone >= kSelectedReforgesPerItem ?
				0u :
				static_cast<std::uint8_t>(kSelectedReforgesPerItem - a_reforgesDone);
		}

		[[nodiscard]] constexpr std::uint8_t NextSelectedReforgeCount(std::uint8_t a_reforgesDone) noexcept
		{
			return a_reforgesDone == 0xFFu ? a_reforgesDone : static_cast<std::uint8_t>(a_reforgesDone + 1u);
		}

		[[nodiscard]] constexpr std::uint8_t IdentifyAffixCount(std::uint32_t a_roll) noexcept
		{
			return a_roll < 60u ? 1u : (a_roll < 90u ? 2u : 3u);
		}

		[[nodiscard]] constexpr std::string_view CraftCurrency(AffixCraftAction a_action) noexcept
		{
			switch (a_action) {
			case AffixCraftAction::kIdentify: return "CAFF_Misc_IdentifyScroll";
			case AffixCraftAction::kReforge: return "CAFF_Misc_ReforgeOrb";
			case AffixCraftAction::kScour: return "CAFF_Misc_ScouringOrb";
			}
			return {};
		}

		inline constexpr std::uint8_t kCraftRollMaxAttempts = 4u;

		// A complete regular layout: 1 Prefix + (count - 1) Suffixes, up to 3.
		// Not IsCanonicalRegularAffixExpansionLayout, which only accepts the
		// expandable 1-2 layouts and would reject every finished 3-affix base.
		[[nodiscard]] constexpr bool IsCanonicalRegularAffixLayout(
			std::uint8_t a_count, std::uint8_t a_prefixCount, std::uint8_t a_suffixCount) noexcept
		{
			return a_count >= 1u && a_count <= kMaxRegularAffixesPerItem &&
				a_prefixCount == 1u && a_suffixCount + 1u == a_count;
		}

		// Scour is the recovery path: it rerolls legacy layouts (unknown or
		// unslotted tokens, duplicate suffix families, other splits) into a
		// canonical one. Identify and selected reforge need a canonical base.
		[[nodiscard]] constexpr bool CanCraftAffixLayout(
			AffixCraftAction a_action, std::uint8_t a_regularCount, std::uint8_t a_prefixCount,
			std::uint8_t a_suffixCount, bool a_hasLegacyAffix) noexcept
		{
			return a_regularCount == 0u || a_action == AffixCraftAction::kScour ||
				(!a_hasLegacyAffix && IsCanonicalRegularAffixLayout(a_regularCount, a_prefixCount, a_suffixCount));
		}

		// Scour keeps the slot count; legacy overflow is clamped to the regular maximum.
		[[nodiscard]] constexpr std::uint8_t ScourAffixCount(std::uint8_t a_regularCount) noexcept
		{
			return a_regularCount < kMaxRegularAffixesPerItem ?
				a_regularCount : static_cast<std::uint8_t>(kMaxRegularAffixesPerItem);
		}

		// Identify Scrolls have no use once gear is identified (scour keeps the slot
		// count), so they pile up. One-way trades turn them into the currencies that
		// run out: 3:1 follows the 20% : 12% drop ratio with a small loss. 10:1
		// (15:1 before the reforge limit) keeps the Scouring Orb scarcer than a
		// Reforge Orb but reachable, since it is now how a spent item unlocks.
		inline constexpr std::uint32_t kExchangeScrollsPerReforgeOrb = 3u;
		inline constexpr std::uint32_t kExchangeScrollsPerScouringOrb = 10u;

		struct CurrencyExchangeRecipe
		{
			std::string_view sourceEditorId;
			std::string_view targetEditorId;
			std::uint32_t sourceCost{ 0u };
		};

		[[nodiscard]] constexpr CurrencyExchangeRecipe ResolveCurrencyExchange(CurrencyExchange a_exchange) noexcept
		{
			switch (a_exchange) {
			case CurrencyExchange::kReforgeOrb:
				return { CraftCurrency(AffixCraftAction::kIdentify), CraftCurrency(AffixCraftAction::kReforge),
					kExchangeScrollsPerReforgeOrb };
			case CurrencyExchange::kScouringOrb:
				return { CraftCurrency(AffixCraftAction::kIdentify), CraftCurrency(AffixCraftAction::kScour),
					kExchangeScrollsPerScouringOrb };
			}
			return {};
		}

		[[nodiscard]] constexpr bool CanCraftAffixes(
			AffixCraftAction a_action, const InstanceAffixSlots& a_regular,
			std::uint64_t a_selectedToken) noexcept
		{
			switch (a_action) {
			case AffixCraftAction::kIdentify: return a_regular.count == 0u && a_selectedToken == 0u;
			case AffixCraftAction::kReforge:
				return a_regular.count <= kMaxRegularAffixesPerItem && a_selectedToken != 0u &&
					a_regular.HasToken(a_selectedToken);
			case AffixCraftAction::kScour: return a_regular.count > 0u && a_selectedToken == 0u;
			}
			return false;
		}

		// Replacing a slot must preserve its position and every unselected token.
		[[nodiscard]] constexpr bool ReplaceSelectedAffix(
			InstanceAffixSlots& a_slots, std::uint64_t a_old, std::uint64_t a_new) noexcept
		{
			if (a_old == 0u || a_new == 0u || a_slots.HasToken(a_new)) return false;
			for (std::uint8_t i = 0; i < a_slots.count; ++i) {
				if (a_slots.tokens[i] == a_old) {
					a_slots.tokens[i] = a_new;
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] constexpr bool IsValidCraftResult(
			AffixCraftAction action, const InstanceAffixSlots& before, const InstanceAffixSlots& after,
			std::uint64_t runeword, std::uint64_t selected) noexcept
		{
			if (before.count > kMaxAffixesPerItem || after.count > kMaxAffixesPerItem) return false;
			for (std::uint8_t i = 0; i < after.count; ++i) {
				if (after.tokens[i] == 0u) return false;
				for (std::uint8_t j = 0; j < i; ++j)
					if (after.tokens[i] == after.tokens[j]) return false;
			}
			if (runeword != 0u && (before.GetPrimary() != runeword || after.GetPrimary() != runeword)) return false;
			const auto runeCount = runeword != 0u ? 1u : 0u;
			switch (action) {
			case AffixCraftAction::kIdentify:
				return before.count == runeCount && after.count > runeCount && after.count <= runeCount + 3u;
			case AffixCraftAction::kScour: {
				if (before.count <= runeCount) return false;
				const auto target = ScourAffixCount(static_cast<std::uint8_t>(before.count - runeCount));
				if (after.count != runeCount + target) return false;
				// An identical reroll would spend a rare orb for nothing.
				if (after.count != before.count) return true;
				for (std::uint8_t i = 0; i < after.count; ++i)
					if (!before.HasToken(after.tokens[i])) return true;
				return false;
			}
			case AffixCraftAction::kReforge:
				if (selected == 0u || selected == runeword || !before.HasToken(selected) ||
					after.HasToken(selected) || before.count != after.count) return false;
				for (std::uint8_t i = 0; i < before.count; ++i)
					if (before.tokens[i] != selected && before.tokens[i] != after.tokens[i]) return false;
				return true;
			}
			return false;
		}
	}
}
