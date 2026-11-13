	[[nodiscard]] const char* ResolvePrismaStatusHudText(PrismaAvailabilityStatus a_status) noexcept
	{
		switch (a_status) {
		case PrismaAvailabilityStatus::kApiUnavailable:
			return "Calamity: Prisma UI unavailable; full UI is disabled.";
		case PrismaAvailabilityStatus::kViewLoading:
			return "Calamity: Prisma UI detected; Calamity view is loading.";
		case PrismaAvailabilityStatus::kReady:
			return "Calamity: Prisma UI ready.";
		case PrismaAvailabilityStatus::kViewFailed:
			return "Calamity: Prisma UI view failed. Check the SKSE log.";
		case PrismaAvailabilityStatus::kDomReadyTimedOut:
			return "Calamity: Prisma UI view timed out. Check the SKSE log.";
		case PrismaAvailabilityStatus::kNotChecked:
		default:
			return "Calamity: Prisma UI status unknown.";
		}
	}

	[[nodiscard]] bool IsPrismaFailureStatus(PrismaAvailabilityStatus a_status) noexcept
	{
		return a_status == PrismaAvailabilityStatus::kApiUnavailable ||
			a_status == PrismaAvailabilityStatus::kViewFailed ||
			a_status == PrismaAvailabilityStatus::kDomReadyTimedOut;
	}

	void NotifyPrismaPanelFailureOnce(PrismaAvailabilityStatus a_status)
	{
		if (!IsPrismaFailureStatus(a_status) || g_userFailureNotificationShown.exchange(true)) {
			return;
		}
		RE::DebugNotification(ResolvePrismaStatusHudText(a_status));
	}

	void ScheduleNextPrismaAttempt(std::chrono::steady_clock::time_point a_now) noexcept
	{
		g_nextPrismaAttemptAt = a_now + kPrismaRetryInterval;
	}

	bool EnsurePrismaApiAvailable(
		std::chrono::steady_clock::time_point a_now,
		bool a_forceAttempt)
	{
		if (g_api) {
			return true;
		}
		if (!a_forceAttempt &&
			g_nextPrismaAttemptAt.time_since_epoch().count() != 0 &&
			a_now < g_nextPrismaAttemptAt) {
			return false;
		}

		auto* api = static_cast<PRISMA_UI_API::IVPrismaUI1*>(
			PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1));
		if (!api) {
			g_prismaStatus.store(PrismaAvailabilityStatus::kApiUnavailable);
			ScheduleNextPrismaAttempt(a_now);
			if (!g_loggedMissingApi.exchange(true)) {
				SKSE::log::error("CalamityAffixes: PrismaUI API not available. Is PrismaUI installed and loaded?");
			}
			return false;
		}

		g_api = api;
		g_loggedMissingApi.store(false);
		g_nextPrismaAttemptAt = {};
		g_prismaStatus.store(PrismaAvailabilityStatus::kNotChecked);
		SKSE::log::info("CalamityAffixes: PrismaUI API acquired.");
		return true;
	}

	void DeactivatePrismaViewReadiness()
	{
		const std::scoped_lock lock(g_prismaViewReadinessMutex);
		g_activePrismaView.store(0, std::memory_order_release);
		g_domReadyView.store(0, std::memory_order_release);
		g_pendingDomReadyViews.fill(0);
	}

	[[nodiscard]] bool ActivatePrismaViewReadiness(PRISMA_UI_API::PrismaView a_view)
	{
		const std::scoped_lock lock(g_prismaViewReadinessMutex);
		g_activePrismaView.store(a_view, std::memory_order_release);

		bool wasAlreadyReady = false;
		for (const auto pendingView : g_pendingDomReadyViews) {
			wasAlreadyReady = wasAlreadyReady || pendingView == a_view;
		}
		g_pendingDomReadyViews.fill(0);
		g_domReadyView.store(wasAlreadyReady ? a_view : 0, std::memory_order_release);
		return wasAlreadyReady;
	}

	void RecordPrismaViewDomReady(PRISMA_UI_API::PrismaView a_view)
	{
		if (!a_view) {
			return;
		}

		const std::scoped_lock lock(g_prismaViewReadinessMutex);
		const auto activeView = g_activePrismaView.load(std::memory_order_acquire);
		if (activeView == a_view) {
			g_domReadyView.store(a_view, std::memory_order_release);
			return;
		}
		if (activeView != 0) {
			return;
		}

		for (auto& pendingView : g_pendingDomReadyViews) {
			if (pendingView == a_view) {
				return;
			}
			if (pendingView == 0) {
				pendingView = a_view;
				return;
			}
		}
	}

	void PrepareForNewPrismaView()
	{
		DeactivatePrismaViewReadiness();
		g_prismaViewCreatedAt = {};
		g_lastPanelStateKnown = false;
		g_lastTooltip.clear();
		g_viewCache = {};
		g_runewordCache.baseListJson.clear();
		g_runewordCache.recipeListJson.clear();
		g_runewordCache.panelStateJson.clear();
		g_runewordCache.affixPreview.clear();
		if (g_runeforgeReaderOpen.load(std::memory_order_acquire)) {
			g_runeforgeReaderNeedsPush = true;
		}
		RequestRunewordPanelRefresh(true);
		g_nextPanelSafetyRefreshAt = {};
	}

	void RegisterPrismaCommandListener()
	{
		g_api->RegisterJSListener(g_view, kPrismaCommandListener, [](const char* a_argument) {
			const std::string command = a_argument ? std::string(a_argument) : std::string();
			// Reader commands bypass the task queue (R8.7.4.14, live U1): they go to the reader mailbox, which the input
			// sink drains every frame.
			if (RuneforgeReaderContract::IsReaderCommand(command)) {
				PostRuneforgeReaderJsCommand(command);
				return;
			}
			QueueTask([command]() {
				HandleUiCommand(command);
			});
		});
	}

	void QueueCachedLayoutStateForNewView()
	{
		if (g_panelLayoutState.value.valid) {
			g_panelLayoutState.lastJson.clear();
			g_panelLayoutState.needsPush = true;
		}
		if (g_tooltipLayoutState.value.valid) {
			g_tooltipLayoutState.lastJson.clear();
			g_tooltipLayoutState.needsPush = true;
		}
	}

	void MarkPrismaViewDomReady(PRISMA_UI_API::PrismaView a_view)
	{
		if (!a_view ||
			g_activePrismaView.load(std::memory_order_acquire) != a_view ||
			g_domReadyView.load(std::memory_order_acquire) != a_view) {
			return;
		}

		const auto previous = g_prismaStatus.exchange(PrismaAvailabilityStatus::kReady);
		g_loggedViewFailure.store(false);
		g_loggedDomReadyTimeout.store(false);
		g_userFailureNotificationShown.store(false);
		if (previous != PrismaAvailabilityStatus::kReady) {
			SKSE::log::info("CalamityAffixes: PrismaUI DOM ready (view={}).", a_view);
		}
	}

	PrismaAvailabilityStatus CreatePrismaView(std::chrono::steady_clock::time_point a_now)
	{
		PrepareForNewPrismaView();
		g_prismaStatus.store(PrismaAvailabilityStatus::kViewLoading);

		const auto candidate = g_api->CreateView(kViewPath, [](PRISMA_UI_API::PrismaView a_view) {
			RecordPrismaViewDomReady(a_view);
		});
		if (!candidate || !g_api->IsValid(candidate)) {
			DeactivatePrismaViewReadiness();
			g_view = 0;
			g_prismaViewCreatedAt = {};
			g_prismaStatus.store(PrismaAvailabilityStatus::kViewFailed);
			ScheduleNextPrismaAttempt(a_now);
			if (!g_loggedViewFailure.exchange(true)) {
				SKSE::log::error(
					"CalamityAffixes: PrismaUI CreateView failed or returned an invalid view (view={}, path={}).",
					candidate,
					kViewPath);
			}
			return PrismaAvailabilityStatus::kViewFailed;
		}

		g_view = candidate;
		const bool wasAlreadyReady = ActivatePrismaViewReadiness(candidate);
		g_prismaViewCreatedAt = a_now;
		g_nextPrismaAttemptAt = {};
		g_api->SetOrder(g_view, kViewOrder);
		g_api->Hide(g_view);
		RegisterPrismaCommandListener();
		QueueCachedLayoutStateForNewView();

		if (wasAlreadyReady || g_domReadyView.load(std::memory_order_acquire) == g_view) {
			MarkPrismaViewDomReady(g_view);
		}

		SKSE::log::info("CalamityAffixes: PrismaUI view created (view={}, path={}).", g_view, kViewPath);
		return g_prismaStatus.load();
	}

	void ResetPrismaViewForRetry(
		PrismaAvailabilityStatus a_status,
		std::chrono::steady_clock::time_point a_now)
	{
		DeactivatePrismaViewReadiness();
		g_view = 0;
		g_prismaViewCreatedAt = {};
		g_prismaStatus.store(a_status);
		ScheduleNextPrismaAttempt(a_now);

		if (g_controlPanelOpen.exchange(false)) {
			NotifyPrismaPanelFailureOnce(a_status);
		}
	}

	PrismaAvailabilityStatus EnsurePrisma(bool a_forceAttempt)
	{
		LoadPanelLayoutFromDisk();
		LoadTooltipLayoutFromDisk();

		const auto now = std::chrono::steady_clock::now();
		if (!EnsurePrismaApiAvailable(now, a_forceAttempt)) {
			return g_prismaStatus.load();
		}

		if (g_view) {
			if (!g_api->IsValid(g_view)) {
				if (!g_loggedViewFailure.exchange(true)) {
					SKSE::log::warn(
						"CalamityAffixes: PrismaUI view became invalid; scheduling recreation (view={}).",
						g_view);
				}
				ResetPrismaViewForRetry(PrismaAvailabilityStatus::kViewFailed, now);
				if (!a_forceAttempt) {
					return PrismaAvailabilityStatus::kViewFailed;
				}
			} else if (g_domReadyView.load(std::memory_order_acquire) == g_view) {
				MarkPrismaViewDomReady(g_view);
				return PrismaAvailabilityStatus::kReady;
			} else if (
				g_prismaViewCreatedAt.time_since_epoch().count() != 0 &&
				(now - g_prismaViewCreatedAt) >= kPrismaDomReadyTimeout) {
				const auto expiredView = g_view;
				ResetPrismaViewForRetry(PrismaAvailabilityStatus::kDomReadyTimedOut, now);
				g_api->Destroy(expiredView);
				if (!g_loggedDomReadyTimeout.exchange(true)) {
					SKSE::log::error(
						"CalamityAffixes: PrismaUI DOM ready timed out after {} seconds (view={}, path={}).",
						std::chrono::duration_cast<std::chrono::seconds>(kPrismaDomReadyTimeout).count(),
						expiredView,
						kViewPath);
				}
				if (!a_forceAttempt) {
					return PrismaAvailabilityStatus::kDomReadyTimedOut;
				}
			} else {
				g_prismaStatus.store(PrismaAvailabilityStatus::kViewLoading);
				return PrismaAvailabilityStatus::kViewLoading;
			}
		}

		if (!a_forceAttempt &&
			g_nextPrismaAttemptAt.time_since_epoch().count() != 0 &&
			now < g_nextPrismaAttemptAt) {
			return g_prismaStatus.load();
		}

		return CreatePrismaView(now);
	}

	// R8.7.4.14 reader input/exit contract (include/CalamityAffixes/RuneforgeReaderInputContract.h).
	void ResetRuneforgeReaderSessionState()
	{
		g_runeforgeReaderFocusConfirmed = false;
		g_runeforgeReaderFocusRequests = 0u;
		g_runeforgeReaderAckSeen = false;
		g_runeforgeReaderAckWarned = false;
		g_runeforgeReaderPushedAt = {};
		g_runeforgeReaderNextServiceAt = {};
		g_runeforgeReaderCloseKeysArmed = 0u;
		g_runeforgeReaderAckPending.store(false, std::memory_order_release);
		g_runeforgeReaderPointerReports.store(0u, std::memory_order_release);
		std::scoped_lock mailboxLock(g_runeforgeReaderMailboxMutex);
		g_runeforgeReaderMailbox.clear();
	}

	void RequestRuneforgeReaderFocus()
	{
		++g_runeforgeReaderFocusRequests;
		const bool accepted = g_api->Focus(g_view, RuneforgeReaderContract::kFocusPausesGame, RuneforgeReaderContract::kFocusDisablesFocusMenu);
		if (g_runeforgeReaderFocusRequests == 1u || !accepted) {
			SKSE::log::info("Runeforge reader focus request #{} accepted={} pauseGame={}",
				g_runeforgeReaderFocusRequests, accepted, RuneforgeReaderContract::kFocusPausesGame);
		}
	}

	void CloseRuneforgeReaderOnGameThread(RuneforgeReaderContract::Source a_reason)
	{
		std::scoped_lock lock(g_runeforgeReaderMutex);
		const bool wasOpen = g_runeforgeReaderOpen.exchange(false, std::memory_order_acq_rel);
		g_runeforgeReaderNeedsPush = false;
		const bool focusWasConfirmed = g_runeforgeReaderFocusConfirmed;
		const bool ackSeen = g_runeforgeReaderAckSeen;
		ResetRuneforgeReaderSessionState();
		if (!wasOpen) {
			return;
		}
		bool focusReleased = false;
		if (g_api && g_view && g_api->IsValid(g_view)) {
			if (IsViewReady()) {
				g_api->InteropCall(g_view, kInteropSetRuneforgeReaderVisible, "0");
			}
			if (g_api->HasFocus(g_view)) {
				g_api->Unfocus(g_view);
				focusReleased = true;
			}
			const bool anotherUiNeedsView = g_controlPanelOpen.load() || g_relevantMenusOpen.load() > 0 || !g_lastTooltip.empty();
			SetVisible(anotherUiNeedsView);
		}
		SKSE::log::info("Runeforge reader closed reason={} focusReleased={} focusWasConfirmed={} jsAck={}",
			RuneforgeReaderContract::ToString(a_reason), focusReleased, focusWasConfirmed, ackSeen);
	}

	void ApplyRuneforgeReaderAction(RuneforgeReaderContract::Action a_action, RuneforgeReaderContract::Source a_source)
	{
		if (a_action == RuneforgeReaderContract::Action::kNone) {
			return;
		}
		std::scoped_lock lock(g_runeforgeReaderMutex);
		if (!g_runeforgeReaderOpen.load(std::memory_order_acquire) || g_runeforgeReaderDocument.pages.empty()) {
			return;
		}
		const auto count = g_runeforgeReaderDocument.pages.size();
		const auto before = g_runeforgeReaderPage;
		const auto result = RuneforgeReaderContract::Apply(before, count, a_action);
		SKSE::log::info("Runeforge reader input source={} action={} page={}->{} of {}{}",
			RuneforgeReaderContract::ToString(a_source), RuneforgeReaderContract::ToString(a_action),
			before + 1u, result.page + 1u, count, result.close ? " close" : "");
		if (result.close) {
			CloseRuneforgeReaderOnGameThread(a_source);
			return;
		}
		if (!result.changed) {
			return;
		}
		g_runeforgeReaderPage = result.page;
		if (IsViewReady()) {
			const auto page = std::to_string(g_runeforgeReaderPage);
			g_api->InteropCall(g_view, kInteropSetRuneforgeReaderPage, page.c_str());
		}
	}

	// Called from the JS listener. Never takes the reader mutex and never touches Prisma: it only records the command.
	void PostRuneforgeReaderJsCommand(std::string_view a_command)
	{
		if (RuneforgeReaderContract::IsPointerReport(a_command)) {
			// Evidence only (no lock, no Prisma call, no page change): where did a mouse press land?
			if (g_runeforgeReaderOpen.load(std::memory_order_acquire) &&
				g_runeforgeReaderPointerReports.fetch_add(1u, std::memory_order_acq_rel) < RuneforgeReaderContract::kMaxPointerReports) {
				SKSE::log::info("Runeforge reader pointer down at={}", RuneforgeReaderContract::SanitizePointerReport(a_command));
			}
			return;
		}
		if (a_command == RuneforgeReaderContract::kJsAck) {
			g_runeforgeReaderAckPending.store(true, std::memory_order_release);
		} else {
			const auto action = RuneforgeReaderContract::ParseJsCommand(a_command);
			if (action == RuneforgeReaderContract::Action::kNone) {
				return;
			}
			std::scoped_lock mailboxLock(g_runeforgeReaderMailboxMutex);
			if (g_runeforgeReaderMailbox.size() >= RuneforgeReaderContract::kMaxPendingActions) {
				return;
			}
			g_runeforgeReaderMailbox.push_back(action);
		}
		// Second drain path; the input-device sink also drains every frame, so neither path alone is required.
		QueueTask([]() { ServiceRuneforgeReader(); });
	}

	// Drains the JS mailbox and enforces the focus contract. Runs from the input-device sink (every frame while the reader
	// is open) and from the tick. It never blocks: a contended pass is retried on the next frame.
	void ServiceRuneforgeReader()
	{
		std::unique_lock lock(g_runeforgeReaderMutex, std::try_to_lock);
		if (!lock.owns_lock()) {
			return;
		}
		std::vector<RuneforgeReaderContract::Action> pending;
		{
			std::scoped_lock mailboxLock(g_runeforgeReaderMailboxMutex);
			pending.swap(g_runeforgeReaderMailbox);
		}
		if (!g_runeforgeReaderOpen.load(std::memory_order_acquire)) {
			return;
		}
		if (g_runeforgeReaderAckPending.exchange(false, std::memory_order_acq_rel) && !g_runeforgeReaderAckSeen) {
			g_runeforgeReaderAckSeen = true;
			SKSE::log::info("Runeforge reader JS route confirmed (ack) pages={}", g_runeforgeReaderDocument.pages.size());
		}
		for (const auto action : pending) {
			ApplyRuneforgeReaderAction(action, RuneforgeReaderContract::Source::kJs);
			if (!g_runeforgeReaderOpen.load(std::memory_order_acquire)) {
				return;
			}
		}
		if (!g_api || !g_view || !g_api->IsValid(g_view) || !IsViewReady() || g_runeforgeReaderNeedsPush) {
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		if (g_runeforgeReaderNextServiceAt.time_since_epoch().count() != 0 && now < g_runeforgeReaderNextServiceAt) {
			return;
		}
		g_runeforgeReaderNextServiceAt = now + std::chrono::milliseconds(250);
		if (!g_runeforgeReaderAckSeen && !g_runeforgeReaderAckWarned &&
			g_runeforgeReaderPushedAt.time_since_epoch().count() != 0 && now - g_runeforgeReaderPushedAt > std::chrono::seconds(3)) {
			g_runeforgeReaderAckWarned = true;
			SKSE::log::warn("Runeforge reader: no JS ack 3 s after the document push; button route unconfirmed (keyboard and gamepad stay native).");
		}
		switch (RuneforgeReaderContract::DecideFocus(g_runeforgeReaderFocusConfirmed, g_api->HasFocus(g_view), g_runeforgeReaderFocusRequests)) {
		case RuneforgeReaderContract::FocusDecision::kConfirm:
			g_runeforgeReaderFocusConfirmed = true;
			SKSE::log::info("Runeforge reader focus confirmed after {} request(s)", g_runeforgeReaderFocusRequests);
			break;
		case RuneforgeReaderContract::FocusDecision::kRequest:
			RequestRuneforgeReaderFocus();
			break;
		case RuneforgeReaderContract::FocusDecision::kCloseLost:
			CloseRuneforgeReaderOnGameThread(RuneforgeReaderContract::Source::kFocusLost);
			break;
		case RuneforgeReaderContract::FocusDecision::kCloseUnavailable:
			SKSE::log::warn("Runeforge reader: focus not acquired after {} requests; closing instead of leaving the reader without input.",
				g_runeforgeReaderFocusRequests);
			CloseRuneforgeReaderOnGameThread(RuneforgeReaderContract::Source::kFocusUnavailable);
			break;
		case RuneforgeReaderContract::FocusDecision::kHold:
		default:
			break;
		}
	}

	void TickRuneforgeReader()
	{
		std::scoped_lock lock(g_runeforgeReaderMutex);
		if (!g_runeforgeReaderOpen.load(std::memory_order_acquire) || !IsViewReady()) {
			return;
		}
		if (g_runeforgeReaderNeedsPush) {
			g_controlPanelOpen.store(false, std::memory_order_release);
			SetControlPanelJsState(false);
			const nlohmann::json payload = {
				{"title", g_runeforgeReaderDocument.title},
				{"substrate", g_runeforgeReaderDocument.substrate},
				{"pages", g_runeforgeReaderDocument.pages},
			};
			const auto serialized = payload.dump();
			g_api->InteropCall(g_view, kInteropSetRuneforgeReader, serialized.c_str());
			if (g_runeforgeReaderPage > 0) {
				const auto page = std::to_string(g_runeforgeReaderPage);
				g_api->InteropCall(g_view, kInteropSetRuneforgeReaderPage, page.c_str());
			}
			g_api->InteropCall(g_view, kInteropSetRuneforgeReaderVisible, "1");
			g_runeforgeReaderNeedsPush = false;
			g_runeforgeReaderPushedAt = std::chrono::steady_clock::now();
			SetVisible(true);
			if (!g_api->HasFocus(g_view)) {
				RequestRuneforgeReaderFocus();
			}
			g_runeforgeReaderNextServiceAt = g_runeforgeReaderPushedAt + std::chrono::milliseconds(250);
			return;
		}
		SetVisible(true);
		ServiceRuneforgeReader();
	}

	void SetControlPanelOpenImpl(bool a_open)
	{
		if (a_open && g_runeforgeReaderOpen.load(std::memory_order_acquire)) {
			CloseRuneforgeReaderOnGameThread(RuneforgeReaderContract::Source::kPanelOpened);
		}
		if (!a_open) {
			g_controlPanelOpen.store(false);
			ApplyControlPanelState(false);
			g_lastPanelState = false;
			g_lastPanelStateKnown = true;
			PushUiFeedback("Prisma panel closed.");
			return;
		}
		RequestRunewordPanelRefresh(g_runewordCache.recipeListJson.empty());

		const auto status = EnsurePrisma(true);
		if (IsPrismaFailureStatus(status)) {
			g_controlPanelOpen.store(false);
			NotifyPrismaPanelFailureOnce(status);
			return;
		}

		g_controlPanelOpen.store(true);
		if (status == PrismaAvailabilityStatus::kReady) {
			g_lastPanelState = true;
			g_lastPanelStateKnown = IsGameplayReady();
			ApplyControlPanelState(true);
			PushUiFeedback("Prisma panel opened.");
		}
	}

	void RegisterPrismaEventSinks()
	{
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuSink);
			g_gatePollingOnMenus = true;
		}
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink(&g_hotkeySink);
		}
	}

	class TickTaskPendingReset final
	{
	public:
		~TickTaskPendingReset()
		{
			g_tickTaskPending.store(false, std::memory_order_release);
		}
	};

	void StartPrismaWorker()
	{
		g_worker = std::jthread([](std::stop_token stopToken) {
			while (!stopToken.stop_requested()) {
				std::this_thread::sleep_for(kPollInterval);

				if (g_gatePollingOnMenus.load() &&
					g_relevantMenusOpen.load() <= 0 &&
					!g_controlPanelOpen.load() &&
					!g_runeforgeReaderOpen.load() &&
					g_prismaStatus.load() != PrismaAvailabilityStatus::kViewLoading) {
					continue;
				}

				if (auto* tasks = SKSE::GetTaskInterface()) {
					bool expected = false;
					if (!g_tickTaskPending.compare_exchange_strong(
							expected, true, std::memory_order_acq_rel)) {
						PrismaTelemetryController::RecordCoalescedTick();
						continue;
					}
					PrismaTelemetryController::RecordWorkerEnqueue();
					tasks->AddTask([]() {
						[[maybe_unused]] const TickTaskPendingReset pendingReset{};
						Tick();
					});
				}
			}
		});
	}
