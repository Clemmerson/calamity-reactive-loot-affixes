#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include <RE/Skyrim.h>
#include <SKSE/Events.h>
#include <SKSE/SKSE.h>

#include "CalamityAffixes/ItemSubtypePolicy.h"
#include "CalamityAffixes/DropPickupInventoryIdentityPolicy.h"
#include "CalamityAffixes/AffixCraftingPolicy.h"
#include "CalamityAffixes/TrapCellPolicy.h"
#include "CalamityAffixes/DeferredEventDispatcher.h"

	#include "CalamityAffixes/AdaptiveElement.h"
	#include "CalamityAffixes/AffixSpecialActionState.h"
	#include "CalamityAffixes/AffixRegistryState.h"
	#include "CalamityAffixes/AffixToken.h"
	#include "CalamityAffixes/CombatRuntimeState.h"
	#include "CalamityAffixes/CorpseCurrencyPolicy.h"
	#include "CalamityAffixes/InstanceAffixSlots.h"
	#include "CalamityAffixes/InstanceStateKey.h"
	#include "CalamityAffixes/LootCurrencyLedger.h"
	#include "CalamityAffixes/LootRuntimeState.h"
	#include "CalamityAffixes/MagnitudeScaling.h"
	#include "CalamityAffixes/RuntimeUserSettingsDebounce.h"
#include "CalamityAffixes/EventNames.h"
#include "CalamityAffixes/RuneforgePresentation.h"
#include "CalamityAffixes/RunewordUiContracts.h"
#include "CalamityAffixes/RuntimePolicy.h"
#include "CalamityAffixes/ResyncScheduler.h"

namespace CalamityAffixes
{
	namespace RunewordSynthesis
	{
		struct SpellSet;
	}

	class EventBridge final :
		public RE::BSTEventSink<RE::TESHitEvent>,
		public RE::BSTEventSink<RE::TESDeathEvent>,
		public RE::BSTEventSink<RE::TESEquipEvent>,
		public RE::BSTEventSink<RE::TESActivateEvent>,
		public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>,
		public RE::BSTEventSink<RE::TESContainerChangedEvent>,
		public RE::BSTEventSink<RE::TESUniqueIDChangeEvent>,
		public RE::BSTEventSink<SKSE::ModCallbackEvent>
	{
	public:
		using OperationResult = ::CalamityAffixes::OperationResult;
		using RunewordBaseInventoryEntry = ::CalamityAffixes::RunewordBaseInventoryEntry;
		using RuneforgeInventoryEntry = ::CalamityAffixes::RuneforgeInventoryEntry;
		using RuneforgeInventorySnapshot = ::CalamityAffixes::RuneforgeInventorySnapshot;
		using RunewordPanelState = ::CalamityAffixes::RunewordPanelState;
		using RunewordRecipeEntry = ::CalamityAffixes::RunewordRecipeEntry;
		using RunewordRuneRequirement = ::CalamityAffixes::RunewordRuneRequirement;

		// Public result/value DTOs.
		struct ConversionResult
		{
			RE::SpellItem* spell{ nullptr };
			float convertedDamage{ 0.0f };
			float effectiveness{ 1.0f };
			bool noHitEffectArt{ false };
		};

		static constexpr std::size_t kMaxConversionsPerHit = 3;
		struct ConversionResults
		{
			std::array<ConversionResult, kMaxConversionsPerHit> entries{};
			std::size_t count{ 0 };
		};

		struct CastOnCritResult
		{
			RE::SpellItem* spell{ nullptr };
			float effectiveness{ 1.0f };
			float magnitudeOverride{ 0.0f };
			bool noHitEffectArt{ false };
		};

		static constexpr std::size_t kMaxCastOnCritPerHit = 2;
		struct CastOnCritResults
		{
			std::array<CastOnCritResult, kMaxCastOnCritPerHit> entries{};
			std::size_t count{ 0 };
		};

		struct MindOverMatterResult
		{
			float redirectedDamage{ 0.0f };
			float consumedMagicka{ 0.0f };
			float redirectPct{ 0.0f };
		};

		#define CALAMITYAFFIXES_EVENTBRIDGE_PUBLIC_API_INL_CONTEXT 1
		#include "detail/EventBridge.PublicApi.inl"
		#undef CALAMITYAFFIXES_EVENTBRIDGE_PUBLIC_API_INL_CONTEXT

	private:
		EventBridge() = default;

		// Internal domain types (enums, value-type structs).
		#define CALAMITYAFFIXES_EVENTBRIDGE_TYPES_INL_CONTEXT 1
		#include "detail/EventBridge.Types.inl"
		#undef CALAMITYAFFIXES_EVENTBRIDGE_TYPES_INL_CONTEXT

		// Shared wrappers (implemented via dedicated helper headers).
		static bool IsPlayerOwned(RE::Actor* a_actor);
		static RE::Actor* GetPlayerOwner(RE::Actor* a_actor);
		static void SendModEvent(std::string_view a_eventName, RE::TESForm* a_sender);
		static void SendModEvent(std::string_view a_eventName, std::string_view a_strArg, float a_numArg, RE::TESForm* a_sender);

		// Config/event string constants, policy constants, serialization constants.
		#define CALAMITYAFFIXES_EVENTBRIDGE_CONSTANTS_INL_CONTEXT 1
		#include "detail/EventBridge.Constants.inl"
		#undef CALAMITYAFFIXES_EVENTBRIDGE_CONSTANTS_INL_CONTEXT

		#define CALAMITYAFFIXES_EVENTBRIDGE_STATE_GROUPS_INL_CONTEXT 1
		#include "detail/EventBridge.StateGroups.inl"
		#undef CALAMITYAFFIXES_EVENTBRIDGE_STATE_GROUPS_INL_CONTEXT

		// Runtime state storage.
		CombatRuntimeState _combatState{};

		AffixRuntimeCacheState _affixRuntimeState{};

		// Special action caches — affixes with non-standard action types.
		AffixSpecialActionState _affixSpecialActions{};

		LootRuntimeState _lootState{};
		InstanceTrackingState _instanceTrackingState{};
		RunewordRuntimeState _runewordState{};
		std::uint64_t _runeforgeInventorySnapshotRevision{ 0u };
		TrapRuntimeState _trapState{};
		LootConfig _loot{};
		RuntimeSettingsState _runtimeSettings{};
		bool _miscCurrencyMigrated{ false };
		bool _miscCurrencyRecovered{ false };
		// Non-persistent enforcement cache. FE ASR's quest owns saved discovery
		// and repopulates this mirror after load through the versioned event API.
		std::unordered_set<std::string> _feasrKnownRunewordRecipeIds{};
		std::string _feasrDiscoveryRevision{};
		std::uint32_t _feasrDiscoveryExpectedCount{ 0u };
		std::uint32_t _feasrDiscoveryReceivedCount{ 0u };
		bool _feasrDiscoverySyncActive{ false };
		bool _feasrDiscoveryReady{ false };
		bool _feasrDiscoverySyncValid{ false };
		bool _feasrRuneforgeContractCompatible{ false };
		bool _configLoaded{ false };
		std::atomic_bool _eventSinksRegistered{ false };
		static std::mt19937::result_type MakeRngSeed() noexcept
		{
			auto rd = std::random_device{}();
			auto ts = static_cast<std::mt19937::result_type>(
				std::chrono::steady_clock::now().time_since_epoch().count());
			// Mix entropy sources to defend against poor random_device implementations.
			rd ^= ts;
			rd ^= static_cast<std::mt19937::result_type>(reinterpret_cast<std::uintptr_t>(&rd) >> 3);
			return rd;
		}
		std::mt19937 _rng{ MakeRngSeed() };
		mutable std::mutex _rngMutex;
		// Single coarse-grained lock by design: cross-domain interactions
		// (trigger→loot, loot→serialization, etc.) make fine-grained splitting
		// deadlock-prone without measurable contention benefit in a game mod.
		mutable std::recursive_mutex _stateMutex;
		// BSTEventSource invokes sinks under its engine lock. Sink admission must
		// never wait for _stateMutex; contended events own their input until a
		// game task can acquire state outside the engine callback. One inbox also
		// preserves container/unique-ID ordering across event types.
		detail::DeferredEventDispatcher _eventDispatcher;
		using EventStateLock = detail::DeferredEventDispatcher::StateLock;
		struct EngineEventContext
		{
			std::chrono::steady_clock::time_point observedAt{};
			std::uint64_t ingressSequence{ 0u };
			bool procOrigin{ false };
			std::optional<RE::HitData> hitData;
		};

		ResyncScheduler _equipResync{ .nextAtMs = 0, .intervalMs = static_cast<std::uint64_t>(kEquipResyncInterval.count()) };

		#define CALAMITYAFFIXES_EVENTBRIDGE_PRIVATE_API_INL_CONTEXT 1
		#include "detail/EventBridge.PrivateApi.inl"
		#undef CALAMITYAFFIXES_EVENTBRIDGE_PRIVATE_API_INL_CONTEXT
	};
}
