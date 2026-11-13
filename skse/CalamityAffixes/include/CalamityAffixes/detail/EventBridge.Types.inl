#if !defined(CALAMITYAFFIXES_EVENTBRIDGE_TYPES_INL_CONTEXT)
#error "Do not include EventBridge.Types.inl directly; include CalamityAffixes/EventBridge.h"
#endif

		// Internal domain enums.
		enum class Trigger : std::uint8_t
		{
			kHit,
			kIncomingHit,
			kDotApply,
			kKill,
			kLowHealth,
		};

		enum class PlayerCombatEvidenceSource : std::uint8_t
		{
			kUnknown = 0,
			kHealthDamageRoutedHit,
			kTesHitOutgoing,
			kTesHitIncoming,
		};

		enum class ActionType : std::uint8_t
		{
			kDebugNotify,
			kCastSpell,
			kCastSpellAdaptiveElement,
			kCastOnCrit,
			kConvertDamage,
			kMindOverMatter,
			kArchmage,
			kCorpseExplosion,
			kSummonCorpseExplosion,
			kSpawnTrap,
			kEchoStrike,
		};

		enum class TrapSpawnAt : std::uint8_t
		{
			kOwnerFeet,
			kTargetFeet,
		};

		enum class LootItemType : std::uint8_t
		{
			kWeapon,
			kArmor,
		};

		enum class AffixSlot : std::uint8_t
		{
			kNone,      // runeword-only or legacy
			kPrefix,    // active proc effects
			kSuffix,    // passive stat bonuses
		};

		enum class Element : std::uint8_t
		{
			kNone,
			kFire,
			kFrost,
			kShock,
		};

		enum class ActionFeedbackTarget : std::uint8_t
		{
			kOwner,
			kTarget,
			kCorpse,
		};

		enum class ActionFeedbackPlayOn : std::uint8_t
		{
			kNone,
			kProc,
			kPassiveAdd,
		};

		struct ActionFeedback
		{
			RE::BGSArtObject* art{ nullptr };
			RE::BGSSoundDescriptorForm* sound{ nullptr };
			float durationSeconds{ 0.0f };
			ActionFeedbackTarget target{ ActionFeedbackTarget::kOwner };
			ActionFeedbackPlayOn playOn{ ActionFeedbackPlayOn::kNone };
			bool spatialSound{ false };
		};

		struct TrapFeedbackCue
		{
			RE::BGSArtObject* art{ nullptr };
			RE::BGSSoundDescriptorForm* sound{ nullptr };
			float durationSeconds{ 0.0f };
			float scale{ 1.0f };
		};

		struct TrapWorldMarkerAnimation
		{
			std::string initialEvent{};
			std::string triggerEvent{};
			std::string rearmEvent{};
			std::uint32_t openGateMilliseconds{ 0u };
		};

		struct TrapFeedback
		{
			RE::BGSArtObject* markerArt{ nullptr };
			// Optional scriptless static-family world object used when a trap needs
			// a stable, physical vanilla-shaped marker. TESObjectSTAT also accepts
			// BGSMovableStatic, while excluding scripted activators from this path.
			RE::TESObjectSTAT* markerWorldObject{ nullptr };
			std::optional<TrapWorldMarkerAnimation> worldMarkerAnimation{};
			float unarmedScale{ 0.75f };
			float armedScale{ 1.0f };
			TrapFeedbackCue placed{};
			TrapFeedbackCue armed{};
			TrapFeedbackCue triggered{};
			TrapFeedbackCue expired{};
			bool configured{ false };
		};

		enum class TrapVisualState : std::uint8_t
		{
			kNone,
			kUnarmed,
			kArmed,
		};

		enum class TrapMarkerAnimationPhase : std::uint8_t
		{
			kNone,
			kInitial,
			kTrigger,
			kRearm,
		};

		enum class TrapRemovalReason : std::uint8_t
		{
			kExpired,
			kConsumed,
			kPerAffixCap,
			kGlobalCap,
			kInvalid,
			kReset,
		};

		// Internal domain structs.
		//
		// Action — tagged union discriminated by ActionType.
		//
		// Field validity per ActionType (✓ = used, · = ignored):
		//
		//  Field group             | Notify | Cast | Adaptive | OnCrit | Convert | MoM | Archmage | Corpse | Trap | Echo |
		// -------------------------|--------|------|----------|--------|---------|-----|----------|--------|------|------|
		//  text                    |   ✓    |  ·   |    ·     |   ·    |    ·    |  ·  |    ·     |   ·    |  ·   |  ·   |
		//  spell                   |   ·    |  ✓   |    ·     |   ✓    |    ✓    |  ·  |    ✓     |   ✓    |  ✓   |  ✓   |
		//  applyToSelf             |   ·    |  ✓   |    ✓     |   ·    |    ·    |  ·  |    ·     |   ·    |  ·   |  ·   |
		//  effectiveness           |   ·    |  ✓   |    ✓     |   ✓    |    ✓    |  ·  |    ✓     |   ✓    |  ✓   |  ✓   |
		//  magnitudeOverride       |   ·    |  ✓   |    ✓     |   ✓    |    ·    |  ·  |    ·     |   ·    |  ✓   |  ·   |
		//  magnitudeScaling        |   ·    |  ✓   |    ✓     |   ✓    |    ·    |  ·  |    ·     |   ·    |  ✓   |  ✓   |
		//  noHitEffectArt          |   ·    |  ✓   |    ✓     |   ✓    |    ✓    |  ·  |    ✓     |   ✓    |  ✓   |  ✓   |
		//  adaptive{Mode,F/Fr/Sh}  |   ·    |  ·   |    ✓     |   ·    |    ·    |  ·  |    ·     |   ·    |  ·   |  ·   |
		//  element, convertPct     |   ·    |  ·   |    ·     |   ·    |    ✓    |  ·  |    ·     |   ·    |  ·   |  ·   |
		//  mindOverMatter*         |   ·    |  ·   |    ·     |   ·    |    ·    |  ✓  |    ·     |   ·    |  ·   |  ·   |
		//  archmage*               |   ·    |  ·   |    ·     |   ·    |    ·    |  ·  |    ✓     |   ·    |  ·   |  ·   |
		//  corpseExplosion*        |   ·    |  ·   |    ·     |   ·    |    ·    |  ·  |    ·     |   ✓    |  ·   |  ·   |
		//  trap*                   |   ·    |  ·   |    ·     |   ·    |    ·    |  ·  |    ·     |   ·    |  ✓   |  ·   |
		//  echo*                   |   ·    |  ·   |    ·     |   ·    |    ·    |  ·  |    ·     |   ·    |  ·   |  ✓   |
		//  evolution*              |   ·    |  ✓   |    ✓     |   ·    |    ·    |  ·  |    ·     |   ·    |  ·   |  ·   |
		//  modeCycle*              |   ·    |  ✓   |    ✓     |   ·    |    ·    |  ·  |    ·     |   ·    |  ·   |  ·   |
		//
		// "Corpse" column applies to both kCorpseExplosion and kSummonCorpseExplosion.
		//
		struct Action
		{
			ActionType type{ ActionType::kDebugNotify };
			std::uint64_t sourceToken{ 0 };
			std::string text{};
			RE::SpellItem* spell{ nullptr };
			float effectiveness{ 1.0f };
			float magnitudeOverride{ 0.0f };
			MagnitudeScaling magnitudeScaling{};
			bool noHitEffectArt{ false };
			bool debugNotify{ false };
			bool applyToSelf{ false };
			ActionFeedback feedback{};
			TrapFeedback trapFeedback{};

			// CastSpellAdaptiveElement
			AdaptiveElementMode adaptiveMode{ AdaptiveElementMode::kWeakestResist };
			RE::SpellItem* adaptiveFire{ nullptr };
			RE::SpellItem* adaptiveFrost{ nullptr };
			RE::SpellItem* adaptiveShock{ nullptr };

			// ConvertDamage
			Element element{ Element::kNone };
			float convertPct{ 0.0f };

			// MindOverMatter
			float mindOverMatterDamageToMagickaPct{ 0.0f };
			float mindOverMatterMaxRedirectPerHit{ 0.0f };

			// Archmage
			float archmageDamagePctOfMaxMagicka{ 0.0f };
			float archmageCostPctOfMaxMagicka{ 0.0f };

			// CorpseExplosion
			std::int32_t corpseExplosionSelectionPriority{ 0 };
			float corpseExplosionFlatDamage{ 0.0f };
			float corpseExplosionPctOfCorpseMaxHealth{ 0.0f };
			float corpseExplosionRadius{ 0.0f };
			std::uint32_t corpseExplosionMaxTargets{ 0 };
			std::uint32_t corpseExplosionMaxChainDepth{ 1 };
			float corpseExplosionChainFalloff{ 1.0f };
			std::chrono::milliseconds corpseExplosionChainWindow{ 0 };
			std::chrono::milliseconds corpseExplosionRateLimitWindow{ 0 };
			std::uint32_t corpseExplosionMaxExplosionsPerWindow{ 0 };

			// SpawnTrap
			TrapSpawnAt trapSpawnAt{ TrapSpawnAt::kTargetFeet };
			float trapRadius{ 0.0f };
			std::chrono::milliseconds trapTtl{ 0 };
			std::chrono::milliseconds trapArmDelay{ 0 };
			std::chrono::milliseconds trapRearmDelay{ 0 };
			std::uint32_t trapMaxActive{ 0 };
			std::uint32_t trapMaxTriggers{ 1 };
			std::uint32_t trapMaxTargetsPerTrigger{ 1 };
			std::vector<RE::SpellItem*> trapExtraSpells{};
			bool trapRequireCritOrPowerAttack{ false };
			bool trapRequireWeaponHit{ true };

			// EchoStrike: a melee hit (a power attack when echoRequirePowerAttack)
			// casts the stance spell on the owner and opens the window; each melee
			// hit inside it casts `spell` on the target after the delay, scaled
			// from that hit by magnitudeScaling.
			RE::SpellItem* echoStanceSpell{ nullptr };
			bool echoRequirePowerAttack{ false };
			std::chrono::milliseconds echoWindow{ 0 };
			std::chrono::milliseconds echoDelay{ 0 };

			// Evolution (growth): each successful proc grants XP and scales magnitude by stage.
			bool evolutionEnabled{ false };
			std::uint32_t evolutionXpPerProc{ 1 };
			std::vector<std::uint32_t> evolutionThresholds{};
			std::vector<float> evolutionMultipliers{};

			// Mode cycle: rotate spell payload across modes after N successful procs.
			bool modeCycleEnabled{ false };
			std::vector<RE::SpellItem*> modeCycleSpells{};
			std::vector<std::string> modeCycleLabels{};
			std::uint32_t modeCycleSwitchEveryProcs{ 1 };
			bool modeCycleManualOnly{ false };
		};

		struct TrapInstance
		{
			std::uint64_t sourceToken{ 0 };
			RE::FormID ownerFormID{ 0 };
			RE::NiPoint3 position{};
			RE::TESObjectCELL* cell{ nullptr };
			float radius{ 0.0f };

			RE::SpellItem* spell{ nullptr };
			float effectiveness{ 1.0f };
			float magnitudeOverride{ 0.0f };
			bool noHitEffectArt{ false };
			RE::SpellItem* extraSpell{ nullptr };

			std::chrono::steady_clock::time_point armedAt{};
			std::chrono::steady_clock::time_point expiresAt{};
			std::chrono::steady_clock::time_point createdAt{};

			std::chrono::milliseconds rearmDelay{ 0 };
			std::uint32_t maxTriggers{ 1 };
			std::uint32_t triggeredCount{ 0 };
			std::uint32_t maxTargetsPerTrigger{ 1 };
			TrapFeedback feedback{};
			TrapVisualState visualState{ TrapVisualState::kNone };
			RE::NiPointer<RE::BSTempEffectParticle> markerEffect{};
			RE::ObjectRefHandle markerReference{};
			RE::NiPointer<RE::TESObjectREFR> markerReferenceOwner{};
			TrapMarkerAnimationPhase markerAnimationPhase{ TrapMarkerAnimationPhase::kNone };
			std::uint8_t markerAnimationAttempts{ 0u };
			std::chrono::steady_clock::time_point markerAnimationNextAttemptAt{};
			bool markerRearmAnimationResolved{ false };
		};

		struct TrapRuntimeState
		{
			std::vector<TrapInstance> activeTraps{};
			std::array<RE::ObjectRefHandle, detail::kMaxPlacedTrapMarkers> pendingMarkerCleanup{};
			std::size_t tickCursor{ 0 };
			std::atomic_bool hasActiveTraps{ false };
			std::chrono::steady_clock::time_point staleCombatLastClearAt{};
			std::chrono::steady_clock::time_point staleCombatLastHardClearAt{};
			std::uint32_t staleCombatClearConfidence{ 0u };
			std::chrono::steady_clock::time_point forceStopLastPulseAt{};
			std::chrono::steady_clock::time_point playerOwnedCombatCleanupExpiresAt{};

			[[nodiscard]] std::size_t PendingMarkerCleanupCount() const noexcept
			{
				std::size_t count = 0u;
				for (const auto& handle : pendingMarkerCleanup) {
					count += handle ? 1u : 0u;
				}
				return count;
			}

			[[nodiscard]] bool HasRuntimeWork() const noexcept
			{
				return !activeTraps.empty() || PendingMarkerCleanupCount() != 0u;
			}

			void RefreshRuntimeWorkFlag() noexcept
			{
				hasActiveTraps.store(HasRuntimeWork(), std::memory_order_relaxed);
			}

			[[nodiscard]] bool QueuePendingMarkerCleanup(const RE::ObjectRefHandle& a_handle) noexcept
			{
				if (!a_handle) {
					return false;
				}
				for (const auto& handle : pendingMarkerCleanup) {
					if (handle && handle == a_handle) {
						return true;
					}
				}
				for (auto& handle : pendingMarkerCleanup) {
					if (!handle) {
						handle = a_handle;
						RefreshRuntimeWorkFlag();
						return true;
					}
				}
				return false;
			}

			void DiscardPendingMarkerCleanup() noexcept
			{
				for (auto& handle : pendingMarkerCleanup) {
					handle.reset();
				}
				RefreshRuntimeWorkFlag();
			}

			void ClearTrapQueue() noexcept
			{
				activeTraps.clear();
				tickCursor = 0u;
				RefreshRuntimeWorkFlag();
			}

			void ClearStaleCombatTracking() noexcept
			{
				staleCombatLastClearAt = {};
				staleCombatLastHardClearAt = {};
				staleCombatClearConfidence = 0u;
				forceStopLastPulseAt = {};
				playerOwnedCombatCleanupExpiresAt = {};
			}

			void Reset() noexcept
			{
				ClearTrapQueue();
				ClearStaleCombatTracking();
			}
		};

		struct RuntimeSettingsState
		{
			std::atomic_bool enabled{ true };
			float procChanceMult{ 1.0f };
			std::atomic_bool allowNonHostilePlayerOwnedOutgoingProcs{ false };
			bool combatDebugLog{ false };
			bool disableCombatEvidenceLease{ false };
			bool disableHealthDamageRouting{ false };
			bool allowPlayerHealthDamageHook{ true };
			bool disablePassiveSuffixSpells{ false };
			bool disableTrapSystemTick{ false };
			bool disableTrapCasts{ false };
			bool forceStopAlarmPulse{ false };
			RuntimeUserSettingsDebounce::State userSettingsPersist{};

			void Reset() noexcept
			{
				enabled.store(true, std::memory_order_relaxed);
				procChanceMult = 1.0f;
				allowNonHostilePlayerOwnedOutgoingProcs.store(false, std::memory_order_relaxed);
				combatDebugLog = false;
				disableCombatEvidenceLease = false;
				disableHealthDamageRouting = false;
				allowPlayerHealthDamageHook = true;
				disablePassiveSuffixSpells = false;
				disableTrapSystemTick = false;
				disableTrapCasts = false;
				forceStopAlarmPulse = false;
				userSettingsPersist = {};
			}
		};

		struct InstanceRuntimeState
		{
			std::uint32_t evolutionXp{ 0 };
			std::uint32_t modeCycleCounter{ 0 };
			std::uint32_t modeIndex{ 0 };
		};

		struct RunewordRecipe
		{
			std::string id{};
			std::uint64_t token{ 0 };
			std::string displayName{};  // Korean; the panel picks En/Ko by UI language.
			std::string displayNameEn{};
			std::string displayNameKo{};
			std::vector<std::string> runeIds{};
			std::vector<std::uint64_t> runeTokens{};
			std::uint64_t resultAffixToken{ 0 };
			std::optional<LootItemType> recommendedBaseType{};

			// displayName/displayNameEn/displayNameKo above keep the upstream contract
			// names. Player-facing English text goes through the FE-ASR presentation
			// resolver below; id and runeIds stay the mechanical keys.

			// English UI name: the FE-ASR name for manifest recipes, upstream English
			// for anything else (empty when upstream has none).
			[[nodiscard]] std::string PresentationNameEn() const
			{
				return std::string(RuneforgePresentation::RecipeNameOr(id, displayNameEn));
			}

			// HUD notes are English sentences, and the English HUD font may lack Hangul.
			[[nodiscard]] std::string HudName() const
			{
				return std::string(RuneforgePresentation::RecipeNameOr(
					id,
					displayNameEn.empty() ? std::string_view(displayName) : std::string_view(displayNameEn)));
			}

			// Player-facing rune sequence in mechanical order, e.g. "Eldr-Fenn-Draum".
			[[nodiscard]] std::string PresentationRuneSequence() const
			{
				return RuneforgePresentation::PresentRuneSequence(runeIds);
			}
		};

		enum class SyntheticRunewordStyle : std::uint8_t
		{
			kAdaptiveStrike,
			kAdaptiveExposure,
			kSignatureGrief,
			kSignatureInfinity,
			kSignatureEnigma,
			kSignatureCallToArms,
			kFireStrike,
			kFrostStrike,
			kShockStrike,
			kPoisonBloom,
			kTarBloom,
			kSiphonBloom,
			kCurseFragile,
			kCurseSlowAttack,
			kCurseFear,
			kCurseFrenzy,
			kSelfHaste,
			kSelfWard,
			kSelfBarrier,
			kSelfMeditation,
			kSelfPhase,
			kSelfPhoenix,
			kSelfFlameCloak,
			kSelfFrostCloak,
			kSelfShockCloak,
			kSelfOakflesh,
			kSelfStoneflesh,
			kSelfIronflesh,
			kSelfEbonyflesh,
			kSelfMuffle,
			kSelfInvisibility,
			kSoulTrap,
			kSummonFamiliar,
			kSummonFlameAtronach,
			kSummonFrostAtronach,
			kSummonStormAtronach,
			kSummonDremoraLord,
			kLegacyFallback
		};

		struct RunewordInstanceState
		{
			std::uint64_t recipeToken{ 0 };
			std::uint32_t insertedRunes{ 0 };
		};

		struct RunewordRuntimeState
		{
			// Catalog/cache data synthesized from runtime contracts or fallback rows.
			std::vector<RunewordRecipe> recipes{};
			std::unordered_map<std::uint64_t, std::size_t> recipeIndexByToken{};
			std::unordered_map<std::uint64_t, std::size_t> recipeIndexByResultAffixToken{};
			std::unordered_map<std::uint64_t, std::string> runeNameByToken{};
			std::vector<std::uint64_t> runeTokenPool{};
			std::vector<double> runeTokenWeights{};

			// Mutable player progress / UI selection state for runeword flows.
			std::unordered_map<std::uint64_t, std::uint32_t> runeFragments{};
			std::unordered_map<std::uint64_t, RunewordInstanceState> instanceStates{};
			std::optional<std::uint64_t> selectedBaseKey{};
			std::uint32_t baseCycleCursor{ 0 };
			std::uint32_t recipeCycleCursor{ 0 };
			bool transmuteInProgress{ false };
			bool affixExpansionInProgress{ false };

			void ResetCatalog() noexcept
			{
				recipes.clear();
				recipeIndexByToken.clear();
				recipeIndexByResultAffixToken.clear();
				runeNameByToken.clear();
				runeTokenPool.clear();
				runeTokenWeights.clear();
			}

			void ResetSelectionAndProgress() noexcept
			{
				runeFragments.clear();
				instanceStates.clear();
				selectedBaseKey.reset();
				baseCycleCursor = 0u;
				recipeCycleCursor = 0u;
				transmuteInProgress = false;
				affixExpansionInProgress = false;
			}

			void Reset() noexcept
			{
				ResetCatalog();
				ResetSelectionAndProgress();
			}
		};

		struct AffixRuntime
		{
			std::string id{};
			std::uint64_t token{ 0 };
			std::string keywordEditorId{};
			std::string label{};
			std::string displayName{};
			std::string displayNameEn{};
			std::string displayNameKo{};
			RE::BGSKeyword* keyword{ nullptr };
			std::optional<LootItemType> lootType{};
			Trigger trigger{ Trigger::kHit };
			float procChancePct{ 0.0f };
			float normalWeaponHitProcChancePct{ 0.0f };
			// Weighted loot source:
			// - lootWeight >= 0 uses that value
			// - lootWeight < 0 means "not rollable from loot-time pool"
			// Values <=0 are excluded from weighted rolling.
			float lootWeight{ -1.0f };
			[[nodiscard]] constexpr float EffectiveLootWeight() const noexcept { return (lootWeight >= 0.0f) ? lootWeight : 0.0f; }
			std::chrono::milliseconds icd{ 0 };
			std::chrono::milliseconds perTargetIcd{ 0 };
			std::chrono::milliseconds requireRecentlyHit{ 0 };
			std::chrono::milliseconds requireRecentlyKill{ 0 };
			std::chrono::milliseconds requireNotHitRecently{ 0 };
			float lowHealthThresholdPct{ 35.0f };
			float lowHealthRearmPct{ 45.0f };
			float luckyHitChancePct{ 0.0f };
			float luckyHitProcCoefficient{ 1.0f };
			Action action{};

			// Prefix/Suffix classification
			AffixSlot slot{ AffixSlot::kNone };
			std::string family{};            // suffix family grouping (e.g., "max_health")
			std::uint8_t weaponSubtypeMask{ 0u };  // 0 = unrestricted; applied only while rolling new weapon suffixes
			RE::SpellItem* passiveSpell{ nullptr };  // suffix: Ability spell to add/remove on equip
			bool refreshPassiveSpellOnPostLoad{ false };  // refresh configured desired passives after save-load
			float critDamageBonusPct{ 0.0f };        // suffix: crit damage bonus (applied in HandleHealthDamage hook)
			float scrollNoConsumeChancePct{ 0.0f };  // equipped: additive chance to preserve consumed scrolls

			std::chrono::steady_clock::time_point nextAllowed{};
		};

		// Runtime config/state PODs.
		enum class LootNameMarkerPosition : std::uint8_t
		{
			kLeading = 0,
			kTrailing = 1,
		};

		enum class CurrencyDropMode : std::uint8_t
		{
			kHybrid = 2,
		};

		struct LootConfig
		{
			float chancePercent{ 0.0f };
			float runewordFragmentChancePercent{ 8.0f };
			float reforgeOrbChancePercent{ 12.0f };
			// Independent corpse rolls for the crafting currencies (MCM-tunable).
			float identifyScrollChancePercent{ 20.0f };
			float scouringOrbChancePercent{ 4.0f };
			float uniqueActorGuaranteedRunewordChancePercent{ 40.0f };
			float configuredRunewordFragmentChancePercent{ 8.0f };
			float configuredReforgeOrbChancePercent{ 12.0f };
			CurrencyDropMode currencyDropMode{ CurrencyDropMode::kHybrid };
			bool runtimeCurrencyDropsEnabled{ false };
			bool runtimeCorpseDeathCurrencyDropsEnabled{ true };
			float lootSourceChanceMultCorpse{ 1.00f };
			float lootSourceChanceMultContainer{ 1.00f };
			float lootSourceChanceMultBossContainer{ 1.15f };
			float lootSourceChanceMultWorld{ 1.00f };
			bool renameItem{ false };
			bool sharedPool{ false };
			bool debugLog{ false };
			bool debugHudNotifications{ false };
			bool dotTagSafetyAutoDisable{ false };
			std::uint32_t dotTagSafetyUniqueEffectThreshold{ 96 };
			std::uint32_t trapGlobalMaxActive{ 48 };
			std::uint32_t trapCastBudgetPerTick{ 6 };
			std::uint32_t triggerProcBudgetPerWindow{ 12 };
			std::uint32_t triggerProcBudgetWindowMs{ 100 };
			bool cleanupInvalidLegacyAffixes{ true };
			bool stripTrackedSuffixSlots{ true };
			std::vector<std::string> armorEditorIdDenyContains{};
			std::vector<std::string> bossContainerEditorIdAllowContains{};
			std::vector<std::string> bossContainerEditorIdDenyContains{};
			std::string nameFormat{ "{base} [{affix}]" };
			LootNameMarkerPosition nameMarkerPosition{ LootNameMarkerPosition::kTrailing };
		};

		struct CurrencyRollExecutionResult
		{
			bool runewordDropGranted{ false };
			bool reforgeDropGranted{ false };
		};

		struct PerTargetCooldownKey
		{
			std::uint64_t token{ 0 };
			RE::FormID target{ 0 };
		};

		// Special-action selection/runtime structs.
		enum class CorpseExplosionBudgetDenyReason : std::uint8_t
		{
			kNone = 0,
			kDuplicateCorpse,
			kRateLimited,
			kChainDepthExceeded,
		};

		struct CorpseExplosionSelection
		{
			std::optional<std::size_t> bestIdx{};
			PerTargetCooldownKey bestPerTargetKey{};
			bool bestUsesPerTargetIcd{ false };
			std::int32_t bestSelectionPriority{ 0 };
			float bestBaseDamage{ 0.0f };
			std::uint32_t activeAffixes{ 0 };
			std::uint32_t blockedCooldown{ 0 };
			std::uint32_t blockedNoSpell{ 0 };
			std::uint32_t blockedZeroDamage{ 0 };
		};

		struct ArchmageSelection
		{
			std::optional<std::size_t> bestIdx{};
			const Action* bestAction{ nullptr };
			float bestDamagePct{ 0.0f };
			float bestCostPct{ 0.0f };
			float bestMaxMagicka{ 0.0f };
			float bestExtraCost{ 0.0f };
			float bestExtraDamage{ 0.0f };
		};

		struct AdaptiveCastSelection
		{
			AdaptiveElement pick{ AdaptiveElement::kFire };
			RE::SpellItem* spell{ nullptr };
			float resistFire{ 0.0f };
			float resistFrost{ 0.0f };
			float resistShock{ 0.0f };
		};

		enum class RunewordApplyBlockReason : std::uint8_t
		{
			kNone = 0,
			kMissingResultAffix,
			kAlreadyComplete,
			kAffixSlotsFull,
			kIneligibleBase,
		};
