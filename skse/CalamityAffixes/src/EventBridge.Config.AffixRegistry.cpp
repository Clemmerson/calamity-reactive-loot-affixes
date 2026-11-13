#include "CalamityAffixes/EventBridge.h"

#include "CalamityAffixes/SynthesizedAffixDisplayNamePolicy.h"

namespace CalamityAffixes
{
	void EventBridge::RegisterSynthesizedAffix(AffixRuntime&& a_affix, bool a_warnOnDuplicate)
	{
		a_affix.displayName = detail::ResolveSynthesizedAffixDisplayName(
			a_affix.displayName,
			a_affix.label,
			a_affix.id);
		a_affix.displayNameEn = detail::ResolveSynthesizedLocalizedDisplayName(
			a_affix.displayNameEn,
			a_affix.id);
		a_affix.displayNameKo = detail::ResolveSynthesizedLocalizedDisplayName(
			a_affix.displayNameKo,
			a_affix.displayName);

		_affixRuntimeState.affixes.push_back(std::move(a_affix));
		const auto idx = _affixRuntimeState.affixes.size() - 1;
		const auto& affix = _affixRuntimeState.affixes[idx];

		IndexAffixTriggerBucket(affix, idx);
		IndexAffixSpecialActionBucket(affix, idx);

		IndexAffixLookupKeys(affix, idx, true, a_warnOnDuplicate);
		IndexAffixLootPool(affix, idx);
	}
}
