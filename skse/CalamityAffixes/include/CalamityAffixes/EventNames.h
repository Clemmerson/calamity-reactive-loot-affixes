#pragma once

#include <string_view>

namespace CalamityAffixes::EventNames
{
	inline constexpr std::string_view kManualModeCycleNext = "CalamityAffixes_ModeCycle_Next";
	inline constexpr std::string_view kManualModeCyclePrev = "CalamityAffixes_ModeCycle_Prev";
	inline constexpr std::string_view kRunewordBaseNext = "CalamityAffixes_Runeword_Base_Next";
	inline constexpr std::string_view kRunewordBasePrev = "CalamityAffixes_Runeword_Base_Prev";
	inline constexpr std::string_view kRunewordRecipeNext = "CalamityAffixes_Runeword_Recipe_Next";
	inline constexpr std::string_view kRunewordRecipePrev = "CalamityAffixes_Runeword_Recipe_Prev";
	inline constexpr std::string_view kRunewordInsertRune = "CalamityAffixes_Runeword_InsertRune";
	inline constexpr std::string_view kRunewordStatus = "CalamityAffixes_Runeword_Status";
	inline constexpr std::string_view kRunewordGrantNextRune = "CalamityAffixes_Runeword_GrantNextRune";
	inline constexpr std::string_view kRunewordGrantRecipeSet = "CalamityAffixes_Runeword_GrantRecipeSet";
	inline constexpr std::string_view kRunewordGrantStarterOrbs = "CalamityAffixes_Runeword_GrantStarterOrbs";
	inline constexpr std::string_view kUiSetPanel = "CalamityAffixes_UI_SetPanel";
	inline constexpr std::string_view kUiTogglePanel = "CalamityAffixes_UI_TogglePanel";
	inline constexpr std::string_view kUiStatus = "CalamityAffixes_UI_Status";
	inline constexpr std::string_view kMcmSpawnTestItem = "CalamityAffixes_MCM_SpawnTestItem";
	inline constexpr std::string_view kMcmGrantTrapAffix = "CalamityAffixes_MCM_GrantTrapAffix";
	inline constexpr std::string_view kMcmTrapMarkerProbe = "CalamityAffixes_MCM_TrapMarkerProbe";
	inline constexpr std::string_view kMcmForceRebuild = "CalamityAffixes_MCM_ForceRebuild";
	inline constexpr std::string_view kMcmGrantRecoveryPack = "CalamityAffixes_MCM_GrantRecoveryPack";
	inline constexpr std::string_view kMcmRecoverMiscCurrency = "CalamityAffixes_MCM_RecoverMiscCurrency";

	// FE ASR owns persistent discovery and sends a full snapshot after DataLoaded
	// and every save load. Numeric argument is API version 1.
	inline constexpr std::string_view kFeasrRuneforgeDiscoveryBegin = "FEASR_Runeforge_Discovery_Begin";
	inline constexpr std::string_view kFeasrRuneforgeDiscoveryRecipe = "FEASR_Runeforge_Discovery_Recipe";
	inline constexpr std::string_view kFeasrRuneforgeDiscoveryComplete = "FEASR_Runeforge_Discovery_Complete";
	inline constexpr std::string_view kFeasrRuneforgeOpen = "FEASR_Runeforge_Open";
}
