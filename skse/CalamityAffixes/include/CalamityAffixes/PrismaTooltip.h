#pragma once

#include <string_view>

namespace CalamityAffixes::PrismaTooltip
{
	void Install();
	[[nodiscard]] bool IsAvailable();
	// UI language setting: 0 = English, 1 = Korean, 2 = both.
	[[nodiscard]] int GetUiLanguageMode();
	void ReportStatus();
	void SetControlPanelOpen(bool a_open);
	void ToggleControlPanel();
	void HandleRuneforgeReaderEvent(std::string_view a_eventName, std::string_view a_strArg, float a_numArg);
	void StepRuneforgeReader(int a_delta);
	void CloseRuneforgeReader();
}
