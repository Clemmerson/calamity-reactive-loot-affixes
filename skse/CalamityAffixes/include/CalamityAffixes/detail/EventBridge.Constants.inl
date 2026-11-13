#if !defined(CALAMITYAFFIXES_EVENTBRIDGE_CONSTANTS_INL_CONTEXT)
#error "Do not include EventBridge.Constants.inl directly; include CalamityAffixes/EventBridge.h"
#endif

		// Config/event string constants and runtime guards.
		static constexpr std::string_view kDotKeywordEditorID = "CAFF_TAG_DOT";
		static constexpr std::chrono::milliseconds kDotApplyICD{ 1500 };
		static constexpr std::chrono::milliseconds kCastOnCritICD{ 150 };
		static constexpr std::chrono::milliseconds kDuplicateHitWindow{ 100 };
		static constexpr std::chrono::milliseconds kPapyrusHitEventWindow{ 80 };
		static constexpr std::chrono::milliseconds kHealthDamageHookStaleWindow{ 5000 };
		static constexpr std::string_view kRuntimeConfigRelativePath = RuntimePolicy::kRuntimeConfigRelativePath;
		static constexpr std::string_view kRuntimeContractRelativePath = RuntimePolicy::kRuntimeContractRelativePath;
		static constexpr std::string_view kUserSettingsRelativePath = RuntimePolicy::kUserSettingsRelativePath;
		static constexpr std::string_view kManualModeCycleNextEvent = EventNames::kManualModeCycleNext;
		static constexpr std::string_view kManualModeCyclePrevEvent = EventNames::kManualModeCyclePrev;
		static constexpr std::string_view kRunewordBaseNextEvent = EventNames::kRunewordBaseNext;
		static constexpr std::string_view kRunewordBasePrevEvent = EventNames::kRunewordBasePrev;
		static constexpr std::string_view kRunewordRecipeNextEvent = EventNames::kRunewordRecipeNext;
		static constexpr std::string_view kRunewordRecipePrevEvent = EventNames::kRunewordRecipePrev;
		static constexpr std::string_view kRunewordInsertRuneEvent = EventNames::kRunewordInsertRune;
		static constexpr std::string_view kRunewordStatusEvent = EventNames::kRunewordStatus;
		static constexpr std::string_view kRunewordGrantNextRuneEvent = EventNames::kRunewordGrantNextRune;
		static constexpr std::string_view kRunewordGrantRecipeSetEvent = EventNames::kRunewordGrantRecipeSet;
		static constexpr std::string_view kRunewordGrantStarterOrbsEvent = EventNames::kRunewordGrantStarterOrbs;
		static constexpr std::string_view kUiSetPanelEvent = EventNames::kUiSetPanel;
		static constexpr std::string_view kUiTogglePanelEvent = EventNames::kUiTogglePanel;
		static constexpr std::string_view kMcmGrantTrapAffixEvent = EventNames::kMcmGrantTrapAffix;
		static constexpr std::string_view kMcmTrapMarkerProbeEvent = EventNames::kMcmTrapMarkerProbe;
		static constexpr std::string_view kUiStatusEvent = EventNames::kUiStatus;
		static constexpr std::string_view kMcmSetEnabledEvent = RuntimePolicy::kMcmSetEnabledEvent;
		static constexpr std::string_view kLegacyMcmSetDebugNotificationsEvent = RuntimePolicy::kLegacyMcmSetDebugNotificationsEvent;
		static constexpr std::string_view kMcmSetDebugHudNotificationsEvent = RuntimePolicy::kMcmSetDebugHudNotificationsEvent;
		static constexpr std::string_view kMcmSetDebugVerboseLoggingEvent = RuntimePolicy::kMcmSetDebugVerboseLoggingEvent;
		static constexpr std::string_view kMcmSetValidationIntervalEvent = RuntimePolicy::kMcmSetValidationIntervalEvent;
		static constexpr std::string_view kMcmSetProcChanceMultEvent = RuntimePolicy::kMcmSetProcChanceMultEvent;
		static constexpr std::string_view kMcmSetRunewordFragmentChanceEvent = RuntimePolicy::kMcmSetRunewordFragmentChanceEvent;
		static constexpr std::string_view kMcmSetReforgeOrbChanceEvent = RuntimePolicy::kMcmSetReforgeOrbChanceEvent;
		static constexpr std::string_view kMcmSetIdentifyScrollChanceEvent = RuntimePolicy::kMcmSetIdentifyScrollChanceEvent;
		static constexpr std::string_view kMcmSetScouringOrbChanceEvent = RuntimePolicy::kMcmSetScouringOrbChanceEvent;
		static constexpr std::string_view kMcmSetDotSafetyAutoDisableEvent = RuntimePolicy::kMcmSetDotSafetyAutoDisableEvent;
		static constexpr std::string_view kMcmSetAllowNonHostileFirstHitProcEvent = RuntimePolicy::kMcmSetAllowNonHostileFirstHitProcEvent;
		static constexpr std::string_view kMcmSpawnTestItemEvent = EventNames::kMcmSpawnTestItem;
		static constexpr std::string_view kMcmForceRebuildEvent = EventNames::kMcmForceRebuild;
		static constexpr std::string_view kMcmGrantRecoveryPackEvent = EventNames::kMcmGrantRecoveryPack;
		static constexpr std::string_view kMcmRecoverMiscCurrencyEvent = EventNames::kMcmRecoverMiscCurrency;

		// Policy constants.
		// kAffixCountWeights now lives in CalamityAffixes::detail (LootRollSelection.h)
		// so the preview path, the live roll, and the runtime gate share one array.
		static constexpr std::uint32_t kLootChancePityFailThreshold = 3u;
		static constexpr std::uint32_t kRunewordFragmentPityFailThreshold = 99u;
		static constexpr std::uint32_t kReforgeOrbPityFailThreshold = 39u;
		static constexpr std::size_t kLootEvaluatedRecentKeep = 2048;
		static constexpr std::size_t kLootEvaluatedPruneEveryInserts = 128;
		static constexpr std::size_t kLootPreviewCacheMax = 1024;
		static constexpr std::size_t kLootCurrencyLedgerMaxEntries = 8192;
		static constexpr std::uint32_t kLootCurrencyLedgerTtlDays = 30u;

		// Serialization constants.
		static constexpr std::uint32_t kSerializationVersion = 7;
		static constexpr std::uint32_t kSerializationVersionV6 = 6;
		static constexpr std::uint32_t kSerializationVersionV5 = 5;
		static constexpr std::uint32_t kSerializationVersionV4 = 4;
		static constexpr std::uint32_t kSerializationVersionV3 = 3;
		static constexpr std::uint32_t kSerializationVersionV2 = 2;
		static constexpr std::uint32_t kSerializationVersionV1 = 1;
		static constexpr std::uint32_t kSerializationRecordInstanceAffixes = 'IAXF';
		static constexpr std::uint32_t kSerializationRecordInstanceRuntimeStates = 'IRST';
		static constexpr std::uint32_t kSerializationRecordRunewordState = 'RWRD';
		static constexpr std::uint32_t kSerializationRecordLootEvaluated = 'LRLD';
		static constexpr std::uint32_t kSerializationRecordLootCurrencyLedger = 'LCLD';
		static constexpr std::uint32_t kSerializationRecordLootShuffleBags = 'LSBG';
		static constexpr std::uint32_t kSerializationRecordMigrationFlags = 'MFLG';
		static constexpr std::uint32_t kSerializationRecordInstanceReforgeCounts = 'IRFC';
		static constexpr std::uint32_t kSerializationRecordPendingDropOwnership = 'PDOP';
		static constexpr std::uint32_t kRunewordSerializationVersion = 1;
		static constexpr std::uint32_t kMigrationFlagsVersion = 1;
		static constexpr std::uint32_t kSerializationRecordCorpseCurrencyRuntime = 'CCRT';
		static constexpr std::uint32_t kInstanceRuntimeStateSerializationVersion = 1;
		static constexpr std::uint32_t kLootEvaluatedSerializationVersion = 1;
		static constexpr std::uint32_t kInstanceReforgeCountSerializationVersion = 1;
		static constexpr std::uint32_t kPendingDropOwnershipSerializationVersion = 3;
		static constexpr std::uint32_t kLootCurrencyLedgerSerializationVersion = 2;
		static constexpr std::uint32_t kLootCurrencyLedgerSerializationVersionV1 = 1;
		static constexpr std::uint32_t kLootShuffleBagSerializationVersion = 2;

		// Runtime debounce/resync intervals.
		static constexpr std::chrono::milliseconds kRuntimeUserSettingsPersistDebounce{ 250 };
		static constexpr std::chrono::milliseconds kEquipResyncInterval{ 8000 };
		static constexpr std::uint32_t kCorpseCurrencyRuntimeSerializationVersion = 1;
