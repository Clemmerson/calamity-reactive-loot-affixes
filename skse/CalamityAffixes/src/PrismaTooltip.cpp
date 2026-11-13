#include "CalamityAffixes/PrismaTooltip.h"
#include "CalamityAffixes/RunewordUiSerialization.h"

#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/EventNames.h"
#include "CalamityAffixes/LootUiGuards.h"
#include "CalamityAffixes/PrismaLayoutPersistence.h"
#include "CalamityAffixes/RuneforgeMenu.h"
#include "CalamityAffixes/RuneforgeReaderInputContract.h"
#include "CalamityAffixes/RuneforgeWorkstation.h"
#include "CalamityAffixes/RuneforgeWorkstationContextPolicy.h"
#include "CalamityAffixes/RunewordUiPolicy.h"
#include "CalamityAffixes/UserSettingsPersistence.h"

#include "PrismaUI_API.h"

namespace CalamityAffixes::PrismaTooltip
{
	namespace
	{
		#include "PrismaTooltip.SharedState.inl"

		void SetControlPanelOpenImpl(bool a_open);
		void HandleUiCommand(std::string_view a_command);
		void RefreshControlPanelHotkeyFromMcm(bool a_force = false);
		void RefreshUiLanguageFromMcm(bool a_force = false);
		void LoadPanelLayoutFromDisk();
		void LoadTooltipLayoutFromDisk();
		void SavePanelLayoutToDisk(const PanelLayout& a_layout);
		void SaveTooltipLayoutToDisk(const TooltipLayout& a_layout);
		void PushPanelLayoutToJs();
		void PushTooltipLayoutToJs();
		[[nodiscard]] bool TryReadPanelHotkeyFromMcmJson(std::istream& a_in, std::uint32_t& a_outResolvedKey);
		[[nodiscard]] bool TryResolveUiLanguageSettingsPath(std::filesystem::path& a_outPath);
		[[nodiscard]] bool TryReadUiLanguageModeFromIni(std::istream& a_in, int& a_outResolvedMode);
		[[nodiscard]] std::optional<PanelLayout> TryLoadPanelLayoutStateFromDisk();
		[[nodiscard]] std::optional<TooltipLayout> TryLoadTooltipLayoutStateFromDisk();
		void SetRunewordBaseInventoryList(const std::vector<RunewordBaseInventoryEntry>& a_entries);
		void SetRunewordRecipeList(const std::vector<RunewordRecipeEntry>& a_entries);
		void SetRunewordAffixPreview(std::string_view a_text);
		void SetRunewordPanelState(const RunewordPanelState& a_state);
		[[nodiscard]] bool TryReadSelectedItemContextFromMenus(
			SelectedItemContext& a_result,
			RE::UI& a_ui,
			CalamityAffixes::EventBridge& a_bridge);
		void QueueTask(std::function<void()> a_task);
		void ClearSelectedTooltipFromView(bool a_force, bool a_selectionChanged);
		bool PushSelectedTooltipSnapshot(bool a_force = false);
		void ClearTooltipViewState(bool a_clearRunewordPanelData);
		void RequestRunewordPanelRefresh(bool a_includeRecipes) noexcept;
		void RefreshRunewordPanelBindings(CalamityAffixes::EventBridge& a_bridge, bool a_includeRecipes);
		void MaybeLogPrismaTelemetry(std::chrono::steady_clock::time_point a_now, bool a_uiActive);
		[[nodiscard]] bool HandleStructuredUiCommand(std::string_view a_command);
		void HandleEventBackedUiCommand(std::string_view a_command);
		[[nodiscard]] bool EnsurePrismaApiAvailable(
			std::chrono::steady_clock::time_point a_now,
			bool a_forceAttempt);
		[[nodiscard]] PrismaAvailabilityStatus EnsurePrisma(bool a_forceAttempt = false);
		void PrepareForNewPrismaView();
		void RegisterPrismaCommandListener();
		void QueueCachedLayoutStateForNewView();
		[[nodiscard]] PrismaAvailabilityStatus CreatePrismaView(std::chrono::steady_clock::time_point a_now);
		void Tick();
		void RegisterPrismaEventSinks();
		void StartPrismaWorker();
		[[nodiscard]] std::uint32_t ResolveConfiguredControlPanelHotkey() noexcept;
		[[nodiscard]] bool ShouldToggleControlPanelForButton(
			const RE::ButtonEvent* a_event,
			std::uint32_t a_configuredHotkey) noexcept;
		void QueueToggleControlPanelTask();
		[[nodiscard]] bool IsRelevantMenuEvent(const RE::MenuOpenCloseEvent& a_event) noexcept;
		void HandleRelevantMenuOpened();
		[[nodiscard]] int ResolveRelevantMenuCountAfterClose() noexcept;
		void SyncViewStateAfterRelevantMenusClosed(int a_openCount);
		void TickRuneforgeReader();
		void ServiceRuneforgeReader();
		void ResetRuneforgeReaderSessionState();
		void RequestRuneforgeReaderFocus();
		void CloseRuneforgeReaderOnGameThread(RuneforgeReaderContract::Source a_reason);
		void ApplyRuneforgeReaderAction(RuneforgeReaderContract::Action a_action, RuneforgeReaderContract::Source a_source);
		void PostRuneforgeReaderJsCommand(std::string_view a_command);
		bool HandleRuneforgeReaderInputBatch(const RE::InputEvent* a_events);

		[[nodiscard]] bool IsRelevantMenu(std::string_view a_name) noexcept
		{
			return CalamityAffixes::IsPrismaTooltipRelevantMenu(a_name);
		}

		[[nodiscard]] int CountRelevantMenusOpenFromUi() noexcept
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return -1;
			}

			int count = 0;
			if (ui->IsMenuOpen(kMenuInventory)) {
				++count;
			}
			if (ui->IsMenuOpen(kMenuBarter)) {
				++count;
			}
			if (ui->IsMenuOpen(kMenuContainer)) {
				++count;
			}
			if (ui->IsMenuOpen(kMenuGift)) {
				++count;
			}
			return count;
		}

		[[nodiscard]] std::string StripRunewordOverlayTooltipLines(std::string_view a_tooltip)
		{
			if (a_tooltip.empty() || ShouldShowRunewordTooltipInItemOverlay()) {
				return std::string(a_tooltip);
			}

			std::string filtered;
			filtered.reserve(a_tooltip.size());
			std::size_t start = 0;
			while (start <= a_tooltip.size()) {
				const auto end = a_tooltip.find('\n', start);
				const auto lineEnd = end == std::string_view::npos ? a_tooltip.size() : end;
				const auto line = a_tooltip.substr(start, lineEnd - start);
				if (!IsRunewordOverlayTooltipLine(line)) {
					if (!filtered.empty()) {
						filtered.push_back('\n');
					}
					filtered.append(line);
				}
				if (lineEnd == a_tooltip.size()) {
					break;
				}
				start = lineEnd + 1;
			}

			return filtered;
		}

		[[nodiscard]] bool IsViewReady() noexcept
		{
			const auto activeView = g_activePrismaView.load(std::memory_order_acquire);
			return g_api && activeView && g_api->IsValid(activeView) &&
				g_domReadyView.load(std::memory_order_acquire) == activeView;
		}

		[[nodiscard]] bool IsGameplayReady() noexcept
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->GetParentCell()) {
				return false;
			}

			auto* ui = RE::UI::GetSingleton();
			if (ui &&
				(ui->IsMenuOpen("MainMenu") ||
				 ui->IsMenuOpen("LoadingMenu") ||
				 ui->IsMenuOpen("RaceSex Menu"))) {
				return false;
			}

			return true;
		}

		void SetVisible(bool a_visible)
		{
			if (!g_api || !g_view || !g_api->IsValid(g_view)) {
				return;
			}

			const bool hidden = g_api->IsHidden(g_view);
			if (a_visible) {
				if (hidden) {
					g_api->Show(g_view);
				}
			} else {
				if (!hidden) {
					g_api->Hide(g_view);
				}
			}
		}

		void SetControlPanelJsState(bool a_open)
		{
			if (!IsViewReady()) {
				return;
			}

			const char* value = a_open ? "1" : "0";
			g_api->InteropCall(g_view, kInteropSetControlPanel, value);
		}

		void ApplyControlPanelState(bool a_open)
		{
			if (!IsViewReady()) {
				return;
			}

			if (!IsGameplayReady()) {
				SetControlPanelJsState(false);
				if (g_api->HasFocus(g_view)) {
					g_api->Unfocus(g_view);
				}
				SetVisible(false);
				return;
			}

			SetControlPanelJsState(a_open);
			if (a_open) {
				SetVisible(true);
				if (!g_api->HasFocus(g_view)) {
					// Always use FocusMenu so the panel is modal (prevents click-through to InventoryMenu).
					g_api->Focus(g_view, false, false);
				}
			} else {
				if (g_api->HasFocus(g_view)) {
					g_api->Unfocus(g_view);
				}
				if (g_lastTooltip.empty()) {
					SetVisible(false);
				}
			}
		}

		void PushUiFeedback(std::string_view a_message)
		{
			if (!IsViewReady()) {
				return;
			}

			const std::string message(a_message);
			g_api->InteropCall(g_view, kInteropSetActionFeedback, message.c_str());
		}

		[[nodiscard]] bool EmitModEvent(std::string_view a_eventName, std::string_view a_strArg = {}, float a_numArg = 0.0f)
		{
			auto* source = SKSE::GetModCallbackEventSource();
			if (!source) {
				return false;
			}

			const std::string eventName(a_eventName);
			const std::string strArg(a_strArg);
			SKSE::ModCallbackEvent event{
				RE::BSFixedString(eventName.c_str()),
				RE::BSFixedString(strArg.c_str()),
				a_numArg,
				nullptr
			};
			source->SendEvent(&event);
			return true;
		}

		#include "PrismaTooltip.SettingsLayout.inl"

		#include "PrismaTooltip.MenuInput.inl"

		#include "PrismaTooltip.ViewState.inl"

		#include "PrismaTooltip.PanelData.inl"

		#include "PrismaTooltip.HandleUiCommand.inl"

		#include "PrismaTooltip.ViewLifecycle.inl"

		#include "PrismaTooltip.Tick.inl"

	}

	void Install()
	{
		bool expected = false;
		if (!g_installed.compare_exchange_strong(expected, true)) {
			return;
		}

		RegisterPrismaEventSinks();
		RefreshControlPanelHotkeyFromMcm(true);
		RefreshUiLanguageFromMcm(true);
		LoadPanelLayoutFromDisk();
		StartPrismaWorker();

		SKSE::log::info("CalamityAffixes: Prisma tooltip/control overlay enabled (poll={}ms).", kPollInterval.count());
	}

	bool IsAvailable()
	{
		return IsViewReady();
	}

	int GetUiLanguageMode()
	{
		return NormalizeUiLanguageMode(g_uiLanguageMode.load());
	}

	void ReportStatus()
	{
		QueueTask([]() {
			const auto status = EnsurePrisma(false);
			const char* message = "Calamity: Prisma UI status unknown.";
			switch (status) {
			case PrismaAvailabilityStatus::kApiUnavailable:
				message = "Calamity: Prisma UI unavailable; full UI is disabled.";
				break;
			case PrismaAvailabilityStatus::kViewLoading:
				message = "Calamity: Prisma UI detected; Calamity view is loading.";
				break;
			case PrismaAvailabilityStatus::kReady:
				message = "Calamity: Prisma UI ready.";
				break;
			case PrismaAvailabilityStatus::kViewFailed:
				message = "Calamity: Prisma UI view failed. Check the SKSE log.";
				break;
			case PrismaAvailabilityStatus::kDomReadyTimedOut:
				message = "Calamity: Prisma UI view timed out. Check the SKSE log.";
				break;
			case PrismaAvailabilityStatus::kNotChecked:
			default:
				break;
			}
			RE::DebugNotification(message);
		});
	}

	void SetControlPanelOpen(bool a_open)
	{
		QueueTask([a_open]() {
			SetControlPanelOpenImpl(a_open);
		});
	}

	void ToggleControlPanel()
	{
		QueueTask([]() {
			SetControlPanelOpenImpl(!g_controlPanelOpen.load());
		});
	}

	void HandleRuneforgeReaderEvent(std::string_view a_eventName, std::string_view a_strArg, float a_numArg)
	{
		constexpr std::string_view beginStone = "FEASR_Runeforge_Reader_BeginStone";
		constexpr std::string_view beginMetal = "FEASR_Runeforge_Reader_BeginMetal";
		constexpr std::string_view pageEvent = "FEASR_Runeforge_Reader_Page";
		constexpr std::string_view commitEvent = "FEASR_Runeforge_Reader_Commit";
		if (a_eventName == beginStone || a_eventName == beginMetal) {
			if (a_strArg.empty() || a_strArg.size() > 160u || !std::isfinite(a_numArg) ||
				std::floor(a_numArg) != a_numArg || a_numArg < 1.0f || a_numArg > 20.0f) {
				SKSE::log::warn("Runeforge reader begin rejected: invalid title or page count.");
				return;
			}
			std::scoped_lock lock(g_runeforgeReaderDraftMutex);
			g_runeforgeReaderDraft = {};
			g_runeforgeReaderDraft.active = true;
			g_runeforgeReaderDraft.expectedPages = static_cast<std::size_t>(a_numArg);
			g_runeforgeReaderDraft.document.title.assign(a_strArg);
			g_runeforgeReaderDraft.document.substrate = a_eventName == beginMetal ? "metal" : "stone";
			g_runeforgeReaderDraft.document.pages.resize(g_runeforgeReaderDraft.expectedPages);
			return;
		}
		if (a_eventName == pageEvent) {
			if (!std::isfinite(a_numArg) || std::floor(a_numArg) != a_numArg || a_numArg < 1.0f || a_numArg > 20.0f ||
				a_strArg.empty() || a_strArg.size() > 6000u) {
				SKSE::log::warn("Runeforge reader page rejected: invalid index or page size.");
				return;
			}
			std::scoped_lock lock(g_runeforgeReaderDraftMutex);
			if (!g_runeforgeReaderDraft.active) return;
			const auto index = static_cast<std::size_t>(a_numArg - 1.0f);
			if (index >= g_runeforgeReaderDraft.expectedPages) return;
			g_runeforgeReaderDraft.document.pages[index].assign(a_strArg);
			return;
		}
		if (a_eventName != commitEvent) return;

		RuneforgeReaderDocument document;
		{
			std::scoped_lock lock(g_runeforgeReaderDraftMutex);
			if (!g_runeforgeReaderDraft.active || !std::isfinite(a_numArg) ||
				std::floor(a_numArg) != a_numArg || static_cast<std::size_t>(a_numArg) != g_runeforgeReaderDraft.expectedPages) {
				SKSE::log::warn("Runeforge reader commit rejected: no matching bounded page draft.");
				g_runeforgeReaderDraft = {};
				return;
			}
			const auto totalBytes = std::accumulate(
				g_runeforgeReaderDraft.document.pages.begin(),
				g_runeforgeReaderDraft.document.pages.end(),
				std::size_t{ 0u }, [](std::size_t a_total, const std::string& a_page) { return a_total + a_page.size(); });
			if (totalBytes > 30000u || std::any_of(g_runeforgeReaderDraft.document.pages.begin(), g_runeforgeReaderDraft.document.pages.end(),
				[](const std::string& a_page) { return a_page.empty(); })) {
				SKSE::log::warn("Runeforge reader commit rejected: incomplete or oversized inscription.");
				g_runeforgeReaderDraft = {};
				return;
			}
			document = std::move(g_runeforgeReaderDraft.document);
			g_runeforgeReaderDraft = {};
		}

		QueueTask([document = std::move(document)]() mutable {
			if (!IsGameplayReady()) {
				SKSE::log::warn("Runeforge reader open rejected: gameplay UI is unavailable.");
				return;
			}
			std::scoped_lock readerLock(g_runeforgeReaderMutex);
			ResetRuneforgeReaderSessionState();
			g_runeforgeReaderDocument = std::move(document);
			g_runeforgeReaderPage = 0;
			g_runeforgeReaderNeedsPush = true;
			g_controlPanelOpen.store(false, std::memory_order_release);
			g_runeforgeReaderOpen.store(true, std::memory_order_release);
			const auto status = EnsurePrisma(true);
			if (IsPrismaFailureStatus(status)) {
				g_runeforgeReaderOpen.store(false, std::memory_order_release);
				g_runeforgeReaderNeedsPush = false;
				NotifyPrismaPanelFailureOnce(status);
				return;
			}
			TickRuneforgeReader();
			SKSE::log::info("Runeforge reader opened substrate={} pages={}",
				g_runeforgeReaderDocument.substrate, g_runeforgeReaderDocument.pages.size());
		});
	}

	// Public entry points (HandleUiCommand, external callers). They post to the same mailbox as the view's buttons, so the
	// reader has exactly one action path.
	void StepRuneforgeReader(int a_delta)
	{
		if (a_delta == 0) return;
		PostRuneforgeReaderJsCommand(a_delta < 0 ? RuneforgeReaderContract::kJsPrevious : RuneforgeReaderContract::kJsNext);
	}

	void CloseRuneforgeReader()
	{
		PostRuneforgeReaderJsCommand(RuneforgeReaderContract::kJsClose);
	}
}
