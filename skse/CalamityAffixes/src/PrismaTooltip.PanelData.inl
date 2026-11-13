		void PushCachedInteropPayload(std::string& a_cache, const char* a_method, std::string a_payload)
		{
			if (!IsViewReady()) {
				return;
			}

			if (a_payload == a_cache) {
				return;
			}

			a_cache = std::move(a_payload);
			g_api->InteropCall(g_view, a_method, a_cache.c_str());
		}

		[[nodiscard]] nlohmann::json BuildRunewordBaseInventoryPayload(
			const std::vector<RunewordBaseInventoryEntry>& a_entries)
		{
			nlohmann::json payload = nlohmann::json::array();
			// RC1 legacy-UI policy: with FEASR Runeforge active, Runeforge rows are locked to the FE-ASR (English)
			// names in every language mode; Calamity's Korean legacy runeword names never show on them.
			const bool lockFeasrNames = RuneforgeWorkstation::IsFeasrRuneforgeActive();
			for (const auto& entry : a_entries) {
				payload.push_back({
					{ "key", std::to_string(entry.instanceKey) },
					{ "name", entry.displayName },
					{ "baseName", entry.baseName },
					{ "runewordNameEn", entry.runewordNameEn },
					{ "runewordNameKo", entry.runewordNameKo },
					{ "selected", entry.selected }
				});
				if (lockFeasrNames && !entry.runewordNameEn.empty()) {
					payload.back()["runewordNameKo"] = entry.runewordNameEn;
				}
			}

			return payload;
		}

		void SetRunewordBaseInventoryList(const std::vector<RunewordBaseInventoryEntry>& a_entries)
		{
			PushCachedInteropPayload(
				g_runewordCache.baseListJson,
				kInteropSetInventoryItems,
				BuildRunewordBaseInventoryPayload(a_entries).dump());
		}

		[[nodiscard]] nlohmann::json BuildRunewordRecipePayload(
			const std::vector<RunewordRecipeEntry>& a_entries)
		{
			nlohmann::json payload = nlohmann::json::array();
			const bool feasrActive = RuneforgeWorkstation::IsFeasrRuneforgeActive();
			for (const auto& entry : a_entries) {
				// RC1 legacy-UI policy: FE-ASR rows are locked to their English FE-ASR text in every language mode.
				const bool lock = feasrActive && RuneforgePresentation::IsKnownRecipe(entry.recipeId);
				payload.push_back({
					{ "token", std::to_string(entry.recipeToken) },
					{ "name", entry.displayName },
					{ "nameEn", entry.displayNameEn },
					{ "nameKo", entry.displayNameKo },
					{ "runes", entry.runeSequence },
					{ "runeTokens", BuildRunewordRuneTokenArrayJson(entry.runeTokens) },
					{ "summaryKey", entry.effectSummaryKey },
					// English fields above already carry FE-ASR presentation for this row.
					{ "feasrPresentation", RuneforgePresentation::IsKnownRecipe(entry.recipeId) },
					{ "summaryEn", entry.effectSummaryTextEn },
					{ "summaryKo", entry.effectSummaryTextKo },
					{ "detailEn", entry.effectDetailTextEn },
					{ "detailKo", entry.effectDetailTextKo },
					{ "summary", entry.effectSummaryTextKo },
					{ "detail", entry.effectDetailTextKo },
					{ "baseKey", entry.recommendedBaseKey },
					{ "selected", entry.selected }
				});
				if (lock) {
					auto& row = payload.back();
					row["name"] = entry.displayNameEn;
					row["nameKo"] = entry.displayNameEn;
					row["summaryKo"] = entry.effectSummaryTextEn;
					row["detailKo"] = entry.effectDetailTextEn;
					row["summary"] = entry.effectSummaryTextEn;
					row["detail"] = entry.effectDetailTextEn;
				}
			}

			return payload;
		}

		void SetRunewordRecipeList(const std::vector<RunewordRecipeEntry>& a_entries)
		{
			PushCachedInteropPayload(
				g_runewordCache.recipeListJson,
				kInteropSetRecipeItems,
				BuildRunewordRecipePayload(a_entries).dump());
		}

		void SetRunewordAffixPreview(std::string_view a_text)
		{
			PushCachedInteropPayload(
				g_runewordCache.affixPreview,
				kInteropSetRunewordAffixPreview,
				std::string(a_text));
		}

		[[nodiscard]] nlohmann::json BuildEquippedBuildSummaryPayload(
			const EquippedBuildSummary& a_summary)
		{
			nlohmann::json entries = nlohmann::json::array();
			for (const auto& entry : a_summary.entries) {
				entries.push_back({
					{ "token", entry.token == 0u ? std::string{} : std::to_string(entry.token) },
					{ "displayNameEn", entry.displayNameEn },
					{ "displayNameKo", entry.displayNameKo },
					{ "effectSummaryEn", entry.effectSummaryEn },
					{ "group", entry.group },
					{ "triggerKey", entry.triggerKey },
					{ "slotKind", entry.slotKind },
					{ "suffixState", entry.suffixState },
					{ "equippedCount", entry.equippedCount },
					{ "suffixTierRank", entry.suffixTierRank },
					{ "suffixFamilyRankPoints", entry.suffixFamilyRankPoints },
					{ "effectiveSuffixTierRank", entry.effectiveSuffixTierRank },
					{ "effectiveSuffixDisplayNameEn", entry.effectiveSuffixDisplayNameEn },
					{ "effectiveSuffixDisplayNameKo", entry.effectiveSuffixDisplayNameKo },
					{ "hasPassiveContribution", entry.hasPassiveContribution },
					{ "passiveContributionActive", entry.passiveContributionActive },
					{ "passiveSpellDisabled", entry.passiveSpellDisabled },
					{ "hasProcRoll", entry.hasProcRoll },
					{ "procRollChancePct", entry.procRollChancePct },
					{ "procRollStackCount", entry.procRollStackCount },
					{ "castOnCritSelectionLimited", entry.castOnCritSelectionLimited },
					{ "hasNormalWeaponHitProcRoll", entry.hasNormalWeaponHitProcRoll },
					{ "normalWeaponHitProcChancePct", entry.normalWeaponHitProcChancePct },
					{ "normalWeaponHitProcStackCount", entry.normalWeaponHitProcStackCount },
					{ "hasLuckyHitGate", entry.hasLuckyHitGate },
					{ "luckyHitGateChancePct", entry.luckyHitGateChancePct },
					{ "cooldownSeconds", entry.cooldownSeconds },
					{ "perTargetCooldownSeconds", entry.perTargetCooldownSeconds },
					{ "lowHealthThresholdPct", entry.lowHealthThresholdPct },
					{ "lowHealthRearmPct", entry.lowHealthRearmPct },
				});
			}

			return {
				{ "ready", a_summary.ready },
				{ "runtimeEnabled", a_summary.runtimeEnabled },
				{ "equippedAffixSlots", a_summary.equippedAffixSlots },
				{ "entries", std::move(entries) },
			};
		}

		[[nodiscard]] nlohmann::json BuildRunewordPanelStatePayload(const RunewordPanelState& a_state)
		{
			// RC1 legacy-UI policy: the selected Runeforge recipe shows its FE-ASR (English) name in every language mode.
			const bool lockFeasrRecipeName = a_state.hasRecipe && !a_state.recipeNameEn.empty() && RuneforgeWorkstation::IsFeasrRuneforgeActive();
			nlohmann::json requiredRunes = nlohmann::json::array();
			for (const auto& req : a_state.requiredRunes) {
				requiredRunes.push_back({
					{ "name", req.runeName },
					{ "required", req.required },
					{ "owned", req.owned },
				});
			}

			nlohmann::json reforgeLockCandidates = nlohmann::json::array();
			for (const auto& candidate : a_state.reforgeLockCandidates) {
				reforgeLockCandidates.push_back({
					{ "affixToken", candidate.affixToken == 0u ? std::string{} : std::to_string(candidate.affixToken) },
					{ "displayNameEn", candidate.displayNameEn },
					{ "displayNameKo", candidate.displayNameKo },
					{ "slotKind", candidate.slotKind },
				});
			}

			const nlohmann::json payload{
				{ "hasBase", a_state.hasBase },
				{ "hasRecipe", a_state.hasRecipe },
				{ "isComplete", a_state.isComplete },
				{ "recipeName", a_state.recipeName },
				{ "recipeNameEn", a_state.recipeNameEn },
				{ "recipeNameKo", a_state.recipeNameKo },
				{ "recipeToken", a_state.recipeToken == 0u ? std::string{} : std::to_string(a_state.recipeToken) },
				{ "insertedRunes", a_state.insertedRunes },
				{ "totalRunes", a_state.totalRunes },
				{ "nextRuneName", a_state.nextRuneName },
				{ "nextRuneOwned", a_state.nextRuneOwned },
				{ "canInsert", a_state.canInsert },
				{ "missingSummary", a_state.missingSummary },
				{ "baseCompatibilityWarning", a_state.baseCompatibilityWarning },
				{ "baseCompatibilityMessageEn", a_state.baseCompatibilityMessageEn },
				{ "baseCompatibilityMessageKo", a_state.baseCompatibilityMessageKo },
				{ "requiredRunes", requiredRunes },
				{ "runeInventoryKnown", a_state.runeInventoryKnown },
				{ "runeInventoryExpectedCount", a_state.runeInventoryExpectedCount },
				{ "runeInventory", BuildRunewordRuneInventoryJson(a_state.runeInventory) },
				{ "regularAffixCount", a_state.regularAffixCount },
				{ "maxRegularAffixCount", a_state.maxRegularAffixCount },
				{ "expandAffixCost", a_state.expandAffixCost },
				{ "canExpandAffix", a_state.canExpandAffix },
				{ "expandAffixUnavailableReason", a_state.expandAffixUnavailableReason },
				{ "reforgeOrbsKnown", a_state.reforgeOrbsKnown },
				{ "reforgeOrbsOwned", a_state.reforgeOrbsOwned },
                { "identifyScrollsKnown", a_state.identifyScrollsKnown },
                { "identifyScrollsOwned", a_state.identifyScrollsOwned },
                { "scouringOrbsKnown", a_state.scouringOrbsKnown },
                { "scouringOrbsOwned", a_state.scouringOrbsOwned },
                { "exchangeReforgeScrollCost", a_state.exchangeReforgeScrollCost },
                { "exchangeScourScrollCost", a_state.exchangeScourScrollCost },
				{ "pityKnown", a_state.pityKnown },
				{ "runewordFragmentFailStreak", a_state.runewordFragmentFailStreak },
				{ "runewordFragmentFailStreakThreshold", a_state.runewordFragmentFailStreakThreshold },
				{ "reforgeOrbFailStreak", a_state.reforgeOrbFailStreak },
				{ "reforgeOrbFailStreakThreshold", a_state.reforgeOrbFailStreakThreshold },
				{ "standardReforgeCost", a_state.standardReforgeCost },
				{ "lockedReforgeCost", a_state.lockedReforgeCost },
				{ "selectedReforgesLeft", a_state.selectedReforgesLeft },
				{ "selectedReforgesPerItem", a_state.selectedReforgesPerItem },
				{ "reforgeLockCandidates", reforgeLockCandidates },
				{ "equippedBuild", BuildEquippedBuildSummaryPayload(a_state.equippedBuild) },
				{ "debugTools", a_state.debugTools }
			};

			if (lockFeasrRecipeName) {
				auto locked = payload;
				locked["recipeName"] = a_state.recipeNameEn;
				locked["recipeNameKo"] = a_state.recipeNameEn;
				return locked;
			}
			return payload;
		}

		void SetRunewordPanelState(const RunewordPanelState& a_state)
		{
			PushCachedInteropPayload(
				g_runewordCache.panelStateJson,
				kInteropSetRunewordPanelState,
				BuildRunewordPanelStatePayload(a_state).dump());
		}

		[[nodiscard]] std::string ResolveSelectedItemDisplayName(const RE::ItemList::Item* a_item)
		{
			if (!a_item) {
				return {};
			}

			std::string selectedName;

			if (a_item->obj.IsObject()) {
				RE::GFxValue value;
				if (a_item->obj.GetMember("name", &value) && value.IsString()) {
					const char* text = value.GetString();
					if (text && *text) {
						selectedName = text;
					}
				}

				if (selectedName.empty() && a_item->obj.GetMember("text", &value) && value.IsString()) {
					const char* text = value.GetString();
					if (text && *text) {
						selectedName = text;
					}
				}
			}

			if (selectedName.empty() && a_item->data.objDesc) {
				const char* text = a_item->data.objDesc->GetDisplayName();
				if (text && *text) {
					selectedName = text;
				}
			}

			return selectedName;
		}

		struct SelectedItemContext
		{
			std::string itemName{};
			std::string itemSource{};
			std::optional<std::string> tooltip{ std::nullopt };
			bool hasSelection{ false };
		};

		[[nodiscard]] RE::FormID ResolveRefHandleToFormID(RE::RefHandle a_handle)
		{
			if (a_handle == 0) {
				return 0u;
			}
			if (auto ref = RE::TESObjectREFR::LookupByHandle(a_handle); ref) {
				return ref->GetFormID();
			}
			return 0u;
		}

		[[nodiscard]] bool TryPopulateSelectedItemContext(
			SelectedItemContext& a_result,
			RE::ItemList* a_itemList,
			std::string_view a_source,
			RE::FormID a_sourceContainerFormID,
			CalamityAffixes::EventBridge& a_bridge)
		{
			if (!a_itemList) {
				return false;
			}

			auto* item = a_itemList->GetSelectedItem();
			if (!item) {
				return false;
			}

			auto selectedName = ResolveSelectedItemDisplayName(item);
			if (selectedName.empty()) {
				selectedName = "<Unknown>";
			}
			a_result.hasSelection = true;
			a_result.itemName = std::move(selectedName);
			a_result.itemSource = std::string(a_source);
			a_result.tooltip = a_bridge.GetInstanceAffixTooltip(
				item->data.objDesc,
				a_result.itemName,
				g_uiLanguageMode.load(),
				a_result.itemSource,
				a_sourceContainerFormID);
			return true;
		}

		[[nodiscard]] bool TryReadInventorySelectedItemContext(
			SelectedItemContext& a_result,
			RE::UI& a_ui,
			CalamityAffixes::EventBridge& a_bridge)
		{
			if (!a_ui.IsMenuOpen(kMenuInventory)) {
				return false;
			}

			auto menu = a_ui.GetMenu<RE::InventoryMenu>();
			if (!menu) {
				return false;
			}

			auto& data = menu->GetRuntimeData();
			return TryPopulateSelectedItemContext(a_result, data.itemList, kItemSourceInventory, 0u, a_bridge);
		}

		[[nodiscard]] bool TryReadBarterSelectedItemContext(
			SelectedItemContext& a_result,
			RE::UI& a_ui,
			CalamityAffixes::EventBridge& a_bridge)
		{
			if (!a_ui.IsMenuOpen(kMenuBarter)) {
				return false;
			}

			auto menu = a_ui.GetMenu<RE::BarterMenu>();
			if (!menu) {
				return false;
			}

			auto& data = menu->GetRuntimeData();
			const auto sourceContainer = ResolveRefHandleToFormID(RE::BarterMenu::GetTargetRefHandle());
			return TryPopulateSelectedItemContext(a_result, data.itemList, kItemSourceBarter, sourceContainer, a_bridge);
		}

		[[nodiscard]] bool TryReadContainerSelectedItemContext(
			SelectedItemContext& a_result,
			RE::UI& a_ui,
			CalamityAffixes::EventBridge& a_bridge)
		{
			if (!a_ui.IsMenuOpen(kMenuContainer)) {
				return false;
			}

			auto menu = a_ui.GetMenu<RE::ContainerMenu>();
			if (!menu) {
				return false;
			}

			auto& data = menu->GetRuntimeData();
			const auto sourceContainer = ResolveRefHandleToFormID(RE::ContainerMenu::GetTargetRefHandle());
			return TryPopulateSelectedItemContext(a_result, data.itemList, kItemSourceContainer, sourceContainer, a_bridge);
		}

		[[nodiscard]] bool TryReadGiftSelectedItemContext(
			SelectedItemContext& a_result,
			RE::UI& a_ui,
			CalamityAffixes::EventBridge& a_bridge)
		{
			if (!a_ui.IsMenuOpen(kMenuGift)) {
				return false;
			}

			auto menu = a_ui.GetMenu<RE::GiftMenu>();
			if (!menu) {
				return false;
			}

			auto& data = menu->GetRuntimeData();
			const auto sourceContainer = ResolveRefHandleToFormID(RE::GiftMenu::GetTargetRefHandle());
			return TryPopulateSelectedItemContext(a_result, data.itemList, kItemSourceGift, sourceContainer, a_bridge);
		}

		SelectedItemContext GetSelectedItemContext()
		{
			SelectedItemContext result;

			auto* bridge = CalamityAffixes::EventBridge::GetSingleton();
			if (!bridge) {
				return result;
			}

			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return result;
			}

			if (TryReadSelectedItemContextFromMenus(result, *ui, *bridge)) {
				return result;
			}

			return result;
		}

		[[nodiscard]] bool TryReadSelectedItemContextFromMenus(
			SelectedItemContext& a_result,
			RE::UI& a_ui,
			CalamityAffixes::EventBridge& a_bridge)
		{
			for (const auto tryReadContext : {
					 &TryReadInventorySelectedItemContext,
					 &TryReadBarterSelectedItemContext,
					 &TryReadContainerSelectedItemContext,
					 &TryReadGiftSelectedItemContext }) {
				if (tryReadContext(a_result, a_ui, a_bridge)) {
					return true;
				}
			}

			return false;
		}

		[[nodiscard]] bool HasSelectedItemContextChanged(const SelectedItemContext& a_selected) noexcept
		{
			return
				a_selected.itemName != g_viewCache.selectedItemName ||
				a_selected.itemSource != g_viewCache.selectedItemSource;
		}

		[[nodiscard]] std::optional<std::string> BuildSelectedTooltipText(const SelectedItemContext& selected)
		{
			if (!selected.tooltip || selected.tooltip->empty()) {
				return std::nullopt;
			}

			const std::string next = StripRunewordOverlayTooltipLines(*selected.tooltip);
			if (next.empty()) {
				return std::nullopt;
			}

			return next;
		}

		void PushSelectedTooltipToView(std::string a_text)
		{
			g_lastTooltip = std::move(a_text);
			PrismaTelemetryController::RecordTooltipPush();
			g_api->InteropCall(g_view, kInteropSetTooltip, g_lastTooltip.c_str());
		}

		void ClearSelectedTooltipFromView(bool a_force, bool a_selectionChanged)
		{
			if (!a_force && !a_selectionChanged && g_lastTooltip.empty()) {
				return;
			}

			if (!g_lastTooltip.empty()) {
				PrismaTelemetryController::RecordTooltipClear();
			}
			g_lastTooltip.clear();
			g_api->InteropCall(g_view, kInteropSetTooltip, "");
		}

		bool PushSelectedTooltipSnapshot(bool a_force /*= false*/)
		{
			if (!IsViewReady()) {
				return false;
			}

			const auto selected = GetSelectedItemContext();
			const bool selectionChanged = HasSelectedItemContextChanged(selected);
			SetSelectedItemContext(selected.itemName, selected.itemSource);

			const auto nextTooltip = BuildSelectedTooltipText(selected);
			if (!nextTooltip.has_value()) {
				ClearSelectedTooltipFromView(a_force, selectionChanged);
				return false;
			}

			const auto& next = *nextTooltip;
			if (a_force || selectionChanged || next != g_lastTooltip) {
				PushSelectedTooltipToView(next);
			}
			return true;
		}
