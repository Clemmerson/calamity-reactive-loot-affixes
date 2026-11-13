#include "CalamityAffixes/Papyrus.h"
#include "CalamityAffixes/RuneforgeWorkstation.h"

#include <string>
#include <string_view>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace CalamityAffixes::Papyrus
{
	namespace
	{
		std::string ClaimRuneforgeActivationIntent(RE::StaticFunctionTag*)
		{
			const auto sequence = RuneforgeWorkstation::ClaimActivationIntent();
			SKSE::log::info("RuneforgeDiag: Papyrus activation claim sequence={} accepted={}", sequence, sequence != 0);
			return sequence != 0 ? std::to_string(sequence) : std::string{};
		}

		void SendModCallbackEvent(RE::StaticFunctionTag*, std::string a_eventName, std::string a_strArg, float a_numArg)
		{
			if (a_eventName.empty()) {
				return;
			}
			const bool isRuneforgeEvent = std::string_view(a_eventName).starts_with("FEASR_Runeforge_");
			if (isRuneforgeEvent) {
				SKSE::log::info(
					"RuneforgeDiag: Papyrus emitter event='{}' str='{}' num={}",
					a_eventName,
					a_strArg,
					a_numArg);
			}

			auto* source = SKSE::GetModCallbackEventSource();
			if (!source) {
				if (isRuneforgeEvent) {
					SKSE::log::error("RuneforgeDiag: Papyrus mod callback event source is null");
				}
				return;
			}

			const std::string eventName = std::move(a_eventName);
			const std::string strArg = std::move(a_strArg);

			SKSE::ModCallbackEvent event{
				RE::BSFixedString(eventName.c_str()),
				RE::BSFixedString(strArg.c_str()),
				a_numArg,
				nullptr
			};
			source->SendEvent(&event);
			if (isRuneforgeEvent) {
				SKSE::log::info("RuneforgeDiag: Papyrus emitter sent event='{}' str='{}' num={}",
					a_eventName,
					a_strArg,
					a_numArg);
			}
		}
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			return false;
		}

		a_vm->RegisterFunction("Send", "CalamityAffixes_ModEventEmitter", SendModCallbackEvent);
		a_vm->RegisterFunction("ClaimRuneforgeActivationIntent", "CalamityAffixes_ModEventEmitter", ClaimRuneforgeActivationIntent);
		return true;
	}
}

