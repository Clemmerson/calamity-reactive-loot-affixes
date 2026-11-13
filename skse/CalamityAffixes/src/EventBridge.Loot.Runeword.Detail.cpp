#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/PluginEditorIds.h"
#include "CalamityAffixes/RunewordRuneWeights.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace CalamityAffixes::RunewordDetail
{
			std::string ToLowerAscii(std::string_view a_text)
		{
			std::string out(a_text);
			std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return out;
		}

				[[nodiscard]] bool IsWeaponOrArmor(const RE::TESBoundObject* a_object)
				{
					if (!a_object) {
						return false;
					}

					return a_object->As<RE::TESObjectWEAP>() != nullptr || a_object->As<RE::TESObjectARMO>() != nullptr;
				}

				[[nodiscard]] std::string ResolveInventoryDisplayName(
					const RE::InventoryEntryData* a_entry,
					RE::ExtraDataList* a_xList)
				{
					if (!a_entry || !a_entry->object) {
						return "<Unknown>";
					}

					auto usableName = [](std::string_view a_name) {
						if (a_name.empty()) {
							return false;
						}
						const auto folded = ToLowerAscii(a_name);
						return folded != "<missing name>" && folded != "<unknown>" && folded != "unknown" && folded != "unnamed";
					};

					const char* displayRaw = a_xList ? a_xList->GetDisplayName(a_entry->object) : nullptr;
					const std::string rawDisplayName = displayRaw ? displayRaw : "";
					const char* objectNameRaw = a_entry->object->GetName();
					const std::string objectName = objectNameRaw ? objectNameRaw : "";
					const std::string editorId{ PluginEditorIds::OwnEditorIdOf(a_entry->object) };

					std::string displayName = rawDisplayName;
					if (!usableName(displayName)) {
						displayName = objectName;
					}
					if (!usableName(displayName)) {
						const auto* object = a_entry->object;
						const std::string_view kind = object->As<RE::TESObjectWEAP>() ? "weapon" : "armor";
						if (usableName(editorId)) {
							displayName = "Unnamed " + std::string(kind) + " [EditorID: " + editorId + "]";
						} else {
							displayName = "Unnamed " + std::string(kind) + " [Form " +
								std::to_string(object->GetFormID()) + "]";
						}
					}

					const auto foldedName = ToLowerAscii(displayName);
					if (foldedName.find("unknown") != std::string::npos ||
						foldedName.find("missing name") != std::string::npos ||
						foldedName.find("unnamed") != std::string::npos) {
						SKSE::log::info(
							"RuneforgeDiag: suspicious item label formID={} rawDisplay='{}' objectName='{}' editorID='{}' resolved='{}'",
							a_entry->object->GetFormID(), rawDisplayName, objectName, editorId, displayName);
					}

					return displayName;
				}

				// Four broad rarity tiers preserve rune progression without an extreme tail.
				// Low/mid/high/top weights are capped at a 4:1 ratio.
				[[nodiscard]] double ResolveRunewordFragmentWeight(std::string_view a_runeName)
				{
					if (a_runeName.empty()) {
						return 1.0;
					}

					for (const auto& [name, weight] : kPinnedRunewordRuneWeights) {
						if (a_runeName == name) {
							return weight;
						}
					}

					return 1.0;
				}
					constexpr std::string_view kRunewordFragmentEditorIdPrefix = "CAFF_RuneFrag_";

				RE::TESObjectMISC* LookupRunewordFragmentItem(
					const std::unordered_map<std::uint64_t, std::string>& a_runeNameByToken,
					std::uint64_t a_runeToken)
				{
					if (a_runeToken == 0u) {
						return nullptr;
					}

					static std::unordered_map<std::uint64_t, RE::TESObjectMISC*> cache;
					if (const auto cacheIt = cache.find(a_runeToken); cacheIt != cache.end()) {
						return cacheIt->second;
					}

					const auto nameIt = a_runeNameByToken.find(a_runeToken);
					if (nameIt == a_runeNameByToken.end() || nameIt->second.empty()) {
						return nullptr;
					}

					std::string editorId;
					editorId.reserve(kRunewordFragmentEditorIdPrefix.size() + nameIt->second.size());
					editorId.append(kRunewordFragmentEditorIdPrefix);
					editorId.append(nameIt->second);

					auto* item = PluginEditorIds::Lookup<RE::TESObjectMISC>(editorId);
					if (item) {
						cache.emplace(a_runeToken, item);
					}
					return item;
				}

				std::uint32_t GetOwnedRunewordFragmentCount(
					RE::PlayerCharacter* a_player,
					const std::unordered_map<std::uint64_t, std::string>& a_runeNameByToken,
					std::uint64_t a_runeToken)
				{
					if (!a_player) {
						return 0u;
					}

					auto* item = LookupRunewordFragmentItem(a_runeNameByToken, a_runeToken);
					if (!item) {
						return 0u;
					}

					const auto owned = a_player->GetItemCount(item);
					return owned > 0 ? static_cast<std::uint32_t>(owned) : 0u;
				}

				std::uint32_t GrantRunewordFragments(
					RE::PlayerCharacter* a_player,
					const std::unordered_map<std::uint64_t, std::string>& a_runeNameByToken,
					std::uint64_t a_runeToken,
					std::uint32_t a_amount)
				{
					if (!a_player || a_amount == 0u) {
						return 0u;
					}

					auto* item = LookupRunewordFragmentItem(a_runeNameByToken, a_runeToken);
					if (!item) {
						return 0u;
					}

					const auto ownedBeforeSigned = std::max(0, a_player->GetItemCount(item));
					const auto ownedBefore = static_cast<std::uint32_t>(ownedBeforeSigned);

					const auto maxGive = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
					const auto give = (a_amount > maxGive) ? maxGive : a_amount;
					a_player->AddObjectToContainer(item, nullptr, static_cast<std::int32_t>(give), nullptr);

					const auto ownedAfterSigned = std::max(0, a_player->GetItemCount(item));
					const auto ownedAfter = static_cast<std::uint32_t>(ownedAfterSigned);
					return (ownedAfter > ownedBefore) ? (ownedAfter - ownedBefore) : 0u;
				}

				bool TryConsumeRunewordFragment(
					RE::PlayerCharacter* a_player,
					const std::unordered_map<std::uint64_t, std::string>& a_runeNameByToken,
					std::uint64_t a_runeToken)
				{
					if (!a_player) {
						return false;
					}

					auto* item = LookupRunewordFragmentItem(a_runeNameByToken, a_runeToken);
					if (!item) {
						return false;
					}

					if (a_player->GetItemCount(item) <= 0) {
						return false;
					}

					a_player->RemoveItem(item, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					return true;
				}
}
