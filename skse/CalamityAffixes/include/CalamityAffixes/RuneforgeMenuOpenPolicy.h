#pragma once

#include <cstdint>

#include "CalamityAffixes/RuneforgeMenuRacePolicy.h"

namespace CalamityAffixes::RuneforgeMenu::OpenPolicy
{
	enum class Refusal : std::uint8_t
	{
		kNone,
		kUiUnavailable,
		kAlreadyOpen,
		kBlockingMenu,
		kPlayerUnavailable,
		kCombat,
		kMounted,
		kRaceUnavailable,
		kForbiddenForm,
		kRaceOverride,
		kRuntimeUnavailable,
		kDiscoveryUnavailable
	};

	struct Context
	{
		bool uiAvailable{ false };
		bool runeforgeAlreadyOpen{ false };
		bool blockingMenuOpen{ false };
		bool playerAvailable{ false };
		bool inCombat{ false };
		bool mounted{ false };
		RacePolicy::Context race{};
		bool runtimeReady{ false };
		bool discoveryReady{ false };
		bool debugMode{ false };
	};

	[[nodiscard]] constexpr Refusal Resolve(const Context& a_context) noexcept
	{
		if (!a_context.uiAvailable) {
			return Refusal::kUiUnavailable;
		}
		if (a_context.runeforgeAlreadyOpen) {
			return Refusal::kAlreadyOpen;
		}
		if (a_context.blockingMenuOpen) {
			return Refusal::kBlockingMenu;
		}
		if (!a_context.playerAvailable) {
			return Refusal::kPlayerUnavailable;
		}
		if (a_context.inCombat) {
			return Refusal::kCombat;
		}
		if (a_context.mounted) {
			return Refusal::kMounted;
		}
		switch (RacePolicy::Resolve(a_context.race)) {
		case RacePolicy::Refusal::kRaceUnavailable:
			return Refusal::kRaceUnavailable;
		case RacePolicy::Refusal::kForbiddenForm:
			return Refusal::kForbiddenForm;
		case RacePolicy::Refusal::kRaceOverride:
			return Refusal::kRaceOverride;
		case RacePolicy::Refusal::kNone:
			break;
		}
		if (!a_context.debugMode && !a_context.runtimeReady) {
			return Refusal::kRuntimeUnavailable;
		}
		if (!a_context.debugMode && !a_context.discoveryReady) {
			return Refusal::kDiscoveryUnavailable;
		}
		return Refusal::kNone;
	}
}
