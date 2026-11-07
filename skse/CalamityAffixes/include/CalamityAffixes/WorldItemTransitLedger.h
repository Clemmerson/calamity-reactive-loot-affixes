#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace CalamityAffixes
{
	// The engine reports a player item that became a loose world reference as a
	// unique-ID change from the player's key to that reference with unique ID 0.
	// A script drop does this with no reference on the container event (Temper
	// and Recharge Services drops the item, tempers the reference and adds it
	// back), so this change is the only link between the item and the reference.
	[[nodiscard]] constexpr bool IsPlayerItemBecomingWorldReference(
		std::uint32_t a_playerID,
		std::uint32_t a_oldOwnerID,
		std::uint32_t a_newOwnerID,
		std::uint16_t a_oldUniqueID,
		std::uint16_t a_newUniqueID) noexcept
	{
		return a_playerID != 0u &&
		       a_oldOwnerID == a_playerID &&
		       a_newOwnerID != 0u &&
		       a_newOwnerID != a_playerID &&
		       a_oldUniqueID != 0u &&
		       a_newUniqueID == 0u;
	}

	// Player items whose state moved onto a world reference, so a pickup can find
	// it again when its container event names no reference (v2.3.2).
	struct WorldItemTransit
	{
		std::uint32_t baseFormID{ 0u };
		std::uint32_t referenceFormID{ 0u };
		std::uint64_t detachedKey{ 0u };
		std::int64_t recordedAtMs{ 0 };
		// The early delete signal came and was held back for the return.
		bool deleteSeen{ false };
	};

	// Which inventory copy a returned item is (v2.3.2). A copy whose unique ID
	// still names the world reference is it; otherwise only a single copy with
	// no stable unique ID may be taken. Anything else is not ours to guess:
	// binding the wrong copy moves affixes onto an unrelated item.
	enum class ReturnedItemChoice : std::uint8_t
	{
		kNone,
		kExact,
		kLoose,
	};

	[[nodiscard]] constexpr ReturnedItemChoice ChooseReturnedItem(std::uint32_t a_exactCount, std::uint32_t a_looseCount) noexcept
	{
		if (a_exactCount == 1u) {
			return ReturnedItemChoice::kExact;
		}
		if (a_exactCount == 0u && a_looseCount == 1u) {
			return ReturnedItemChoice::kLoose;
		}
		return ReturnedItemChoice::kNone;
	}

	class WorldItemTransitLedger
	{
	public:
		static constexpr std::size_t kCapacity = 16;
		// A script round trip (NPC temper services) took about 2 s in game.
		static constexpr std::int64_t kReturnWindowMs = 30'000;

		// The oldest entry makes room; its state stays where it is (a later
		// pickup or delete handles it as before).
		constexpr void Record(const WorldItemTransit& a_transit) noexcept
		{
			if (a_transit.baseFormID == 0u || a_transit.referenceFormID == 0u || a_transit.detachedKey == 0u) {
				return;
			}
			Forget(a_transit.detachedKey);
			if (_count == kCapacity) {
				EraseAt(0);
			}
			_entries[_count++] = a_transit;
		}

		// The engine reports a script-dropped reference as deleted right after
		// the drop (SKSE form-delete callback, 16 ms) while the script still adds
		// it back about 2 s later. Within the window that signal is not a delete;
		// after it, a delete is real and prunes the state as before.
		[[nodiscard]] constexpr bool IsAwaitingReturn(std::uint64_t a_detachedKey, std::int64_t a_nowMs) const noexcept
		{
			for (std::size_t i = 0; i < _count; ++i) {
				if (_entries[i].detachedKey == a_detachedKey) {
					const auto age = a_nowMs - _entries[i].recordedAtMs;
					return age >= 0 && age <= kReturnWindowMs;
				}
			}
			return false;
		}

		// The transit a pickup of a_baseFormID returns. A known reference must
		// match exactly. Without one, only a single transit of that base still
		// inside the return window counts: an item left on the ground long ago
		// must not be claimed by an unrelated item of the same base.
		[[nodiscard]] constexpr std::optional<WorldItemTransit> Find(
			std::uint32_t a_baseFormID,
			std::uint32_t a_referenceFormID,
			std::int64_t a_nowMs) const noexcept
		{
			std::optional<WorldItemTransit> found;
			for (std::size_t i = 0; i < _count; ++i) {
				const auto& entry = _entries[i];
				if (entry.baseFormID != a_baseFormID) {
					continue;
				}
				if (a_referenceFormID != 0u) {
					if (entry.referenceFormID == a_referenceFormID) {
						return entry;
					}
					continue;
				}
				const auto age = a_nowMs - entry.recordedAtMs;
				if (age < 0 || age > kReturnWindowMs) {
					continue;
				}
				if (found) {
					return std::nullopt;  // two candidates: not this pickup's to guess
				}
				found = entry;
			}
			return found;
		}

		constexpr void MarkDeleteSeen(std::uint64_t a_detachedKey) noexcept
		{
			for (std::size_t i = 0; i < _count; ++i) {
				if (_entries[i].detachedKey == a_detachedKey) {
					_entries[i].deleteSeen = true;
				}
			}
		}

		// Items whose delete signal was held back and that did not come back
		// inside the window: the delete was real, and no second signal comes.
		// Removes them and returns how many keys it wrote to a_out.
		constexpr std::size_t TakeAbandoned(std::int64_t a_nowMs, std::array<std::uint64_t, kCapacity>& a_out) noexcept
		{
			std::size_t taken = 0;
			for (std::size_t i = 0; i < _count;) {
				const auto age = a_nowMs - _entries[i].recordedAtMs;
				if (_entries[i].deleteSeen && age > kReturnWindowMs) {
					a_out[taken++] = _entries[i].detachedKey;
					EraseAt(i);
				} else {
					++i;
				}
			}
			return taken;
		}

		constexpr void Forget(std::uint64_t a_detachedKey) noexcept
		{
			for (std::size_t i = 0; i < _count;) {
				if (_entries[i].detachedKey == a_detachedKey) {
					EraseAt(i);
				} else {
					++i;
				}
			}
		}

		constexpr void Clear() noexcept { _count = 0; }
		[[nodiscard]] constexpr std::size_t Size() const noexcept { return _count; }

	private:
		constexpr void EraseAt(std::size_t a_index) noexcept
		{
			for (std::size_t i = a_index + 1; i < _count; ++i) {
				_entries[i - 1] = _entries[i];
			}
			--_count;
		}

		std::array<WorldItemTransit, kCapacity> _entries{};
		std::size_t _count{ 0 };
	};
}
