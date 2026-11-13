#include "CalamityAffixes/EventBridge.h"

#include "CalamityAffixes/PlayerOwnership.h"
#include "CalamityAffixes/PrismaTooltip.h"
#include <string_view>

namespace CalamityAffixes
{
	RE::BSEventNotifyControl EventBridge::HandleModCallbackEvent(
		const SKSE::ModCallbackEvent* a_event,
		EventStateLock&,
		const EngineEventContext& a_context)
	{
		const auto now = a_context.observedAt;
		MaybeFlushRuntimeUserSettings(now, false);

		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const char* eventNameRaw = a_event->eventName.c_str();
		if (!eventNameRaw || !*eventNameRaw) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const std::string_view eventName(eventNameRaw);
		if (eventName.starts_with("FEASR_Runeforge_Reader_")) {
			const char* stringArgRaw = a_event->strArg.c_str();
			PrismaTooltip::HandleRuneforgeReaderEvent(
				eventName,
				stringArgRaw ? std::string_view(stringArgRaw) : std::string_view{},
				a_event->numArg);
			return RE::BSEventNotifyControl::kContinue;
		}
		if (eventName.starts_with("FEASR_Runeforge_")) {
			SKSE::log::info("RuneforgeDiag: EventBridge received event='{}' configLoaded={}", eventName, _configLoaded);
		}
		if (HandleUiModCallback(eventName, a_event->numArg)) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (!_configLoaded) {
			if (eventName.starts_with("FEASR_Runeforge_")) {
				SKSE::log::warn("RuneforgeDiag: event='{}' rejected reason=runtime_config_not_loaded; consumer_not_called", eventName);
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		const char* filterRaw = a_event->strArg.c_str();
		const std::string_view affixFilter = (filterRaw && *filterRaw) ? std::string_view(filterRaw) : std::string_view{};
		if (HandleFeasrRuneforgeModCallback(eventName, affixFilter, a_event->numArg)) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (HandleRuntimeSettingsModCallback(eventName, a_event->numArg, now)) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (HandleDebugUtilityModCallback(eventName, a_event->numArg)) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (!_runtimeSettings.enabled.load(std::memory_order_relaxed)) {
			return RE::BSEventNotifyControl::kContinue;
		}

		(void)HandleRuntimeGameplayModCallback(eventName, a_event->numArg, affixFilter);

		return RE::BSEventNotifyControl::kContinue;
	}

	bool EventBridge::IsPlayerOwned(RE::Actor* a_actor)
	{
		return IsPlayerOwnedActor(a_actor);
	}

	RE::Actor* EventBridge::GetPlayerOwner(RE::Actor* a_actor)
	{
		return ResolvePlayerOwnerActor(a_actor);
	}

	void EventBridge::SendModEvent(std::string_view a_eventName, RE::TESForm* a_sender)
	{
		auto* source = SKSE::GetModCallbackEventSource();
		if (!source) {
			return;
		}

		SKSE::ModCallbackEvent event{
			RE::BSFixedString(a_eventName.data()),
			RE::BSFixedString(""),
			0.0f,
			a_sender
		};

		source->SendEvent(&event);
	}

	void EventBridge::SendModEvent(std::string_view a_eventName, std::string_view a_strArg, float a_numArg, RE::TESForm* a_sender)
	{
		auto* source = SKSE::GetModCallbackEventSource();
		if (!source || a_eventName.empty()) {
			return;
		}

		const std::string eventName(a_eventName);
		const std::string strArg(a_strArg);
		SKSE::ModCallbackEvent event{
			RE::BSFixedString(eventName.c_str()),
			RE::BSFixedString(strArg.c_str()),
			a_numArg,
			a_sender
		};
		source->SendEvent(&event);
	}

	void EventBridge::MaybeResyncEquippedAffixes(std::chrono::steady_clock::time_point a_now)
	{
		if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed)) {
			return;
		}

		if (_equipResync.intervalMs == 0u) {
			return;
		}

		const auto nowMs =
			static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(a_now.time_since_epoch()).count());

		if (!_equipResync.ShouldRun(nowMs)) {
			return;
		}

		RebuildActiveCounts();
	}

}
