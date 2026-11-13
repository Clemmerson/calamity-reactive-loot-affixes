#include "CalamityAffixes/EventBridge.h"
#include "EventBridge.Config.RunewordSynthesis.SpellSet.h"
#include "RunewordIdentityOverrides.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace CalamityAffixes
{
		void EventBridge::SynthesizeRunewordRuntimeAffixes()
		{
			const auto spellSet = RunewordSynthesis::BuildSpellSet(RE::TESDataHandler::GetSingleton());

		auto resolveIdentitySpell = [&](detail::RunewordIdentitySpellRole a_role) -> RE::SpellItem* {
			return spellSet.ResolveIdentitySpell(a_role);
		};

			auto styleName = [](SyntheticRunewordStyle a_style) -> std::string_view {
				return DescribeSyntheticRunewordStyle(a_style);
			};

			auto resolveRunewordStyle = [&](const RunewordRecipe& a_recipe) -> SyntheticRunewordStyle {
				return ResolveSyntheticRunewordStyle(a_recipe);
			};

		std::string runewordTemplateKeywordEditorId;
		RE::BGSKeyword* runewordTemplateKeyword = nullptr;
		for (const auto& affix : _affixRuntimeState.affixes) {
			if (affix.id.rfind("runeword_", 0) == 0 && affix.keyword) {
				runewordTemplateKeywordEditorId = affix.keywordEditorId;
				runewordTemplateKeyword = affix.keyword;
				break;
			}
		}

		if (!_runewordState.recipes.empty()) {
			// Runeword synthesis appends many entries; reserve once to reduce realloc churn.
			_affixRuntimeState.affixes.reserve(_affixRuntimeState.affixes.size() + _runewordState.recipes.size());
		}

		std::uint32_t synthesizedRunewordAffixes = 0u;
		for (const auto& recipe : _runewordState.recipes) {
			if (recipe.resultAffixToken == 0u || _affixRuntimeState.affixRegistry.affixIndexByToken.contains(recipe.resultAffixToken)) {
				continue;
			}

			const std::uint32_t runeCount =
				static_cast<std::uint32_t>(std::max<std::size_t>(recipe.runeIds.size(), 1u));

			std::string runeSequence;
			for (std::size_t i = 0; i < recipe.runeIds.size(); ++i) {
				if (i > 0) {
					runeSequence.push_back('-');
				}
				runeSequence.append(recipe.runeIds[i]);
			}

			AffixRuntime out{};
			out.id = recipe.id + "_auto";
			out.token = recipe.resultAffixToken;
			out.action.sourceToken = out.token;
			const std::string suffix = runeSequence.empty() ? "(Auto)" : ("[" + runeSequence + "]");
			const std::string recipeNameEn = recipe.displayNameEn.empty() ? recipe.id : recipe.displayNameEn;
			const std::string recipeNameKo = recipe.displayNameKo.empty() ? recipe.displayName : recipe.displayNameKo;
			out.displayNameEn = "Runeword " + recipeNameEn + " " + suffix;
			// FE-ASR presentation for manifest recipes; unknown recipes keep the upstream name.
			out.displayNameEn = RuneforgePresentation::RunewordResultAffixNameEn(recipe.id, recipe.runeIds, out.displayNameEn);
			out.displayNameKo = "룬워드 " + recipeNameKo + " " + suffix;
			out.displayName = out.displayNameKo;
			out.label = recipeNameKo;
			if (runewordTemplateKeyword) {
				out.keywordEditorId = runewordTemplateKeywordEditorId;
				out.keyword = runewordTemplateKeyword;
			}

				auto style = resolveRunewordStyle(recipe);
				bool ready = ConfigureSyntheticRunewordStyle(out, style, recipe, runeCount, spellSet);

			const auto applyRunewordIndividualTuning = [&](std::string_view a_recipeId, SyntheticRunewordStyle a_style) {
				ApplyRunewordIndividualTuning(out, a_recipeId, a_style);
			};

				if (ready) {
					if (const auto identityOverride = detail::ResolveRunewordIdentityOverride(recipe.id)) {
						if (identityOverride->actionKind == detail::RunewordIdentityActionKind::kCastSpellAdaptiveElement) {
							out.action.type = ActionType::kCastSpellAdaptiveElement;
							out.action.adaptiveMode = identityOverride->adaptiveMode;
							out.action.adaptiveFire = resolveIdentitySpell(identityOverride->adaptiveFire);
							out.action.adaptiveFrost = resolveIdentitySpell(identityOverride->adaptiveFrost);
							out.action.adaptiveShock = resolveIdentitySpell(identityOverride->adaptiveShock);
							out.action.applyToSelf = false;
							out.action.noHitEffectArt = identityOverride->noHitEffectArt;
						} else {
							if (auto* overrideSpell = resolveIdentitySpell(identityOverride->spell)) {
								out.action.type = ActionType::kCastSpell;
								out.action.spell = overrideSpell;
								out.action.applyToSelf = identityOverride->applyToSelf;
								out.action.noHitEffectArt = identityOverride->noHitEffectArt;
							}
						}
					}
					applyRunewordIndividualTuning(recipe.id, style);
				}

			if (!ready) {
				SKSE::log::warn(
					"CalamityAffixes: runeword result affix unresolved (recipe={}, resultToken={:016X}).",
					recipe.id,
					recipe.resultAffixToken);
				continue;
			}

				RegisterSynthesizedAffix(std::move(out), false);
				synthesizedRunewordAffixes += 1u;
				SKSE::log::info(
					"CalamityAffixes: synthesized runeword runtime affix (recipe={}, style={}, resultToken={:016X}).",
					recipe.id,
					styleName(style),
					recipe.resultAffixToken);
		}

		if (synthesizedRunewordAffixes > 0u) {
			SKSE::log::info(
				"CalamityAffixes: synthesized {} runeword runtime affix definitions.",
				synthesizedRunewordAffixes);
		}
	}

}
