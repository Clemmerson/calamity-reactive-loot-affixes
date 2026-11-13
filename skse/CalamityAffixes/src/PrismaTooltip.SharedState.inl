		constexpr auto kPollInterval = std::chrono::milliseconds(300);
		constexpr auto kHotkeyRefreshInterval = std::chrono::seconds(2);
		constexpr auto kPanelSafetyRefreshInterval = std::chrono::seconds(2);
		constexpr auto kSlowTickThreshold = std::chrono::milliseconds(8);
		constexpr auto kSlowTickLogInterval = std::chrono::seconds(30);
		constexpr auto kPrismaRetryInterval = std::chrono::seconds(5);
		constexpr auto kPrismaDomReadyTimeout = std::chrono::seconds(15);
		constexpr auto kViewPath = "CalamityAffixes/index.html";
		constexpr auto kViewOrder = 5000;
		constexpr auto kMouseButtonOffset = 256u;
		constexpr std::uint32_t kFallbackPanelHotkey = RE::BSKeyboardDevice::Keys::kF11;
		// The catalog bypass is available only in Debug builds for development.
#if !defined(NDEBUG)
		constexpr std::uint32_t kRuneforgeDebugHotkey = RE::BSKeyboardDevice::Keys::kF10;
#endif
		constexpr int kDefaultUiLanguageMode = 0;  // 0=en, 1=ko, 2=both
		constexpr auto kPrismaCommandListener = "calamityCommand";
		constexpr auto kInteropSetTooltip = "setTooltip";
		constexpr auto kInteropSetControlPanel = "setControlPanel";
		constexpr auto kInteropSetActionFeedback = "setActionFeedback";
		constexpr auto kInteropSetPanelHotkeyText = "setPanelHotkeyText";
		constexpr auto kInteropSetUiLanguage = "setUiLanguage";
		constexpr auto kInteropSetSelectedItemName = "setSelectedItemName";
		constexpr auto kInteropSetSelectedItemSource = "setSelectedItemSource";
		constexpr auto kInteropSetInventoryItems = "setInventoryItems";
		constexpr auto kInteropSetRecipeItems = "setRecipeItems";
		constexpr auto kInteropSetRunewordPanelState = "setRunewordPanelState";
		constexpr auto kInteropSetRunewordAffixPreview = "setRunewordAffixPreview";
		constexpr auto kInteropSetPanelLayout = "setPanelLayout";
		constexpr auto kInteropSetTooltipLayout = "setTooltipLayout";
		constexpr auto kInteropSetRuneforgeReader = "setRuneforgeReader";
		constexpr auto kInteropSetRuneforgeReaderPage = "setRuneforgeReaderPage";
		constexpr auto kInteropSetRuneforgeReaderVisible = "setRuneforgeReaderVisible";

		constexpr std::string_view kMenuInventory = RE::InventoryMenu::MENU_NAME;
		constexpr std::string_view kMenuBarter = RE::BarterMenu::MENU_NAME;
		constexpr std::string_view kMenuContainer = RE::ContainerMenu::MENU_NAME;
		constexpr std::string_view kMenuGift = RE::GiftMenu::MENU_NAME;
		constexpr std::string_view kMcmKeybindSettingsPath = "Data/MCM/Settings/keybinds.json";
		constexpr std::string_view kMcmModSettingsPath = "Data/MCM/Settings/CalamityAffixes.ini";
		constexpr std::string_view kMcmModDefaultSettingsPath = "Data/MCM/Config/CalamityAffixes/settings.ini";
		constexpr std::string_view kMcmModName = "CalamityAffixes";
		constexpr std::string_view kPanelToggleKeybindId = "prisma_panel_toggle";
		constexpr std::string_view kMcmGeneralSectionLower = "general";
		constexpr std::string_view kMcmUiLanguageKeyLower = "iuilanguage";
		constexpr std::string_view kRunewordBaseSelectPrefix = "runeword.base.select:";
		constexpr std::string_view kRunewordRecipeSelectPrefix = "runeword.recipe.select:";
		constexpr std::string_view kAffixExpandPrefix = "affix.expand:";
		constexpr std::string_view kPanelLayoutSavePrefix = "ui.layout.save:";
		constexpr std::string_view kTooltipLayoutSavePrefix = "ui.tooltip.save:";
		constexpr std::string_view kItemSourceInventory = "inventory";
		constexpr std::string_view kItemSourceBarter = "barter";
		constexpr std::string_view kItemSourceContainer = "container";
		constexpr std::string_view kItemSourceGift = "gift";
		constexpr std::string_view kPanelLayoutPath = "Data/SKSE/Plugins/CalamityAffixes/prisma_panel_layout.json";
		constexpr std::string_view kTooltipLayoutPath = "Data/SKSE/Plugins/CalamityAffixes/prisma_tooltip_layout.json";
		constexpr std::string_view kUserSettingsPath = "Data/SKSE/Plugins/CalamityAffixes/user_settings.json";
		constexpr int kDefaultTooltipRight = 70;
		constexpr int kDefaultTooltipTop = 255;
		constexpr int kDefaultTooltipFontPermille = 1000;
		constexpr int kMinTooltipFontPermille = 700;
		constexpr int kMaxTooltipFontPermille = 1800;

		std::atomic_bool g_installed{ false };
		std::atomic_bool g_tickTaskPending{ false };
		enum class PrismaAvailabilityStatus : std::uint8_t
		{
			kNotChecked,
			kApiUnavailable,
			kViewLoading,
			kReady,
			kViewFailed,
			kDomReadyTimedOut
		};

		std::atomic<PrismaAvailabilityStatus> g_prismaStatus{ PrismaAvailabilityStatus::kNotChecked };
		std::atomic<PRISMA_UI_API::PrismaView> g_activePrismaView{ 0 };
		std::atomic<PRISMA_UI_API::PrismaView> g_domReadyView{ 0 };
		std::mutex g_prismaViewReadinessMutex;
		std::array<PRISMA_UI_API::PrismaView, 8> g_pendingDomReadyViews{};
		std::atomic_bool g_loggedMissingApi{ false };
		std::atomic_bool g_loggedViewFailure{ false };
		std::atomic_bool g_loggedDomReadyTimeout{ false };
		std::atomic_bool g_userFailureNotificationShown{ false };
		std::atomic_int g_relevantMenusOpen{ 0 };
		std::atomic_bool g_gatePollingOnMenus{ false };
		std::atomic_bool g_controlPanelOpen{ false };
		std::atomic_bool g_runeforgeReaderOpen{ false };
		struct RuneforgeReaderDocument
		{
			std::string title;
			std::string substrate;
			std::vector<std::string> pages;
		};
		struct RuneforgeReaderDraft
		{
			RuneforgeReaderDocument document;
			std::size_t expectedPages{};
			bool active{};
		};
		RuneforgeReaderDocument g_runeforgeReaderDocument{};
		RuneforgeReaderDraft g_runeforgeReaderDraft{};
		std::mutex g_runeforgeReaderDraftMutex;
		std::size_t g_runeforgeReaderPage{};
		bool g_runeforgeReaderNeedsPush{};
		// R8.7.4.14 reader input/exit contract (RuneforgeReaderInputContract.h). The reader state above is touched from the
		// task thread (open, tick), the input-device sink (keyboard/gamepad, mailbox drain) and the menu sink, so every
		// transition holds this mutex. JS commands never take it: they only post to the mailbox.
		std::recursive_mutex g_runeforgeReaderMutex;
		bool g_runeforgeReaderFocusConfirmed{};
		unsigned g_runeforgeReaderFocusRequests{};
		bool g_runeforgeReaderAckSeen{};
		bool g_runeforgeReaderAckWarned{};
		std::chrono::steady_clock::time_point g_runeforgeReaderPushedAt{};
		std::chrono::steady_clock::time_point g_runeforgeReaderNextServiceAt{};
		std::mutex g_runeforgeReaderMailboxMutex;
		std::vector<RuneforgeReaderContract::Action> g_runeforgeReaderMailbox;
		std::atomic_bool g_runeforgeReaderAckPending{ false };
		std::atomic_uint32_t g_runeforgeReaderPointerReports{ 0u };
		std::uint8_t g_runeforgeReaderCloseKeysArmed{};
		std::atomic_uint32_t g_controlPanelHotkey{ 0u };
		std::atomic_int g_uiLanguageMode{ kDefaultUiLanguageMode };

		PRISMA_UI_API::IVPrismaUI1* g_api = nullptr;
		PRISMA_UI_API::PrismaView g_view = 0;
		std::chrono::steady_clock::time_point g_nextPrismaAttemptAt{};
		std::chrono::steady_clock::time_point g_prismaViewCreatedAt{};

		std::string g_lastTooltip;
		std::jthread g_worker;

		bool g_lastPanelStateKnown = false;
		bool g_lastPanelState = false;
		bool g_runewordPanelDynamicDirty = true;
		bool g_runewordRecipeListDirty = true;
		std::chrono::steady_clock::time_point g_nextPanelSafetyRefreshAt{};
		std::chrono::steady_clock::time_point g_nextSlowTickLogAt{};

		using PanelLayout = PrismaLayoutPersistence::PanelLayout;
		using TooltipLayout = PrismaLayoutPersistence::TooltipLayout;

		struct PrismaViewCacheState
		{
			std::string selectedItemName{};
			std::string selectedItemSource{};
			std::string panelHotkeyText{};
			std::string uiLanguageCode{};
		};

		struct PrismaRunewordCacheState
		{
			std::string baseListJson = "[]";
			std::string recipeListJson = "[]";
			std::string panelStateJson = "{}";
			std::string affixPreview{};
		};

		template <class LayoutT>
		struct PrismaLayoutCacheState
		{
			LayoutT value{};
			bool loaded = false;
			bool needsPush = false;
			std::string lastJson = "{}";
		};

		PrismaViewCacheState g_viewCache{};
		PrismaRunewordCacheState g_runewordCache{};
		PrismaLayoutCacheState<PanelLayout> g_panelLayoutState{};
		PrismaLayoutCacheState<TooltipLayout> g_tooltipLayoutState{};

		struct PrismaSettingsRefreshState
		{
			std::mutex lock;
			std::chrono::steady_clock::time_point nextRefreshAt{};
			std::filesystem::file_time_type sourceMtime{};
			std::filesystem::path sourcePath{};
			bool hasSourceSnapshot = false;
		};

		struct SelectedItemContext;

		PrismaSettingsRefreshState g_panelHotkeyRefresh{};
		PrismaSettingsRefreshState g_uiLanguageRefresh{};

		struct PrismaTelemetryCounters
		{
			std::atomic<std::uint64_t> workerEnqueues{ 0u };
			std::atomic<std::uint64_t> tickRuns{ 0u };
			std::atomic<std::uint64_t> tooltipClears{ 0u };
			std::atomic<std::uint64_t> tooltipPushes{ 0u };
			std::atomic<std::uint64_t> selectedContextRefreshes{ 0u };
			std::atomic<std::uint64_t> panelRefreshes{ 0u };
			std::atomic<std::uint64_t> recipeRefreshes{ 0u };
			std::atomic<std::uint64_t> coalescedTicks{ 0u };
			std::atomic<std::uint64_t> slowTicks{ 0u };
			std::atomic<std::uint64_t> uiCommands{ 0u };
		};

		PrismaTelemetryCounters g_prismaTelemetry{};
		std::chrono::steady_clock::time_point g_nextTelemetryLogAt{};
