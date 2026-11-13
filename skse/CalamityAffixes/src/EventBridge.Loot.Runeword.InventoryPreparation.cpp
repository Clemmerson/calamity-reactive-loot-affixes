#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/PluginEditorIds.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <cstdint>
#include <limits>
#include <memory>

namespace CalamityAffixes
{
	using namespace RunewordDetail;

	namespace
	{
		constexpr std::uint32_t kSkyrimUnarmedWeaponFormID = 0x000001F4u;

		RE::ExtraDataList* CreateRuntimeExtraDataList()
		{
			std::size_t runtimeSize = 0u;
			const auto version = REL::Module::get().version();
			if (version == REL::Version{ 1, 6, 1170, 0 }) {
				runtimeSize = 0x20u;
			} else if (version == REL::Version{ 1, 5, 97, 0 }) {
				runtimeSize = 0x18u;
			} else {
				SKSE::log::error(
					"RuneforgeDiag: refusing ExtraDataList materialization on unverified runtime {}",
					version.string("."));
				return nullptr;
			}

			auto* memory = RE::malloc(runtimeSize);
			if (!memory) {
				return nullptr;
			}
			auto* list = static_cast<RE::ExtraDataList*>(memory);
			using ctor_t = void(RE::ExtraDataList*);
			REL::Relocation<ctor_t> ctor{ RELOCATION_ID(11437, 11583) };
			ctor(list);
			return list;
		}

		constexpr std::int32_t kMaxIdentityMaterializationStack = 16;

		bool EnsureSingletonIdentity(
			RE::InventoryChanges* a_changes,
			RE::PlayerCharacter* a_player,
			RE::TESBoundObject* a_object,
			RE::ExtraDataList* a_xList,
			std::uint32_t& a_materialized)
		{
			if (!a_changes || !a_player || !a_object || !a_xList || a_xList->GetCount() != 1) {
				return false;
			}

			auto* uid = a_xList->GetByType<RE::ExtraUniqueID>();
			if (uid && uid->baseID != 0u && uid->uniqueID != 0u) {
				return true;
			}

			const auto nextUniqueID = a_changes->GetNextUniqueID();
			if (nextUniqueID == 0u) {
				return false;
			}
			if (!uid) {
				uid = RE::BSExtraData::Create<RE::ExtraUniqueID>();
				if (!uid) {
					return false;
				}
				a_xList->Add(uid);
			}
			uid->baseID = a_player->GetFormID();
			uid->uniqueID = nextUniqueID;
			++a_materialized;

			const auto editorId = PluginEditorIds::OwnEditorIdOf(a_object);
			SKSE::log::info(
				"RuneforgeDiag: workstation materialized singleton identity editorId='{}' form={:08X} owner={:08X} uniqueID={}",
				editorId,
				a_object->GetFormID(),
				uid->baseID,
				uid->uniqueID);
			return true;
		}
	}

	std::uint32_t EventBridge::PrepareRuneforgeSingletonInventoryIdentities()
	{
		const std::scoped_lock lock(_stateMutex);
		if (!_configLoaded) {
			return 0u;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* changes = player ? player->GetInventoryChanges() : nullptr;
		if (!player || !changes || !changes->entryList) {
			return 0u;
		}

		// GetInventory() provides the authoritative aggregate player-owned count for
		// each base object. Identity preparation may expose a small plain stack as
		// multiple singleton ExtraDataList instances, but never changes the aggregate
		// item count and refuses any existing multi-count ExtraDataList rather than
		// guessing how to clone item-specific metadata.
		const auto inventory = player->GetInventory([](RE::TESBoundObject& a_object) {
			return IsWeaponOrArmor(std::addressof(a_object));
		});

		std::uint32_t materialized = 0u;
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object ||
				entry->object->GetFormID() == kSkyrimUnarmedWeaponFormID ||
				!IsWeaponOrArmor(entry->object) ||
				!IsLootObjectEligibleForAffixes(entry->object)) {
				continue;
			}

			const auto inventoryIt = inventory.find(entry->object);
			if (inventoryIt == inventory.end()) {
				continue;
			}
			const auto aggregateCount = inventoryIt->second.first;
			if (aggregateCount <= 0 || aggregateCount > kMaxIdentityMaterializationStack) {
				continue;
			}

			std::int32_t representedCount = 0;
			bool unsafeExistingStack = false;
			if (entry->extraLists) {
				for (auto* xList : *entry->extraLists) {
					if (!xList) {
						continue;
					}
					if (xList->GetCount() != 1) {
						unsafeExistingStack = true;
						break;
					}
					++representedCount;
				}
			}
			if (unsafeExistingStack || representedCount > aggregateCount) {
				continue;
			}

			if (entry->extraLists) {
				for (auto* xList : *entry->extraLists) {
					if (xList && !EnsureSingletonIdentity(changes, player, entry->object, xList, materialized)) {
						unsafeExistingStack = true;
						break;
					}
				}
			}
			if (unsafeExistingStack) {
				continue;
			}

			for (auto implicitIndex = representedCount; implicitIndex < aggregateCount; ++implicitIndex) {
				auto* created = CreateRuntimeExtraDataList();
				if (!created || !EnsureSingletonIdentity(changes, player, entry->object, created, materialized)) {
					break;
				}
				entry->AddExtraList(created);
			}
		}

		return materialized;
	}
}
