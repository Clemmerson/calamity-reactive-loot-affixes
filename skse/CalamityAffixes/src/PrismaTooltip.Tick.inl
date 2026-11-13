		class SlowPrismaTickProbe final
		{
		public:
			explicit SlowPrismaTickProbe(std::chrono::steady_clock::time_point a_started) noexcept :
				_started(a_started)
			{}

			~SlowPrismaTickProbe()
			{
				const auto finished = std::chrono::steady_clock::now();
				const auto elapsed = finished - _started;
				if (elapsed < kSlowTickThreshold) {
					return;
				}

				PrismaTelemetryController::RecordSlowTick();
				if (g_nextSlowTickLogAt.time_since_epoch().count() != 0 &&
					finished < g_nextSlowTickLogAt) {
					return;
				}
				g_nextSlowTickLogAt = finished + kSlowTickLogInterval;

				const auto elapsedMs = std::chrono::duration<double, std::milli>(elapsed).count();
				SKSE::log::warn(
					"CalamityAffixes: slow Prisma tick ({:.2f}ms, panelOpen={}, dynamicDirty={}, recipeDirty={}, panelRefreshes={}, recipeRefreshes={}, coalesced={}).",
					elapsedMs,
					g_controlPanelOpen.load(std::memory_order_relaxed),
					g_runewordPanelDynamicDirty,
					g_runewordRecipeListDirty,
					g_prismaTelemetry.panelRefreshes.load(std::memory_order_relaxed),
					g_prismaTelemetry.recipeRefreshes.load(std::memory_order_relaxed),
					g_prismaTelemetry.coalescedTicks.load(std::memory_order_relaxed));
			}

		private:
			std::chrono::steady_clock::time_point _started;
		};

		[[nodiscard]] bool TickShouldWaitForReadyView(std::chrono::steady_clock::time_point now)
		{
			if (IsViewReady()) {
				return false;
			}

			MaybeLogPrismaTelemetry(now, g_controlPanelOpen.load() || g_relevantMenusOpen.load() > 0);
			return true;
		}

		void PushTickUiSettingsAndLayouts()
		{
			PushPanelHotkeyToJs();
			PushUiLanguageToJs();

			if (g_panelLayoutState.needsPush) {
				PushPanelLayoutToJs();
				g_panelLayoutState.needsPush = false;
			}
			if (g_tooltipLayoutState.needsPush) {
				PushTooltipLayoutToJs();
				g_tooltipLayoutState.needsPush = false;
			}
		}

		[[nodiscard]] bool HandleTickWhileGameplayUnavailable(std::chrono::steady_clock::time_point now)
		{
			if (IsGameplayReady()) {
				return false;
			}
			if (g_runeforgeReaderOpen.load(std::memory_order_acquire)) {
				CloseRuneforgeReaderOnGameThread(RuneforgeReaderContract::Source::kGameplayUnavailable);
			}

			g_lastPanelStateKnown = false;
			ClearTooltipViewState(true);
			SetControlPanelJsState(false);
			if (g_api->HasFocus(g_view)) {
				g_api->Unfocus(g_view);
			}
			SetVisible(false);
			MaybeLogPrismaTelemetry(now, true);
			return true;
		}

		void SyncTickControlPanelState(bool a_panelRequested)
		{
			if (!g_lastPanelStateKnown || g_lastPanelState != a_panelRequested) {
				ApplyControlPanelState(a_panelRequested);
				g_lastPanelState = a_panelRequested;
				g_lastPanelStateKnown = true;
			}
		}

		[[nodiscard]] bool HandleTickWhenNoRelevantMenusRemain(
			bool a_panelRequested,
			std::chrono::steady_clock::time_point now)
		{
			// Failsafe: heal stale menu-open counters from actual UI state.
			// Some close paths can race event delivery and leave cursor/focus behind.
			const int observedRelevantMenus = CountRelevantMenusOpenFromUi();
			if (observedRelevantMenus < 0) {
				return false;
			}

			g_relevantMenusOpen.store(observedRelevantMenus);
			if (a_panelRequested || observedRelevantMenus != 0) {
				return false;
			}

			ClearTooltipViewState(false);
			if (g_api->HasFocus(g_view)) {
				g_api->Unfocus(g_view);
			}
			SetVisible(false);
			MaybeLogPrismaTelemetry(now, false);
			return true;
		}

		void RefreshTickRunewordPanelBindings(
			bool a_panelRequested,
			std::chrono::steady_clock::time_point a_now)
		{
			if (!a_panelRequested) {
				return;
			}

			const bool safetyRefreshDue =
				g_nextPanelSafetyRefreshAt.time_since_epoch().count() == 0 ||
				a_now >= g_nextPanelSafetyRefreshAt;
			if (!g_runewordPanelDynamicDirty && !g_runewordRecipeListDirty && !safetyRefreshDue) {
				return;
			}

			if (auto* bridge = CalamityAffixes::EventBridge::GetSingleton()) {
				RefreshRunewordPanelBindings(*bridge, g_runewordRecipeListDirty);
			}
		}

		void FinalizeTickTooltipPresentation(
			bool a_panelRequested,
			std::chrono::steady_clock::time_point now)
		{
			const bool hasTooltip = PushSelectedTooltipSnapshot(false);
			if (!hasTooltip) {
				if (!a_panelRequested && g_api->HasFocus(g_view)) {
					g_api->Unfocus(g_view);
				}

				if (!a_panelRequested) {
					// Keep the view visible in inventory so the quick-launch button can open the control panel
					// even when the selected item has no affix tooltip.
					const bool inventoryOpen = g_gatePollingOnMenus.load() && g_relevantMenusOpen.load() > 0;
					SetVisible(inventoryOpen);
				}
				MaybeLogPrismaTelemetry(now, a_panelRequested || g_relevantMenusOpen.load() > 0);
				return;
			}

			SetVisible(true);
			if (!a_panelRequested && g_api->HasFocus(g_view)) {
				// Stability mode: tooltip-only state should never own cursor/focus.
				// This disables drag-to-move, but prevents lingering cursor state after menu close.
				g_api->Unfocus(g_view);
			}
			MaybeLogPrismaTelemetry(now, true);
		}

		void Tick()
		{
			const auto now = std::chrono::steady_clock::now();
			const SlowPrismaTickProbe slowTickProbe{ now };
			PrismaTelemetryController::RecordTickRun();
			RefreshControlPanelHotkeyFromMcm(false);
			const auto previousUiLanguageMode = g_uiLanguageMode.load(std::memory_order_relaxed);
			RefreshUiLanguageFromMcm(false);
			if (g_uiLanguageMode.load(std::memory_order_relaxed) != previousUiLanguageMode) {
				RequestRunewordPanelRefresh(true);
			}
			(void)EnsurePrisma(false);

			if (TickShouldWaitForReadyView(now)) {
				return;
			}

			PushTickUiSettingsAndLayouts();

			if (HandleTickWhileGameplayUnavailable(now)) {
				return;
			}
			if (g_runeforgeReaderOpen.load(std::memory_order_acquire)) {
				TickRuneforgeReader();
				return;
			}

			const bool panelRequested = g_controlPanelOpen.load();
			SyncTickControlPanelState(panelRequested);
			if (HandleTickWhenNoRelevantMenusRemain(panelRequested, now)) {
				return;
			}

			RefreshTickRunewordPanelBindings(panelRequested, now);
			FinalizeTickTooltipPresentation(panelRequested, now);
		}
