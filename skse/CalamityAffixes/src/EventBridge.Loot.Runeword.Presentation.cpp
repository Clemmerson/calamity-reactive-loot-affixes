#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/RuneforgePresentation.h"

#include <string>
#include <string_view>

// FE-ASR presentation boundary for text that leaves the native runeword layer.
// Everything here reads mechanical state (_runewordState.recipes,
// recipeIndexByResultAffixToken, runeNameByToken) and never writes it.
namespace CalamityAffixes
{
	static_assert(
		RuneforgePresentation::IsGeneratedTableConsistent(),
		"RuneforgePresentation.generated.h is malformed; regenerate it with tools/generate_runeforge_presentation.py");
	static_assert(RuneforgePresentation::Generated::kRunes.size() == 33u);
	static_assert(RuneforgePresentation::Generated::kRecipes.size() == 95u);

	const EventBridge::RunewordRecipe* EventBridge::FindRunewordRecipeByResultAffixToken(
		std::uint64_t a_resultAffixToken) const
	{
		if (a_resultAffixToken == 0u) {
			return nullptr;
		}
		const auto it = _runewordState.recipeIndexByResultAffixToken.find(a_resultAffixToken);
		if (it == _runewordState.recipeIndexByResultAffixToken.end() || it->second >= _runewordState.recipes.size()) {
			return nullptr;
		}
		return std::addressof(_runewordState.recipes[it->second]);
	}

	std::string EventBridge::ResolveAffixDisplayNameEn(const AffixRuntime& a_affix) const
	{
		// Runeword result affixes (JSON-defined or synthesized) carry the recipe name
		// and rune sequence in their English label; every other affix is unchanged.
		if (const auto* recipe = FindRunewordRecipeByResultAffixToken(a_affix.token)) {
			return RuneforgePresentation::RunewordResultAffixNameEn(recipe->id, recipe->runeIds, a_affix.displayNameEn);
		}
		return a_affix.displayNameEn;
	}

	std::string EventBridge::ResolveRuneDisplayName(std::uint64_t a_runeToken, std::string_view a_fallback) const
	{
		// runeNameByToken holds the internal rune key (fragment EditorID, weights);
		// only the returned copy is mapped to the FE-ASR name.
		const auto nameIt = _runewordState.runeNameByToken.find(a_runeToken);
		if (nameIt == _runewordState.runeNameByToken.end() || nameIt->second.empty()) {
			return std::string(a_fallback);
		}
		return std::string(RuneforgePresentation::RuneNameOr(nameIt->second));
	}
}
