#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace CalamityAffixes::RunewordEffectDetailPolicy
{
	struct SpellEffect
	{
		std::string name{};
		float magnitude{ 0.0f };
		std::uint32_t durationSeconds{ 0u };
		std::uint32_t area{ 0u };
	};

	[[nodiscard]] inline std::string FormatNumber(float a_value)
	{
		if (!std::isfinite(a_value)) {
			return {};
		}
		std::array<char, 48> buffer{};
		const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), a_value,
			std::chars_format::fixed, 1);
		if (result.ec != std::errc{}) {
			return {};
		}
		std::string out(buffer.data(), result.ptr);
		if (out.size() >= 2u && out.compare(out.size() - 2u, 2u, ".0") == 0) {
			out.erase(out.size() - 2u);
		}
		return out;
	}

	[[nodiscard]] inline std::string FormatSpellEffect(
		const SpellEffect& a_effect,
		bool a_korean = false)
	{
		if (a_effect.name.empty()) {
			return {};
		}

		std::string out = a_effect.name;
		std::string metrics;
		auto appendMetric = [&](std::string_view a_label, std::string a_value) {
			if (a_value.empty()) {
				return;
			}
			if (!metrics.empty()) {
				metrics.append(a_korean ? ", " : "; ");
			}
			metrics.append(a_label);
			metrics.push_back(' ');
			metrics.append(a_value);
		};

		if (std::abs(a_effect.magnitude) >= 0.01f) {
			appendMetric(a_korean ? "위력" : "Magnitude", FormatNumber(a_effect.magnitude));
		}
		if (a_effect.durationSeconds > 0u) {
			appendMetric(a_korean ? "지속" : "Duration",
				std::to_string(a_effect.durationSeconds) + (a_korean ? "초" : "s"));
		}
		if (a_effect.area > 0u) {
			appendMetric(a_korean ? "범위" : "Area", std::to_string(a_effect.area));
		}
		if (!metrics.empty()) {
			out.append(" (" + metrics + ")");
		}
		return out;
	}

	[[nodiscard]] inline std::string FormatSpellEffects(
		std::span<const SpellEffect> a_effects,
		std::size_t a_totalEffectCount,
		bool a_korean = false)
	{
		std::string out;
		for (const auto& effect : a_effects) {
			auto line = FormatSpellEffect(effect, a_korean);
			if (line.empty()) {
				continue;
			}
			if (!out.empty()) {
				out.push_back('\n');
			}
			out.append(line);
		}
		if (a_totalEffectCount > a_effects.size()) {
			if (!out.empty()) {
				out.push_back('\n');
			}
			if (a_korean) {
				out.append("외 ");
			} else {
				out.append("plus ");
			}
			out.append(std::to_string(a_totalEffectCount - a_effects.size()));
			out.append(a_korean ? "개 효과" : " more effect(s)");
		}
		return out;
	}
}
