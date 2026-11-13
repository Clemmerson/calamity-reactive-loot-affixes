#pragma once

// The Runeforge operations shared by the flat panel callbacks (RuneforgeMenu.cpp) and the workstation session
// (RuneforgeWorkstation.cpp). Each wraps the existing EventBridge call with the same race / contract / discovery gates,
// so the workstation adds no second path: Transmute is the one commit path (TransmuteRunewordBase ->
// ApplyRunewordResult, the single producer of the Core successful-use event).

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace RE
{
	class TESBoundObject;
}

namespace CalamityAffixes::RuneforgeMenu::Operations
{
	struct Outcome
	{
		bool ok{ false };
		std::string message{};
	};

	enum class Craft : std::uint8_t
	{
		kIdentify,
		kReforge,
		kScour
	};

	// The flat panel's snapshot (bases, recipes, panel state), English only.
	[[nodiscard]] nlohmann::json Snapshot();
	[[nodiscard]] Outcome SelectBase(std::uint64_t a_instanceKey);
	[[nodiscard]] Outcome SelectRecipe(std::uint64_t a_instanceKey, const std::string& a_recipeId);
	[[nodiscard]] Outcome Transmute(std::uint64_t a_instanceKey, const std::string& a_recipeId, std::uint32_t a_insertedRunes);
	[[nodiscard]] Outcome CraftAffixes(Craft a_action, std::uint64_t a_instanceKey, std::uint64_t a_token, const std::string& a_signature, std::uint32_t a_count);
	[[nodiscard]] Outcome ExpandAffix(std::uint64_t a_instanceKey, std::uint32_t a_count, const std::string& a_signature);
	[[nodiscard]] Outcome ExchangeCurrency(bool a_scouringOrb);
	// The base form of a unique inventory instance (never the instance itself; the real item never moves).
	[[nodiscard]] RE::TESBoundObject* ResolveBase(std::uint64_t a_instanceKey);
	// FE-ASR glyph keys of a recipe, formula order, ',' separated.
	[[nodiscard]] std::string GlyphSequence(const std::string& a_recipeId);
	// R8.7.4.21 I3: read-only RunewordEffectCard v1 JSON of the Runeword engraved on an instance ({} when none), with that
	// instance's effective proc chance. For the workstation's focused (not yet selected) item.
	[[nodiscard]] nlohmann::json ItemEffectCard(std::uint64_t a_instanceKey, bool a_worn);
	void SetDebugMode(bool a_debugMode);
}
