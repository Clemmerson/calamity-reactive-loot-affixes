#if !defined(CALAMITYAFFIXES_EVENTBRIDGE_STATE_GROUPS_INL_CONTEXT)
#error "Do not include EventBridge.StateGroups.inl directly; include CalamityAffixes/EventBridge.h"
#endif

		struct AffixRuntimeCacheState
		{
			std::vector<AffixRuntime> affixes{};
			std::vector<std::uint32_t> activeCounts{};
			// ESP FULL names are ASCII-only by design (ToPluginSafeName), so localized
			// record display names come from the runtime config's records specs:
			// formID -> { english, korean } halves of "Calamity: EN / KO".
			std::unordered_map<RE::FormID, std::pair<std::string, std::string>> recordDisplayNames{};
			float activeCritDamageBonusPct{ 0.0f };
			AffixRegistryState affixRegistry{};
			std::vector<std::size_t> activeHitTriggerAffixIndices{};
			std::vector<std::size_t> activeIncomingHitTriggerAffixIndices{};
			std::vector<std::size_t> activeDotApplyTriggerAffixIndices{};
			std::vector<std::size_t> activeKillTriggerAffixIndices{};
			std::vector<std::size_t> activeLowHealthTriggerAffixIndices{};

			void RebuildActiveTriggerIndexCaches()
			{
				const auto rebuildFor = [&](const std::vector<std::size_t>& a_source, std::vector<std::size_t>& a_out) {
					a_out.clear();
					a_out.reserve(a_source.size());
					for (const auto idx : a_source) {
						if (idx >= activeCounts.size() || activeCounts[idx] == 0u) {
							continue;
						}
						a_out.push_back(idx);
					}
				};

				rebuildFor(affixRegistry.hitTriggerAffixIndices, activeHitTriggerAffixIndices);
				rebuildFor(affixRegistry.incomingHitTriggerAffixIndices, activeIncomingHitTriggerAffixIndices);
				rebuildFor(affixRegistry.dotApplyTriggerAffixIndices, activeDotApplyTriggerAffixIndices);
				rebuildFor(affixRegistry.killTriggerAffixIndices, activeKillTriggerAffixIndices);
				rebuildFor(affixRegistry.lowHealthTriggerAffixIndices, activeLowHealthTriggerAffixIndices);
			}

			[[nodiscard]] const std::vector<std::size_t>* ResolveActiveTriggerIndices(Trigger a_trigger) const noexcept
			{
				switch (a_trigger) {
				case Trigger::kHit:
					return &activeHitTriggerAffixIndices;
				case Trigger::kIncomingHit:
					return &activeIncomingHitTriggerAffixIndices;
				case Trigger::kDotApply:
					return &activeDotApplyTriggerAffixIndices;
				case Trigger::kKill:
					return &activeKillTriggerAffixIndices;
				case Trigger::kLowHealth:
					return &activeLowHealthTriggerAffixIndices;
				default:
					return nullptr;
				}
			}
		};

		struct InstanceTrackingState
		{
			std::unordered_set<RE::SpellItem*> appliedPassiveSpells{};
			std::unordered_map<std::uint64_t, InstanceAffixSlots> instanceAffixes{};
			// Derived identity index: tracked instance key -> item's TESBoundObject FormID.
			// It is rebuilt from player inventory after load and follows key remaps;
			// it is not authoritative serialized state.
			std::unordered_map<std::uint64_t, RE::FormID> instanceBaseObjects{};
			std::unordered_map<InstanceStateKey, InstanceRuntimeState, InstanceStateKeyHash> instanceStates{};
			// Selected reforges done on each item since it was last identified or
			// scoured; drives the rising reforge price (saved in IRFC).
			std::unordered_map<std::uint64_t, std::uint8_t> selectedReforgeCounts{};
			std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> equippedInstanceKeysByToken{};
			bool equippedTokenCacheReady{ false };
		};
