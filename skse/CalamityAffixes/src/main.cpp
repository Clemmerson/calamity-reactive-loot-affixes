#include <cstdint>
#include <cstdio>
#include <filesystem>

#include <RE/Skyrim.h>
#include <SKSE/Logger.h>
#include <SKSE/SKSE.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/Hooks.h"
#include "CalamityAffixes/HostileEffectGuard.h"
#include "CalamityAffixes/Papyrus.h"
#include "CalamityAffixes/PluginEditorIds.h"
#include "CalamityAffixes/PluginLogging.h"
#include "CalamityAffixes/PrismaTooltip.h"
#include "CalamityAffixes/RuneforgeMenu.h"
#include "CalamityAffixes/RuneforgeWorkstation.h"
#include "CalamityAffixes/EchoStrikeSystem.h"
#include "CalamityAffixes/TrapSystem.h"
#include "CalamityAffixes/Version.h"

using namespace std::literals;

SKSEPluginInfo(
	.Version = REL::Version{
		CALAMITYAFFIXES_VERSION_MAJOR,
		CALAMITYAFFIXES_VERSION_MINOR,
		CALAMITYAFFIXES_VERSION_PATCH,
		CALAMITYAFFIXES_VERSION_TWEAK },
	.Name = "CalamityAffixes"sv,
	.Author = ""sv,
	.SupportEmail = ""sv,
	.StructCompatibility = SKSE::StructCompatibility::Independent,
	.RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary,
	.MinimumSKSEVersion = REL::Version{ 0, 0, 0, 0 }
)

	namespace
	{
	constexpr std::uint32_t kBuildSeq = 40;

		bool SetupLogging()
		{
			const auto logDir = SKSE::log::log_directory();
			if (!logDir) {
				return false;
			}

			auto path = *logDir / "CalamityAffixes.log";
			return CalamityAffixes::ConfigurePluginLogger(
				[&path]() {
					auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
					auto logger = std::make_shared<spdlog::logger>("CalamityAffixes", std::move(sink));
					spdlog::set_default_logger(std::move(logger));
					spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
					spdlog::set_level(spdlog::level::info);
					spdlog::flush_on(spdlog::level::info);
				},
				[&path](std::string_view a_reason) {
					std::fprintf(
						stderr,
						"CalamityAffixes: failed to initialize file logger at %s (%.*s)\n",
						path.string().c_str(),
						static_cast<int>(a_reason.size()),
						a_reason.data());
				});
		}

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		CalamityAffixes::EventBridge::GetSingleton()->Save(a_intfc);
		CalamityAffixes::SaveCalamitySummons(a_intfc);
	}

	void OnLoad(SKSE::SerializationInterface* a_intfc)
	{
		CalamityAffixes::EventBridge::GetSingleton()->Load(a_intfc);
	}

	void OnRevert(SKSE::SerializationInterface* a_intfc)
	{
		CalamityAffixes::EventBridge::GetSingleton()->Revert(a_intfc);
	}

	void OnFormDelete(RE::VMHandle a_handle)
	{
		CalamityAffixes::EventBridge::GetSingleton()->OnFormDelete(a_handle);
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		if (!a_message) {
			return;
		}

		switch (a_message->type) {
		case SKSE::MessagingInterface::kDataLoaded: {
			// Index our own editor IDs first: the config resolves its forms through it.
			CalamityAffixes::PluginEditorIds::Load();
			auto* bridge = CalamityAffixes::EventBridge::GetSingleton();
			bridge->LoadConfig();
			bridge->Register();
			// Install process-lifetime systems independently of the persisted enabled flag.
			// Their runtime entrypoints stay dormant until the MCM enables the mod.
			if (!bridge->IsHealthDamageRoutingDisabled()) {
				CalamityAffixes::Hooks::Install();
			} else {
				SKSE::log::info("CalamityAffixes: disableHealthDamageRouting=true; skipping HandleHealthDamage hooks install.");
			}

			if (!bridge->IsTrapSystemTickDisabled()) {
				CalamityAffixes::TrapSystem::Install();
			} else {
				SKSE::log::info("CalamityAffixes: disableTrapSystemTick=true; skipping TrapSystem install.");
			}
			CalamityAffixes::EchoStrikeSystem::Install();

			if (!bridge->IsRuntimeEnabled()) {
				SKSE::log::info("CalamityAffixes: runtime starts disabled; installed systems remain dormant until enabled.");
			}
			CalamityAffixes::PrismaTooltip::Install();
			CalamityAffixes::RuneforgeMenu::Install();
			CalamityAffixes::RuneforgeWorkstation::Install();

			if (auto* console = RE::ConsoleLog::GetSingleton()) {
				console->Print("CalamityAffixes: EventBridge registered.");
			}
			break;
		}
		case SKSE::MessagingInterface::kPostLoadGame:
			CalamityAffixes::EventBridge::GetSingleton()->OnPostLoadGame();
			CalamityAffixes::RestoreCalamitySummonsAfterLoad();
			// Runeforge workstation T6: IDLE, WS_Reset(0), native sweep of the forge cell.
			CalamityAffixes::RuneforgeWorkstation::OnPostLoadGame();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			// Runeforge workstation T6: abort the session, close its menu, restore the camera at once.
			CalamityAffixes::RuneforgeWorkstation::OnPreLoadGame();
			CalamityAffixes::EventBridge::GetSingleton()->OnPreLoadGame();
			break;
		case SKSE::MessagingInterface::kSaveGame:
			CalamityAffixes::EventBridge::GetSingleton()->OnPreSaveGame();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLogging();
	SKSE::log::info("CalamityAffixes: build {} {} (seq={})", __DATE__, __TIME__, kBuildSeq);
	SKSE::AllocTrampoline(1 << 10);

	SKSE::log::info("CalamityAffixes: plugin loaded, waiting for kDataLoaded.");
	if (auto* serialization = SKSE::GetSerializationInterface()) {
		serialization->SetUniqueID('CAFX');
		serialization->SetSaveCallback(OnSave);
		serialization->SetLoadCallback(OnLoad);
		serialization->SetRevertCallback(OnRevert);
		serialization->SetFormDeleteCallback(OnFormDelete);
	}
	if (auto* messaging = SKSE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}
	if (auto* papyrus = SKSE::GetPapyrusInterface()) {
		papyrus->Register(CalamityAffixes::Papyrus::Register);
	}
	return true;
}
