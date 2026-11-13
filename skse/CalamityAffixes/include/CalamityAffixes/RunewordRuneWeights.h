#pragma once

#include <array>
#include <string_view>
#include <utility>

namespace CalamityAffixes
{
	inline constexpr std::array<std::pair<std::string_view, double>, 33> kPinnedRunewordRuneWeights{ {
		{ "El", 4.0 }, { "Eld", 4.0 }, { "Tir", 4.0 }, { "Nef", 4.0 }, { "Eth", 4.0 },
		{ "Ith", 4.0 }, { "Tal", 4.0 }, { "Ral", 4.0 }, { "Ort", 4.0 }, { "Thul", 4.0 },
		{ "Amn", 4.0 }, { "Sol", 3.0 }, { "Shael", 3.0 }, { "Dol", 3.0 }, { "Hel", 3.0 },
		{ "Io", 3.0 }, { "Lum", 3.0 }, { "Ko", 3.0 }, { "Fal", 3.0 }, { "Lem", 3.0 },
		{ "Pul", 3.0 }, { "Um", 3.0 }, { "Mal", 2.0 }, { "Ist", 2.0 }, { "Gul", 2.0 },
		{ "Vex", 2.0 }, { "Ohm", 2.0 }, { "Lo", 2.0 }, { "Sur", 1.0 }, { "Ber", 1.0 },
		{ "Jah", 1.0 }, { "Cham", 1.0 }, { "Zod", 1.0 },
	} };
}
