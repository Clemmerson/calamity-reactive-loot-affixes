#include "runtime_gate_store_checks_common.h"

namespace RuntimeGateStoreChecks
{
	bool CheckRunewordTooltipOverlayPolicy()
	{
		if (CalamityAffixes::ShouldShowRunewordTooltipInItemOverlay()) {
			std::cerr << "tooltip_policy: runeword text must stay panel-only\n";
			return false;
		}

		return true;
	}

	bool CheckLootPreviewRuntimePolicy()
	{
		if (CalamityAffixes::ShouldEnableSyntheticLootPreviewTooltip()) {
			std::cerr << "loot_preview_policy: synthetic preview rollout must remain disabled by policy helper\n";
			return false;
		}

		if (CalamityAffixes::RuntimePolicy::kAllowPickupRandomAffixAssignment ||
			CalamityAffixes::RuntimePolicy::kAllowLegacyPickupAffixRollBranch ||
			CalamityAffixes::RuntimePolicy::kAllowWorldPickupCurrencyRoll ||
			CalamityAffixes::RuntimePolicy::kAllowPickupCurrencyRollFromContainerSources ||
			CalamityAffixes::RuntimePolicy::kAllowActivationCurrencyRoll ||
			CalamityAffixes::RuntimePolicy::kAllowWorldCurrencyPlacement ||
			!CalamityAffixes::RuntimePolicy::kAllowCorpseDeathRuntimeCurrencyRoll) {
			std::cerr << "loot_preview_policy: runtime currency policy must stay hostile-corpse-death only\n";
			return false;
		}

		return true;
	}

	bool CheckLootCurrencyLedgerSerializationPolicy()
	{
		namespace fs = std::filesystem;
		using CalamityAffixes::detail::CorpseRuntimeCurrencyEligibilityInput;

		const CorpseRuntimeCurrencyEligibilityInput eligible{
			.eventIsDeath = true,
			.dyingIsDead = true,
			.dyingIsPlayerOwned = false,
			.dyingIsPlayerTeammate = false,
			.dyingIsSummoned = false,
			.dyingIsCommanded = false,
			.dyingIsChild = false,
			.killerIsPlayerOwned = true,
			.hostileToPlayerOwner = true
		};
		if (!CalamityAffixes::detail::IsCorpseRuntimeCurrencyDropEligible(eligible)) {
			std::cerr << "corpse_currency_policy: normal player-side hostile kill must be eligible\n";
			return false;
		}

		auto mustReject = [](CorpseRuntimeCurrencyEligibilityInput input) {
			return !CalamityAffixes::detail::IsCorpseRuntimeCurrencyDropEligible(input);
		};
		auto rejected = eligible;
		rejected.eventIsDeath = false;
		if (!mustReject(rejected)) {
			return false;
		}
		rejected = eligible;
		rejected.dyingIsDead = false;
		if (!mustReject(rejected)) {
			return false;
		}
		rejected = eligible;
		rejected.dyingIsPlayerOwned = true;
		if (!mustReject(rejected)) {
			return false;
		}
		rejected = eligible;
		rejected.dyingIsPlayerTeammate = true;
		if (!mustReject(rejected)) {
			return false;
		}
		rejected = eligible;
		rejected.dyingIsSummoned = true;
		if (!mustReject(rejected)) {
			return false;
		}
		rejected = eligible;
		rejected.dyingIsCommanded = true;
		if (!mustReject(rejected)) {
			return false;
		}
		rejected = eligible;
		rejected.dyingIsChild = true;
		if (!mustReject(rejected)) {
			return false;
		}
		rejected = eligible;
		rejected.killerIsPlayerOwned = false;
		if (!mustReject(rejected)) {
			return false;
		}
		rejected = eligible;
		rejected.hostileToPlayerOwner = false;
		if (!mustReject(rejected)) {
			return false;
		}

		const auto bothAllowed = CalamityAffixes::detail::ResolveCorpseCurrencyRollAllowance(0u, false, false);
		const auto runewordPresent = CalamityAffixes::detail::ResolveCorpseCurrencyRollAllowance(0u, true, false);
		const auto runewordProcessed = CalamityAffixes::detail::ResolveCorpseCurrencyRollAllowance(CalamityAffixes::detail::kCorpseCurrencyRunewordProcessed, false, false);
		const auto allProcessed = CalamityAffixes::detail::ResolveCorpseCurrencyRollAllowance(CalamityAffixes::detail::kCorpseCurrencyAllProcessed, false, false);
		if (!bothAllowed.runeword || !bothAllowed.reforge ||
			runewordPresent.runeword || !runewordPresent.reforge ||
			runewordProcessed.runeword || !runewordProcessed.reforge ||
			allProcessed.runeword || allProcessed.reforge) {
			std::cerr << "corpse_currency_policy: category-specific migration/duplicate allowance is incorrect\n";
			return false;
		}

		if (!CalamityAffixes::detail::IsLootCurrencyLedgerExpired(5u, 35u, 30u) ||
			CalamityAffixes::detail::IsLootCurrencyLedgerExpired(6u, 35u, 30u) ||
			CalamityAffixes::detail::IsLootCurrencyLedgerExpired(10u, 9u, 30u)) {
			std::cerr << "corpse_currency_policy: lifecycle TTL behavior is incorrect\n";
			return false;
		}

		const fs::path testFile{ __FILE__ };
		const fs::path repoRoot = testFile.parent_path().parent_path();
		auto loadText = [](const fs::path& path) -> std::optional<std::string> {
			std::ifstream in(path);
			if (!in.is_open()) {
				return std::nullopt;
			}
			return std::string(
				(std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());
		};

		const auto stateText = loadText(repoRoot / "include" / "CalamityAffixes" / "LootRuntimeState.h");
		const auto constantsText = loadText(repoRoot / "include" / "CalamityAffixes" / "detail" / "EventBridge.Constants.inl");
		const auto serviceText = loadText(repoRoot / "src" / "EventBridge.Loot.Service.cpp");
		const auto deathText = loadText(repoRoot / "src" / "EventBridge.Triggers.DeathEvent.cpp");
		const auto hitText = loadText(repoRoot / "src" / "EventBridge.Triggers.HitEvent.cpp");
		const auto eventsDetailText = loadText(repoRoot / "src" / "EventBridge.Triggers.Events.Detail.h");
		const auto activateText = loadText(repoRoot / "src" / "EventBridge.Triggers.ActivateEvent.cpp");
		const auto craftingText = loadText(repoRoot / "src" / "EventBridge.Loot.Runeword.Crafting.cpp");
		if (!stateText || !constantsText || !serviceText || !deathText || !hitText || !eventsDetailText ||
			!activateText || !craftingText) {
			std::cerr << "corpse_currency_policy: failed to load runtime sources\n";
			return false;
		}

		if (stateText->find("corpseCurrencyRollLedger{}") == std::string::npos ||
			stateText->find("runewordFragmentFailStreak") == std::string::npos ||
			stateText->find("reforgeOrbFailStreak") == std::string::npos ||
			constantsText->find("kSerializationRecordCorpseCurrencyRuntime") == std::string::npos ||
			serviceText->find("GetCorpseCurrencyProcessedMask(") == std::string::npos ||
			serviceText->find("MarkCorpseCurrencyProcessed(") == std::string::npos ||
			serviceText->find("ExecuteCorpseCurrencyDropRolls(") == std::string::npos ||
			deathText->find("IsCorpseRuntimeCurrencyDropEligible(") == std::string::npos ||
			deathText->find("SnapshotCorpseCurrencyInventory(dying)") == std::string::npos ||
			deathText->find("ForgetCorpseCurrencyProcessed(dying->GetFormID())") == std::string::npos ||
			deathText->find("ExecuteCorpseCurrencyDropRolls(") == std::string::npos ||
			eventsDetailText->find("a_ref->As<RE::Projectile>()") == std::string::npos ||
			eventsDetailText->find("RE::NiPointer<RE::Actor> ResolveActorFromCombatRef") == std::string::npos ||
			deathText->find("const auto killerHolder = ResolveActorFromCombatRef(killerRef);") == std::string::npos ||
			hitText->find("const auto aggressorHolder = ResolveActorFromCombatRef(causeRef);") == std::string::npos ||
			eventsDetailText->find("a_ref->GetActorCause()") != std::string::npos ||
			activateText->find("ExecuteCorpseCurrencyDropRolls(") != std::string::npos ||
			activateText->find("ResolveLootCurrencySourceTier(") != std::string::npos ||
			craftingText->find("TryAddLootCurrencyToCorpseInventory(") == std::string::npos ||
			craftingText->find("AddObjectToContainer(a_dropItem") == std::string::npos ||
			craftingText->find("PlaceObjectAtMe(a_dropItem") != std::string::npos) {
			std::cerr << "corpse_currency_policy: corpse-only runtime source contract is incomplete\n";
			return false;
		}

		const auto countOccurrences = [](std::string_view text, std::string_view needle) {
			std::size_t count = 0u;
			for (std::size_t pos = 0u; (pos = text.find(needle, pos)) != std::string_view::npos;
				pos += needle.size()) {
				++count;
			}
			return count;
		};
		if (countOccurrences(*craftingText, "_lootState.runewordFragmentFailStreak = 0;") != 1u ||
			countOccurrences(*craftingText, "_lootState.reforgeOrbFailStreak = 0;") != 1u) {
			std::cerr << "corpse_currency_policy: currency pity may reset outside successful corpse insertion commit\n";
			return false;
		}

		std::string serializationText;
		for (const auto& path : {
			repoRoot / "src" / "EventBridge.Serialization.Save.cpp",
			repoRoot / "src" / "EventBridge.Serialization.Load.cpp",
			repoRoot / "src" / "EventBridge.Serialization.Load.Records.inl",
			repoRoot / "include" / "CalamityAffixes" / "SerializationLoadState.h" }) {
			const auto part = loadText(path);
			if (!part) {
				return false;
			}
			serializationText += *part;
		}
		if (serializationText.find("kSerializationRecordCorpseCurrencyRuntime") == std::string::npos ||
			serializationText.find("_lootState.runewordFragmentFailStreak") == std::string::npos ||
			serializationText.find("_lootState.reforgeOrbFailStreak") == std::string::npos ||
			serializationText.find("ResolveFormID(corpseFormId") == std::string::npos ||
			serializationText.find("RebuildCorpseCurrencyLedgerRecent") == std::string::npos) {
			std::cerr << "corpse_currency_policy: CCRT pity/ledger save-load contract is incomplete\n";
			return false;
		}

		return true;
	}

	bool CheckCorpseCurrencySpecialRewardPolicy()
	{
		namespace fs = std::filesystem;
		using CalamityAffixes::detail::CorpseCurrencyActorTierInput;
		using CalamityAffixes::detail::CorpseCurrencyRewardTier;
		using CalamityAffixes::detail::ResolveCorpseCurrencyRewardPlan;
		using CalamityAffixes::detail::ResolveCorpseCurrencyRewardTier;

		if (ResolveCorpseCurrencyRewardTier({}) != CorpseCurrencyRewardTier::kNormal ||
			ResolveCorpseCurrencyRewardTier(CorpseCurrencyActorTierInput{ .actorBaseIsUnique = true }) != CorpseCurrencyRewardTier::kUnique ||
			ResolveCorpseCurrencyRewardTier(CorpseCurrencyActorTierInput{ .hasBossLocationRefType = true }) != CorpseCurrencyRewardTier::kBoss ||
			ResolveCorpseCurrencyRewardTier(CorpseCurrencyActorTierInput{ .actorIsDragon = true }) != CorpseCurrencyRewardTier::kBoss ||
			ResolveCorpseCurrencyRewardTier(CorpseCurrencyActorTierInput{ .hasBossLocationRefType = true, .actorBaseIsUnique = true }) != CorpseCurrencyRewardTier::kBoss ||
			ResolveCorpseCurrencyRewardTier(CorpseCurrencyActorTierInput{ .actorIsDragon = true, .actorBaseIsUnique = true }) != CorpseCurrencyRewardTier::kBoss) {
			std::cerr << "corpse_currency_special_reward: boss/dragon/unique tier precedence is incorrect\n";
			return false;
		}

		const auto normal = ResolveCorpseCurrencyRewardPlan(
			CorpseCurrencyRewardTier::kNormal, true, true, 0.0f, 40.0f);
		const auto uniqueRune = ResolveCorpseCurrencyRewardPlan(
			CorpseCurrencyRewardTier::kUnique, true, true, 39.999f, 40.0f);
		const auto uniqueOrb = ResolveCorpseCurrencyRewardPlan(
			CorpseCurrencyRewardTier::kUnique, true, true, 40.0f, 40.0f);
		const auto uniqueRuneOnly = ResolveCorpseCurrencyRewardPlan(
			CorpseCurrencyRewardTier::kUnique, true, false, 99.0f, 40.0f);
		const auto uniqueOrbOnly = ResolveCorpseCurrencyRewardPlan(
			CorpseCurrencyRewardTier::kUnique, false, true, 0.0f, 40.0f);
		const auto boss = ResolveCorpseCurrencyRewardPlan(
			CorpseCurrencyRewardTier::kBoss, true, true, 0.0f, 40.0f);
		const auto blockedBoss = ResolveCorpseCurrencyRewardPlan(
			CorpseCurrencyRewardTier::kBoss, false, false, 0.0f, 40.0f);

		if (!normal.useNormalRandomRolls || normal.grantRunewordFragment || normal.grantReforgeOrb ||
			uniqueRune.useNormalRandomRolls || !uniqueRune.grantRunewordFragment || uniqueRune.grantReforgeOrb ||
			uniqueOrb.useNormalRandomRolls || uniqueOrb.grantRunewordFragment || !uniqueOrb.grantReforgeOrb ||
			!uniqueRuneOnly.grantRunewordFragment || uniqueRuneOnly.grantReforgeOrb ||
			uniqueOrbOnly.grantRunewordFragment || !uniqueOrbOnly.grantReforgeOrb ||
			boss.useNormalRandomRolls || !boss.grantRunewordFragment || !boss.grantReforgeOrb ||
			blockedBoss.useNormalRandomRolls || blockedBoss.grantRunewordFragment || blockedBoss.grantReforgeOrb) {
			std::cerr << "corpse_currency_special_reward: guaranteed reward plan is incorrect\n";
			return false;
		}

		const fs::path testFile{ __FILE__ };
		const fs::path repoRoot = testFile.parent_path().parent_path();
		auto loadText = [](const fs::path& path) -> std::optional<std::string> {
			std::ifstream in(path);
			if (!in.is_open()) {
				return std::nullopt;
			}
			return std::string(
				(std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());
		};

		const auto deathText = loadText(repoRoot / "src" / "EventBridge.Triggers.DeathEvent.cpp");
		const auto serviceText = loadText(repoRoot / "src" / "EventBridge.Loot.Service.cpp");
		if (!deathText || !serviceText ||
			deathText->find("kLocRefTypeBoss") == std::string::npos ||
			deathText->find("actorBase->IsUnique()") == std::string::npos ||
			deathText->find("ResolveCorpseCurrencyRewardPlan(") == std::string::npos ||
			deathText->find("ExecuteGuaranteedCorpseCurrencyDrops(") == std::string::npos) {
			std::cerr << "corpse_currency_special_reward: runtime tier/grant integration is incomplete\n";
			return false;
		}
		if (deathText->find("defaultObjects->GetObject") != std::string::npos ||
			deathText->find("defaultObjects->objects[bossObjectIndex]") == std::string::npos) {
			std::cerr << "corpse_currency_special_reward: boss probe must bypass the unsafe default-object initialization helper\n";
			return false;
		}

		const auto normalRollStart = serviceText->find(
			"EventBridge::CurrencyRollExecutionResult EventBridge::ExecuteCorpseCurrencyDropRolls(");
		const auto guaranteedStart = serviceText->find(
			"EventBridge::CurrencyRollExecutionResult EventBridge::ExecuteGuaranteedCorpseCurrencyDrops(");
		if (normalRollStart == std::string::npos ||
			guaranteedStart == std::string::npos ||
			normalRollStart >= guaranteedStart) {
			return false;
		}
		const auto normalRollBody = std::string_view(*serviceText).substr(
			normalRollStart,
			guaranteedStart - normalRollStart);
		if (normalRollBody.find("if (a_allowRunewordRoll)") == std::string_view::npos ||
			normalRollBody.find("if (a_allowReforgeRoll)") == std::string_view::npos ||
			normalRollBody.find("else if (a_allowReforgeRoll)") != std::string_view::npos) {
			std::cerr << "corpse_currency_special_reward: normal fragment and orb rolls must remain independent\n";
			return false;
		}

		const auto guaranteedBody = std::string_view(*serviceText).substr(guaranteedStart);
		if (guaranteedBody.find("CommitRunewordFragmentGrant(") != std::string_view::npos ||
			guaranteedBody.find("CommitReforgeOrbGrant(") != std::string_view::npos ||
			guaranteedBody.find("TryRollRunewordFragmentToken(") != std::string_view::npos ||
			guaranteedBody.find("TryRollReforgeOrbGrant(") != std::string_view::npos) {
			std::cerr << "corpse_currency_special_reward: guaranteed rewards must not consume normal pity\n";
			return false;
		}

		return true;
	}

	bool CheckLootServiceExtractionPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path repoRoot = testFile.parent_path().parent_path();
		const fs::path cmakeFile = repoRoot / "CMakeLists.txt";
		const fs::path assignFile = repoRoot / "src" / "EventBridge.Loot.Assign.cpp";
		const fs::path serviceFile = repoRoot / "src" / "EventBridge.Loot.Service.cpp";

		auto loadText = [](const fs::path& path) -> std::optional<std::string> {
			std::ifstream in(path);
			if (!in.is_open()) {
				return std::nullopt;
			}
			return std::string(
				(std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());
		};

		const auto cmakeText = loadText(cmakeFile);
		const auto assignText = loadText(assignFile);
		const auto serviceText = loadText(serviceFile);
		if (!cmakeText.has_value() || !assignText.has_value() || !serviceText.has_value()) {
			std::cerr << "loot_service_extraction: failed to load source files\n";
			return false;
		}

		if (cmakeText->find("src/EventBridge.Loot.Service.cpp") == std::string::npos ||
			assignText->find("void EventBridge::EnsureMultiAffixDisplayName(") == std::string::npos ||
			assignText->find("void EventBridge::CleanupInvalidLootInstance(") == std::string::npos ||
			assignText->find("void EventBridge::SanitizeAllTrackedLootInstancesForCurrentLootRules(") == std::string::npos ||
			assignText->find("EventBridge::CurrencyRollExecutionResult EventBridge::ExecuteCorpseCurrencyDropRolls(") != std::string::npos ||
			assignText->find("bool EventBridge::RollLootChanceGateForEligibleInstance()") != std::string::npos ||
			assignText->find("bool EventBridge::IsLootObjectEligibleForAffixes(") != std::string::npos ||
			serviceText->find("EventBridge::CurrencyRollExecutionResult EventBridge::ExecuteCorpseCurrencyDropRolls(") == std::string::npos ||
			serviceText->find("bool EventBridge::RollLootChanceGateForEligibleInstance()") == std::string::npos ||
			serviceText->find("bool EventBridge::IsLootObjectEligibleForAffixes(") == std::string::npos ||
			serviceText->find("bool EventBridge::TryBeginLootCurrencyLedgerRoll(") == std::string::npos ||
			serviceText->find("void EventBridge::ProcessLootAcquired(") == std::string::npos) {
			std::cerr << "loot_service_extraction: loot service/mutation responsibilities are not cleanly separated\n";
			return false;
		}

		return true;
	}

	bool CheckLootDisplayNameExtractionPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path assignFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Loot.Assign.cpp";

		std::ifstream in(assignFile);
		if (!in.is_open()) {
			std::cerr << "loot_display_name_extraction: failed to open assign source: " << assignFile << "\n";
			return false;
		}

		const std::string source(
			(std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());

		const auto resolvePos = source.find("std::string EventBridge::ResolveStoredLootDisplayBaseName(");
		const auto stripPos = source.find("std::string EventBridge::StripKnownLootAffixTags(");
		const auto markerPos = source.find("std::string EventBridge::BuildLootNameMarker(");
		const auto ensurePos = source.find("void EventBridge::EnsureMultiAffixDisplayName(");
		const auto resolveCallPos = source.find("ResolveStoredLootDisplayBaseName(a_entry, a_xList, &storedCustomName)", ensurePos);
		const auto markerCallPos = source.find("BuildLootNameMarker(a_slots.count)", ensurePos);

		if (resolvePos == std::string::npos ||
			stripPos == std::string::npos ||
			markerPos == std::string::npos ||
			ensurePos == std::string::npos ||
			resolveCallPos == std::string::npos ||
			markerCallPos == std::string::npos) {
			std::cerr << "loot_display_name_extraction: expected display-name helpers or calls are missing\n";
			return false;
		}

		if (!(resolvePos < stripPos && stripPos < markerPos && markerPos < ensurePos &&
			ensurePos < resolveCallPos && resolveCallPos < markerCallPos)) {
			std::cerr << "loot_display_name_extraction: helper extraction order regressed\n";
			return false;
		}

		if (source.find("auto stripKnownAffixTags = [&](std::string_view a_name)", ensurePos) != std::string::npos ||
			source.find("std::string marker;", ensurePos) != std::string::npos) {
			std::cerr << "loot_display_name_extraction: EnsureMultiAffixDisplayName regained inline parsing/marker state\n";
			return false;
		}

		return true;
	}

	bool CheckLootTrackedSanitizeExtractionPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path assignFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Loot.Assign.cpp";

		std::ifstream in(assignFile);
		if (!in.is_open()) {
			std::cerr << "loot_tracked_sanitize_extraction: failed to open assign source: " << assignFile << "\n";
			return false;
		}

		const std::string source(
			(std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());

		const auto perInstancePos = source.find("bool EventBridge::SanitizeTrackedLootInstanceForCurrentLootRules(");
		const auto summaryPos = source.find("void EventBridge::LogTrackedLootSanitizationSummary(");
		const auto aggregatePos = source.find("void EventBridge::SanitizeAllTrackedLootInstancesForCurrentLootRules(");
		const auto helperCallPos = source.find("SanitizeTrackedLootInstanceForCurrentLootRules(it, a_context, sanitizedInstances, erasedInstances)", aggregatePos);
		const auto summaryCallPos = source.find("LogTrackedLootSanitizationSummary(a_context, sanitizedInstances, erasedInstances);", aggregatePos);

		if (perInstancePos == std::string::npos ||
			summaryPos == std::string::npos ||
			aggregatePos == std::string::npos ||
			helperCallPos == std::string::npos ||
			summaryCallPos == std::string::npos) {
			std::cerr << "loot_tracked_sanitize_extraction: expected tracked-loot sanitize helpers or calls are missing\n";
			return false;
		}

		if (!(perInstancePos < summaryPos && summaryPos < aggregatePos &&
			aggregatePos < helperCallPos && helperCallPos < summaryCallPos)) {
			std::cerr << "loot_tracked_sanitize_extraction: tracked-loot sanitize helper order regressed\n";
			return false;
		}

		if (source.find("ForgetLootEvaluatedInstance(it->first);", aggregatePos) != std::string::npos ||
			source.find("_instanceTrackingState.instanceAffixes.erase(it);", aggregatePos) != std::string::npos ||
			source.find("sanitized tracked loot instances (context={}, changed={}, erased={}).", aggregatePos) != std::string::npos) {
			std::cerr << "loot_tracked_sanitize_extraction: aggregate sanitize function regained inline erase/logging responsibilities\n";
			return false;
		}

		return true;
	}

	bool CheckLootSlotSanitizeHelperExtractionPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path assignFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Loot.Assign.cpp";

		std::ifstream in(assignFile);
		if (!in.is_open()) {
			std::cerr << "loot_slot_sanitize_helper_extraction: failed to open assign source: " << assignFile << "\n";
			return false;
		}

		const std::string source(
			(std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());

		const auto keepPos = source.find("bool EventBridge::ShouldKeepTrackedLootAffixToken(");
		const auto equalPos = source.find("bool EventBridge::InstanceAffixSlotsEqual(");
		const auto applyPos = source.find("void EventBridge::ApplySanitizedInstanceAffixSlots(");
		const auto sanitizePos = source.find("bool EventBridge::SanitizeInstanceAffixSlotsForCurrentLootRules(");
		const auto keepCallPos = source.find("return ShouldKeepTrackedLootAffixToken(a_token);", sanitizePos);
		const auto equalCallPos = source.find("if (InstanceAffixSlotsEqual(a_slots, sanitized)) {", sanitizePos);
		const auto applyCallPos = source.find("ApplySanitizedInstanceAffixSlots(a_instanceKey, a_slots, sanitized, removedTokens, removedCount);", sanitizePos);

		if (keepPos == std::string::npos ||
			equalPos == std::string::npos ||
			applyPos == std::string::npos ||
			sanitizePos == std::string::npos ||
			keepCallPos == std::string::npos ||
			equalCallPos == std::string::npos ||
			applyCallPos == std::string::npos) {
			std::cerr << "loot_slot_sanitize_helper_extraction: expected sanitize helpers or calls are missing\n";
			return false;
		}

		if (!(keepPos < equalPos && equalPos < applyPos && applyPos < sanitizePos &&
			sanitizePos < keepCallPos && keepCallPos < equalCallPos && equalCallPos < applyCallPos)) {
			std::cerr << "loot_slot_sanitize_helper_extraction: sanitize helper order regressed\n";
			return false;
		}

		if (source.find("const auto idxIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(a_token);", sanitizePos) != std::string::npos ||
			source.find("for (std::uint8_t i = 0; i < a_slots.count; ++i)", sanitizePos) != std::string::npos ||
			source.find("_instanceTrackingState.instanceStates.erase(MakeInstanceStateKey(a_instanceKey, removedToken));", sanitizePos) != std::string::npos) {
			std::cerr << "loot_slot_sanitize_helper_extraction: sanitize function regained inline token/equality/apply responsibilities\n";
			return false;
		}

		return true;
	}

	bool CheckSerializationTransientRuntimeResetPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path repoRoot = testFile.parent_path().parent_path();
		const fs::path serializationLoadFile = repoRoot / "src" / "EventBridge.Serialization.Load.cpp";
		const fs::path serializationLifecycleFile = repoRoot / "src" / "EventBridge.Serialization.Lifecycle.cpp";

		auto loadFile = [](const fs::path& path) -> std::optional<std::string> {
			std::ifstream in(path);
			if (!in.is_open()) {
				return std::nullopt;
			}
			return std::string(
				(std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());
		};

		// Patterns must appear in both Load and Revert (now split across two files).
		std::string source;
		for (const auto& sf : { serializationLoadFile, serializationLifecycleFile }) {
			const auto part = loadFile(sf);
			if (!part.has_value()) {
				std::cerr << "serialization_transient_reset: failed to open source file: " << sf << "\n";
				return false;
			}
			source += *part;
		}

		const auto countOccurrences = [&](std::string_view needle) -> std::size_t {
			std::size_t count = 0;
			std::size_t pos = 0;
			while ((pos = source.find(needle, pos)) != std::string::npos) {
				++count;
				pos += needle.size();
			}
			return count;
		};

		const std::array<std::string_view, 3> requiredBothLoadAndRevert{
			"_affixRuntimeState.activeCounts.clear();",
			"_affixRuntimeState.activeHitTriggerAffixIndices.clear();",
			"_combatState.ResetTransientState();",
		};

		for (const auto needle : requiredBothLoadAndRevert) {
			if (countOccurrences(needle) < 2) {
				std::cerr << "serialization_transient_reset: expected Load/Revert reset guard missing for pattern: " << needle << "\n";
				return false;
			}
		}

		return true;
	}

	bool CheckSuffixProcChanceParsingPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path parsingFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Config.AffixParsing.cpp";

		std::ifstream in(parsingFile);
		if (!in.is_open()) {
			std::cerr << "suffix_proc_chance_parsing: failed to open parsing source: " << parsingFile << "\n";
			return false;
		}

		const std::string source(
			(std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());

		if (source.find("detail::ResolveSuffixProcChancePct()") == std::string::npos ||
			source.find("detail::ResolveAffixKidLootWeight(") == std::string::npos) {
			std::cerr << "suffix_proc_chance_parsing: extracted suffix/loot parsing policies must remain wired\n";
			return false;
		}

		return true;
	}

	bool CheckSerializationDrainSafetyPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path repoRoot = testFile.parent_path().parent_path();
		const fs::path serializationLoadFile = repoRoot / "src" / "EventBridge.Serialization.Load.cpp";
		const fs::path serializationLoadRecordsFile = repoRoot / "src" / "EventBridge.Serialization.Load.Records.inl";

		auto loadText = [](const fs::path& path) -> std::optional<std::string> {
			std::ifstream in(path);
			if (!in.is_open()) {
				return std::nullopt;
			}
			return std::string(
				(std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());
		};

		const auto loadTextMain = loadText(serializationLoadFile);
		const auto loadTextRecords = loadText(serializationLoadRecordsFile);
		if (!loadTextMain.has_value() || !loadTextRecords.has_value()) {
			std::cerr << "serialization_drain_safety: failed to open serialization sources\n";
			return false;
		}

		const std::string source = *loadTextMain + *loadTextRecords;

		if (source.find("bool DrainRecordBytes(") == std::string::npos ||
			source.find("detail::ShouldWarnUnusuallyLargeSerializationDrain(") == std::string::npos ||
			source.find("detail::ResolveSerializationDrainChunkSize(") == std::string::npos ||
			source.find("DrainRemaining(") == std::string::npos ||
			source.find("\"unsupported-iaxf-version\"") == std::string::npos) {
			std::cerr << "serialization_drain_safety: bounded drain policy must remain wired for recovery paths\n";
			return false;
		}

		return true;
	}

	bool CheckSpecialActionProcSafetyPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path repoRoot = testFile.parent_path().parent_path();
		const fs::path specialFile = repoRoot / "src" / "EventBridge.Actions.Special.cpp";
		const fs::path archmageFile = repoRoot / "src" / "EventBridge.Actions.Archmage.cpp";

		auto loadFile = [](const fs::path& path) -> std::optional<std::string> {
			std::ifstream in(path);
			if (!in.is_open()) {
				return std::nullopt;
			}
			return std::string(
				(std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());
		};

		// Special + Archmage are split but share proc safety patterns.
		std::string source;
		for (const auto& sf : { specialFile, archmageFile }) {
			const auto part = loadFile(sf);
			if (!part.has_value()) {
				std::cerr << "special_action_proc_safety: failed to open source: " << sf << "\n";
				return false;
			}
			source += *part;
		}

		if (source.find("ResolveSpecialActionProcChancePct") == std::string::npos ||
			source.find("RollProcChance") == std::string::npos ||
			source.find("PassesLuckyHitGate") == std::string::npos ||
			source.find("if (_combatState.procDepth > 0)") == std::string::npos ||
			source.find("detail::IsCalamityProcSource(") == std::string::npos ||
			source.find("ResolveArchmageResourceUsage(") == std::string::npos ||
			source.find("ExecuteArchmageCast(") == std::string::npos) {
			std::cerr << "special_action_proc_safety: special-action proc/lucky-hit/recursion guards are missing\n";
			return false;
		}

		return true;
	}

	bool CheckCorpseExplosionBudgetSafetyPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path corpseFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Actions.Corpse.cpp";
		const fs::path parserFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Config.AffixActionParsing.cpp";

		std::ifstream in(corpseFile);
		std::ifstream parserIn(parserFile);
		if (!in.is_open() || !parserIn.is_open()) {
			std::cerr << "corpse_explosion_budget_safety: failed to open corpse-actions/parser sources\n";
			return false;
		}

		const std::string source(
			(std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());

		const std::string parserSource(
			(std::istreambuf_iterator<char>(parserIn)),
			std::istreambuf_iterator<char>());

		const auto budgetPos = source.find("if (!TryConsumeCorpseExplosionBudget(");
		const auto icdPos = source.find("bestAffix.nextAllowed = now + bestAffix.icd;");
		if (budgetPos == std::string::npos ||
			source.find("CorpseExplosionBudgetDenyReason::kDuplicateCorpse") == std::string::npos ||
			source.find("CorpseExplosionBudgetDenyReason::kRateLimited") == std::string::npos ||
			source.find("CorpseExplosionBudgetDenyReason::kChainDepthExceeded") == std::string::npos ||
			source.find("seenCorpses.size() > 512u") == std::string::npos ||
			source.find("kCorpseExplosionDefaultMaxTargets = 48u") == std::string::npos ||
			source.find("RollProcChance(_rng, _rngMutex, chancePct)") == std::string::npos ||
			source.find("detail::IsPreferredCorpseExplosionCandidate(") == std::string::npos ||
			source.find("selection.bestSelectionPriority = action.corpseExplosionSelectionPriority;") == std::string::npos ||
			parserSource.find("detail::ClampCorpseExplosionSelectionPriority(") == std::string::npos ||
			parserSource.find("a_action.value(\"selectionPriority\", 0)") == std::string::npos ||
			source.find("selection.bestUsesPerTargetIcd") == std::string::npos ||
			source.find("if (selection.bestUsesPerTargetIcd && targetsHit > 0u)") == std::string::npos ||
			icdPos == std::string::npos ||
			icdPos < budgetPos) {
			std::cerr << "corpse_explosion_budget_safety: corpse budget/rate-limit/target-cap guards are missing\n";
			return false;
		}

		return true;
	}

	bool CheckLootRerollExploitGuardPolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path lootFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Loot.cpp";
		const fs::path lifecycleFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Serialization.Lifecycle.cpp";
		const fs::path baseSelectionFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Loot.Runeword.BaseSelection.cpp";
		const fs::path runtimeSettingsHandlerFile = testFile.parent_path().parent_path() / "src" / "EventBridge.Triggers.ModCallback.Handlers.cpp";

		const auto loadText = [](const fs::path& a_path) -> std::optional<std::string> {
			std::ifstream in(a_path);
			if (!in.is_open()) {
				return std::nullopt;
			}
			return std::string(
				(std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());
		};
		const auto source = loadText(lootFile);
		const auto lifecycleSource = loadText(lifecycleFile);
		const auto baseSelectionSource = loadText(baseSelectionFile);
		const auto runtimeSettingsHandlerSource = loadText(runtimeSettingsHandlerFile);
		if (!source || !lifecycleSource || !baseSelectionSource || !runtimeSettingsHandlerSource) {
			std::cerr << "loot_reroll_exploit_guard: failed to open instance ownership sources\n";
			return false;
		}

		if (source->find("IsLootSourceCorpseChestOrWorld(") == std::string::npos ||
			source->find("_lootState.rerollGuard.ConsumePlayerDropPickupInstanceKey(") == std::string::npos ||
			source->find("MakeDetachedWorldInstanceKey(referenceFormID)") == std::string::npos ||
			source->find("IsDetachedWorldReferenceMatch(") == std::string::npos ||
			source->find("if (dropTransferDecision == DropTransferDecision::kConflictRetain)") == std::string::npos ||
			source->find("if (dropSourceKey == 0u)") == std::string::npos ||
			source->find("TrackPendingDropOwnership(") == std::string::npos ||
			source->find("if (dropTransferDecision == DropTransferDecision::kRemap)") == std::string::npos ||
			source->find("if (HasMaterialInstanceState(pickupKey))") == std::string::npos ||
			source->find("EstablishPendingDropReferenceIdentity(") == std::string::npos ||
			source->find("if (RemapInstanceKey(detachedWorldKey, pickupKey))") == std::string::npos ||
			source->find("_lootState.playerContainerStash[stashKey] += a_event->itemCount;") == std::string::npos ||
			source->find("skipping loot roll (player dropped + re-picked)") == std::string::npos ||
			source->find("skipping loot roll (player stashed + retrieved)") == std::string::npos) {
			std::cerr << "loot_reroll_exploit_guard: anti-reroll pickup guards are missing\n";
			return false;
		}

		if (source->find("const auto oldKey = MakeInstanceKey(oldOwnerID, a_event->oldUniqueID);") == std::string::npos ||
			source->find("const auto newKey = MakeInstanceKey(newOwnerID, a_event->newUniqueID);") == std::string::npos ||
			source->find("const auto legacyItemKey = MakeInstanceKey(itemFormID, a_event->oldUniqueID);") == std::string::npos ||
			source->find("if (materialOld && materialLegacyItemKey)") == std::string::npos ||
			source->find(".legacyItemKeyTracked = policyLegacyTracked") == std::string::npos ||
			source->find("if (HasMaterialInstanceState(a_newKey))") == std::string::npos ||
			source->find("if (!RemapInstanceKey(transferPlan.oldKey, transferPlan.newKey))") == std::string::npos ||
			source->find("if (pendingTransfer == PendingDropTransferResult::kConflict)") == std::string::npos ||
			source->find("AddToken(affixNode") != std::string::npos ||
			baseSelectionSource->find("if (!ResolvePlayerInventoryInstance(a_instanceKey, entry, xList)") == std::string::npos ||
			lifecycleSource->find("bool EventBridge::NormalizeLegacyPlayerInstanceKeys()") == std::string::npos ||
			lifecycleSource->find("bool EventBridge::PruneOrphanedPlayerInstanceKeys()") == std::string::npos ||
			lifecycleSource->find("(void)PruneOrphanedPlayerInstanceKeys();") == std::string::npos ||
			lifecycleSource->find("RebuildActiveCounts(true);") == std::string::npos ||
			lifecycleSource->find("IsOrphanedPlayerInstanceKey(") == std::string::npos ||
			lifecycleSource->find("uidUseCounts[uid->uniqueID]") == std::string::npos ||
			lifecycleSource->find("if (duplicateUID)") == std::string::npos ||
			lifecycleSource->find("DiscardInstanceKeyState(canonicalKey);") == std::string::npos ||
			lifecycleSource->find("uid->baseID != itemFormID") == std::string::npos ||
			lifecycleSource->find("RemapInstanceKey(legacyKey, canonicalKey);") == std::string::npos ||
			lifecycleSource->find("uid->baseID = playerID;") == std::string::npos ||
			runtimeSettingsHandlerSource->find("(void)PruneOrphanedPlayerInstanceKeys();") == std::string::npos ||
			runtimeSettingsHandlerSource->find("DeactivateRuntimeState();") == std::string::npos) {
			std::cerr << "loot_reroll_exploit_guard: owner+UID transfer or legacy normalization policy is incomplete\n";
			return false;
		}

		return true;
	}

	bool CheckLootChanceMcmCleanupPolicy()
	{
		if (CalamityAffixes::RuntimePolicy::IsPersistedRuntimeUserSettingEvent("CalamityAffixes_MCM_SetLootChance")) {
			std::cerr << "loot_chance_mcm_cleanup: deprecated loot chance event must not be in persisted runtime-setting events\n";
			return false;
		}

		return true;
	}

	bool CheckMcmDropChanceRuntimeBridgePolicy()
	{
		namespace fs = std::filesystem;
		const fs::path testFile{ __FILE__ };
		const fs::path repoRoot = testFile.parent_path().parent_path().parent_path().parent_path();
		const fs::path configJsonFile = repoRoot / "Data" / "MCM" / "Config" / "CalamityAffixes" / "config.json";
		const fs::path mcmConfigScriptFile = repoRoot / "Data" / "Scripts" / "Source" / "CalamityAffixes_MCMConfig.psc";
		const fs::path modeControlScriptFile = repoRoot / "Data" / "Scripts" / "Source" / "CalamityAffixes_ModeControl.psc";
			const fs::path triggerDispatchFile = repoRoot / "skse" / "CalamityAffixes" / "src" / "EventBridge.Triggers.ModCallback.cpp";
			const fs::path triggerHandlersFile = repoRoot / "skse" / "CalamityAffixes" / "src" / "EventBridge.Triggers.ModCallback.Handlers.cpp";

		auto loadText = [](const fs::path& path) -> std::optional<std::string> {
			std::ifstream in(path);
			if (!in.is_open()) {
				return std::nullopt;
			}
			return std::string(
				(std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());
		};

		const auto configJsonText = loadText(configJsonFile);
		if (!configJsonText.has_value()) {
			std::cerr << "mcm_drop_chance_bridge: failed to open MCM config: " << configJsonFile << "\n";
			return false;
		}
		if (configJsonText->find("\"id\": \"fRunewordFragmentChancePercent:General\"") == std::string::npos ||
			configJsonText->find("\"function\": \"SetRunewordFragmentChancePercent\"") == std::string::npos ||
			configJsonText->find("\"id\": \"fReforgeOrbChancePercent:General\"") == std::string::npos ||
			configJsonText->find("\"function\": \"SetReforgeOrbChancePercent\"") == std::string::npos) {
			std::cerr << "mcm_drop_chance_bridge: MCM slider/action wiring is missing\n";
			return false;
		}
		const auto countOccurrences = [](std::string_view haystack, std::string_view needle) -> std::size_t {
			std::size_t count = 0;
			std::size_t pos = 0;
			while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
				++count;
				pos += needle.size();
			}
			return count;
		};
		if (countOccurrences(*configJsonText, "\"formatString\": \"{1}%\"") < 2 ||
			configJsonText->find("\"formatString\": \"%.1f%%\"") != std::string::npos) {
			std::cerr << "mcm_drop_chance_bridge: chance slider formatString must use MCM Helper token syntax ({1}%).\n";
			return false;
		}

		const auto mcmConfigText = loadText(mcmConfigScriptFile);
		if (!mcmConfigText.has_value()) {
			std::cerr << "mcm_drop_chance_bridge: failed to open MCM script: " << mcmConfigScriptFile << "\n";
			return false;
		}
		if (mcmConfigText->find("Event OnSettingChange(string a_ID)") == std::string::npos ||
			mcmConfigText->find("a_ID == RunewordFragmentChanceSettingName || a_ID == ReforgeOrbChanceSettingName") == std::string::npos ||
			mcmConfigText->find("float runewordChance = GetModSettingFloat(RunewordFragmentChanceSettingName)") == std::string::npos ||
			mcmConfigText->find("runewordChance = RunewordFragmentChanceDefault") == std::string::npos ||
			mcmConfigText->find("float reforgeChance = GetModSettingFloat(ReforgeOrbChanceSettingName)") == std::string::npos ||
			mcmConfigText->find("reforgeChance = ReforgeOrbChanceDefault") == std::string::npos) {
			std::cerr << "mcm_drop_chance_bridge: MCM setting-change/sentinel fallback guard is missing\n";
			return false;
		}

		const auto modeControlText = loadText(modeControlScriptFile);
		if (!modeControlText.has_value()) {
			std::cerr << "mcm_drop_chance_bridge: failed to open mode-control script: " << modeControlScriptFile << "\n";
			return false;
		}
		// Papyrus must emit exactly the event names the runtime listens for, and each
		// drop-chance event must persist to user_settings.json.
		for (const auto eventName : {
				 CalamityAffixes::RuntimePolicy::kMcmSetRunewordFragmentChanceEvent,
				 CalamityAffixes::RuntimePolicy::kMcmSetReforgeOrbChanceEvent,
				 CalamityAffixes::RuntimePolicy::kMcmSetIdentifyScrollChanceEvent,
				 CalamityAffixes::RuntimePolicy::kMcmSetScouringOrbChanceEvent }) {
			const std::string emitCall = "Emit(\"" + std::string(eventName) + "\"";
			if (modeControlText->find(emitCall) == std::string::npos ||
				!CalamityAffixes::RuntimePolicy::IsPersistedRuntimeUserSettingEvent(eventName)) {
				std::cerr << "mcm_drop_chance_bridge: mode-control does not emit persisted event " << eventName << "\n";
				return false;
			}
		}

		if (!CalamityAffixes::RuntimePolicy::IsPersistedRuntimeUserSettingEvent("CalamityAffixes_MCM_SetRunewordFragmentChance") ||
			!CalamityAffixes::RuntimePolicy::IsPersistedRuntimeUserSettingEvent("CalamityAffixes_MCM_SetReforgeOrbChance")) {
			std::cerr << "mcm_drop_chance_bridge: runtime policy event constants are missing\n";
			return false;
		}

			std::string triggerTextCombined;
			for (const auto& sf : { triggerDispatchFile, triggerHandlersFile }) {
				const auto part = loadText(sf);
				if (!part.has_value()) {
					std::cerr << "mcm_drop_chance_bridge: failed to open trigger source: " << sf << "\n";
					return false;
				}
				triggerTextCombined += *part;
			}
			if (triggerTextCombined.find("eventName == kMcmSetRunewordFragmentChanceEvent") == std::string::npos ||
				triggerTextCombined.find("_loot.runewordFragmentChancePercent = std::clamp(a_numArg, 0.0f, 100.0f);") == std::string::npos ||
				triggerTextCombined.find("eventName == kMcmSetReforgeOrbChanceEvent") == std::string::npos ||
				triggerTextCombined.find("_loot.reforgeOrbChancePercent = std::clamp(a_numArg, 0.0f, 100.0f);") == std::string::npos ||
				triggerTextCombined.find("previousRunewordChance") == std::string::npos ||
				triggerTextCombined.find("previousReforgeChance") == std::string::npos ||
				triggerTextCombined.find("ShouldEmitChanceNotification(") == std::string::npos) {
				std::cerr << "mcm_drop_chance_bridge: trigger event handlers are missing\n";
				return false;
			}

		// Crafting currencies ride the same MCM -> Papyrus -> ModEvent -> runtime chain:
		// every slider id must be read by the MCM script, and its action must exist there.
		for (const auto& [settingId, setter] : {
				 std::pair<std::string_view, std::string_view>{ "fIdentifyScrollChancePercent:General", "SetIdentifyScrollChancePercent" },
				 std::pair<std::string_view, std::string_view>{ "fScouringOrbChancePercent:General", "SetScouringOrbChancePercent" } }) {
			const std::string idField = "\"id\": \"" + std::string(settingId) + "\"";
			const std::string actionField = "\"function\": \"" + std::string(setter) + "\"";
			const std::string settingLiteral = "\"" + std::string(settingId) + "\"";
			const std::string setterDecl = "Function " + std::string(setter) + "(";
			if (configJsonText->find(idField) == std::string::npos ||
				configJsonText->find(actionField) == std::string::npos ||
				mcmConfigText->find(settingLiteral) == std::string::npos ||
				mcmConfigText->find(setterDecl) == std::string::npos ||
				modeControlText->find(setterDecl) == std::string::npos) {
				std::cerr << "mcm_drop_chance_bridge: crafting currency slider wiring is missing: " << settingId << "\n";
				return false;
			}
		}
		if (triggerTextCombined.find("kMcmSetIdentifyScrollChanceEvent") == std::string::npos ||
			triggerTextCombined.find("kMcmSetScouringOrbChanceEvent") == std::string::npos) {
			std::cerr << "mcm_drop_chance_bridge: crafting currency chance handler is missing\n";
			return false;
		}

		return true;
	}

}
