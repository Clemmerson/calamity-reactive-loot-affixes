	[[nodiscard]] std::uint32_t NormalizeButtonKeyCode(const RE::ButtonEvent* a_event) noexcept
	{
		if (!a_event) {
			return 0;
		}

		auto keyCode = a_event->GetIDCode();
		if (keyCode == 0) {
			return 0;
		}

		switch (a_event->device.get()) {
		case RE::INPUT_DEVICES::kKeyboard:
			return keyCode;
		case RE::INPUT_DEVICES::kMouse:
			return kMouseButtonOffset + keyCode;
		default:
			return 0;
		}
	}

	[[nodiscard]] std::uint32_t ResolveConfiguredControlPanelHotkey() noexcept
	{
		auto configuredHotkey = g_controlPanelHotkey.load();
		if (configuredHotkey == 0u) {
			// Fallback: allow opening the panel even if the user hasn't configured a keybind yet.
			configuredHotkey = kFallbackPanelHotkey;
		}
		return configuredHotkey;
	}

	[[nodiscard]] bool ShouldToggleControlPanelForButton(
		const RE::ButtonEvent* a_event,
		std::uint32_t a_configuredHotkey) noexcept
	{
		if (!a_event || !a_event->IsDown()) {
			return false;
		}

		const auto pressedKey = NormalizeButtonKeyCode(a_event);
		if (pressedKey == 0u || pressedKey != a_configuredHotkey) {
			return false;
		}

		return IsGameplayReady();
	}

	void QueueToggleControlPanelTask()
	{
		QueueTask([]() {
			SetControlPanelOpenImpl(!g_controlPanelOpen.load());
		});
	}

	// R8.7.4.14: the reader's keyboard and gamepad input is handled here, on the input-device dispatch, without the JS
	// view or the SKSE task queue (RuneforgeReaderInputContract.h). Returns true when the batch held gamepad input, which
	// is consumed so it cannot also reach gameplay or Prisma's own gamepad listener (which would double-step).
	bool HandleRuneforgeReaderInputBatch(const RE::InputEvent* a_events)
	{
		bool sawGamepad = false;
		for (auto* current = a_events; current; current = current->next) {
			if (!g_runeforgeReaderOpen.load(std::memory_order_acquire)) {
				break;
			}
			const auto* button = current->AsButtonEvent();
			if (!button) {
				continue;
			}
			const auto device = button->GetDevice();
			if (device == RE::INPUT_DEVICES::kKeyboard) {
				RuneforgeReaderContract::Action action = RuneforgeReaderContract::Action::kNone;
				{
					std::scoped_lock lock(g_runeforgeReaderMutex);
					action = RuneforgeReaderContract::MapKeyboard(
						button->GetIDCode(), button->IsDown(), button->IsUp(), g_runeforgeReaderCloseKeysArmed);
				}
				ApplyRuneforgeReaderAction(action, RuneforgeReaderContract::Source::kKeyboard);
			} else if (device == RE::INPUT_DEVICES::kGamepad) {
				sawGamepad = true;
				ApplyRuneforgeReaderAction(
					RuneforgeReaderContract::MapGamepad(button->GetIDCode(), button->IsDown()),
					RuneforgeReaderContract::Source::kGamepad);
			}
		}
		return sawGamepad;
	}

	[[nodiscard]] bool IsRelevantMenuEvent(const RE::MenuOpenCloseEvent& a_event) noexcept
	{
		const char* raw = a_event.menuName.c_str();
		const std::string_view menuName = raw ? std::string_view(raw) : std::string_view{};
		return IsRelevantMenu(menuName);
	}

	void HandleRelevantMenuOpened()
	{
		const int openCount = CountRelevantMenusOpenFromUi();
		if (openCount > 0) {
			g_relevantMenusOpen.store(openCount);
		} else {
			// Opening events can race before UI::IsMenuOpen reflects the new state.
			// Fall back to event-based increment so polling doesn't stall at zero.
			g_relevantMenusOpen.fetch_add(1);
		}
	}

	[[nodiscard]] int ResolveRelevantMenuCountAfterClose() noexcept
	{
		int openCount = CountRelevantMenusOpenFromUi();
		if (openCount < 0) {
			const int prev = g_relevantMenusOpen.fetch_sub(1);
			openCount = std::max(0, prev - 1);
		}
		g_relevantMenusOpen.store(std::max(0, openCount));
		return openCount;
	}

	void SyncViewStateAfterRelevantMenusClosed(int a_openCount)
	{
		if (a_openCount > 0) {
			return;
		}

		g_relevantMenusOpen.store(0);

		// The open reader owns the view's focus; a relevant menu closing must not take it away (that would be a focus
		// loss, and the reader contract closes on focus loss).
		if (!g_controlPanelOpen.load() && !g_runeforgeReaderOpen.load(std::memory_order_acquire) &&
			g_api && g_view && g_api->IsValid(g_view)) {
			// Keep cursor/focus state in sync with menu lifetime.
			// If focus remains after menu close, the cursor can linger until reopening a menu.
			if (g_api->HasFocus(g_view)) {
				g_api->Unfocus(g_view);
			}

			if (IsViewReady()) {
				ClearTooltipViewState(false);
			}

			SetVisible(false);
		}
	}

	class PanelHotkeyEventSink final : public RE::BSInputDeviceManager::Sink
	{
	public:
		using Event = RE::InputEvent*;

		RE::BSEventNotifyControl ProcessEvent(
			const Event* a_event,
			RE::BSTEventSource<Event>*) override
		{
			const auto* inputEvent = a_event ? *a_event : nullptr;
			// The reader is serviced before any hotkey logic, every dispatch (including empty frames), so its mailbox and
			// focus contract never depend on the task queue or on the configured panel hotkey.
			if (g_runeforgeReaderOpen.load(std::memory_order_acquire)) {
				ServiceRuneforgeReader();
				if (HandleRuneforgeReaderInputBatch(inputEvent)) {
					return RE::BSEventNotifyControl::kStop;
				}
			}

			const auto configuredHotkey = ResolveConfiguredControlPanelHotkey();
			if (configuredHotkey == 0u) {
				return RE::BSEventNotifyControl::kContinue;
			}

			for (auto* current = inputEvent; current; current = current->next) {
				const auto* buttonEvent = current->AsButtonEvent();
				// The full-catalog bypass is compiled out of Release builds.
#if !defined(NDEBUG)
				if (kRuneforgeDebugHotkey != configuredHotkey && buttonEvent && buttonEvent->IsDown() &&
					NormalizeButtonKeyCode(buttonEvent) == kRuneforgeDebugHotkey) {
					if (RuneforgeMenu::IsOpen()) {
						RuneforgeMenu::QueueClose(true);
						return RE::BSEventNotifyControl::kContinue;
					}
					if (IsGameplayReady()) {
						RuneforgeMenu::QueueOpen(true);
						return RE::BSEventNotifyControl::kContinue;
					}
				}
#endif
				if (!ShouldToggleControlPanelForButton(buttonEvent, configuredHotkey)) {
					continue;
				}

				QueueToggleControlPanelTask();
				return RE::BSEventNotifyControl::kContinue;
			}

			return RE::BSEventNotifyControl::kContinue;
		}
	};

	class MenuEventSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent* a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// MCM edits happen inside the Journal Menu, but the worker that re-reads
			// the MCM settings files sleeps until a relevant menu or the panel has
			// been open at least once. On a fresh install that left a rebound panel
			// hotkey (and the UI language) cached at their startup values until the
			// panel was first opened manually, so re-read both when the Journal
			// closes. Both refreshers are thread-safe and cheap (mtime-sourced file
			// read at menu-close frequency).
			if (!a_event->opening) {
				const char* rawName = a_event->menuName.c_str();
				const std::string_view menuName = rawName ? std::string_view(rawName) : std::string_view{};
				if (menuName == RE::JournalMenu::MENU_NAME) {
					RefreshControlPanelHotkeyFromMcm(true);
					RefreshUiLanguageFromMcm(true);
				}
			}

			{
				const char* rawName = a_event->menuName.c_str();
				const std::string_view menuName = rawName ? std::string_view(rawName) : std::string_view{};
				bool closeReader = false;
				if (g_runeforgeReaderOpen.load(std::memory_order_acquire)) {
					std::unique_lock lock(g_runeforgeReaderMutex, std::try_to_lock);
					closeReader = lock.owns_lock() && RuneforgeReaderContract::ShouldCloseOnMenuClose(
						menuName, a_event->opening, true, g_runeforgeReaderFocusConfirmed);
				}
				if (closeReader) {
					CloseRuneforgeReaderOnGameThread(RuneforgeReaderContract::Source::kFocusMenuClosed);
				}
			}

			if (!IsRelevantMenuEvent(*a_event)) {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (a_event->opening) {
				HandleRelevantMenuOpened();
				return RE::BSEventNotifyControl::kContinue;
			}

			SyncViewStateAfterRelevantMenusClosed(ResolveRelevantMenuCountAfterClose());

			return RE::BSEventNotifyControl::kContinue;
		}
	};

	MenuEventSink g_menuSink;
	PanelHotkeyEventSink g_hotkeySink;
