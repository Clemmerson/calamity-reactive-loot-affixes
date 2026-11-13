		[[nodiscard]] std::string DescribeHotkey(std::uint32_t a_keyCode)
		{
			if (a_keyCode == 0u) {
				return {};
			}

			if (a_keyCode >= kMouseButtonOffset && a_keyCode <= kMouseButtonOffset + 9u) {
				const auto mouse = a_keyCode - kMouseButtonOffset;
				switch (mouse) {
				case 0: return "Mouse Left";
				case 1: return "Mouse Right";
				case 2: return "Mouse Middle";
				case 3: return "Mouse 4";
				case 4: return "Mouse 5";
				case 5: return "Mouse 6";
				case 6: return "Mouse 7";
				case 7: return "Mouse 8";
				case 8: return "Wheel Up";
				case 9: return "Wheel Down";
				default: break;
				}
			}

			if (a_keyCode >= 266u && a_keyCode <= 281u) {
				switch (a_keyCode) {
				case 266u: return "Gamepad D-Pad Up";
				case 267u: return "Gamepad D-Pad Down";
				case 268u: return "Gamepad D-Pad Left";
				case 269u: return "Gamepad D-Pad Right";
				case 270u: return "Gamepad Start";
				case 271u: return "Gamepad Back";
				case 272u: return "Gamepad Left Stick";
				case 273u: return "Gamepad Right Stick";
				case 274u: return "Gamepad LB";
				case 275u: return "Gamepad RB";
				case 276u: return "Gamepad A";
				case 277u: return "Gamepad B";
				case 278u: return "Gamepad X";
				case 279u: return "Gamepad Y";
				case 280u: return "Gamepad LT";
				case 281u: return "Gamepad RT";
				default: break;
				}
			}

			switch (a_keyCode) {
			case RE::BSKeyboardDevice::Keys::kEscape:
				return "Esc";
			case RE::BSKeyboardDevice::Keys::kNum1:
				return "1";
			case RE::BSKeyboardDevice::Keys::kNum2:
				return "2";
			case RE::BSKeyboardDevice::Keys::kNum3:
				return "3";
			case RE::BSKeyboardDevice::Keys::kNum4:
				return "4";
			case RE::BSKeyboardDevice::Keys::kNum5:
				return "5";
			case RE::BSKeyboardDevice::Keys::kNum6:
				return "6";
			case RE::BSKeyboardDevice::Keys::kNum7:
				return "7";
			case RE::BSKeyboardDevice::Keys::kNum8:
				return "8";
			case RE::BSKeyboardDevice::Keys::kNum9:
				return "9";
			case RE::BSKeyboardDevice::Keys::kNum0:
				return "0";
			case RE::BSKeyboardDevice::Keys::kMinus:
				return "-";
			case RE::BSKeyboardDevice::Keys::kEquals:
				return "=";
			case RE::BSKeyboardDevice::Keys::kBackspace:
				return "Backspace";
			case RE::BSKeyboardDevice::Keys::kTab:
				return "Tab";
			case RE::BSKeyboardDevice::Keys::kQ:
				return "Q";
			case RE::BSKeyboardDevice::Keys::kW:
				return "W";
			case RE::BSKeyboardDevice::Keys::kE:
				return "E";
			case RE::BSKeyboardDevice::Keys::kR:
				return "R";
			case RE::BSKeyboardDevice::Keys::kT:
				return "T";
			case RE::BSKeyboardDevice::Keys::kY:
				return "Y";
			case RE::BSKeyboardDevice::Keys::kU:
				return "U";
			case RE::BSKeyboardDevice::Keys::kI:
				return "I";
			case RE::BSKeyboardDevice::Keys::kO:
				return "O";
			case RE::BSKeyboardDevice::Keys::kP:
				return "P";
			case RE::BSKeyboardDevice::Keys::kBracketLeft:
				return "[";
			case RE::BSKeyboardDevice::Keys::kBracketRight:
				return "]";
			case RE::BSKeyboardDevice::Keys::kSpacebar:
				return "Space";
			case RE::BSKeyboardDevice::Keys::kEnter:
				return "Enter";
			case RE::BSKeyboardDevice::Keys::kLeftShift:
				return "LShift";
			case RE::BSKeyboardDevice::Keys::kRightShift:
				return "RShift";
			case RE::BSKeyboardDevice::Keys::kLeftControl:
				return "LCtrl";
			case RE::BSKeyboardDevice::Keys::kRightControl:
				return "RCtrl";
			case RE::BSKeyboardDevice::Keys::kLeftAlt:
				return "LAlt";
			case RE::BSKeyboardDevice::Keys::kRightAlt:
				return "RAlt";
			case RE::BSKeyboardDevice::Keys::kA:
				return "A";
			case RE::BSKeyboardDevice::Keys::kS:
				return "S";
			case RE::BSKeyboardDevice::Keys::kD:
				return "D";
			case RE::BSKeyboardDevice::Keys::kF:
				return "F";
			case RE::BSKeyboardDevice::Keys::kG:
				return "G";
			case RE::BSKeyboardDevice::Keys::kH:
				return "H";
			case RE::BSKeyboardDevice::Keys::kJ:
				return "J";
			case RE::BSKeyboardDevice::Keys::kK:
				return "K";
			case RE::BSKeyboardDevice::Keys::kL:
				return "L";
			case RE::BSKeyboardDevice::Keys::kZ:
				return "Z";
			case RE::BSKeyboardDevice::Keys::kX:
				return "X";
			case RE::BSKeyboardDevice::Keys::kC:
				return "C";
			case RE::BSKeyboardDevice::Keys::kV:
				return "V";
			case RE::BSKeyboardDevice::Keys::kB:
				return "B";
			case RE::BSKeyboardDevice::Keys::kN:
				return "N";
			case RE::BSKeyboardDevice::Keys::kM:
				return "M";
			case RE::BSKeyboardDevice::Keys::kF1:
				return "F1";
			case RE::BSKeyboardDevice::Keys::kF2:
				return "F2";
			case RE::BSKeyboardDevice::Keys::kF3:
				return "F3";
			case RE::BSKeyboardDevice::Keys::kF4:
				return "F4";
			case RE::BSKeyboardDevice::Keys::kF5:
				return "F5";
			case RE::BSKeyboardDevice::Keys::kF6:
				return "F6";
			case RE::BSKeyboardDevice::Keys::kF7:
				return "F7";
			case RE::BSKeyboardDevice::Keys::kF8:
				return "F8";
			case RE::BSKeyboardDevice::Keys::kF9:
				return "F9";
			case RE::BSKeyboardDevice::Keys::kF10:
				return "F10";
			case RE::BSKeyboardDevice::Keys::kF11:
				return "F11";
			case RE::BSKeyboardDevice::Keys::kF12:
				return "F12";
			case RE::BSKeyboardDevice::Keys::kUp:
				return "Up";
			case RE::BSKeyboardDevice::Keys::kDown:
				return "Down";
			case RE::BSKeyboardDevice::Keys::kLeft:
				return "Left";
			case RE::BSKeyboardDevice::Keys::kRight:
				return "Right";
			default:
				break;
			}

			return "Input " + std::to_string(a_keyCode);
		}

		[[nodiscard]] bool ShouldThrottleSettingsRefresh(
			PrismaSettingsRefreshState& a_state,
			std::chrono::steady_clock::time_point a_now,
			bool a_force)
		{
			if (!a_force &&
				a_state.nextRefreshAt.time_since_epoch().count() != 0 &&
				a_now < a_state.nextRefreshAt) {
				return true;
			}

			a_state.nextRefreshAt = a_now + kHotkeyRefreshInterval;
			return false;
		}

		void ClearSettingsRefreshSource(PrismaSettingsRefreshState& a_state)
		{
			a_state.hasSourceSnapshot = false;
			a_state.sourcePath.clear();
		}

		[[nodiscard]] bool IsKnownSettingsRefreshSource(
			const PrismaSettingsRefreshState& a_state,
			const std::filesystem::path& a_path,
			std::filesystem::file_time_type a_mtime) noexcept
		{
			return a_state.hasSourceSnapshot &&
				a_path == a_state.sourcePath &&
				a_mtime == a_state.sourceMtime;
		}

		void RememberSettingsRefreshSource(
			PrismaSettingsRefreshState& a_state,
			const std::filesystem::path& a_path,
			std::filesystem::file_time_type a_mtime)
		{
			a_state.hasSourceSnapshot = true;
			a_state.sourcePath = a_path;
			a_state.sourceMtime = a_mtime;
		}

		[[nodiscard]] std::string_view TrimAscii(std::string_view a_text) noexcept
		{
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.front()))) {
				a_text.remove_prefix(1);
			}
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.back()))) {
				a_text.remove_suffix(1);
			}
			return a_text;
		}

		[[nodiscard]] std::string ToLowerAscii(std::string_view a_text)
		{
			std::string lower;
			lower.reserve(a_text.size());
			for (char ch : a_text) {
				lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
			}
			return lower;
		}

		[[nodiscard]] int NormalizeUiLanguageMode(int a_value) noexcept
		{
			switch (a_value) {
			case 0:
			case 1:
			case 2:
				return a_value;
			default:
				return kDefaultUiLanguageMode;
			}
		}

		[[nodiscard]] int ParseUiLanguageMode(std::string_view a_value) noexcept
		{
			const auto trimmed = TrimAscii(a_value);
			if (trimmed.empty()) {
				return kDefaultUiLanguageMode;
			}

			const auto lower = ToLowerAscii(trimmed);

			int parsedInt = 0;
			const auto* begin = lower.data();
			const auto* end = begin + lower.size();
			const auto [ptr, ec] = std::from_chars(begin, end, parsedInt);
			if (ec == std::errc{} && ptr == end) {
				return NormalizeUiLanguageMode(parsedInt);
			}

			if (lower == "en" || lower == "english") {
				return 0;
			}
			if (lower == "ko" || lower == "korean" || lower == "kr") {
				return 1;
			}
			if (lower == "both" || lower == "bilingual" || lower == "en+ko" || lower == "ko+en") {
				return 2;
			}

			return kDefaultUiLanguageMode;
		}

		[[nodiscard]] const char* ResolveUiLanguageCode(int a_mode) noexcept
		{
			switch (NormalizeUiLanguageMode(a_mode)) {
			case 0:
				return "en";
			case 1:
				return "ko";
			default:
				return "both";
			}
		}

		[[nodiscard]] bool TryReadPanelHotkeyFromMcmJson(std::istream& a_in, std::uint32_t& a_outResolvedKey)
		{
			a_outResolvedKey = 0u;

			nlohmann::json j;
			a_in >> j;

			if (const auto keybindsIt = j.find("keybinds");
				keybindsIt != j.end() && keybindsIt->is_array()) {
				for (const auto& entry : *keybindsIt) {
					if (!entry.is_object()) {
						continue;
					}

					const std::string modName = entry.value("modName", std::string{});
					const std::string id = entry.value("id", std::string{});
					if (modName != kMcmModName || id != kPanelToggleKeybindId) {
						continue;
					}

					if (const auto keyIt = entry.find("keycode");
						keyIt != entry.end() && keyIt->is_number_integer()) {
						const int keyCode = keyIt->get<int>();
						if (keyCode > 0) {
							a_outResolvedKey = static_cast<std::uint32_t>(keyCode);
						}
					}
					return true;
				}
			}

			return false;
		}

		[[nodiscard]] bool TryResolveUiLanguageSettingsPath(std::filesystem::path& a_outPath)
		{
			std::error_code ec;
			const std::filesystem::path settingsPath{ std::string(kMcmModSettingsPath) };
			const std::filesystem::path defaultSettingsPath{ std::string(kMcmModDefaultSettingsPath) };

			a_outPath.clear();
			if (std::filesystem::exists(settingsPath, ec) && !ec) {
				a_outPath = settingsPath;
				return true;
			}

			ec.clear();
			if (std::filesystem::exists(defaultSettingsPath, ec) && !ec) {
				a_outPath = defaultSettingsPath;
				return true;
			}

			return false;
		}

		[[nodiscard]] bool TryReadUiLanguageModeFromIni(std::istream& a_in, int& a_outResolvedMode)
		{
			a_outResolvedMode = kDefaultUiLanguageMode;

			std::string sectionLower;
			std::string line;
			while (std::getline(a_in, line)) {
				std::string_view text(line);
				if (text.size() >= 3 &&
					static_cast<unsigned char>(text[0]) == 0xEF &&
					static_cast<unsigned char>(text[1]) == 0xBB &&
					static_cast<unsigned char>(text[2]) == 0xBF) {
					text.remove_prefix(3);
				}

				if (const auto comment = text.find_first_of(";#"); comment != std::string_view::npos) {
					text = text.substr(0, comment);
				}

				text = TrimAscii(text);
				if (text.empty()) {
					continue;
				}

				if (text.size() >= 2 && text.front() == '[' && text.back() == ']') {
					sectionLower = ToLowerAscii(TrimAscii(text.substr(1, text.size() - 2)));
					continue;
				}

				if (sectionLower != kMcmGeneralSectionLower) {
					continue;
				}

				const auto sep = text.find('=');
				if (sep == std::string_view::npos) {
					continue;
				}

				const auto keyLower = ToLowerAscii(TrimAscii(text.substr(0, sep)));
				if (keyLower != kMcmUiLanguageKeyLower) {
					continue;
				}

				a_outResolvedMode = ParseUiLanguageMode(text.substr(sep + 1));
				return true;
			}

			return false;
		}

		template <class LayoutT>
		void MarkLayoutStateLoadedForPush(PrismaLayoutCacheState<LayoutT>& a_state, const LayoutT& a_loaded)
		{
			a_state.value = a_loaded;
			a_state.needsPush = true;
			a_state.lastJson.clear();
		}

		[[nodiscard]] std::optional<PanelLayout> TryLoadPanelLayoutStateFromDisk()
		{
			return PrismaLayoutPersistence::LoadPanelLayoutFile(kPanelLayoutPath);
		}

		[[nodiscard]] std::optional<TooltipLayout> TryLoadTooltipLayoutStateFromDisk()
		{
			return PrismaLayoutPersistence::LoadTooltipLayoutFile(
				kTooltipLayoutPath,
				kDefaultTooltipRight,
				kDefaultTooltipTop,
				kDefaultTooltipFontPermille,
				kMinTooltipFontPermille,
				kMaxTooltipFontPermille);
		}

		template <class LayoutT>
		void PushLayoutStateJsonToJs(
			PrismaLayoutCacheState<LayoutT>& a_state,
			std::string a_payload,
			const char* a_method)
		{
			if (a_payload == a_state.lastJson) {
				return;
			}

			a_state.lastJson = std::move(a_payload);
			g_api->InteropCall(g_view, a_method, a_state.lastJson.c_str());
		}

		[[nodiscard]] std::optional<nlohmann::json> LoadUserUiSettingsSection()
		{
			nlohmann::json root;
			if (!UserSettingsPersistence::LoadJsonObject(kUserSettingsPath, root) || !root.is_object()) {
				return std::nullopt;
			}

			const auto uiIt = root.find("ui");
			if (uiIt == root.end() || !uiIt->is_object()) {
				return std::nullopt;
			}
			return *uiIt;
		}

		[[nodiscard]] std::optional<std::uint32_t> LoadPanelHotkeyFromUserSettings()
		{
			const auto ui = LoadUserUiSettingsSection();
			if (!ui) {
				return std::nullopt;
			}

			const auto hotkeyIt = ui->find("panelHotkey");
			if (hotkeyIt == ui->end()) {
				return std::nullopt;
			}

			if (hotkeyIt->is_number_unsigned()) {
				const auto rawKeyCode = hotkeyIt->get<std::uint64_t>();
				const auto maxKeyCode = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());
				if (rawKeyCode <= maxKeyCode) {
					return static_cast<std::uint32_t>(rawKeyCode);
				}
				return std::nullopt;
			}
			if (hotkeyIt->is_number_integer()) {
				const int keyCode = hotkeyIt->get<int>();
				if (keyCode >= 0) {
					return static_cast<std::uint32_t>(keyCode);
				}
			}

			return std::nullopt;
		}

		void PersistPanelHotkeyToUserSettings(std::uint32_t a_keyCode)
		{
			const bool ok = UserSettingsPersistence::UpdateJsonObject(
				kUserSettingsPath,
				[&](nlohmann::json& root) {
					root["version"] = 1;
					auto& ui = root["ui"];
					if (!ui.is_object()) {
						ui = nlohmann::json::object();
					}
					ui["panelHotkey"] = a_keyCode;
				});

			if (!ok) {
				SKSE::log::warn("CalamityAffixes: failed to persist panel hotkey to {}.", kUserSettingsPath);
			}
		}

		void RestorePanelHotkeyFromUserSettings()
		{
			const auto persistedHotkey = LoadPanelHotkeyFromUserSettings();
			const std::uint32_t resolvedKey = persistedHotkey.value_or(0u);
			const auto previous = g_controlPanelHotkey.exchange(resolvedKey);
			if (previous != resolvedKey && persistedHotkey.has_value()) {
				SKSE::log::info("CalamityAffixes: Prisma panel hotkey restored from {} (keycode={}).", kUserSettingsPath, resolvedKey);
			}
		}

		[[nodiscard]] std::optional<int> LoadUiLanguageModeFromUserSettings()
		{
			const auto ui = LoadUserUiSettingsSection();
			if (!ui) {
				return std::nullopt;
			}

			const auto languageIt = ui->find("uiLanguageMode");
			if (languageIt == ui->end()) {
				return std::nullopt;
			}
			if (languageIt->is_number_integer()) {
				return NormalizeUiLanguageMode(languageIt->get<int>());
			}
			if (languageIt->is_string()) {
				return ParseUiLanguageMode(languageIt->get<std::string>());
			}

			return std::nullopt;
		}

		void PersistUiLanguageModeToUserSettings(int a_mode)
		{
			const int normalizedMode = NormalizeUiLanguageMode(a_mode);
			const bool ok = UserSettingsPersistence::UpdateJsonObject(
				kUserSettingsPath,
				[&](nlohmann::json& root) {
					root["version"] = 1;
					auto& ui = root["ui"];
					if (!ui.is_object()) {
						ui = nlohmann::json::object();
					}
					ui["uiLanguageMode"] = normalizedMode;
				});

			if (!ok) {
				SKSE::log::warn("CalamityAffixes: failed to persist UI language mode to {}.", kUserSettingsPath);
			}
		}

		void RestoreUiLanguageModeFromUserSettings()
		{
			const auto persistedMode = LoadUiLanguageModeFromUserSettings();
			const int resolvedMode = NormalizeUiLanguageMode(persistedMode.value_or(kDefaultUiLanguageMode));
			const auto previous = g_uiLanguageMode.exchange(resolvedMode);
			if (previous != resolvedMode && persistedMode.has_value()) {
				SKSE::log::info("CalamityAffixes: UI language restored from {} (mode={}).", kUserSettingsPath, resolvedMode);
			}
		}

		void PushPanelHotkeyToJs()
		{
			if (!IsViewReady()) {
				return;
			}

			auto keyCode = g_controlPanelHotkey.load();
			if (keyCode == 0u) {
				keyCode = kFallbackPanelHotkey;
			}

			const auto text = DescribeHotkey(keyCode);
			if (text == g_viewCache.panelHotkeyText) {
				return;
			}

			g_viewCache.panelHotkeyText = text;
			g_api->InteropCall(g_view, kInteropSetPanelHotkeyText, g_viewCache.panelHotkeyText.c_str());
		}

		void PushUiLanguageToJs()
		{
			if (!IsViewReady()) {
				return;
			}

			const std::string mode = ResolveUiLanguageCode(g_uiLanguageMode.load());
			if (mode == g_viewCache.uiLanguageCode) {
				return;
			}

			g_viewCache.uiLanguageCode = mode;
			g_api->InteropCall(g_view, kInteropSetUiLanguage, g_viewCache.uiLanguageCode.c_str());
		}

		void RefreshControlPanelHotkeyFromMcm(bool a_force)
		{
			const auto now = std::chrono::steady_clock::now();
			std::scoped_lock lk{ g_panelHotkeyRefresh.lock };

			if (ShouldThrottleSettingsRefresh(g_panelHotkeyRefresh, now, a_force)) {
				return;
			}

			const std::filesystem::path settingsPath{ std::string(kMcmKeybindSettingsPath) };
			std::error_code ec;
			const bool exists = std::filesystem::exists(settingsPath, ec);
			if (ec || !exists) {
				ClearSettingsRefreshSource(g_panelHotkeyRefresh);
				RestorePanelHotkeyFromUserSettings();
				return;
			}

			const auto mtime = std::filesystem::last_write_time(settingsPath, ec);
			if (ec) {
				return;
			}
			if (!a_force && IsKnownSettingsRefreshSource(g_panelHotkeyRefresh, settingsPath, mtime)) {
				return;
			}

			std::ifstream in(settingsPath);
			if (!in.good()) {
				return;
			}

			std::uint32_t resolvedKey = 0u;
			try {
				(void)TryReadPanelHotkeyFromMcmJson(in, resolvedKey);
			} catch (const std::exception& e) {
				SKSE::log::warn("CalamityAffixes: failed to parse {} ({})",
					kMcmKeybindSettingsPath,
					e.what());
				RestorePanelHotkeyFromUserSettings();
				return;
			}

			const auto previous = g_controlPanelHotkey.exchange(resolvedKey);
			RememberSettingsRefreshSource(g_panelHotkeyRefresh, settingsPath, mtime);

			if (previous != resolvedKey) {
				PersistPanelHotkeyToUserSettings(resolvedKey);
				SKSE::log::info("CalamityAffixes: Prisma panel hotkey updated (keycode={}).", resolvedKey);
			}
		}

		void RefreshUiLanguageFromMcm(bool a_force)
		{
			const auto now = std::chrono::steady_clock::now();
			std::scoped_lock lk{ g_uiLanguageRefresh.lock };

			if (ShouldThrottleSettingsRefresh(g_uiLanguageRefresh, now, a_force)) {
				return;
			}

			std::filesystem::path activePath;
			(void)TryResolveUiLanguageSettingsPath(activePath);
			if (activePath.empty()) {
				RestoreUiLanguageModeFromUserSettings();
				ClearSettingsRefreshSource(g_uiLanguageRefresh);
				return;
			}

			std::error_code ec;
			const auto mtime = std::filesystem::last_write_time(activePath, ec);
			if (ec) {
				return;
			}

			if (!a_force && IsKnownSettingsRefreshSource(g_uiLanguageRefresh, activePath, mtime)) {
				return;
			}

			std::ifstream in(activePath);
			if (!in.good()) {
				return;
			}

			int resolvedMode = kDefaultUiLanguageMode;
			(void)TryReadUiLanguageModeFromIni(in, resolvedMode);

			resolvedMode = NormalizeUiLanguageMode(resolvedMode);
			const auto previous = g_uiLanguageMode.exchange(resolvedMode);
			RememberSettingsRefreshSource(g_uiLanguageRefresh, activePath, mtime);

			if (previous != resolvedMode) {
				PersistUiLanguageModeToUserSettings(resolvedMode);
				SKSE::log::info("CalamityAffixes: UI language updated (mode={}).", resolvedMode);
			}
		}

		void LoadPanelLayoutFromDisk()
		{
			if (g_panelLayoutState.loaded) {
				return;
			}
			g_panelLayoutState.loaded = true;

			const std::filesystem::path path{ std::string(kPanelLayoutPath) };
			std::error_code ec;
			if (!std::filesystem::exists(path, ec) || ec) {
				return;
			}

			try {
				const auto loaded = TryLoadPanelLayoutStateFromDisk();
				if (!loaded) {
					SKSE::log::warn("CalamityAffixes: invalid Prisma panel layout file at {}.", kPanelLayoutPath);
					return;
				}

				MarkLayoutStateLoadedForPush(g_panelLayoutState, *loaded);
				SKSE::log::info("CalamityAffixes: loaded Prisma panel layout from {}.", kPanelLayoutPath);
			} catch (const std::exception& e) {
				SKSE::log::warn("CalamityAffixes: failed to parse panel layout {} ({})", kPanelLayoutPath, e.what());
			}
		}

		void LoadTooltipLayoutFromDisk()
		{
			if (g_tooltipLayoutState.loaded) {
				return;
			}
			g_tooltipLayoutState.loaded = true;

			const std::filesystem::path path{ std::string(kTooltipLayoutPath) };
			std::error_code ec;
			if (!std::filesystem::exists(path, ec) || ec) {
				return;
			}

			try {
				const auto loaded = TryLoadTooltipLayoutStateFromDisk();
				if (!loaded) {
					SKSE::log::warn("CalamityAffixes: invalid Prisma tooltip layout file at {}.", kTooltipLayoutPath);
					return;
				}

				MarkLayoutStateLoadedForPush(g_tooltipLayoutState, *loaded);
				SKSE::log::info("CalamityAffixes: loaded Prisma tooltip layout from {}.", kTooltipLayoutPath);
			} catch (const std::exception& e) {
				SKSE::log::warn("CalamityAffixes: failed to parse tooltip layout {} ({})", kTooltipLayoutPath, e.what());
			}
		}

		void SavePanelLayoutToDisk(const PanelLayout& a_layout)
		{
			if (!a_layout.valid) {
				return;
			}

			if (!PrismaLayoutPersistence::SavePanelLayoutFile(kPanelLayoutPath, a_layout)) {
				SKSE::log::warn("CalamityAffixes: failed to save panel layout file {}.", kPanelLayoutPath);
				return;
			}

			g_panelLayoutState.lastJson = PrismaLayoutPersistence::EncodePanelLayoutJson(a_layout);
		}

		void SaveTooltipLayoutToDisk(const TooltipLayout& a_layout)
		{
			if (!a_layout.valid) {
				return;
			}

			if (!PrismaLayoutPersistence::SaveTooltipLayoutFile(kTooltipLayoutPath, a_layout)) {
				SKSE::log::warn("CalamityAffixes: failed to save tooltip layout file {}.", kTooltipLayoutPath);
				return;
			}

			g_tooltipLayoutState.lastJson = PrismaLayoutPersistence::EncodeTooltipLayoutJsonForDisk(a_layout);
		}

		void PushPanelLayoutToJs()
		{
			if (!IsViewReady() || !g_panelLayoutState.value.valid) {
				return;
			}

			PushLayoutStateJsonToJs(
				g_panelLayoutState,
				PrismaLayoutPersistence::EncodePanelLayoutJson(g_panelLayoutState.value),
				kInteropSetPanelLayout);
		}

		void PushTooltipLayoutToJs()
		{
			if (!IsViewReady() || !g_tooltipLayoutState.value.valid) {
				return;
			}

			PushLayoutStateJsonToJs(
				g_tooltipLayoutState,
				PrismaLayoutPersistence::EncodeTooltipLayoutJsonForJs(g_tooltipLayoutState.value),
				kInteropSetTooltipLayout);
		}
