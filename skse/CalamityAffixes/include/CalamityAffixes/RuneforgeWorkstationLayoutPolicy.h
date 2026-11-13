#pragma once

// Runeforge workstation slab layout (RC1 decisions WS-D08, WS-D09, WS-I02).
// Pure policy: the bounded per-type support matrix and display pose of the S1 item clone, and the rune row.
// Mirrors feasr-core/alpha/workstation/model/workstation_layout.mjs and data/workstation-orientation.v1.json;
// the vectors' S and I lines are checked against this header by tests/runeforge_workstation_policy_tests.cpp.

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace CalamityAffixes::RuneforgeWorkstation::LayoutPolicy
{
	enum class ItemClass : std::uint8_t
	{
		kNeutral,
		kOneHanded,
		kTwoHanded,
		kBow,
		kCrossbow,
		kStaff,
		kShield,
		kBody,
		kHead,
		kHands,
		kFeet,
		kOtherArmor,
		kJewelry
	};

	struct Bounds
	{
		std::int32_t minX{ 0 };
		std::int32_t minY{ 0 };
		std::int32_t minZ{ 0 };
		std::int32_t maxX{ 0 };
		std::int32_t maxY{ 0 };
		std::int32_t maxZ{ 0 };
	};

	struct ItemPose
	{
		ItemClass itemClass{ ItemClass::kNeutral };
		double x{ 0.0 };
		double y{ 0.0 };
		double z{ 0.0 };
		std::int32_t yawDegrees{ 0 };
		double scale{ 1.0 };
	};

	struct RuneSlot
	{
		double x{ 0.0 };
		double y{ 0.0 };
		double z{ 0.0 };
	};

	inline constexpr double kRunePitch = 20.0;
	inline constexpr double kRuneRowX = -70.0;
	inline constexpr double kRuneRowY = -48.5;
	inline constexpr double kRuneRestZ = 66.28;
	inline constexpr double kSlabTopZ = 64.0;
	inline constexpr double kGearAnchorX = 23.6;
	inline constexpr double kGearAnchorY = -48.5;
	inline constexpr double kFootprintYMin = -64.0;
	inline constexpr double kFootprintYMax = 30.0;
	inline constexpr double kMaxExtentX = 260.0;
	inline constexpr double kMaxExtentY = 94.0;
	inline constexpr double kMaxExtentZ = 192.0;
	inline constexpr std::uint32_t kMaxRunes = 6u;

	// WS-D09: left to right in formula order, centred on the anchor row line, constant pitch, upright (rotation 0).
	[[nodiscard]] constexpr RuneSlot Slot(std::uint32_t a_index, std::uint32_t a_count) noexcept
	{
		const double offset = static_cast<double>(a_index) - (static_cast<double>(a_count) - 1.0) / 2.0;
		return { kRuneRowX + offset * kRunePitch, kRuneRowY, kRuneRestZ };
	}

	[[nodiscard]] constexpr std::array<std::int32_t, 2> YawOrder(ItemClass a_class) noexcept
	{
		switch (a_class) {
		case ItemClass::kOneHanded:
		case ItemClass::kTwoHanded:
		case ItemClass::kBow:
		case ItemClass::kCrossbow:
		case ItemClass::kStaff:
			return { 90, 0 };
		case ItemClass::kNeutral:
			return { -1, -1 };
		default:
			return { 0, 90 };
		}
	}

	[[nodiscard]] constexpr ItemPose Pose(ItemClass a_class, const Bounds& a_bounds) noexcept
	{
		const auto order = YawOrder(a_class);
		if (order[0] < 0 || !(a_bounds.maxX > a_bounds.minX) || !(a_bounds.maxY > a_bounds.minY) || a_bounds.maxZ < a_bounds.minZ) {
			return {};
		}
		// R6: give each category an authored apparent length before applying fit tests.
		// The clamp protects unfamiliar bounds and jewelry keeps enough scale to read on the slab.
		double target = 56.0;
		double maxScale = 1.25;
		switch (a_class) {
		case ItemClass::kOneHanded: target = 56.0; break;
		case ItemClass::kTwoHanded: target = 76.0; maxScale = 1.2; break;
		case ItemClass::kBow: target = 68.0; maxScale = 1.15; break;
		case ItemClass::kCrossbow: target = 58.0; break;
		case ItemClass::kStaff: target = 72.0; maxScale = 1.1; break;
		case ItemClass::kShield: target = 44.0; break;
		case ItemClass::kBody: target = 48.0; break;
		case ItemClass::kHead: target = 36.0; break;
		case ItemClass::kHands: target = 26.0; break;
		case ItemClass::kFeet: target = 32.0; break;
		case ItemClass::kOtherArmor: target = 36.0; break;
		case ItemClass::kJewelry: target = 16.0; maxScale = 2.0; break;
		case ItemClass::kNeutral: return {};
		}
		const double largest = static_cast<double>(std::max({ a_bounds.maxX-a_bounds.minX, a_bounds.maxY-a_bounds.minY, a_bounds.maxZ-a_bounds.minZ }));
		if (!(largest > 0.0)) return {};
		const double scale = std::max(0.15, std::min(maxScale, target / largest));
		for (const auto yaw : order) {
			double rx0 = a_bounds.minX * scale;
			double ry0 = a_bounds.minY * scale;
			double rx1 = a_bounds.maxX * scale;
			double ry1 = a_bounds.maxY * scale;
			if (yaw == 90) {
				// heading +90: model +Y turns to world +X, model +X to world -Y
				rx0 = a_bounds.minY * scale;
				ry0 = -static_cast<double>(a_bounds.maxX) * scale;
				rx1 = a_bounds.maxY * scale;
				ry1 = -static_cast<double>(a_bounds.minX) * scale;
			}
			if (rx1 - rx0 > kMaxExtentX || ry1 - ry0 > kMaxExtentY || static_cast<double>(a_bounds.maxZ - a_bounds.minZ) * scale > kMaxExtentZ) {
				continue;
			}
			const double x = kGearAnchorX - (rx0 + rx1) / 2.0;
			double y = kGearAnchorY - (ry0 + ry1) / 2.0;
			if (y + ry0 < kFootprintYMin) y += kFootprintYMin - (y + ry0);
			if (y + ry1 > kFootprintYMax) y -= (y + ry1) - kFootprintYMax;
			return { a_class, x, y, kSlabTopZ - a_bounds.minZ * scale, yaw, scale };
		}
		return {};
	}

	// R8.7.4.23: the displayed clone's slab-local axis-aligned box. Its OBND x the transmitted display scale, turned by the display
	// yaw (0 or 90, the mapping Pose() fits with), at the transmitted display origin (the stage host places and scales the clone from
	// the StageItem pose rounded to two decimals, so the box uses the same rounded values). The stage host keeps the projected
	// inscription clear of this box (feasr-core/alpha/rc/r87423). Presentation only: never part of eligibility or a transaction.
	struct DisplayBox
	{
		double x0{ 0.0 };
		double y0{ 0.0 };
		double z0{ 0.0 };
		double x1{ 0.0 };
		double y1{ 0.0 };
		double z1{ 0.0 };
	};

	[[nodiscard]] inline DisplayBox DisplayBounds(const ItemPose& a_pose, const Bounds& a_bounds) noexcept
	{
		const auto r2 = [](double a_v) { return std::round(a_v * 100.0) / 100.0; };
		const double s = r2(a_pose.scale);
		double rx0 = a_bounds.minX * s;
		double ry0 = a_bounds.minY * s;
		double rx1 = a_bounds.maxX * s;
		double ry1 = a_bounds.maxY * s;
		if (a_pose.yawDegrees == 90) {
			rx0 = a_bounds.minY * s;
			ry0 = -static_cast<double>(a_bounds.maxX) * s;
			rx1 = a_bounds.maxY * s;
			ry1 = -static_cast<double>(a_bounds.minX) * s;
		}
		const double x = r2(a_pose.x);
		const double y = r2(a_pose.y);
		const double z = r2(a_pose.z);
		return { x + rx0, y + ry0, z + a_bounds.minZ * s, x + rx1, y + ry1, z + a_bounds.maxZ * s };
	}

	// Weapon animation types (RE::WEAPON_TYPE values: 1 sword, 2 dagger, 3 axe, 4 mace, 5 two-hand sword,
	// 6 two-hand axe, 7 bow, 8 staff, 9 crossbow; 0 hand-to-hand is not displayable).
	[[nodiscard]] constexpr ItemClass ClassifyWeapon(std::uint32_t a_animationType) noexcept
	{
		switch (a_animationType) {
		case 1u:
		case 2u:
		case 3u:
		case 4u:
			return ItemClass::kOneHanded;
		case 5u:
		case 6u:
			return ItemClass::kTwoHanded;
		case 7u:
			return ItemClass::kBow;
		case 8u:
			return ItemClass::kStaff;
		case 9u:
			return ItemClass::kCrossbow;
		default:
			return ItemClass::kNeutral;
		}
	}

	// Biped slot mask bits (1 << (slot - 30)).
	[[nodiscard]] constexpr ItemClass ClassifyArmor(std::uint32_t a_slotMask) noexcept
	{
		constexpr auto bit = [](std::uint32_t a_slot) { return 1u << (a_slot - 30u); };
		if (a_slotMask == 0u) return ItemClass::kNeutral;
		if (a_slotMask & bit(39u)) return ItemClass::kShield;
		if (a_slotMask & (bit(35u) | bit(36u))) return ItemClass::kJewelry;
		if (a_slotMask & bit(32u)) return ItemClass::kBody;
		if (a_slotMask & (bit(30u) | bit(31u) | bit(42u))) return ItemClass::kHead;
		if (a_slotMask & (bit(33u) | bit(34u))) return ItemClass::kHands;
		if (a_slotMask & (bit(37u) | bit(38u))) return ItemClass::kFeet;
		return ItemClass::kOtherArmor;
	}

	[[nodiscard]] constexpr std::string_view Name(ItemClass a_class) noexcept
	{
		switch (a_class) {
		case ItemClass::kNeutral: return "NEUTRAL";
		case ItemClass::kOneHanded: return "ONE_HANDED";
		case ItemClass::kTwoHanded: return "TWO_HANDED";
		case ItemClass::kBow: return "BOW";
		case ItemClass::kCrossbow: return "CROSSBOW";
		case ItemClass::kStaff: return "STAFF";
		case ItemClass::kShield: return "SHIELD";
		case ItemClass::kBody: return "BODY";
		case ItemClass::kHead: return "HEAD";
		case ItemClass::kHands: return "HANDS";
		case ItemClass::kFeet: return "FEET";
		case ItemClass::kOtherArmor: return "OTHER_ARMOR";
		case ItemClass::kJewelry: return "JEWELRY";
		}
		return "NEUTRAL";
	}

	[[nodiscard]] constexpr ItemClass ClassFromName(std::string_view a_name) noexcept
	{
		for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(ItemClass::kJewelry); ++i) {
			if (Name(static_cast<ItemClass>(i)) == a_name) return static_cast<ItemClass>(i);
		}
		return ItemClass::kNeutral;
	}

	// R8.7.4.21 (A3): the item cradle, slab-local: the gear anchor +/- 39 in X (the largest authored presentation length 76 / 2 plus
	// margin), the footprint in Y, from 24 below to 76 above the slab top. Kept identical to feasr-core/alpha/rc/r87421/A3-preview
	// cradle_gate.js (its static gate uses z >= 60 for placed dressing; the native orphan rule also accepts a clone that sank to z 40).
	inline constexpr double kCradleMinX = -16.0;
	inline constexpr double kCradleMaxX = 62.0;
	inline constexpr double kCradleMinY = kFootprintYMin;
	inline constexpr double kCradleMaxY = kFootprintYMax;
	inline constexpr double kCradleMinZ = 40.0;
	inline constexpr double kCradleMaxZ = 140.0;

	[[nodiscard]] constexpr bool CradleContains(double a_x, double a_y, double a_z) noexcept
	{
		return a_x >= kCradleMinX && a_x <= kCradleMaxX && a_y >= kCradleMinY && a_y <= kCradleMaxY && a_z >= kCradleMinZ && a_z <= kCradleMaxZ;
	}

	struct LocalPoint
	{
		double x{ 0.0 };
		double y{ 0.0 };
		double z{ 0.0 };
	};

	// Slab-local -> world (the stage host's PlaceLocal and RuneforgeWorkstation.cpp SlabPoint; yaw in radians, Skyrim heading).
	[[nodiscard]] inline LocalPoint SlabToWorld(const LocalPoint& a_slab, double a_yaw, const LocalPoint& a_local) noexcept
	{
		const double c = std::cos(a_yaw);
		const double s = std::sin(a_yaw);
		return { a_slab.x + a_local.x * c + a_local.y * s, a_slab.y - a_local.x * s + a_local.y * c, a_slab.z + a_local.z };
	}

	// World -> slab-local, the exact inverse of SlabToWorld.
	[[nodiscard]] inline LocalPoint WorldToSlab(const LocalPoint& a_slab, double a_yaw, const LocalPoint& a_world) noexcept
	{
		const double c = std::cos(a_yaw);
		const double s = std::sin(a_yaw);
		const double dx = a_world.x - a_slab.x;
		const double dy = a_world.y - a_slab.y;
		return { dx * c - dy * s, dx * s + dy * c, a_world.z - a_slab.z };
	}
}
