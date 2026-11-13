#if !defined(CALAMITYAFFIXES_EVENTBRIDGE_PUBLIC_API_INL_CONTEXT)
#error "Do not include EventBridge.PublicApi.inl directly; include CalamityAffixes/EventBridge.h"
#endif

		// Lifecycle and config entrypoints.
		static EventBridge* GetSingleton();

		void Register();
		void LoadConfig();

		// Serialization callbacks.
		void Save(SKSE::SerializationInterface* a_intfc);
		void Load(SKSE::SerializationInterface* a_intfc);
		void Revert(SKSE::SerializationInterface* a_intfc);

		// Called from kPostLoadGame messaging callback — game state (player inventory)
		// is fully loaded at this point, unlike the Serialization Load callback.
		void OnPostLoadGame();
		void OnPreLoadGame();
		void OnPreSaveGame();

		// Called from SKSE SerializationInterface form delete callback.
		// We use this to prune instance-affix entries for player-dropped world refs that are later deleted.
		void OnFormDelete(RE::VMHandle a_handle);

		// Runtime polling/feature gates.
		// Tick lightweight runtime systems that need polling (e.g., ground traps).
		void TickTraps();
		[[nodiscard]] bool HasActiveTraps() const noexcept { return _trapState.hasActiveTraps.load(std::memory_order_relaxed); }
		// Land the echo strikes whose delay has elapsed (main thread).
		void TickEchoStrikes();
		[[nodiscard]] bool HasPendingEchoStrikes() const noexcept
		{
			return _combatState.echoStrike.hasPending.load(std::memory_order_acquire);
		}
		[[nodiscard]] bool IsRuntimeEnabled() const noexcept { return _runtimeSettings.enabled.load(std::memory_order_relaxed); }
		[[nodiscard]] bool IsHealthDamageRoutingDisabled() const noexcept { return _runtimeSettings.disableHealthDamageRouting; }
		[[nodiscard]] bool IsTrapSystemTickDisabled() const noexcept { return _runtimeSettings.disableTrapSystemTick; }
		[[nodiscard]] bool AllowsPlayerHealthDamageHook() const noexcept { return _runtimeSettings.allowPlayerHealthDamageHook; }
		[[nodiscard]] bool AllowsNonHostilePlayerOwnedOutgoingProcs() const noexcept
		{
			return _runtimeSettings.allowNonHostilePlayerOwnedOutgoingProcs.load(std::memory_order_relaxed);
		}
		[[nodiscard]] bool ResolveNonHostileOutgoingFirstHitAllowance(
			RE::Actor* a_owner,
			RE::Actor* a_target,
			bool a_hostileEitherDirection,
			std::chrono::steady_clock::time_point a_now);

		// UI-facing helpers.
		// UI helper: returns the affix tooltip text for this entry (if any).
		// Instance affixes are stored per item instance (ExtraUniqueID -> _instanceTrackingState.instanceAffixes).
		[[nodiscard]] std::optional<std::string> GetInstanceAffixTooltip(
			const RE::InventoryEntryData* a_item,
			std::string_view a_selectedDisplayName = {},
			int a_uiLanguageMode = 2,
			std::string_view a_itemSource = {},
			RE::FormID a_sourceContainerFormID = 0u,
			std::uint64_t a_preferredInstanceKey = 0u);

		// UI helper: enumerate runeword-base candidates from player's inventory.
		[[nodiscard]] std::vector<RunewordBaseInventoryEntry> GetRunewordBaseInventoryEntries();
		// UI helper: snapshot already keyed single-copy weapon/armor instances.
		// Workstation-entry preparation: materialize stable identity only for eligible
		// player-owned weapon/armor bases whose entire inventory count is exactly one.
		// The subsequent snapshot remains observational and still rejects stacks/ambiguity.
		[[nodiscard]] std::uint32_t PrepareRuneforgeSingletonInventoryIdentities();
		[[nodiscard]] RuneforgeInventorySnapshot GetRuneforgeInventorySnapshot();
		// Generic Calamity selection retains the current catalog recipe by default;
		// Runeforge passes false and requires an explicitly learned recipe selection.
		[[nodiscard]] bool SelectRunewordBase(
			std::uint64_t a_instanceKey,
			bool a_allowUndiscoveredRecipe = true);
		// UI helper: enumerate runeword recipes with selected state.
		[[nodiscard]] std::vector<RunewordRecipeEntry> GetRunewordRecipeEntries();
		// UI helper: select specific runeword recipe by token.
		[[nodiscard]] bool SelectRunewordRecipe(std::uint64_t a_recipeToken, bool a_allowUnknownRecipeDebug = false);
		[[nodiscard]] bool SelectRunewordRecipeForBase(
			std::uint64_t a_expectedInstanceKey,
			std::string_view a_recipeId,
			bool a_allowUnknownRecipeDebug = false);
		// FE ASR owns persistent discovery. Calamity keeps only the current
		// session mirror received over the versioned ModCallback event contract.
		[[nodiscard]] bool IsFeasrRuneforgeDiscoveryReady() const;
		[[nodiscard]] bool IsFeasrRuneforgeContractCompatible() const noexcept { return _feasrRuneforgeContractCompatible; }
		[[nodiscard]] bool IsFeasrRunewordRecipeKnown(std::string_view a_recipeId) const;
		// UI helper: guarded transmute. Rejects stale base, recipe, or progress snapshots.
		[[nodiscard]] OperationResult TransmuteRunewordBase(
			std::uint64_t a_expectedInstanceKey,
			std::string_view a_expectedRecipeId,
			std::uint32_t a_expectedInsertedRunes,
			bool a_allowUnknownRecipeDebug = false);
		// UI helper: current runeword crafting status. Prisma keeps the catalog
		// fallback; Runeforge disables fallback and undiscovered recipes.
		[[nodiscard]] RunewordPanelState GetRunewordPanelState(
			bool a_allowUndiscoveredRecipe = true,
			bool a_allowCurrentRecipeFallback = true);
		// UI helper: affix tooltip for currently selected runeword base.
		[[nodiscard]] std::optional<std::string> GetSelectedRunewordBaseAffixTooltip(int a_uiLanguageMode = 2);
		// R8.7.4.21 I3: effect card of the Runeword engraved on any instance key (the workstation's focused, not yet
		// selected item), with that instance's effective proc chance. Derived on demand from IAXF slots + config, so it
		// follows the item through drop/pickup and co-save reload. std::nullopt: no FE-ASR Runeword on that instance.
		[[nodiscard]] std::optional<RunewordEffectCardPolicy::Card> GetInstanceRunewordEffectCard(
			std::uint64_t a_instanceKey,
			bool a_worn);
        [[nodiscard]] OperationResult CraftSelectedAffixes(
            AffixCraftAction a_action, std::uint64_t a_expectedInstanceKey,
            std::uint64_t a_selectedToken = 0u);
        [[nodiscard]] OperationResult CraftSelectedAffixes(
            AffixCraftAction a_action, std::uint64_t a_expectedInstanceKey,
            std::uint64_t a_selectedToken, std::string_view a_expectedAffixSignature,
            std::uint32_t a_expectedRegularAffixCount);
        [[nodiscard]] OperationResult ExchangeCraftingCurrency(CurrencyExchange a_exchange);
		[[nodiscard]] OperationResult ExpandSelectedRunewordBaseAffixes(
			std::uint64_t a_expectedInstanceKey,
			std::uint8_t a_expectedRegularAffixCount);
		[[nodiscard]] OperationResult ExpandSelectedRunewordBaseAffixes(
			std::uint64_t a_expectedInstanceKey,
			std::uint8_t a_expectedRegularAffixCount,
			std::string_view a_expectedAffixSignature);
		[[nodiscard]] OperationResult ResetSelectedRunewordBaseCalamityState();

		// MCM manual recovery: grant starter orbs + missing rune fragments.
		[[nodiscard]] std::string RecoverMiscCurrency();

		// Combat/runtime evaluation entrypoints.
		// Called from Actor::HandleHealthDamage hook to drive hit/incoming-hit procs reliably,
		// even when TESHitEvent is delayed or suppressed by other mods.
		void OnHealthDamage(
			RE::Actor* a_target,
			RE::Actor* a_attacker,
			const RE::HitData* a_hitData,
			float a_damage);

		[[nodiscard]] ConversionResults EvaluateConversion(
			RE::Actor* a_attacker,
			RE::Actor* a_target,
			const RE::HitData* a_hitData,
			float& a_inOutDamage,
			bool a_allowResync = true);

		[[nodiscard]] MindOverMatterResult EvaluateMindOverMatter(
			RE::Actor* a_target,
			RE::Actor* a_attacker,
			const RE::HitData* a_hitData,
			float& a_inOutDamage,
			bool a_allowResync = true);

		// Returns one original-hit batch: at most two qualifying melee procs, or
		// one normal-melee/ranged proc.
		[[nodiscard]] CastOnCritResults EvaluateCastOnCrit(
			RE::Actor* a_attacker,
			RE::Actor* a_target,
			const RE::HitData* a_hitData,
			bool a_allowResync = true);

		[[nodiscard]] float GetCritDamageMultiplier(
			RE::Actor* a_attacker,
			const RE::HitData* a_hitData) const;

		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESHitEvent* a_event,
			RE::BSTEventSource<RE::TESHitEvent>* a_eventSource) override;

		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESDeathEvent* a_event,
			RE::BSTEventSource<RE::TESDeathEvent>* a_eventSource) override;

		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESEquipEvent* a_event,
			RE::BSTEventSource<RE::TESEquipEvent>* a_eventSource) override;

		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESActivateEvent* a_event,
			RE::BSTEventSource<RE::TESActivateEvent>* a_eventSource) override;

		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESMagicEffectApplyEvent* a_event,
			RE::BSTEventSource<RE::TESMagicEffectApplyEvent>* a_eventSource) override;

		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESContainerChangedEvent* a_event,
			RE::BSTEventSource<RE::TESContainerChangedEvent>* a_eventSource) override;

		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESUniqueIDChangeEvent* a_event,
			RE::BSTEventSource<RE::TESUniqueIDChangeEvent>* a_eventSource) override;

		RE::BSEventNotifyControl ProcessEvent(
			const SKSE::ModCallbackEvent* a_event,
			RE::BSTEventSource<SKSE::ModCallbackEvent>* a_eventSource) override;
