#pragma once

namespace CalamityAffixes::RuneforgeMenu::RacePolicy
{
	enum class Refusal
	{
		kNone,
		kRaceUnavailable,
		kForbiddenForm,
		kRaceOverride
	};

	struct Context
	{
		bool currentRaceResolved{ false };
		bool baseRaceResolved{ false };
		bool racesMatch{ false };
		bool forbiddenForm{ false };
	};

	[[nodiscard]] constexpr Refusal Resolve(const Context& a_context) noexcept
	{
		if (!a_context.currentRaceResolved || !a_context.baseRaceResolved) {
			return Refusal::kRaceUnavailable;
		}
		if (a_context.forbiddenForm) {
			return Refusal::kForbiddenForm;
		}
		return a_context.racesMatch ? Refusal::kNone : Refusal::kRaceOverride;
	}
}
