#pragma once

#include <string>
#include <vector>

namespace CalamityAffixes::RuneforgeWorkstation::CompletionPolicy
{
	struct Summary
	{
		std::string recipeName{};
		std::string itemName{};
		std::string effect{};
		std::string runePattern{};
		// R8.7.4.21 I3: the engraved formula's effect card (RunewordEffectCardPolicy::PanelBody, Panel::kResult), captured
		// before the post-commit snapshot refresh. Presentation only.
		std::vector<std::string> effectLines{};
	};

	[[nodiscard]] inline std::vector<std::string> BuildLines(const Summary& a_summary)
	{
		std::vector<std::string> lines;
		if (!a_summary.recipeName.empty()) lines.push_back(a_summary.recipeName);
		if (!a_summary.itemName.empty()) lines.push_back("Engraved item: " + a_summary.itemName);
		if (!a_summary.effect.empty()) lines.push_back("Effect: " + a_summary.effect);
		if (!a_summary.runePattern.empty()) lines.push_back("Rune pattern: " + a_summary.runePattern);
		for (const auto& line : a_summary.effectLines) lines.push_back(line);
		if (lines.empty()) lines.push_back("Engraving complete.");
		return lines;
	}
}
