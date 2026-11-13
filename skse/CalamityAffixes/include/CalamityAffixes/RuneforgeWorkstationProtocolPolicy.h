#pragma once

// Runeforge workstation staging protocol v3 (RC1-WS, 2026-10-19; feasr-core/alpha/workstation/data/workstation-protocol.v3.json,
// PRES-D29). Pure policy: event names, tokenised payloads (<= 128 characters, the RuneforgeMenuCallbackPolicy.h cap), glyph
// key -> index (content/runeforge-presentation.v1.json order through the generated presentation table) and Ack parsing.
// The Papyrus side is FEASR_RF_WorkstationStage.psc (feasr-core/alpha/workstation).
//
// v3 over v2: StageItem and StageRunes carry a per-session staging seq; StageRunes names its formula and keeps every formula
// slot (0 = deliberately empty: the rune is not owned, PRES-D31); the host answers each request with an ITEM or RUNES ack
// for that seq. Verbs and codes are compared case-insensitively and the echoed formula id is lowercased, because Papyrus
// interns string literals case-insensitively (run 1: "READY" arrived as "Ready").

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "CalamityAffixes/RuneforgePresentation.generated.h"
#include "CalamityAffixes/RuneforgeWorkstationLayoutPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationStatePolicy.h"

namespace CalamityAffixes::RuneforgeWorkstation::ProtocolPolicy
{
	inline constexpr std::string_view kBegin = "FEASR_Runeforge_WS_Begin";
	inline constexpr std::string_view kStageItem = "FEASR_Runeforge_WS_StageItem";
	inline constexpr std::string_view kClearItem = "FEASR_Runeforge_WS_ClearItem";
	inline constexpr std::string_view kStageRunes = "FEASR_Runeforge_WS_StageRunes";
	inline constexpr std::string_view kClearRunes = "FEASR_Runeforge_WS_ClearRunes";
	inline constexpr std::string_view kBeat = "FEASR_Runeforge_WS_Beat";
	inline constexpr std::string_view kEnd = "FEASR_Runeforge_WS_End";
	inline constexpr std::string_view kReset = "FEASR_Runeforge_WS_Reset";
	// Presentation-only observation. It never participates in staging acknowledgements or transaction authority.
	inline constexpr std::string_view kState = "FEASR_Runeforge_WS_State";
	inline constexpr std::string_view kOperationCue = "FEASR_Runeforge_WS_OperationCue";
	inline constexpr std::string_view kAck = "FEASR_Runeforge_WS_Ack";
	// R8.7.4.23 presentation-only companion of StageItem: the displayed clone's slab-local box (LayoutPolicy::DisplayBounds), same
	// token and seq. A host without a handler ignores it; a host that never receives it keeps its bounds-free fallback.
	inline constexpr std::string_view kItemBounds = "FEASR_Runeforge_WS_ItemBounds";
	inline constexpr std::size_t kMaxStringLength = 128u;
	inline constexpr std::size_t kMaxRunes = 6u;
	inline constexpr std::size_t kMaxFormulaIdLength = 40u;
	inline constexpr std::uint8_t kMaxGlyph = static_cast<std::uint8_t>(RuneforgePresentation::Generated::kRuneCount);

	struct Message
	{
		std::string_view name{};
		std::string str{};
		float num{ 0.0f };
	};

	[[nodiscard]] inline bool WithinCap(const Message& a_message) noexcept
	{
		return a_message.str.size() <= kMaxStringLength;
	}

	[[nodiscard]] inline Message TokenOnly(std::string_view a_name, std::uint32_t a_token)
	{
		return { a_name, std::to_string(a_token), static_cast<float>(a_token) };
	}

	[[nodiscard]] inline Message VisualState(std::uint32_t a_token, std::string_view a_state)
	{
		return { kState, std::to_string(a_token) + "|" + std::string(a_state), static_cast<float>(a_token) };
	}

	[[nodiscard]] inline Message OperationCue(std::uint32_t a_token, std::string_view a_operation)
	{
		return { kOperationCue, std::to_string(a_token) + "|" + std::string(a_operation), static_cast<float>(a_token) };
	}

	[[nodiscard]] inline std::string FixedTwo(double a_value)
	{
		const double rounded = std::round(a_value * 100.0) / 100.0;
		char buffer[32]{};
		std::snprintf(buffer, sizeof(buffer), "%.2f", rounded == 0.0 ? 0.0 : rounded);
		return buffer;
	}

	[[nodiscard]] constexpr char Lower(char a_c) noexcept { return a_c >= 'A' && a_c <= 'Z' ? static_cast<char>(a_c - 'A' + 'a') : a_c; }
	[[nodiscard]] constexpr char Upper(char a_c) noexcept { return a_c >= 'a' && a_c <= 'z' ? static_cast<char>(a_c - 'a' + 'A') : a_c; }

	// Formula ids travel lowercased and restricted to [a-z0-9_] (at most 40 characters); "-" when nothing is left.
	[[nodiscard]] inline std::string WireFormulaId(std::string_view a_id)
	{
		std::string out;
		for (const char c : a_id) {
			const char l = Lower(c);
			if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l == '_') out.push_back(l);
			if (out.size() == kMaxFormulaIdLength) break;
		}
		return out.empty() ? std::string("-") : out;
	}

	[[nodiscard]] inline Message StageItem(std::uint32_t a_token, std::int64_t a_seq, const LayoutPolicy::ItemPose& a_pose)
	{
		std::string str = std::to_string(a_token) + "|" + std::to_string(a_seq) + "|" + std::string(LayoutPolicy::Name(a_pose.itemClass)) + "|" +
			FixedTwo(a_pose.x) + "," + FixedTwo(a_pose.y) + "," + FixedTwo(a_pose.z) + "," + std::to_string(a_pose.yawDegrees) + "," + FixedTwo(a_pose.scale);
		return { kStageItem, std::move(str), static_cast<float>(a_token) };
	}

	// "token|seq|x0,y0,z0,x1,y1,z1" (slab-local, two decimals)
	[[nodiscard]] inline Message ItemBounds(std::uint32_t a_token, std::int64_t a_seq, const LayoutPolicy::DisplayBox& a_box)
	{
		std::string str = std::to_string(a_token) + "|" + std::to_string(a_seq) + "|" + FixedTwo(a_box.x0) + "," + FixedTwo(a_box.y0) + "," +
			FixedTwo(a_box.z0) + "," + FixedTwo(a_box.x1) + "," + FixedTwo(a_box.y1) + "," + FixedTwo(a_box.z1);
		return { kItemBounds, std::move(str), static_cast<float>(a_token) };
	}

	// a_slots: one entry per formula slot in formula order; a glyph index 1..33, or 0 for a slot left empty.
	[[nodiscard]] inline Message StageRunes(std::uint32_t a_token, std::int64_t a_seq, std::string_view a_formulaId, const std::vector<std::uint8_t>& a_slots)
	{
		std::string str = std::to_string(a_token) + "|" + std::to_string(a_seq) + "|" + WireFormulaId(a_formulaId) + "|";
		for (std::size_t i = 0; i < a_slots.size() && i < kMaxRunes; ++i) {
			if (i) str += ",";
			str += std::to_string(a_slots[i] <= kMaxGlyph ? a_slots[i] : 0u);
		}
		return { kStageRunes, std::move(str), static_cast<float>(a_token) };
	}

	[[nodiscard]] inline Message BeatStrike(std::uint32_t a_token, std::int64_t a_index)
	{
		return { kBeat, std::to_string(a_token) + "|STRIKE|" + std::to_string(a_index), static_cast<float>(a_token) };
	}

	[[nodiscard]] inline Message BeatBind(std::uint32_t a_token)
	{
		return { kBeat, std::to_string(a_token) + "|BIND", static_cast<float>(a_token) };
	}

	[[nodiscard]] inline Message BeatSkip(std::uint32_t a_token)
	{
		return { kBeat, std::to_string(a_token) + "|SKIP", static_cast<float>(a_token) };
	}

	[[nodiscard]] inline Message End(std::uint32_t a_token, StatePolicy::ExitReason a_reason, StatePolicy::Cause a_cause)
	{
		return { kEnd, std::to_string(a_token) + "|" + StatePolicy::ReasonText(a_reason, a_cause), static_cast<float>(a_token) };
	}

	[[nodiscard]] inline Message Reset()
	{
		return { kReset, "0", 0.0f };
	}

	// Glyph key (aes, brond, ...) -> manifest index 1..33; 0 when unknown.
	[[nodiscard]] constexpr std::uint8_t GlyphIndex(std::string_view a_glyph) noexcept
	{
		for (const auto& rune : RuneforgePresentation::Generated::kRunes) {
			if (rune.glyph == a_glyph) return rune.index;
		}
		return 0u;
	}

	// "aes,brond,cael" -> {1, 2, 3}; unknown keys are dropped (logged by the caller), at most six kept.
	[[nodiscard]] inline std::vector<std::uint8_t> GlyphIndices(std::string_view a_glyphSequence, std::size_t* a_unknown = nullptr)
	{
		std::vector<std::uint8_t> out;
		std::size_t unknown = 0;
		std::size_t start = 0;
		while (start <= a_glyphSequence.size()) {
			const auto comma = a_glyphSequence.find(',', start);
			const auto key = a_glyphSequence.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
			if (!key.empty()) {
				const auto index = GlyphIndex(key);
				if (index == 0u) ++unknown;
				else if (out.size() < kMaxRunes) out.push_back(index);
			}
			if (comma == std::string_view::npos) break;
			start = comma + 1;
		}
		if (a_unknown) *a_unknown = unknown;
		return out;
	}

	// Presentation STAT FormID (light plugin local id) of the display base for a glyph index (WS-I03).
	[[nodiscard]] constexpr std::uint32_t DisplayBaseLocalId(std::uint8_t a_glyphIndex) noexcept
	{
		return a_glyphIndex == 0u ? 0u : 0x800u + (a_glyphIndex - 1u);
	}

	enum class AckKind : std::uint8_t
	{
		kInvalid,
		kReady,
		kItemSpawned,  // N|ITEM|seq|SPAWNED|formId: placed, positioned, enabled (not yet proof of 3D)
		kItemLoaded,   // N|ITEM|seq|LOADED|formId: Is3DLoaded() and keyframed (positive evidence)
		kItemFail,     // N|ITEM|seq|FAIL|code
		kRunes,        // N|RUNES|seq|formula|requested|staged|slots|code
		kCleared,
		kError,
		// R8.7.4.21 presentation acks (never transaction authority; they only pace the performance beats, bounded by a timeout)
		kBeatLanded,   // N|BEAT|k|LANDED: the glyph of strike k landed and its rune presentation ended
		kBeatFail,     // N|BEAT|k|FAIL|code: strike k could not be presented (the beat gate stops waiting for it)
		kSealDone      // N|SEAL|DONE: the SEAL presentation finished
	};

	struct StagedSlot
	{
		std::uint8_t slot{ 0 };   // 1-based formula slot
		std::uint8_t glyph{ 0 };  // 1..33
	};

	struct Ack
	{
		AckKind kind{ AckKind::kInvalid };
		std::uint32_t token{ 0 };
		std::int64_t seq{ 0 };     // ITEM / RUNES: the staging seq; BEAT: the strike index k
		std::uint32_t value{ 0 };  // ITEM SPAWNED / LOADED: the reference form id; CLEARED: the count
		std::string code{};        // ITEM FAIL / RUNES / ERROR: uppercased
		std::string formula{};     // RUNES: lowercased formula id
		std::uint32_t requested{ 0 };
		std::uint32_t staged{ 0 };
		std::vector<StagedSlot> slots{};

		// RUNES: every requested slot is placed, enabled and 3D-loaded.
		[[nodiscard]] bool RowComplete() const noexcept { return kind == AckKind::kRunes && code == "OK" && staged == requested; }
	};

	[[nodiscard]] inline std::optional<std::uint32_t> ParseUint(std::string_view a_text, int a_base = 10) noexcept
	{
		if (a_text.empty() || a_text.size() > 10) return std::nullopt;
		std::uint32_t value = 0;
		const auto [end, error] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), value, a_base);
		if (error != std::errc{} || end != a_text.data() + a_text.size()) return std::nullopt;
		return value;
	}

	// Papyrus Int (decimal, signed): created references are 0xFF...... and arrive negative.
	[[nodiscard]] inline std::optional<std::uint32_t> ParseFormId(std::string_view a_text) noexcept
	{
		const bool negative = !a_text.empty() && a_text.front() == '-';
		const auto magnitude = ParseUint(negative ? a_text.substr(1) : a_text);
		if (!magnitude || *magnitude == 0u) return std::nullopt;
		return negative ? static_cast<std::uint32_t>(0u - *magnitude) : *magnitude;
	}

	[[nodiscard]] inline std::vector<std::string_view> Fields(std::string_view a_text, char a_separator)
	{
		std::vector<std::string_view> out;
		std::size_t start = 0;
		while (true) {
			const auto at = a_text.find(a_separator, start);
			out.push_back(a_text.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start));
			if (at == std::string_view::npos) break;
			start = at + 1;
		}
		return out;
	}

	[[nodiscard]] inline std::string UpperCopy(std::string_view a_text)
	{
		std::string out(a_text);
		for (auto& c : out) c = Upper(c);
		return out;
	}

	[[nodiscard]] inline bool IsCode(std::string_view a_text) noexcept
	{
		if (a_text.empty() || a_text.size() > 24) return false;
		for (const char c : a_text) {
			const char u = Upper(c);
			if (!((u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '_')) return false;
		}
		return true;
	}

	// The acknowledgement forms of workstation-protocol.v3.json; numArg must equal N. Anything else is kInvalid.
	[[nodiscard]] inline Ack ParseAck(std::string_view a_str, float a_num)
	{
		Ack ack{};
		if (a_str.size() > kMaxStringLength) return ack;
		const auto f = Fields(a_str, '|');
		if (f.size() < 2) return ack;
		const auto token = ParseUint(f[0]);
		if (!token || *token == 0u || static_cast<float>(*token) != a_num) return ack;
		const auto verb = UpperCopy(f[1]);
		Ack out{};
		out.token = *token;
		if (verb == "READY" && f.size() == 2) {
			out.kind = AckKind::kReady;
		} else if (verb == "CLEARED" && f.size() == 3) {
			if (const auto count = ParseUint(f[2]); count) {
				out.kind = AckKind::kCleared;
				out.value = *count;
			}
		} else if (verb == "ERROR" && f.size() == 3 && IsCode(f[2])) {
			out.kind = AckKind::kError;
			out.code = UpperCopy(f[2]);
		} else if (verb == "ITEM" && f.size() == 5) {
			const auto seq = ParseUint(f[2]);
			const auto what = UpperCopy(f[3]);
			if (seq && *seq != 0u) {
				out.seq = *seq;
				if (what == "SPAWNED" || what == "LOADED") {
					if (const auto id = ParseFormId(f[4]); id) {
						out.kind = what == "SPAWNED" ? AckKind::kItemSpawned : AckKind::kItemLoaded;
						out.value = *id;
					}
				} else if (what == "FAIL" && IsCode(f[4])) {
					out.kind = AckKind::kItemFail;
					out.code = UpperCopy(f[4]);
				}
			}
		} else if (verb == "RUNES" && f.size() == 8) {
			const auto seq = ParseUint(f[2]);
			const auto requested = ParseUint(f[4]);
			const auto staged = ParseUint(f[5]);
			bool ok = seq && *seq != 0u && requested && staged && *requested <= kMaxRunes && *staged <= *requested && IsCode(f[7]) && !f[3].empty() &&
			          f[3].size() <= kMaxFormulaIdLength;
			std::vector<StagedSlot> slots;
			if (ok && f[6] != "-") {
				for (const auto pair : Fields(f[6], ',')) {
					const auto colon = pair.find(':');
					const auto slot = colon == std::string_view::npos ? std::nullopt : ParseUint(pair.substr(0, colon));
					const auto glyph = colon == std::string_view::npos ? std::nullopt : ParseUint(pair.substr(colon + 1));
					if (!slot || !glyph || *slot < 1u || *slot > kMaxRunes || *glyph < 1u || *glyph > kMaxGlyph ||
						(!slots.empty() && *slot <= slots.back().slot)) {
						ok = false;
						break;
					}
					slots.push_back({ static_cast<std::uint8_t>(*slot), static_cast<std::uint8_t>(*glyph) });
				}
			}
			if (ok && slots.size() == *staged) {
				out.kind = AckKind::kRunes;
				out.seq = *seq;
				out.requested = *requested;
				out.staged = *staged;
				out.slots = std::move(slots);
				out.code = UpperCopy(f[7]);
				for (const char c : f[3]) out.formula.push_back(Lower(c));
			}
		} else if (verb == "BEAT" && (f.size() == 4 || f.size() == 5)) {
			const auto index = ParseUint(f[2]);
			const auto what = UpperCopy(f[3]);
			if (index && *index >= 1u && *index <= kMaxRunes) {
				if (what == "LANDED" && f.size() == 4) {
					out.kind = AckKind::kBeatLanded;
					out.seq = *index;
				} else if (what == "FAIL" && f.size() == 5 && IsCode(f[4])) {
					out.kind = AckKind::kBeatFail;
					out.seq = *index;
					out.code = UpperCopy(f[4]);
				}
			}
		} else if (verb == "SEAL" && f.size() == 3 && UpperCopy(f[2]) == "DONE") {
			out.kind = AckKind::kSealDone;
		}
		if (out.kind == AckKind::kInvalid) return ack;
		return out;
	}

	[[nodiscard]] inline std::string SlotsText(const std::vector<StagedSlot>& a_slots)
	{
		if (a_slots.empty()) return "-";
		std::string out;
		for (const auto& s : a_slots) {
			if (!out.empty()) out += ",";
			out += std::to_string(s.slot) + ":" + std::to_string(s.glyph);
		}
		return out;
	}
}
