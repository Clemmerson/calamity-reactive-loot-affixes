#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/DropReferenceIdentityPolicy.h"
#include "CalamityAffixes/LootCurrencySourceHelpers.h"
#include "CalamityAffixes/PluginEditorIds.h"

#include "CalamityAffixes/PointerSafety.h"

#include <chrono>


namespace CalamityAffixes
{
	static_assert(!RuntimePolicy::kAllowActivationCurrencyRoll);

	RE::BSEventNotifyControl EventBridge::HandleActivateEvent(
		const RE::TESActivateEvent* a_event,
		EventStateLock&,
		const EngineEventContext& a_context)
	{
		const auto now = a_context.observedAt;
		MaybeFlushRuntimeUserSettings(now, false);
		if (a_event) {
			auto* traceActionRef = SanitizeObjectPointer(a_event->actionRef.get());
			auto* traceActor = traceActionRef ? traceActionRef->As<RE::Actor>() : nullptr;
			auto* traceSourceRef = SanitizeObjectPointer(a_event->objectActivated.get());
			const auto* traceBaseObject = traceSourceRef ? traceSourceRef->GetBaseObject() : nullptr;
			if (traceActor && traceActor->IsPlayerRef() && traceSourceRef && traceBaseObject &&
				traceBaseObject->As<RE::TESObjectARMO>()) {
				SKSE::log::info(
					"[R84-XFER] handled activate seq={} refHandle={:016X} refForm={:08X} base={:08X}.",
					a_context.ingressSequence,
					static_cast<std::uint64_t>(traceSourceRef->GetHandle().native_handle()),
					traceSourceRef->GetFormID(),
					traceBaseObject->GetFormID());
			}
		}

		if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed) || !a_event || !a_event->actionRef || !a_event->objectActivated) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto* actionRef = SanitizeObjectPointer(a_event->actionRef.get());
		auto* sourceRef = SanitizeObjectPointer(a_event->objectActivated.get());
		auto* actionActor = actionRef ? actionRef->As<RE::Actor>() : nullptr;
		if (!actionActor || !actionActor->IsPlayerRef() || !sourceRef) {
			return RE::BSEventNotifyControl::kContinue;
		}

		// On Skyrim 1.6.1170 the player-drop container event can arrive before
		// its new dynamic reference is registered: its raw reference slot contains
		// the future FFxxxxxx FormID, but handle.get() and LookupByID both fail.
		// The subsequent player activation supplies the live concrete reference
		// before the world-to-player container event. Bind only when that live
		// FormID and base exactly match one serialized pending drop record.
		if (auto* baseObject = sourceRef->GetBaseObject(); baseObject) {
			const auto activationIdentity = ResolvePendingDropByActivatedReference(
				_lootState.pendingDropOwnership,
				actionActor->GetFormID(),
				baseObject->GetFormID(),
				sourceRef->GetFormID(),
				detail::GetInGameDayStamp());
			if (activationIdentity.ambiguous) {
				SKSE::log::warn(
					"CalamityAffixes: refused dropped-item activation identity because multiple pending owner keys match this exact reference (refForm={:08X}, baseObj={:08X}).",
					sourceRef->GetFormID(),
					baseObject->GetFormID());
			} else if (activationIdentity.instanceKey != 0u && HasMaterialInstanceState(activationIdentity.instanceKey)) {
				const auto sourceOwner = static_cast<RE::FormID>(activationIdentity.instanceKey >> 16u);
				const auto sourceUniqueID = static_cast<std::uint16_t>(activationIdentity.instanceKey & 0xFFFFu);
				auto* referenceUID = sourceRef->extraList.GetByType<RE::ExtraUniqueID>();
				const auto stampPlan = PlanDropReferenceIdentity(DropReferenceIdentityContext{
					.playerFormID = actionActor->GetFormID(),
					.sourceOwnerFormID = sourceOwner,
					.baseObjectFormID = baseObject->GetFormID(),
					.referenceExtraOwnerFormID = referenceUID ? referenceUID->baseID : 0u,
					.sourceKey = activationIdentity.instanceKey,
					.referenceHandle = static_cast<std::uint64_t>(sourceRef->GetHandle().native_handle()),
					.sourceUniqueID = sourceUniqueID,
					.referenceExtraUniqueID = referenceUID ? referenceUID->uniqueID : std::uint16_t{ 0u },
					.itemCount = 1
				});
				if (stampPlan == DropReferenceIdentityAction::kAddIdentity) {
					if (!referenceUID) {
						referenceUID = RE::BSExtraData::Create<RE::ExtraUniqueID>();
						if (referenceUID) {
							sourceRef->extraList.Add(referenceUID);
						}
					}
					if (referenceUID) {
						referenceUID->baseID = actionActor->GetFormID();
						referenceUID->uniqueID = sourceUniqueID;
					}
					const auto* verifiedUID = sourceRef->extraList.GetByType<RE::ExtraUniqueID>();
					if (verifiedUID && verifiedUID->baseID == actionActor->GetFormID() &&
						verifiedUID->uniqueID == sourceUniqueID) {
						const bool referenceBound = EstablishPendingDropReferenceIdentity(
							_lootState.pendingDropOwnership,
							activationIdentity.instanceKey,
							baseObject->GetFormID(),
							sourceRef->GetFormID(),
							detail::GetInGameDayStamp());
						SKSE::log::info(
							"CalamityAffixes: bound pending drop to activated world reference and verified exact identity marker (refForm={:08X}, refHandle={:08X}, baseObj={:08X}, sourceKey={:016X}, pendingBound={}).",
							sourceRef->GetFormID(),
							sourceRef->GetHandle().native_handle(),
							baseObject->GetFormID(),
							activationIdentity.instanceKey,
							referenceBound);
					} else {
						SKSE::log::error(
							"CalamityAffixes: exact activated dropped reference could not retain its owner+UID marker (refForm={:08X}, baseObj={:08X}, sourceKey={:016X}).",
							sourceRef->GetFormID(),
							baseObject->GetFormID(),
							activationIdentity.instanceKey);
					}
				} else if (stampPlan == DropReferenceIdentityAction::kAlreadyExact) {
					const bool referenceBound = EstablishPendingDropReferenceIdentity(
						_lootState.pendingDropOwnership,
						activationIdentity.instanceKey,
						baseObject->GetFormID(),
						sourceRef->GetFormID(),
						detail::GetInGameDayStamp());
					SKSE::log::info(
						"CalamityAffixes: activated dropped reference already carries its exact owner+UID marker (refForm={:08X}, sourceKey={:016X}, pendingBound={}).",
						sourceRef->GetFormID(),
						activationIdentity.instanceKey,
						referenceBound);
				} else {
					SKSE::log::warn(
						"CalamityAffixes: refused activation-time dropped-reference stamp (refForm={:08X}, baseObj={:08X}, sourceKey={:016X}, plan={}).",
						sourceRef->GetFormID(),
						baseObject->GetFormID(),
						activationIdentity.instanceKey,
						static_cast<unsigned>(stampPlan));
				}
			}
		}

		if (_runtimeSettings.combatDebugLog) {
			auto* baseObject = sourceRef->GetBaseObject();
			auto* combatController = actionActor->GetActorRuntimeData().combatController;
			const auto runtimeTargetHandle = actionActor->GetActorRuntimeData().currentCombatTarget.native_handle();
			const auto controllerTargetHandle = combatController ? combatController->targetHandle.native_handle() : 0u;
			const bool murderAlarmBit = actionActor->GetActorRuntimeData().boolBits.any(RE::Actor::BOOL_BITS::kMurderAlarm);
			const bool controllerStarted = combatController ? combatController->startedCombat : false;
			const bool controllerStopped = combatController ? combatController->stoppedCombat : false;
			const std::string baseEditorId = PluginEditorIds::EditorIdOf(baseObject);
			SKSE::log::info(
				"CalamityAffixes: activation probe (targetRef={}({:08X}), base={}({:08X}), baseEditorId={}, inCombat={}, runtimeTarget={:08X}, controllerTarget={:08X}, murderAlarm={}, ctrlStarted={}, ctrlStopped={}).",
				sourceRef->GetName(),
				sourceRef->GetFormID(),
				baseObject ? baseObject->GetName() : "<none>",
				baseObject ? baseObject->GetFormID() : 0u,
				baseEditorId,
				actionActor->IsInCombat(),
				runtimeTargetHandle,
				controllerTargetHandle,
				murderAlarmBit,
				controllerStarted,
				controllerStopped);
		}

		return RE::BSEventNotifyControl::kContinue;
	}

}
