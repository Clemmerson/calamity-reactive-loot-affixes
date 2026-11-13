#pragma once

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace CalamityAffixes::RuneforgeMenu::CallbackPolicy
{
	enum class ArgumentType
	{
		kOther,
		kString,
		kNumber
	};

	struct Argument
	{
		ArgumentType type{ ArgumentType::kOther };
		const char* stringValue{ nullptr };
		double numberValue{ 0.0 };

		[[nodiscard]] static constexpr Argument String(const char* a_value) noexcept
		{
			return { ArgumentType::kString, a_value, 0.0 };
		}

		[[nodiscard]] static constexpr Argument Number(double a_value) noexcept
		{
			return { ArgumentType::kNumber, nullptr, a_value };
		}

		[[nodiscard]] static constexpr Argument Other() noexcept
		{
			return {};
		}
	};

	inline constexpr std::size_t kMaxStringLength = 128;

	[[nodiscard]] inline std::optional<std::string_view> BoundedString(const Argument& a_argument) noexcept
	{
		if (a_argument.type != ArgumentType::kString || !a_argument.stringValue) {
			return std::nullopt;
		}

		for (std::size_t length = 0; length <= kMaxStringLength; ++length) {
			if (a_argument.stringValue[length] == '\0') {
				if (length == 0) {
					return std::nullopt;
				}
				return std::string_view(a_argument.stringValue, length);
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] inline bool ParseUint64(const Argument& a_argument, std::uint64_t& a_output) noexcept
	{
		if (a_argument.type != ArgumentType::kString || !a_argument.stringValue) {
			return false;
		}

		// uint64 decimal text can contain at most 20 digits. Bound the scan so
		// malformed Scaleform strings cannot trigger an unbounded length walk.
		std::size_t length = 0;
		for (; length <= 20; ++length) {
			if (a_argument.stringValue[length] == '\0') {
				break;
			}
		}
		if (length == 0 || length > 20) {
			return false;
		}

		std::uint64_t parsed = 0;
		const char* const begin = a_argument.stringValue;
		const char* const end = begin + length;
		const auto [parsedEnd, error] = std::from_chars(begin, end, parsed, 10);
		if (error != std::errc{} || parsedEnd != end) {
			return false;
		}
		a_output = parsed;
		return true;
	}

	[[nodiscard]] inline bool ParseBoundedString(const Argument& a_argument, std::string& a_output)
	{
		const auto value = BoundedString(a_argument);
		if (!value) {
			return false;
		}
		a_output.assign(value->data(), value->size());
		return true;
	}

	[[nodiscard]] inline bool ParseUnsignedAtMost(
		const Argument& a_argument,
		std::uint64_t a_maximum,
		std::uint64_t& a_output) noexcept
	{
		std::uint64_t parsed = 0;
		if (!ParseUint64(a_argument, parsed) || parsed > a_maximum) {
			return false;
		}
		a_output = parsed;
		return true;
	}

	[[nodiscard]] inline bool IsValidInstanceKey(std::uint64_t a_key) noexcept
	{
		return a_key != 0;
	}

	enum class AffixAction
	{
		kIdentify,
		kReforge,
		kScour
	};

	enum class CurrencyExchangeAction
	{
		kReforgeOrb,
		kScouringOrb
	};

	[[nodiscard]] inline std::optional<AffixAction> ParseAffixAction(const Argument& a_argument)
	{
		const auto value = BoundedString(a_argument);
		if (!value) return std::nullopt;
		if (*value == "identify") return AffixAction::kIdentify;
		if (*value == "reforge") return AffixAction::kReforge;
		if (*value == "scour") return AffixAction::kScour;
		return std::nullopt;
	}

	[[nodiscard]] inline std::optional<CurrencyExchangeAction> ParseCurrencyExchange(const Argument& a_argument)
	{
		const auto value = BoundedString(a_argument);
		if (!value) return std::nullopt;
		if (*value == "reforge") return CurrencyExchangeAction::kReforgeOrb;
		if (*value == "scouring") return CurrencyExchangeAction::kScouringOrb;
		return std::nullopt;
	}

	struct SelectBaseRequest
	{
		std::uint64_t instanceKey{ 0 };
	};

	struct SelectRecipeRequest
	{
		std::uint64_t expectedInstanceKey{ 0 };
		std::string recipeId;
	};

	struct TransmuteRequest
	{
		std::uint64_t expectedInstanceKey{ 0 };
		std::string recipeId;
		std::uint32_t insertedRunes{ 0 };
	};

	struct CraftAffixesRequest
	{
		std::uint64_t expectedInstanceKey{ 0 };
		AffixAction action{ AffixAction::kIdentify };
		std::uint64_t selectedToken{ 0 };
		std::string expectedAffixSignature;
		std::uint32_t expectedAffixCount{ 0 };
	};

	struct ExpandAffixRequest
	{
		std::uint64_t expectedInstanceKey{ 0 };
		std::uint8_t expectedAffixCount{ 0 };
		std::string expectedAffixSignature;
	};

	struct ExchangeCurrencyRequest
	{
		CurrencyExchangeAction exchange{ CurrencyExchangeAction::kReforgeOrb };
	};

	[[nodiscard]] inline bool ParseSelectBase(std::span<const Argument> a_args, SelectBaseRequest& a_output)
	{
		std::uint64_t key = 0;
		if (a_args.size() != 1 || !ParseUint64(a_args[0], key) || !IsValidInstanceKey(key)) return false;
		a_output.instanceKey = key;
		return true;
	}

	[[nodiscard]] inline bool ParseSelectRecipe(std::span<const Argument> a_args, SelectRecipeRequest& a_output)
	{
		SelectRecipeRequest parsed;
		if (a_args.size() != 2 || !ParseUint64(a_args[0], parsed.expectedInstanceKey) ||
			!IsValidInstanceKey(parsed.expectedInstanceKey) || !ParseBoundedString(a_args[1], parsed.recipeId)) return false;
		a_output = std::move(parsed);
		return true;
	}

	[[nodiscard]] inline bool ParseTransmute(std::span<const Argument> a_args, TransmuteRequest& a_output)
	{
		TransmuteRequest parsed;
		std::uint64_t insertedRunes = 0;
		if (a_args.size() != 3 || !ParseUint64(a_args[0], parsed.expectedInstanceKey) ||
			!IsValidInstanceKey(parsed.expectedInstanceKey) || !ParseBoundedString(a_args[1], parsed.recipeId) ||
			!ParseUnsignedAtMost(a_args[2], UINT32_MAX, insertedRunes)) return false;
		parsed.insertedRunes = static_cast<std::uint32_t>(insertedRunes);
		a_output = std::move(parsed);
		return true;
	}

	[[nodiscard]] inline bool ParseCraftAffixes(std::span<const Argument> a_args, CraftAffixesRequest& a_output)
	{
		CraftAffixesRequest parsed;
		std::uint64_t expectedAffixCount = 0;
		const auto action = a_args.size() == 5 ? ParseAffixAction(a_args[1]) : std::nullopt;
		if (a_args.size() != 5 || !ParseUint64(a_args[0], parsed.expectedInstanceKey) ||
			!IsValidInstanceKey(parsed.expectedInstanceKey) || !action ||
			!ParseUint64(a_args[2], parsed.selectedToken) ||
			!ParseBoundedString(a_args[3], parsed.expectedAffixSignature) ||
			!ParseUnsignedAtMost(a_args[4], UINT32_MAX, expectedAffixCount)) return false;
		parsed.action = *action;
		parsed.expectedAffixCount = static_cast<std::uint32_t>(expectedAffixCount);
		a_output = std::move(parsed);
		return true;
	}

	[[nodiscard]] inline bool ParseExpandAffix(std::span<const Argument> a_args, ExpandAffixRequest& a_output)
	{
		ExpandAffixRequest parsed;
		std::uint64_t expectedAffixCount = 0;
		if (a_args.size() != 3 || !ParseUint64(a_args[0], parsed.expectedInstanceKey) ||
			!IsValidInstanceKey(parsed.expectedInstanceKey) ||
			!ParseUnsignedAtMost(a_args[1], UINT8_MAX, expectedAffixCount) ||
			!ParseBoundedString(a_args[2], parsed.expectedAffixSignature)) return false;
		parsed.expectedAffixCount = static_cast<std::uint8_t>(expectedAffixCount);
		a_output = std::move(parsed);
		return true;
	}

	[[nodiscard]] inline bool ParseExchangeCurrency(std::span<const Argument> a_args, ExchangeCurrencyRequest& a_output)
	{
		if (a_args.size() != 1) return false;
		const auto exchange = ParseCurrencyExchange(a_args[0]);
		if (!exchange) return false;
		a_output.exchange = *exchange;
		return true;
	}

	[[nodiscard]] inline bool ParseControlCount(std::span<const Argument> a_args, std::uint32_t& a_output) noexcept
	{
		if (a_args.size() != 1 || a_args[0].type != ArgumentType::kNumber) return false;
		const double value = a_args[0].numberValue;
		if (!std::isfinite(value) || value < 0.0 || value > 256.0 || std::floor(value) != value) return false;
		a_output = static_cast<std::uint32_t>(value);
		return true;
	}
}
