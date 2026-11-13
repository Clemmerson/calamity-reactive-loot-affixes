#pragma once

namespace CalamityAffixes::RuneforgeMenu
{
	void Install();
	void QueueOpen(bool a_debugMode = false);
	void QueueClose(bool a_emergency = false);
	bool IsOpen();
}
