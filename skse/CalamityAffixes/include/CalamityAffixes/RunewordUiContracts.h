#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CalamityAffixes/RunewordEffectCardPolicy.h"

namespace CalamityAffixes
{
	struct RunewordBaseInventoryEntry
	{
		std::uint64_t instanceKey{ 0 };
		std::string displayName{};  // "<item> [Runeword: <Korean name>]" for older panels
		std::string displayNameEn{};  // English-safe label for FEASR's native Runeforge
		std::string baseName{};
		std::string itemKind{};
		std::string runewordNameEn{};
		std::string runewordNameKo{};
		// Mechanical rw_* ID of the completed runeword ("" when none). UIs match on
		// this, never on the display name.
		std::string runewordRecipeId{};
		bool selected{ false };
	};

	struct RuneforgeInventoryEntry
	{
		std::uint64_t instanceKey{ 0 };
		std::string displayName{};
		std::string displayNameEn{};
		std::string baseName{};
		std::string itemKind{};
		std::string runewordNameEn{};
		std::string runewordNameKo{};
		std::string runewordRecipeId{};
		bool selected{ false };
		bool equipped{ false };
		bool favorited{ false };
	};

	struct RuneforgeInventorySnapshot
	{
		// Monotonic read sequence for correlating a UI response with its rows.
		std::uint64_t revision{ 0 };
		bool known{ false };
		std::uint32_t skippedMissingUniqueId{ 0 };
		std::uint32_t skippedNonSingleton{ 0 };
		std::uint32_t skippedAmbiguousKeys{ 0 };
		std::vector<RuneforgeInventoryEntry> entries{};
	};

	struct RunewordRecipeEntry
	{
		std::string recipeId{};
		std::uint64_t recipeToken{ 0 };
		std::string displayName{};
		std::string displayNameEn{};
		std::string displayNameKo{};
		std::string runeSequence{};
		// Preserve recipe order and duplicates. The Prisma view uses this
		// authoritative token sequence to compare requirements with the dynamic
		// inventory snapshot without parsing display text.
		std::vector<std::uint64_t> runeTokens{};
		std::string effectSummaryKey{};
		std::string effectSummaryTextEn{};
		std::string effectSummaryTextKo{};
		std::string effectDetailTextEn{};
		std::string effectDetailTextKo{};
		std::string recommendedBaseKey{};
		bool selected{ false };
		// R8.7.4.21 I3: RunewordEffectCard v1 built from the resolved AffixRuntime + SpellItem data (base proc chance;
		// no instance context). known == false for recipes outside the FE-ASR manifest.
		RunewordEffectCardPolicy::Card effectCard{};
	};

	struct RunewordRuneRequirement
	{
		std::string runeName{};
		std::uint32_t required{ 0 };
		std::uint32_t owned{ 0 };
	};

	struct RunewordRuneInventoryEntry
	{
		std::uint64_t runeToken{ 0 };
		std::string runeName{};
		std::uint32_t owned{ 0 };
	};

	struct RunewordReforgeLockCandidate
	{
		std::uint64_t affixToken{ 0 };
		std::string displayNameEn{};
		std::string displayNameKo{};
		std::string slotKind{};
	};

	struct EquippedBuildEntry
	{
		std::uint64_t token{ 0 };
		std::string displayNameEn{};
		std::string displayNameKo{};
		std::string effectSummaryEn{};
		std::string group{};
		std::string triggerKey{};
		std::string slotKind{};
		std::string suffixState{};
		std::uint32_t equippedCount{ 0 };
		std::uint32_t suffixTierRank{ 0 };
		std::uint32_t suffixFamilyRankPoints{ 0 };
		std::uint32_t effectiveSuffixTierRank{ 0 };
		std::string effectiveSuffixDisplayNameEn{};
		std::string effectiveSuffixDisplayNameKo{};
		bool hasPassiveContribution{ false };
		bool passiveContributionActive{ false };
		bool passiveSpellDisabled{ false };
		bool hasProcRoll{ false };
		float procRollChancePct{ 0.0f };
		std::uint32_t procRollStackCount{ 0 };
		bool castOnCritSelectionLimited{ false };
		bool hasNormalWeaponHitProcRoll{ false };
		float normalWeaponHitProcChancePct{ 0.0f };
		std::uint32_t normalWeaponHitProcStackCount{ 0 };
		bool hasLuckyHitGate{ false };
		float luckyHitGateChancePct{ 0.0f };
		float cooldownSeconds{ 0.0f };
		float perTargetCooldownSeconds{ 0.0f };
		float lowHealthThresholdPct{ 0.0f };
		float lowHealthRearmPct{ 0.0f };
	};

	struct EquippedBuildSummary
	{
		bool ready{ false };
		bool runtimeEnabled{ false };
		std::uint32_t equippedAffixSlots{ 0 };
		std::vector<EquippedBuildEntry> entries{};
	};

	struct RunewordPanelState
	{
		bool hasBase{ false };
		bool hasRecipe{ false };
		bool isComplete{ false };
		std::string recipeName{};
		std::string recipeNameEn{};
		std::string recipeNameKo{};
		std::uint64_t recipeToken{ 0 };
		std::uint32_t insertedRunes{ 0 };
		std::uint32_t totalRunes{ 0 };
		std::string nextRuneName{};
		std::uint32_t nextRuneOwned{ 0 };
		bool canInsert{ false };
		std::string missingSummary{};
		bool baseCompatibilityWarning{ false };
		std::string baseCompatibilityMessageEn{};
		std::string baseCompatibilityMessageKo{};
		std::vector<RunewordRuneRequirement> requiredRunes{};
		// False means the snapshot is unavailable and consumers must not treat
		// the empty vector as an inventory containing zero fragments.
		bool runeInventoryKnown{ false };
		std::uint32_t runeInventoryExpectedCount{ 0 };
		std::vector<RunewordRuneInventoryEntry> runeInventory{};
		std::uint32_t regularAffixCount{ 0 };
		// Ordered runtime tokens for the regular affix state; excludes the completed runeword token.
		std::vector<std::uint64_t> regularAffixTokens{};
		std::uint32_t maxRegularAffixCount{ 0 };
		std::uint32_t expandAffixCost{ 0 };
		bool canExpandAffix{ false };
		std::string expandAffixUnavailableReason{ "unavailable" };
		// Counts are authoritative only when their corresponding known flag is
		// true. This keeps an unavailable snapshot distinct from a real zero.
		bool reforgeOrbsKnown{ false };
		std::uint32_t reforgeOrbsOwned{ 0 };
        bool identifyScrollsKnown{ false };
        std::uint32_t identifyScrollsOwned{ 0 };
        bool scouringOrbsKnown{ false };
        std::uint32_t scouringOrbsOwned{ 0 };
        // Identify Scrolls spent per exchanged orb; 0 means exchange is unavailable.
        std::uint32_t exchangeReforgeScrollCost{ 0 };
        std::uint32_t exchangeScourScrollCost{ 0 };
		bool pityKnown{ false };
		// A streak equal to its threshold means the next eligible ordinary
		// currency evaluation is guaranteed. It is not a kill/drop countdown.
		std::uint32_t runewordFragmentFailStreak{ 0 };
		std::uint32_t runewordFragmentFailStreakThreshold{ 0 };
		std::uint32_t reforgeOrbFailStreak{ 0 };
		std::uint32_t reforgeOrbFailStreakThreshold{ 0 };
		std::uint32_t standardReforgeCost{ 0 };
		std::uint32_t lockedReforgeCost{ 0 };
		// Selected reforges the working base may still use (0 = spent until
		// scoured), out of selectedReforgesPerItem. Older DLLs send neither.
		std::uint32_t selectedReforgesLeft{ 0 };
		std::uint32_t selectedReforgesPerItem{ 0 };
		std::vector<RunewordReforgeLockCandidate> reforgeLockCandidates{};
		EquippedBuildSummary equippedBuild{};
		// Gates the panel's cheat-adjacent debug tools; true only while a debug
		// toggle (HUD notifications or verbose logging) is enabled.
		bool debugTools{ false };
	};

	struct OperationResult
	{
		bool success{ false };
		std::string message{};
	};
}
