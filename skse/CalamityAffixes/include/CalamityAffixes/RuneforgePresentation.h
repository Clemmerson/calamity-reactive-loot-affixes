#pragma once

#include "CalamityAffixes/RuneforgePresentation.generated.h"

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

// FE-ASR native presentation resolver.
//
// One authority: RuneforgePresentation.generated.h, generated from
// feasr-core/content/runeforge-presentation.v1.json. Every function maps a
// Calamity mechanical key (rw_* recipe ID, internal rune name) to the text the
// player sees. Nothing here maps the other way: callers keep the mechanical key
// for lookups, actions, persistence and fragment EditorIDs, and pass only the
// result of these functions to HUD notes, menus and tooltips.
//
// Fallback contract: a key the manifest does not know (third-party or future
// content) resolves to the caller's upstream text unchanged. A key the manifest
// does know always resolves to FE-ASR text; the generated table is validated
// against the pinned contract at generation time and by IsGeneratedTableConsistent().
namespace CalamityAffixes::RuneforgePresentation
{
	using Generated::RecipeRow;
	using Generated::RuneRow;

	namespace impl
	{
		template <class Row, std::size_t N, class KeyOf>
		[[nodiscard]] constexpr const Row* FindSortedRow(
			const std::array<Row, N>& a_rows,
			std::string_view a_key,
			KeyOf a_keyOf) noexcept
		{
			std::size_t low = 0;
			std::size_t high = N;
			while (low < high) {
				const std::size_t mid = low + (high - low) / 2u;
				if (a_keyOf(a_rows[mid]) < a_key) {
					low = mid + 1u;
				} else {
					high = mid;
				}
			}
			return (low < N && a_keyOf(a_rows[low]) == a_key) ? &a_rows[low] : nullptr;
		}

		// Visits each token of a separator-joined sequence; stops early when the
		// visitor returns false. Returns false if any visit returned false.
		template <class Visitor>
		[[nodiscard]] constexpr bool ForEachToken(
			std::string_view a_sequence,
			std::string_view a_separator,
			Visitor a_visit)
		{
			std::size_t start = 0;
			std::size_t position = 0;
			while (true) {
				const auto next = a_sequence.find(a_separator, start);
				const auto token = a_sequence.substr(start, next == std::string_view::npos ? std::string_view::npos : next - start);
				if (!a_visit(token, position)) {
					return false;
				}
				if (next == std::string_view::npos) {
					return true;
				}
				start = next + a_separator.size();
				++position;
			}
		}
	}

	[[nodiscard]] constexpr const RuneRow* FindRune(std::string_view a_internalRune) noexcept
	{
		return impl::FindSortedRow(
			Generated::kRunes,
			a_internalRune,
			[](const RuneRow& a_row) { return a_row.internalName; });
	}

	[[nodiscard]] constexpr const RecipeRow* FindRecipe(std::string_view a_recipeId) noexcept
	{
		return impl::FindSortedRow(
			Generated::kRecipes,
			a_recipeId,
			[](const RecipeRow& a_row) { return a_row.id; });
	}

	[[nodiscard]] constexpr bool IsKnownRune(std::string_view a_internalRune) noexcept
	{
		return FindRune(a_internalRune) != nullptr;
	}

	[[nodiscard]] constexpr bool IsKnownRecipe(std::string_view a_recipeId) noexcept
	{
		return FindRecipe(a_recipeId) != nullptr;
	}

	// Strict lookups: empty for keys the manifest does not know.
	[[nodiscard]] constexpr std::string_view RecipeName(std::string_view a_recipeId) noexcept
	{
		const auto* row = FindRecipe(a_recipeId);
		return row ? row->publicName : std::string_view{};
	}

	[[nodiscard]] constexpr std::string_view RuneName(std::string_view a_internalRune) noexcept
	{
		const auto* row = FindRune(a_internalRune);
		return row ? row->publicName : std::string_view{};
	}

	[[nodiscard]] constexpr std::string_view RuneGlyph(std::string_view a_internalRune) noexcept
	{
		const auto* row = FindRune(a_internalRune);
		return row ? row->glyph : std::string_view{};
	}

	// FE-ASR rune names in the recipe's mechanical order, '-' separated.
	[[nodiscard]] constexpr std::string_view RuneSequence(std::string_view a_recipeId) noexcept
	{
		const auto* row = FindRecipe(a_recipeId);
		return row ? row->publicRuneSequence : std::string_view{};
	}

	// Glyph asset keys in the recipe's mechanical order, ',' separated.
	[[nodiscard]] constexpr std::string_view GlyphSequence(std::string_view a_recipeId) noexcept
	{
		const auto* row = FindRecipe(a_recipeId);
		return row ? row->glyphSequence : std::string_view{};
	}

	[[nodiscard]] constexpr std::string_view EffectSummary(std::string_view a_recipeId) noexcept
	{
		const auto* row = FindRecipe(a_recipeId);
		return row ? row->effectSummary : std::string_view{};
	}

	// Fallback lookups for text boundaries.
	[[nodiscard]] constexpr std::string_view RecipeNameOr(
		std::string_view a_recipeId,
		std::string_view a_upstreamName) noexcept
	{
		const auto name = RecipeName(a_recipeId);
		return name.empty() ? a_upstreamName : name;
	}

	[[nodiscard]] constexpr std::string_view RuneNameOr(std::string_view a_internalRune) noexcept
	{
		const auto name = RuneName(a_internalRune);
		return name.empty() ? a_internalRune : name;
	}

	[[nodiscard]] constexpr std::string_view EffectSummaryOr(
		std::string_view a_recipeId,
		std::string_view a_upstreamSummary) noexcept
	{
		const auto summary = EffectSummary(a_recipeId);
		return summary.empty() ? a_upstreamSummary : summary;
	}

	// Maps a runtime (mechanical) rune list rune-by-rune. The order and any
	// duplicate positions come from the caller's list, so the visible sequence
	// can never diverge from what the transmute actually consumes.
	[[nodiscard]] inline std::string PresentRuneSequence(
		std::span<const std::string> a_internalRunes,
		std::string_view a_separator = Generated::kPublicSequenceSeparator)
	{
		std::string out;
		for (std::size_t i = 0; i < a_internalRunes.size(); ++i) {
			if (i > 0) {
				out.append(a_separator);
			}
			out.append(RuneNameOr(a_internalRunes[i]));
		}
		return out;
	}

	// Glyph keys for a runtime rune list; empty when any rune is unknown, since a
	// partial glyph row would misdescribe the recipe.
	[[nodiscard]] inline std::string PresentGlyphSequence(std::span<const std::string> a_internalRunes)
	{
		std::string out;
		for (std::size_t i = 0; i < a_internalRunes.size(); ++i) {
			const auto glyph = RuneGlyph(a_internalRunes[i]);
			if (glyph.empty()) {
				return {};
			}
			if (i > 0) {
				out.append(Generated::kGlyphSequenceSeparator);
			}
			out.append(glyph);
		}
		return out;
	}

	// "<item> [Runeword: <name>]" -- the completed-base label policy.
	[[nodiscard]] inline std::string CompletedRunewordItemLabel(
		std::string_view a_baseName,
		std::string_view a_runewordName)
	{
		std::string out(a_baseName);
		out.append(" [Runeword: ");
		out.append(a_runewordName);
		out.push_back(']');
		return out;
	}

	// Upstream result-affix names read "Runeword <name> (<runes>): <stats> - <effect label>".
	// The stats segment ("22% on hit / ICD 7s") is mechanical text and is kept; the
	// name, runes and effect label are legacy vocabulary and are not. Returns empty
	// when the upstream text does not have that shape (e.g. synthesized fallbacks).
	[[nodiscard]] constexpr std::string_view UpstreamResultAffixStats(std::string_view a_upstreamName) noexcept
	{
		constexpr std::string_view kPrefix = "Runeword ";
		constexpr std::string_view kHeadEnd = "): ";
		constexpr std::string_view kEffectDelimiter = " - ";
		if (a_upstreamName.substr(0, kPrefix.size()) != kPrefix) {
			return {};
		}
		const auto headEnd = a_upstreamName.find(kHeadEnd);
		if (headEnd == std::string_view::npos) {
			return {};
		}
		const auto statsBegin = headEnd + kHeadEnd.size();
		const auto statsEnd = a_upstreamName.find(kEffectDelimiter, statsBegin);
		if (statsEnd == std::string_view::npos || statsEnd == statsBegin) {
			return {};
		}
		return a_upstreamName.substr(statsBegin, statsEnd - statsBegin);
	}

	// English display name for a Runeword result affix (JSON-defined or synthesized):
	//   "Runeword <FE-ASR name> (<FE-ASR runes>): [<upstream stats> - ]<FE-ASR effect>"
	// Unknown recipes keep the upstream name unchanged. Idempotent: feeding the
	// output back in yields the same text.
	[[nodiscard]] inline std::string RunewordResultAffixNameEn(
		std::string_view a_recipeId,
		std::span<const std::string> a_internalRunes,
		std::string_view a_upstreamNameEn)
	{
		const auto* recipe = FindRecipe(a_recipeId);
		if (!recipe) {
			return std::string(a_upstreamNameEn);
		}

		std::string out = "Runeword ";
		out.append(recipe->publicName);
		const auto sequence = PresentRuneSequence(a_internalRunes);
		if (!sequence.empty()) {
			out.append(" (");
			out.append(sequence);
			out.push_back(')');
		}
		out.append(": ");
		if (const auto stats = UpstreamResultAffixStats(a_upstreamNameEn); !stats.empty()) {
			out.append(stats);
			out.append(" - ");
		}
		out.append(recipe->effectSummary);
		return out;
	}

	// Compact inventory-tooltip presentation for FE-ASR Runewords. The mechanical
	// stats remain sourced from the upstream result-affix name, but are separated
	// from identity/effect text so the inventory overlay does not become one long
	// technical sentence. Unknown recipes return empty and fall back to the normal
	// Calamity affix formatter.
	[[nodiscard]] inline std::string RunewordResultAffixTooltipEn(
		std::string_view a_recipeId,
		std::span<const std::string> a_internalRunes,
		std::string_view a_upstreamNameEn)
	{
		const auto* recipe = FindRecipe(a_recipeId);
		if (!recipe) {
			return {};
		}

		std::string out = "Runeword: ";
		out.append(recipe->publicName);
		const auto sequence = PresentRuneSequence(a_internalRunes);
		if (!sequence.empty()) {
			out.append("\nRunes: ");
			out.append(sequence);
		}

		if (const auto stats = UpstreamResultAffixStats(a_upstreamNameEn); !stats.empty()) {
			std::size_t begin = 0;
			while (begin < stats.size()) {
				const auto end = stats.find(" / ", begin);
				const auto part = stats.substr(begin, end == std::string_view::npos ? stats.size() - begin : end - begin);
				if (part.substr(0, 4) == "ICD ") {
					out.append("\nCooldown: ");
					out.append(part.substr(4));
				} else if (part.find('%') != std::string_view::npos) {
					out.append("\nTrigger: ");
					out.append(part);
				} else {
					out.append("\nMechanic: ");
					out.append(part);
				}
				if (end == std::string_view::npos) {
					break;
				}
				begin = end + 3;
			}
		}

		out.append("\nEffect: ");
		out.append(recipe->effectSummary);
		return out;
	}

	// Generated SPEL/MGEF record names read "Runeword <upstream recipe name>[ suffix]".
	// Native text that quotes them (recipe effect details, synthesized tooltip
	// summaries) replaces that exact phrase with the FE-ASR recipe name. Only the
	// whole phrase for this one recipe is replaced; other text is untouched. The
	// records themselves (and the vanilla Active Effects list) are plugin-owned.
	[[nodiscard]] inline std::string PresentRunewordRecordText(
		std::string_view a_text,
		std::string_view a_recipeId,
		std::string_view a_upstreamRecipeNameEn)
	{
		const auto publicName = RecipeName(a_recipeId);
		if (publicName.empty() || a_upstreamRecipeNameEn.empty()) {
			return std::string(a_text);
		}

		std::string legacyPhrase = "Runeword ";
		legacyPhrase.append(a_upstreamRecipeNameEn);
		auto isWordCharacter = [](char a_ch) {
			return (a_ch >= 'A' && a_ch <= 'Z') || (a_ch >= 'a' && a_ch <= 'z') || (a_ch >= '0' && a_ch <= '9') ||
				a_ch == '\'' || a_ch == '-';
		};

		std::string out;
		out.reserve(a_text.size());
		std::size_t cursor = 0;
		while (cursor < a_text.size()) {
			const auto hit = a_text.find(legacyPhrase, cursor);
			if (hit == std::string_view::npos) {
				break;
			}
			const auto end = hit + legacyPhrase.size();
			const bool startsWord = hit == 0 || !isWordCharacter(a_text[hit - 1]);
			const bool endsWord = end == a_text.size() || !isWordCharacter(a_text[end]);
			out.append(a_text.substr(cursor, hit - cursor));
			if (startsWord && endsWord) {
				out.append("Runeword ");
				out.append(publicName);
			} else {
				out.append(a_text.substr(hit, legacyPhrase.size()));
			}
			cursor = end;
		}
		out.append(a_text.substr(cursor));
		return out;
	}

	// Integrity of the generated table, evaluated at compile time by the plugin
	// (src/RuneforgePresentation.cpp) and the static test target.
	[[nodiscard]] constexpr bool AreRuneRowsSortedAndUnique() noexcept
	{
		for (std::size_t i = 1; i < Generated::kRunes.size(); ++i) {
			if (!(Generated::kRunes[i - 1].internalName < Generated::kRunes[i].internalName)) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] constexpr bool AreRecipeRowsSortedAndUnique() noexcept
	{
		for (std::size_t i = 1; i < Generated::kRecipes.size(); ++i) {
			if (!(Generated::kRecipes[i - 1].id < Generated::kRecipes[i].id)) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] constexpr bool ArePublicNamesPresentAndUnique() noexcept
	{
		for (std::size_t i = 0; i < Generated::kRunes.size(); ++i) {
			const auto& rune = Generated::kRunes[i];
			if (rune.publicName.empty() || rune.glyph.empty() || rune.index == 0u ||
				rune.index > Generated::kRuneCount || rune.publicName == rune.internalName) {
				return false;
			}
			for (std::size_t j = i + 1; j < Generated::kRunes.size(); ++j) {
				const auto& other = Generated::kRunes[j];
				if (rune.publicName == other.publicName || rune.glyph == other.glyph || rune.index == other.index) {
					return false;
				}
			}
		}
		for (std::size_t i = 0; i < Generated::kRecipes.size(); ++i) {
			const auto& recipe = Generated::kRecipes[i];
			if (recipe.publicName.empty() || recipe.effectSummary.empty()) {
				return false;
			}
			for (std::size_t j = i + 1; j < Generated::kRecipes.size(); ++j) {
				if (recipe.publicName == Generated::kRecipes[j].publicName) {
					return false;
				}
			}
		}
		return true;
	}

	// Every public rune sequence and glyph sequence is the internal sequence
	// mapped position by position: same length, same order, same duplicates.
	[[nodiscard]] constexpr bool ArePublicSequencesOrderPreserving() noexcept
	{
		for (const auto& recipe : Generated::kRecipes) {
			std::size_t internalCount = 0;
			const bool internalResolves = impl::ForEachToken(
				recipe.internalRuneSequence,
				Generated::kPublicSequenceSeparator,
				[&](std::string_view a_internal, std::size_t a_position) {
					const auto* rune = FindRune(a_internal);
					if (!rune) {
						return false;
					}
					++internalCount;
					std::string_view expectedPublic{};
					(void)impl::ForEachToken(
						recipe.publicRuneSequence,
						Generated::kPublicSequenceSeparator,
						[&](std::string_view a_public, std::size_t a_publicPosition) {
							if (a_publicPosition == a_position) {
								expectedPublic = a_public;
								return false;
							}
							return true;
						});
					std::string_view expectedGlyph{};
					(void)impl::ForEachToken(
						recipe.glyphSequence,
						Generated::kGlyphSequenceSeparator,
						[&](std::string_view a_glyph, std::size_t a_glyphPosition) {
							if (a_glyphPosition == a_position) {
								expectedGlyph = a_glyph;
								return false;
							}
							return true;
						});
					return expectedPublic == rune->publicName && expectedGlyph == rune->glyph;
				});
			if (!internalResolves || internalCount != recipe.runeCount) {
				return false;
			}

			std::size_t publicCount = 0;
			(void)impl::ForEachToken(
				recipe.publicRuneSequence,
				Generated::kPublicSequenceSeparator,
				[&](std::string_view, std::size_t) {
					++publicCount;
					return true;
				});
			std::size_t glyphCount = 0;
			(void)impl::ForEachToken(
				recipe.glyphSequence,
				Generated::kGlyphSequenceSeparator,
				[&](std::string_view, std::size_t) {
					++glyphCount;
					return true;
				});
			if (publicCount != recipe.runeCount || glyphCount != recipe.runeCount) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] constexpr bool IsGeneratedTableConsistent() noexcept
	{
		return Generated::kSchemaVersion == 1u &&
			Generated::kManifestSha256.size() == 64u &&
			AreRuneRowsSortedAndUnique() &&
			AreRecipeRowsSortedAndUnique() &&
			ArePublicNamesPresentAndUnique() &&
			ArePublicSequencesOrderPreserving();
	}
}
