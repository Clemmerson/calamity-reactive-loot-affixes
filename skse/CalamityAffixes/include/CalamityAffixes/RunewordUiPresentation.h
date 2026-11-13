#pragma once

#include "CalamityAffixes/RuneforgePresentation.h"
#include "CalamityAffixes/RunewordUiContracts.h"

#include <span>
#include <string>
#include <string_view>
#include <utility>

// FE-ASR presentation for the runeword UI DTOs (Prisma panel payloads and the
// native Runeforge AS2 snapshot). The DTOs are first filled from upstream
// Calamity data exactly as before; these helpers then replace the English
// display fields in one place. Mechanical fields (recipeId, recipeToken,
// runeTokens, instance keys) are never touched.
namespace CalamityAffixes::RunewordUiPresentation
{
	// a_internalRunes:      the recipe's mechanical rune list (RunewordRecipe::runeIds).
	// a_upstreamNameEn:     the upstream English recipe name (for record-name text).
	// a_publicSummaryEn:    mechanical summary sentence built around the FE-ASR
	//                       effect (empty -> the FE-ASR qualitative summary alone).
	inline void ApplyRecipeEntryPresentation(
		RunewordRecipeEntry& a_entry,
		std::span<const std::string> a_internalRunes,
		std::string_view a_upstreamNameEn,
		std::string a_publicSummaryEn)
	{
		// Rune vocabulary is global: a known rune reads the same in every recipe,
		// including third-party recipes whose names stay upstream.
		a_entry.runeSequence = RuneforgePresentation::PresentRuneSequence(a_internalRunes);

		const auto* row = RuneforgePresentation::FindRecipe(a_entry.recipeId);
		if (!row) {
			return;
		}
		a_entry.displayNameEn = std::string(row->publicName);
		a_entry.effectSummaryTextEn = a_publicSummaryEn.empty() ?
			std::string(row->effectSummary) :
			std::move(a_publicSummaryEn);
		a_entry.effectDetailTextEn = RuneforgePresentation::PresentRunewordRecordText(
			a_entry.effectDetailTextEn,
			a_entry.recipeId,
			a_upstreamNameEn);
	}

	// English recipe fields for the native Runeforge snapshot. The snapshot keeps
	// "id"/"token"/"runeTokens" mechanical; these are the display fields beside them.
	struct RuneforgeRecipeSnapshotText
	{
		std::string name{};
		std::string runeSequence{};
		std::string glyphSequence{};
		std::string effect{};
		bool feasrPresentation{ false };
	};

	// a_entry must already carry ApplyRecipeEntryPresentation. "effect" is the
	// FE-ASR qualitative summary -- the same value the AS2 bridge assigns by id --
	// so the AS2 transform is a no-op on native output rather than a second
	// translation.
	[[nodiscard]] inline RuneforgeRecipeSnapshotText BuildRuneforgeRecipeSnapshotText(const RunewordRecipeEntry& a_entry)
	{
		RuneforgeRecipeSnapshotText out{};
		out.feasrPresentation = RuneforgePresentation::IsKnownRecipe(a_entry.recipeId);
		out.name = a_entry.displayNameEn;
		out.runeSequence = a_entry.runeSequence;
		out.glyphSequence = std::string(RuneforgePresentation::GlyphSequence(a_entry.recipeId));
		out.effect = std::string(RuneforgePresentation::EffectSummaryOr(a_entry.recipeId, a_entry.effectSummaryTextEn));
		return out;
	}
}
