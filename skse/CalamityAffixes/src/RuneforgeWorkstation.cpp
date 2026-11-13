#include "CalamityAffixes/RuneforgeWorkstation.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/RuneforgeMenuOperations.h"
#include "CalamityAffixes/RuneforgeWorkstationContextPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationControlsPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationCompletionPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationExitSettlementPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationFramingPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationFreeCameraLayout.h"
#include "CalamityAffixes/RuneforgeWorkstationInputPolicy.h"
#include "CalamityAffixes/RuneforgeActivationIntentPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationLayoutPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationLeasePolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationPromptPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationProtocolPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationReadinessPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationSeatPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationShotPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationSnapshotContract.h"
#include "CalamityAffixes/RuneforgeWorkstationStagePolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationStatePolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationSweepPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationUiOwnershipPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationViewPolicy.h"
#include "CalamityAffixes/RuntimePaths.h"
#include "CalamityAffixes/RunewordEffectCardPolicy.h"

// Runeforge workstation session (FE-ASR RC1-WS, 2026-10-19). Native owns the session (state machine, input, camera,
// timeline clock, protocol, sweep); the Papyrus staging host (FEASR_RF_WorkstationStage, FEASR_RuneforgePresentation.esp)
// owns the display objects. Every transition comes from StatePolicy::Step; this file only performs its effects.
// RC1-WS (PRES-D27..D31): the seat is verified before any selection and for the whole session, gameplay controls are leased
// while the workstation owns the player, every staging request is answered by a tokenised acknowledgement, and the list
// shows each learned formula's readiness with the physical row laid out from the runes the player actually owns.
// RC1-WS r2 (R3.1, PRES-D33/D34): the player is placed on the authored work datum before any activation; the lease follows
// the verified seat and is read back on the next tick; the ControlMap words are accessed at the offsets of the running
// runtime behind a layout self-check (never the vendored SE-layout members, which zeroed the input-context stack on
// 1.6.1170 in r3 attempt 1); every release is verified on a later tick (repair, then fail-safe) before IDLE.
// RC1-WS r3 (R3.2/R3.3, 2026-10-20): furniture exit settles asynchronously before camera/HUD/lease restoration,
// and foreign menus synchronously take input ownership and end workstation presentation without replaying committed work.
namespace CalamityAffixes::RuneforgeWorkstation
{
	namespace
	{
		namespace SP = StatePolicy;
		namespace SweepP = SweepPolicy;
		namespace IP = InputPolicy;
		namespace PRP = PromptPolicy;
		namespace FRP = FramingPolicy;
		namespace PP = ProtocolPolicy;
		namespace LP = LeasePolicy;
		namespace CP = ControlsPolicy;
		namespace XP = ExitSettlementPolicy;
		namespace SeatP = SeatPolicy;
		namespace GP = StagePolicy;
		namespace RP = ReadinessPolicy;
		namespace SC = SnapshotContract;
		namespace VP = ViewPolicy;
		namespace UIO = UiOwnershipPolicy;
		namespace FC = FreeCameraLayout;
		namespace EC = RunewordEffectCardPolicy;
		using json = nlohmann::json;
		using UEFlag = RE::UserEvents::USER_EVENT_FLAG;

		static_assert(IP::kKeyboardEscapeCode == static_cast<std::uint32_t>(RE::BSKeyboardDevice::Keys::kEscape));
		static_assert(IP::kKeyboardTabCode == static_cast<std::uint32_t>(RE::BSKeyboardDevice::Keys::kTab));
		static_assert(IP::kGamepadBackCode == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kBack));
		static_assert(IP::kGamepadBCode == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kB));
		// R8.7.4.18 prompt tables (PromptPolicy) use the raw engine codes
		static_assert(PRP::kPadA == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kA) && PRP::kPadY == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kY) &&
		              PRP::kPadLeftShoulder == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kLeftShoulder) &&
		              PRP::kPadRightShoulder == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kRightShoulder) &&
		              PRP::kPadStart == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kStart) &&
		              PRP::kPadBack == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kBack) &&
		              PRP::kPadLeftTrigger == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kLeftTrigger) &&
		              PRP::kPadRightTrigger == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kRightTrigger) &&
		              PRP::kPadUp == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kUp) &&
		              PRP::kPadLeftThumb == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Keys::kLeftThumb));
		static_assert(static_cast<std::uint32_t>(RE::BSKeyboardDevice::Keys::kQ) == 0x10u && static_cast<std::uint32_t>(RE::BSKeyboardDevice::Keys::kA) == 0x1Eu &&
		              static_cast<std::uint32_t>(RE::BSKeyboardDevice::Keys::kZ) == 0x2Cu && static_cast<std::uint32_t>(RE::BSKeyboardDevice::Keys::kF1) == 0x3Bu &&
		              static_cast<std::uint32_t>(RE::BSKeyboardDevice::Keys::kPageUp) == 0xC9u && static_cast<std::uint32_t>(RE::BSKeyboardDevice::Keys::kEnter) == 0x1Cu);

		static_assert(LP::kMovement == static_cast<std::uint32_t>(UEFlag::kMovement) && LP::kLooking == static_cast<std::uint32_t>(UEFlag::kLooking) &&
					  LP::kActivate == static_cast<std::uint32_t>(UEFlag::kActivate) && LP::kMenu == static_cast<std::uint32_t>(UEFlag::kMenu) &&
					  LP::kConsole == static_cast<std::uint32_t>(UEFlag::kConsole) && LP::kPOVSwitch == static_cast<std::uint32_t>(UEFlag::kPOVSwitch) &&
					  LP::kFighting == static_cast<std::uint32_t>(UEFlag::kFighting) && LP::kSneaking == static_cast<std::uint32_t>(UEFlag::kSneaking) &&
					  LP::kMainFour == static_cast<std::uint32_t>(UEFlag::kMainFour) && LP::kWheelZoom == static_cast<std::uint32_t>(UEFlag::kWheelZoom) &&
					  LP::kJumping == static_cast<std::uint32_t>(UEFlag::kJumping) && LP::kVATS == static_cast<std::uint32_t>(UEFlag::kVATS),
			"LeasePolicy flags must mirror RE::UserEvents::USER_EVENT_FLAG");
		static_assert(SeatP::kSitNormal == static_cast<std::uint32_t>(RE::SIT_SLEEP_STATE::kNormal) &&
					  SeatP::kSitIsSitting == static_cast<std::uint32_t>(RE::SIT_SLEEP_STATE::kIsSitting),
			"SeatPolicy sit states must mirror RE::SIT_SLEEP_STATE");
		static_assert(CP::kMovement == static_cast<std::uint32_t>(UEFlag::kMovement) && CP::kLooking == static_cast<std::uint32_t>(UEFlag::kLooking) &&
					  CP::kActivate == static_cast<std::uint32_t>(UEFlag::kActivate) && CP::kMenu == static_cast<std::uint32_t>(UEFlag::kMenu) &&
					  CP::kConsole == static_cast<std::uint32_t>(UEFlag::kConsole) && CP::kInvalid == static_cast<std::uint32_t>(UEFlag::kInvalid),
			"ControlsPolicy flags must mirror RE::UserEvents::USER_EVENT_FLAG");
		static_assert(static_cast<int>(SeatP::Phase::kPlace) == static_cast<int>(SP::AcqPhase::kPlace) &&
					  static_cast<int>(SeatP::Phase::kSettle) == static_cast<int>(SP::AcqPhase::kSettle) &&
					  static_cast<int>(SeatP::Phase::kActivate) == static_cast<int>(SP::AcqPhase::kActivate) &&
					  static_cast<int>(SeatP::Phase::kVerifySeat) == static_cast<int>(SP::AcqPhase::kVerifySeat),
			"SeatPolicy phases must mirror StatePolicy acquisition phases");
		static_assert(RE::UserEvents::INPUT_CONTEXT_IDS::kTotal == CP::kLayoutSE.contexts,
			"the vendored CommonLib declares the SE input-context count; the AE layout adds one (r31-layout-audit)");

		constexpr std::string_view kMenuName = "CalamityRuneforgeWorkstationMenu";
		constexpr std::string_view kPresentation = "FEASR_RuneforgePresentation.esp";
		constexpr std::string_view kSites = "FEASR_RuneforgeSites.esp";
		constexpr std::string_view kCore = "FEASR_Runeforge.esp";
		constexpr RE::FormID kForgeCellId = 0x80B;
		constexpr RE::FormID kSlabId = 0x829;
		constexpr RE::FormID kGearAnchorId = 0x830;
		constexpr RE::FormID kFirstSlotId = 0x82A;
		constexpr RE::FormID kStanceRefId = 0x825;
		constexpr RE::FormID kSlabActivatorRefId = 0x826;
		constexpr RE::FormID kLedgerId = 0x823;
		constexpr RE::FormID kFirstDisplayId = 0x800;
		constexpr RE::FormID kLastDisplayId = 0x820;
		// R8.7.4.21 (A3): workstation references that legitimately sit in the item cradle (never reported as intruders)
		constexpr RE::FormID kStageHostId = 0x824;
		constexpr std::string_view kWorkstationR1 = "FEASR_RF_WorkstationPresentation_R1.esp";
		constexpr RE::FormID kWorkpieceGlowRefId = 0x80B;
		constexpr std::size_t kPageRows = 7;
		constexpr std::int64_t kSeatStatusLogMs = 500;
		// workstation-assets.v1.json (read-only Skyrim.esm scan): vanilla SNDR forms, played by FormID.
		constexpr RE::FormID kSoundStrike = 0x000EA51B;
		constexpr RE::FormID kSoundBind = 0x0010C646;
		constexpr RE::FormID kSoundDecline = 0x0006D1C6;
		constexpr RE::FormID kSoundFocus = 0x0003C759;
		constexpr std::string_view kAudioPresentation = "FEASR_RF_WorkstationAudio_R1.esp";
		constexpr RE::FormID kSoundWakeSpatial = 0x00000803;
		constexpr RE::FormID kSoundStrikeSpatial = 0x00000800;
		constexpr RE::FormID kSoundBindSpatial = 0x00000801;
		constexpr RE::FormID kSoundDeclineSpatial = 0x00000802;
		constexpr std::size_t kSpatialRoleCount = 4;

		struct Settings
		{
			bool flatMode{ false };
			bool freeCamera{ false };
			bool reducedMotion{ false };
			std::string promptFamily{ "xbox" };
			// R8.7.4.18 furniture-camera framing (FramingPolicy): on by default; overrides are clamped to the safe envelope
			bool furnitureFraming{ true };
			float framingPitch{ FramingPolicy::kWorkstation.pitchDegrees };
			float framingPullBack{ FramingPolicy::kWorkstation.pullBack };
		};

		struct CameraSnapshot
		{
			bool valid{ false };
			bool firstPerson{ false };
			bool thirdPersonValid{ false };
			float targetZoom{ 0.0f };
			float currentZoom{ 0.0f };
			float savedZoom{ 0.0f };
			float worldFov{ 0.0f };
		};

		struct HudSnapshot
		{
			bool valid{ false };
			double crosshairAlpha{ 100.0 };
			double compassAlpha{ 100.0 };
		};

		enum class Backend : std::uint8_t
		{
			kFurniture,
			kFree
		};

		struct OpRow
		{
			std::string label;
			std::string kind;  // identify, reforge, expand, scour, exchangeReforge, exchangeScour
			std::uint64_t token{ 0 };
			bool enabled{ false };
		};

		// What the latest StageRunes asked the host for (PRES-D29: the acknowledgement must match it).
		struct RowRequest
		{
			std::int64_t seq{ 0 };
			std::string formula{};
			std::uint32_t requested{ 0 };
			std::uint32_t slotCount{ 0 };
		};

		struct ItemRecord
		{
			RP::ItemVerdict verdict{ RP::ItemVerdict::kUnknown };
			std::string message{};
		};

		struct EquipmentCheckout
		{
			bool active{ false };
			std::uint64_t instanceKey{ 0u };
			bool leftHand{ false };
			bool weapon{ false };
			std::uint32_t armorMask{ 0u };
		};

		struct Runtime
		{
			std::recursive_mutex mutex;
			std::array<RE::BSSoundHandle, kSpatialRoleCount> spatialSounds{};
			Settings settings{};
			SP::Session session{};
			double msRemainder{ 0.0 };
			bool menuOpen{ false };
			bool closeRequested{ false };
			bool menuRegistered{ false };
			// lists (from the flat panel's snapshot)
			json snapshot{};
			std::vector<json> items{};
			std::vector<json> formulas{};
			std::vector<RP::Readiness> readiness{};  // one per formulas[] entry (PRES-D31)
			std::map<std::string, ItemRecord> verdicts{};  // recipe id -> item/recipe precondition verdict for the selected item
			std::vector<OpRow> ops{};
			std::size_t itemFocus{ 0 };
			std::size_t formulaFocus{ 0 };
			std::size_t opsFocus{ 0 };
			int itemFilter{ 0 };
			int formulaFilter{ 0 };
			bool detail{ false };
			bool scourArmed{ false };
			std::string status{};
			std::string selectedKey{};
			std::string selectedRecipe{};
			// Presentation-only summary captured before the post-commit snapshot refresh.
			CompletionPolicy::Summary result{};
			// R8.7.4.21 I3: effect cards. The focused item's card is fetched on demand (read-only) and cached by
			// (instance key, snapshot revision, equipped) -- never rebuilt per frame; the result card is captured in Commit().
			json resultCard{};
			std::string itemCardKey{};
			std::uint64_t itemCardRevision{ ~0ull };
			bool itemCardEquipped{ false };
			json itemCard{};
			EquipmentCheckout checkout{};
			// camera
			CameraSnapshot camera{};
			Backend backend{ Backend::kFurniture };
			ShotPolicy::Pose from{};
			ShotPolicy::Pose to{};
			float moveElapsed{ 0.0f };
			float moveDuration{ 0.0f };
			ShotPolicy::Orbit orbit{};
			float rightX{ 0.0f };
			float rightY{ 0.0f };
			HudSnapshot hud{};
			bool camVerified{ false };
			int camFramesWaited{ 0 };
			std::uint32_t camWrites{ 0 };
			// right-button pan (run 1 / r2)
			bool pointerInPanel{ false };
			bool panning{ false };
			VP::Pan pan{};
			bool panBaseValid{ false };
			RE::NiPoint2 panBase{};
			bool panWritten{ false };
			bool panResetPending{ false };
			bool panUnavailableLogged{ false };
			std::uint32_t panDrags{ 0 };
			// input
			IP::Tracker tracker{};
			IP::Device device{ IP::Device::kKeyboard };
			// R8.7.4.18: the device whose bindings the key caps name (never the mouse)
			IP::Device promptDevice{ IP::Device::kKeyboard };
			// R8.7.4.18 furniture framing state (measured, fail-closed per channel)
			struct Framing
			{
				bool active{ false };
				bool calibrated{ false };
				bool settledLogged{ false };
				FramingPolicy::Channel channel{ FramingPolicy::Channel::kNone };
				bool freeRotationFailed{ false };
				bool lookPitchFailed{ false };
				bool zoomFailed{ false };
				float pitchSign{ 1.0f };
				bool signJudged{ false };
				float elapsed{ 0.0f };
				std::uint8_t forwardAxis{ 1 };
				float startPitch{ 0.0f };
				float startDistance{ 0.0f };
				float baseLookPitch{ 0.0f };
				bool lookPitchWritten{ false };
				float zoom{ 0.0f };
				float baseZoom{ 0.0f };
				bool zoomWritten{ false };
				std::uint32_t zoomFramesIncreasing{ 0 };
				float commandedPitch{ 0.0f };
				float measuredPitch{ 0.0f };
				float measuredDistance{ 0.0f };
				RE::NiPoint3 cameraPos{};
				FramingPolicy::Vec3 forward{};
				// PC1.1: axis-independent elevation of the camera above the orbit pivot, and the bounded activation trace clock
				float startElevation{ 0.0f };
				float measuredElevation{ 0.0f };
				float nextTraceAt{ 0.0f };
			} framing{};
			IP::Button stickDirection{ IP::Button::kOther };
			float stickHeld{ 0.0f };
			std::uint32_t stickRepeats{ 0 };
			// world
			RE::TESRace* race{ nullptr };
			std::vector<RE::ObjectRefHandle> spawned{};
			// PRES-D27 seat, PRES-D28 lease, PRES-D29 staging
			LP::Lease lease{};
			SeatP::Tracker seat{};
			SeatP::Phase seatPhaseReported{ SeatP::Phase::kPlace };
			RE::NiPoint3 lastPlayerPos{};
			bool lastPlayerPosValid{ false };
			// PRES-D34: the controls are verified on later ticks; the frame counter orders the writes and the checks
			std::uint64_t frame{ 0 };
			CP::Restore restore{};
			struct LeaseVerify
			{
				bool pending{ false };
				std::uint32_t token{ 0 };
				std::uint64_t frame{ 0 };
			};
			LeaseVerify leaseVerify{};
			bool seatWatch{ false };
			std::int64_t seatEnterMs{ 0 };
			std::int64_t seatStatusMs{ 0 };
			bool seatForcedThirdPerson{ false };
			// RC1-WS r3: asynchronous furniture-exit settlement and cross-menu ownership.
			struct ExitSettlement
			{
				XP::Tracker tracker{};
				bool preSessionControlsValid{ false };
				std::uint32_t preSessionControls{ 0 };
				std::uint32_t furnitureSuppressed{ 0 };
				bool cameraRestoreQueued{ false };
				bool hudRestoreQueued{ false };
				bool leaseReleaseQueued{ false };
				std::uint32_t leaseReleaseToken{ 0 };
				bool settlementRequired{ false };
				bool settlementReached{ false };
				bool settlementFailed{ false };
				bool entryRecovery{ false };
				std::int64_t lastPollLogMs{ 0 };
				bool finalBaselineLogged{ false };
				std::int64_t finalRestoreCode{ 1 };
				// PC1.1: exit-budget time held for a paused foreign menu (capped), and the one-shot "not held" notice
				std::int64_t heldMs{ 0 };
				bool holdBypassLogged{ false };
			} exit{};
			std::vector<std::string> uiForeignMenus{};
			// PC1.1: unpaused game time a committed session has spent yielded to foreign UI (bounded backstop)
			std::int64_t uiYieldUnpausedMs{ 0 };
			GP::Gate stage{};
			GP::RenderDedup render{};
			RowRequest row{};
			std::string rowStatus{};
			std::string itemStatus{};
			std::vector<SP::Event> deferred{};
		};

		Runtime g;
		ActivationIntentPolicy::State g_activationIntent{};
		std::atomic<std::uint64_t> g_activationSession{ 1 };
		std::atomic<std::uint64_t> g_activationCellEpoch{ 0 };
		std::atomic<std::uint64_t> g_activationMenuEpoch{ 0 };
		std::atomic_bool g_active{ false };
		std::atomic_bool g_uiYield{ false };
		std::atomic_bool g_uiTeardownQueued{ false };
		// Lock-free mirror of the spawned display refs for IsDisplayClone (called from Calamity's loot path, which can
		// run on a Papyrus thread while holding the EventBridge state lock).
		std::array<std::atomic<RE::FormID>, 16> g_cloneIds{};

		void RememberClone(RE::FormID a_id)
		{
			for (auto& slot : g_cloneIds) {
				if (slot.load() == a_id) return;
			}
			for (auto& slot : g_cloneIds) {
				RE::FormID expected = 0;
				if (slot.compare_exchange_strong(expected, a_id)) return;
			}
		}

		void ForgetClones()
		{
			for (auto& slot : g_cloneIds) slot.store(0);
		}
		std::atomic_bool g_tickPending{ false };
		std::jthread g_worker;

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		void Dispatch(const SP::Event& a_event);
		void CapturePreSessionControls();
		void OnLeaseBaseline(std::uint32_t a_controls);
		bool QueueCameraRestore();
		bool QueueHudRestore();
		bool BeginExitSettlement();
		bool QueueLeaseRelease(std::uint32_t a_token);
		void PollExitSettlement(std::int64_t a_deltaMs);
		bool FinalGameplayBaselineReady(std::uint32_t a_controls);
		bool FinalBaselinePending();
		void RememberFinalRestoreCode(std::int64_t a_code);
		std::int64_t FinalRestoreCode();
		bool HandleStaleStanceOnEnter(const SeatP::Observation& a_observed, const std::string& a_occupied);
		bool MayConsumeInput();

		// ---------------------------------------------------------------- world lookups

		RE::TESDataHandler* Data() { return RE::TESDataHandler::GetSingleton(); }

		template <class T>
		T* Lookup(RE::FormID a_id, std::string_view a_plugin)
		{
			auto* data = Data();
			return data ? data->LookupForm<T>(a_id, a_plugin) : nullptr;
		}

		bool PluginLoaded(std::string_view a_plugin)
		{
			auto* data = Data();
			return data && data->LookupModByName(a_plugin) != nullptr;
		}

		RE::TESObjectREFR* Slab() { return Lookup<RE::TESObjectREFR>(kSlabId, kSites); }
		RE::TESObjectCELL* ForgeCell() { return Lookup<RE::TESObjectCELL>(kForgeCellId, kSites); }
		RE::TESObjectREFR* StanceRef() { return Lookup<RE::TESObjectREFR>(kStanceRefId, kPresentation); }
		RE::BGSListForm* Ledger() { return Lookup<RE::BGSListForm>(kLedgerId, kPresentation); }

		bool IsDisplayBase(const RE::TESForm* a_base)
		{
			auto* data = Data();
			if (!a_base || !data) return false;
			const auto first = data->LookupFormID(kFirstDisplayId, kPresentation);
			const auto last = data->LookupFormID(kLastDisplayId, kPresentation);
			return first != 0u && a_base->GetFormID() >= first && a_base->GetFormID() <= last;
		}

		void PlaySound(RE::FormID a_sound)
		{
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				audio->Play(a_sound);
			}
		}

		RE::NiPoint3 SlabPoint(float a_x, float a_y, float a_z)
		{
			const auto* slab = Slab();
			if (!slab) return {};
			const float yaw = slab->GetAngleZ();
			const float c = std::cos(yaw);
			const float s = std::sin(yaw);
			return { slab->GetPositionX() + a_x * c + a_y * s,
				slab->GetPositionY() - a_x * s + a_y * c,
				slab->GetPositionZ() + a_z };
		}

		void ReleaseSpatialSounds(std::uint16_t a_fadeMS = 60)
		{
			for (auto& sound : g.spatialSounds) {
				if (sound.IsValid()) sound.FadeOutAndRelease(a_fadeMS);
			}
		}

		bool PlaySpatialSound(std::size_t a_role, RE::FormID a_localFormID, const RE::NiPoint3& a_position,
			float a_volume, float a_frequency, RE::FormID a_fallback)
		{
			if (a_role >= g.spatialSounds.size()) return false;
			auto* audio = RE::BSAudioManager::GetSingleton();
			auto* descriptor = Lookup<RE::BGSSoundDescriptorForm>(a_localFormID, kAudioPresentation);
			if (!audio || !descriptor) {
				PlaySound(a_fallback);
				return false;
			}
			auto& handle = g.spatialSounds[a_role];
			if (handle.IsValid()) handle.FadeOutAndRelease(60);
			if (!audio->BuildSoundDataFromDescriptor(handle, static_cast<RE::BSISoundDescriptor*>(descriptor))) {
				PlaySound(a_fallback);
				return false;
			}
			const bool positioned = handle.SetPosition(a_position);
			const bool volumeSet = handle.SetVolume(a_volume);
			const bool frequencySet = handle.SetFrequency(a_frequency);
			if (!positioned || !volumeSet || !frequencySet || !handle.Play()) {
				handle.FadeOutAndRelease(0);
				PlaySound(a_fallback);
				return false;
			}
			return true;
		}

		void PlayWakeCue()
		{
			PlaySpatialSound(0, kSoundWakeSpatial, SlabPoint(56.0f, 80.0f, 120.0f), 0.55f, 1.0f, kSoundFocus);
		}

		void PlayStrikeCue()
		{
			// Authored contact anchor: workstation-contact.v1.json, slab-local (-16.4, -48.5, 66.5).
			PlaySpatialSound(1, kSoundStrikeSpatial, SlabPoint(-16.4f, -48.5f, 66.5f), 0.65f, 1.0f, kSoundStrike);
		}

		void PlayBindCue()
		{
			PlaySpatialSound(2, kSoundBindSpatial, SlabPoint(-16.4f, -48.5f, 66.5f), 0.7f, 1.0f, kSoundBind);
		}

		void PlayDeclineCue()
		{
			PlaySpatialSound(3, kSoundDeclineSpatial, SlabPoint(0.0f, -57.4f, 71.6f), 0.55f, 1.0f, kSoundDecline);
		}

		void LoadSettings()
		{
			// Optional: Data/SKSE/Plugins/CalamityAffixes/runeforge_workstation.json
			// {"flatMode": false, "cameraBackend": "furniture"|"free", "reducedMotion": false, "promptFamily": "xbox"|"playstation"}
			const auto path = RuntimePaths::ResolveRuntimeRelativePath("Data/SKSE/Plugins/CalamityAffixes/runeforge_workstation.json");
			std::ifstream in(path);
			if (!in.good()) {
				SKSE::log::info("[FEASR-RF] WS settings: defaults (no {})", path.string());
				return;
			}
			try {
				const auto doc = json::parse(in);
				g.settings.flatMode = doc.value("flatMode", false);
				g.settings.freeCamera = doc.value("cameraBackend", std::string("furniture")) == "free";
				g.settings.reducedMotion = doc.value("reducedMotion", false);
				g.settings.promptFamily = doc.value("promptFamily", std::string("xbox")) == "playstation" ? "playstation" : "xbox";
				g.settings.furnitureFraming = doc.value("furnitureFraming", true);
				const auto framing = FramingPolicy::Clamp({ doc.value("framingPitch", FramingPolicy::kWorkstation.pitchDegrees),
					doc.value("framingPullBack", FramingPolicy::kWorkstation.pullBack) });
				g.settings.framingPitch = framing.pitchDegrees;
				g.settings.framingPullBack = framing.pullBack;
			} catch (const std::exception& e) {
				SKSE::log::warn("[FEASR-RF] WS settings unreadable ({}); defaults kept", e.what());
			}
			SKSE::log::info("[FEASR-RF] WS settings flatMode={} cameraBackend={} reducedMotion={} promptFamily={} furnitureFraming={} framingPitch={:.1f} framingPullBack={:.1f}",
				g.settings.flatMode, g.settings.freeCamera ? "free" : "furniture", g.settings.reducedMotion, g.settings.promptFamily,
				g.settings.furnitureFraming, g.settings.framingPitch, g.settings.framingPullBack);
		}

		// ---------------------------------------------------------------- protocol (native -> Papyrus)

		void Send(const PP::Message& a_message, RE::TESForm* a_sender = nullptr)
		{
			if (!PP::WithinCap(a_message)) {
				SKSE::log::error("[FEASR-RF] WS payload over 128 characters dropped: {}", a_message.name);
				return;
			}
			auto* source = SKSE::GetModCallbackEventSource();
			if (!source) return;
			const std::string name(a_message.name);
			SKSE::ModCallbackEvent event{ RE::BSFixedString(name.c_str()), RE::BSFixedString(a_message.str.c_str()), a_message.num, a_sender };
			source->SendEvent(&event);
			SKSE::log::info("[FEASR-RF] WS send {} '{}'", name, a_message.str);
		}

		// ---------------------------------------------------------------- gameplay-control lease (PRES-D28, PRES-D34)
		// R3-E11: never the vendored RE::ControlMap members or ToggleControls (SE offsets: on 1.6.1170 they hit the input-context
		// stack). The flag word and its companion are read and written at the offsets ControlsPolicy picks for the running
		// runtime, only after the layout self-check passes; every acquire and release is verified on a later tick.

		const CP::Layout& ControlsLayout()
		{
			const auto version = REL::Module::get().version();
			return CP::ForRuntime(version.major(), version.minor(), version.patch());
		}

		bool RegionReadable(const void* a_base, std::size_t a_bytes)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (!a_base || ::VirtualQuery(a_base, &info, sizeof(info)) != sizeof(info)) return false;
			const auto start = reinterpret_cast<std::uintptr_t>(a_base);
			const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
			const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
			return info.State == MEM_COMMIT && (info.Protect & readable) != 0 && (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 && end >= start + a_bytes;
		}

		CP::Raw ReadControls()
		{
			CP::Raw raw{};
			const auto& layout = ControlsLayout();
			const auto* base = reinterpret_cast<const std::uint8_t*>(RE::ControlMap::GetSingleton());
			if (!base || !RegionReadable(base, layout.objectSize)) return raw;
			raw.readable = true;
			std::memcpy(&raw.stackData, base + layout.stackOffset + CP::kStackDataAt, sizeof(raw.stackData));
			std::memcpy(&raw.stackCapacity, base + layout.stackOffset + CP::kStackCapacityAt, sizeof(raw.stackCapacity));
			std::memcpy(&raw.stackSize, base + layout.stackOffset + CP::kStackSizeAt, sizeof(raw.stackSize));
			std::memcpy(&raw.flags, base + layout.flagsOffset, sizeof(raw.flags));
			std::memcpy(&raw.companion, base + layout.companionOffset, sizeof(raw.companion));
			const auto count = std::min<std::uint32_t>(raw.stackSize, CP::kMaxStackEntries);
			const auto* entries = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(raw.stackData));
			if (count > 0 && CP::PlausiblePointer(raw.stackData) && RegionReadable(entries, count * sizeof(std::uint32_t))) {
				std::memcpy(raw.entries.data(), entries, count * sizeof(std::uint32_t));
				raw.entriesReadable = true;
			}
			return raw;
		}

		std::string ControlsText(const CP::Raw& a_raw)
		{
			std::string contexts;
			if (a_raw.entriesReadable) {
				for (std::uint32_t i = 0; i < std::min<std::uint32_t>(a_raw.stackSize, 8u); ++i) contexts += (i ? "," : "") + std::to_string(a_raw.entries[i]);
			}
			return fmt::format("layout={} readable={} flags={:#010x} companion={:#010x} stack={}/{} data={:#x} contexts=[{}]", ControlsLayout().name, a_raw.readable,
				a_raw.flags, a_raw.companion, a_raw.stackSize, a_raw.stackCapacity, a_raw.stackData, contexts);
		}

		// The engine semantics the vendored ToggleControls imitates, at the right offsets: both words, then UserEventEnabled.
		void WriteControls(const CP::Words& a_words, std::uint32_t a_oldFlags)
		{
			auto* map = RE::ControlMap::GetSingleton();
			auto* base = reinterpret_cast<std::uint8_t*>(map);
			if (!base) return;
			const auto& layout = ControlsLayout();
			std::memcpy(base + layout.flagsOffset, &a_words.flags, sizeof(a_words.flags));
			std::memcpy(base + layout.companionOffset, &a_words.companion, sizeof(a_words.companion));
			RE::UserEventEnabled event{};
			event.newUserEventFlag = static_cast<UEFlag>(a_words.flags);
			event.oldUserEventFlag = static_cast<UEFlag>(a_oldFlags);
			map->SendEvent(std::addressof(event));
		}

		// Read-only: the flag word of the running runtime (the review CTRL lines).
		std::uint32_t EnabledControls()
		{
			const auto raw = ReadControls();
			return raw.readable ? raw.flags : 0u;
		}

		void UpdateActive()
		{
			g_active.store(g.session.state != SP::State::kIdle || g.restore.pending || g.leaseVerify.pending || g.exit.tracker.active, std::memory_order_release);
		}

		// After the verified seat (PRES-D34). The result is dispatched as a LEASE event once the current transition's effects are
		// done; the read-back on the next tick is LEASE_VERIFIED.
		void AcquireLease(std::uint32_t a_token)
		{
			const auto raw = ReadControls();
			const auto check = CP::SelfCheck(raw, ControlsLayout(), true);
			if (!check.ok) {
				SKSE::log::error("[FEASR-RF] WS CTRL SELFCHECK when=acquire token={} verdict=REJECT faults={:#04x} {} (nothing written; LEASE_FAILED)", a_token,
					check.faults, ControlsText(raw));
				g.deferred.push_back({ SP::EventKind::kLease, 0, 0, {} });
				return;
			}
			OnLeaseBaseline(raw.flags);
			const auto heldToken = g.lease.token;
			const auto acquire = LP::Acquire(g.lease, a_token, raw.flags);
			if (!acquire.acquired) {
				SKSE::log::error("[FEASR-RF] WS LEASE acquire token={} verdict=FAILED (already held by token {})", a_token, heldToken);
				g.deferred.push_back({ SP::EventKind::kLease, 0, 0, {} });
				return;
			}
			const auto words = CP::Toggle({ raw.flags, raw.companion }, acquire.disable, false);
			if (acquire.disable) WriteControls(words, raw.flags);
			g.leaseVerify = { true, a_token, g.frame };
			UpdateActive();
			SKSE::log::info("[FEASR-RF] WS LEASE acquire token={} before={:#010x} disabled={:#05x} after={:#010x} {} verdict=HELD (read back on the next tick)", a_token,
				raw.flags, acquire.disable, words.flags, ControlsText(raw));
			g.deferred.push_back({ SP::EventKind::kLease, 1, 0, {} });
		}

		// Idempotent: a release with nothing held (or another token's) is a logged no-op. a_token 0 releases whatever is held.
		// The restore is verified on a later tick (VerifyControls); EXITING waits for it (StatePolicy kRestored).
		void ReleaseLease(std::uint32_t a_token, std::string_view a_why)
		{
			const auto raw = ReadControls();
			const auto held = g.lease.token;
			const auto snapshot = g.lease.snapshot;
			const auto release = LP::Release(g.lease, a_token, raw.flags);
			if (!release.effective) {
				SKSE::log::info("[FEASR-RF] WS LEASE release token={} ({}) ignored: nothing held{}", a_token, a_why,
					held ? " for this token (held by " + std::to_string(held) + ")" : std::string{});
				return;
			}
			g.leaseVerify = {};
			const auto words = CP::Toggle({ raw.flags, raw.companion }, release.enable, true);
			if (raw.readable && release.enable) WriteControls(words, raw.flags);
			g.restore = { true, held, snapshot, release.enable, 0, g.frame };
			UpdateActive();
			SKSE::log::info("[FEASR-RF] WS LEASE release token={} ({}) enabled={:#05x} before={:#010x} after={:#010x} changedUnderneath={:#05x} {} "
							"verdict=RELEASED (verified on a later tick)",
				held, a_why, release.enable, raw.flags, words.flags, release.changedUnderneath, ControlsText(raw));
		}

		// PRES-D34: the acquire is read back on the next tick; every release is verified on a later tick, repaired once, then
		// forced open by the fail-safe; a pending restore keeps the worker ticking after the session is gone (UpdateActive).
		void VerifyControls()
		{
			if (g.leaseVerify.pending && g.frame > g.leaseVerify.frame) {
				const auto raw = ReadControls();
				const bool holds = g.lease.held && g.lease.token == g.leaseVerify.token && CP::AcquireHolds(raw, ControlsLayout(), g.lease.mask);
				SKSE::log::info("[FEASR-RF] WS LEASE verify token={} mask={:#05x} {} verdict={}", g.leaseVerify.token, g.lease.mask, ControlsText(raw), holds ? "VERIFIED" : "FAILED");
				g.leaseVerify = {};
				UpdateActive();
				Dispatch({ SP::EventKind::kLeaseVerified, holds ? 1 : 0, 0, {} });
			}
			// RC1-WS r3: after native lease verification, wait for the complete pre-furniture gameplay baseline.
			if (!g.restore.pending && g.session.state == SP::State::kExiting && FinalBaselinePending()) {
				const auto raw = ReadControls();
				if (FinalGameplayBaselineReady(raw.flags)) {
					SKSE::log::info("[FEASR-RF] WS FINAL gameplay baseline converged after lease verification; completing restore");
					UpdateActive();
					Dispatch({ SP::EventKind::kRestored, FinalRestoreCode(), 0, {} });
				}
				return;
			}
			if (!g.restore.pending) return;
			const auto raw = ReadControls();
			const auto step = CP::Verify(g.restore, raw, ControlsLayout(), g.frame);
			switch (step) {
			case CP::Step::kWait:
				return;
			case CP::Step::kVerified:
			case CP::Step::kFailsafeVerified:
				if (g.session.state == SP::State::kExiting && !FinalGameplayBaselineReady(raw.flags)) {
					RememberFinalRestoreCode(step == CP::Step::kVerified ? 1 : 2);
					SKSE::log::warn("[FEASR-RF] WS FINAL gameplay baseline verdict=WAIT_AFTER_LEASE_VERIFY; clean IDLE withheld until observed convergence");
					UpdateActive();
					return;
				}
				SKSE::log::info("[FEASR-RF] WS CTRL VERIFY token={} snapshot={:#010x} mask={:#05x} {} verdict={}", g.restore.token, g.restore.snapshot, g.restore.mask,
					ControlsText(raw), CP::Name(step));
				UpdateActive();
				Dispatch({ SP::EventKind::kRestored, step == CP::Step::kVerified ? 1 : 2, 0, {} });
				return;
			case CP::Step::kRepair:
				SKSE::log::warn("[FEASR-RF] WS CTRL REPAIR token={} missing={:#05x} {} (writing the lease mask again)", g.restore.token,
					CP::Missing(g.restore.snapshot, g.restore.mask, raw.flags), ControlsText(raw));
				if (raw.readable) WriteControls(CP::Toggle({ raw.flags, raw.companion }, g.restore.mask, true), raw.flags);
				return;
			case CP::Step::kFailsafe: {
				const auto bits = g.restore.mask | CP::kFailsafeBits;
				SKSE::log::error("[FEASR-RF] WS CTRL FAILSAFE token={} missing={:#05x} {} (force-enabling {:#05x})", g.restore.token,
					CP::Missing(g.restore.snapshot, g.restore.mask, raw.flags), ControlsText(raw), bits);
				if (raw.readable) WriteControls(CP::Toggle({ raw.flags, raw.companion }, bits, true), raw.flags);
				return;
			}
			case CP::Step::kFailsafeUnverified:
				SKSE::log::error("[FEASR-RF] WS CTRL FAILSAFE verdict=FAILSAFE_UNVERIFIED token={} {} (the controls could not be verified)", g.restore.token, ControlsText(raw));
				UpdateActive();
				Dispatch({ SP::EventKind::kRestored, 2, 0, {} });
				return;
			}
		}

		// ---------------------------------------------------------------- lists

		std::string Str(const json& a_value, const char* a_key, std::string_view a_fallback = {})
		{
			if (!a_value.is_object() || !a_value.contains(a_key) || !a_value[a_key].is_string()) return std::string(a_fallback);
			return a_value[a_key].get<std::string>();
		}

		bool Flag(const json& a_value, const char* a_key)
		{
			return a_value.is_object() && a_value.contains(a_key) && a_value[a_key].is_boolean() && a_value[a_key].get<bool>();
		}

		std::uint64_t ParseKey(const std::string& a_text)
		{
			std::uint64_t value = 0;
			const auto [end, error] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), value);
			return error == std::errc{} && end == a_text.data() + a_text.size() ? value : 0u;
		}

		std::uint64_t InventoryInstanceKey(const RE::ExtraUniqueID& a_uid)
		{
			return (static_cast<std::uint64_t>(a_uid.baseID) << 16) | static_cast<std::uint64_t>(a_uid.uniqueID);
		}

		bool ResolvePlayerInventoryInstance(
			std::uint64_t a_key,
			RE::PlayerCharacter*& a_player,
			RE::TESBoundObject*& a_object,
			RE::ExtraDataList*& a_xList)
		{
			a_player = RE::PlayerCharacter::GetSingleton();
			a_object = nullptr;
			a_xList = nullptr;
			auto* changes = a_player ? a_player->GetInventoryChanges() : nullptr;
			if (a_key == 0u || !a_player || !changes || !changes->entryList) return false;
			for (auto* entry : *changes->entryList) {
				if (!entry || !entry->object || !entry->extraLists) continue;
				for (auto* xList : *entry->extraLists) {
					if (!xList) continue;
					auto* uid = xList->GetByType<RE::ExtraUniqueID>();
					if (!uid || uid->baseID == 0u || uid->uniqueID == 0u || InventoryInstanceKey(*uid) != a_key) continue;
					a_object = entry->object;
					a_xList = xList;
					return true;
				}
			}
			return false;
		}

		const RE::BGSEquipSlot* EquipSlotFor(RE::TESBoundObject* a_object, bool a_leftHand)
		{
			if (!a_object) return nullptr;
			if (a_leftHand && a_object->IsWeapon()) {
				return RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F43u);  // LeftHand
			}
			if (auto* weapon = a_object->As<RE::TESObjectWEAP>()) return weapon->GetEquipSlot();
			if (auto* armor = a_object->As<RE::TESObjectARMO>()) return armor->GetEquipSlot();
			return nullptr;
		}

		bool ArmorRestoreWouldConflict(RE::PlayerCharacter* a_player, RE::TESObjectARMO* a_armor, std::uint32_t a_mask)
		{
			if (!a_player || !a_armor) return true;
			for (std::uint32_t bit = 0; bit < 32; ++bit) {
				const auto flag = 1u << bit;
				if ((a_mask & flag) == 0u) continue;
				const auto slot = static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(flag);
				if (auto* worn = a_player->GetWornArmor(slot); worn && worn != a_armor) return true;
			}
			return false;
		}

		void RestoreCheckedOutEquipment()
		{
			if (!g.checkout.active) return;
			const auto checkout = g.checkout;
			g.checkout = {};

			RE::PlayerCharacter* player = nullptr;
			RE::TESBoundObject* object = nullptr;
			RE::ExtraDataList* xList = nullptr;
			if (!ResolvePlayerInventoryInstance(checkout.instanceKey, player, object, xList) || !player || !object || !xList) {
				SKSE::log::warn("[FEASR-RF] WS equipment restore skipped key={}: exact inventory instance no longer exists", checkout.instanceKey);
				return;
			}
			if (xList->HasType<RE::ExtraWorn>() || xList->HasType<RE::ExtraWornLeft>()) {
				SKSE::log::info("[FEASR-RF] WS equipment restore key={} already worn; no action", checkout.instanceKey);
				return;
			}

			if (checkout.weapon) {
				if (auto* current = player->GetEquippedObject(checkout.leftHand); current && current != object) {
					SKSE::log::warn("[FEASR-RF] WS equipment restore skipped key={}: hand now contains another object", checkout.instanceKey);
					return;
				}
			} else if (auto* armor = object->As<RE::TESObjectARMO>(); armor && ArmorRestoreWouldConflict(player, armor, checkout.armorMask)) {
				SKSE::log::warn("[FEASR-RF] WS equipment restore skipped key={}: armor slot now contains another object", checkout.instanceKey);
				return;
			}

			auto* manager = RE::ActorEquipManager::GetSingleton();
			if (!manager) {
				SKSE::log::warn("[FEASR-RF] WS equipment restore skipped key={}: ActorEquipManager unavailable", checkout.instanceKey);
				return;
			}
			manager->EquipObject(player, object, xList, 1, EquipSlotFor(object, checkout.leftHand), false, false, false, true);
			const bool restored = checkout.leftHand ? xList->HasType<RE::ExtraWornLeft>() : xList->HasType<RE::ExtraWorn>();
			SKSE::log::info("[FEASR-RF] WS equipment restore key={} result={}", checkout.instanceKey, restored ? "restored" : "not-confirmed");
		}

		void CheckoutEquippedItemForPresentation(std::uint64_t a_key)
		{
			if (g.checkout.active && g.checkout.instanceKey != a_key) RestoreCheckedOutEquipment();
			if (g.checkout.active) return;

			RE::PlayerCharacter* player = nullptr;
			RE::TESBoundObject* object = nullptr;
			RE::ExtraDataList* xList = nullptr;
			if (!ResolvePlayerInventoryInstance(a_key, player, object, xList) || !player || !object || !xList) return;
			const bool left = xList->HasType<RE::ExtraWornLeft>();
			const bool worn = left || xList->HasType<RE::ExtraWorn>();
			if (!worn) return;

			auto* manager = RE::ActorEquipManager::GetSingleton();
			if (!manager) return;
			const auto* slot = EquipSlotFor(object, left);
			const bool accepted = manager->UnequipObject(player, object, xList, 1, slot, false, false, false, true);
			const bool stillWorn = xList->HasType<RE::ExtraWorn>() || xList->HasType<RE::ExtraWornLeft>();
			// ActorEquipManager::UnequipObject can return false even when Skyrim has
			// already completed the exact-instance unequip.  The observed worn-state
			// postcondition is authoritative for whether we must arm restoration.
			if (stillWorn) {
				SKSE::log::warn("[FEASR-RF] WS equipment checkout key={} failed accepted={} stillWorn=true", a_key, accepted);
				return;
			}
			if (!accepted) {
				SKSE::log::warn("[FEASR-RF] WS equipment checkout key={} UnequipObject returned false but postcondition confirms unequipped; arming restore", a_key);
			}

			std::uint32_t armorMask = 0u;
			if (auto* armor = object->As<RE::TESObjectARMO>()) armorMask = static_cast<std::uint32_t>(armor->GetSlotMask());
			g.checkout = {
				.active = true,
				.instanceKey = a_key,
				.leftHand = left,
				.weapon = object->IsWeapon(),
				.armorMask = armorMask
			};
			SKSE::log::info("[FEASR-RF] WS equipment checkout key={} left={} armorMask={:08X}", a_key, left, armorMask);
		}

		const json& Panel()
		{
			static const json empty = json::object();
			return g.snapshot.contains("panel") && g.snapshot["panel"].is_object() ? g.snapshot["panel"] : empty;
		}

		bool ItemMatches(const json& a_item)
		{
			const auto kind = Str(a_item, "itemKind");
			switch (g.itemFilter) {
			case 1: return Flag(a_item, "equipped");
			case 2: return kind == "weapon";
			case 3: return kind == "armor";
			default: return true;
			}
		}

		std::string RecommendationGroup(const json& a_recipe)
		{
			const auto key = Str(a_recipe, "recommendedBase", "mixed");
			for (const auto* weapon : { "weapon", "two_handed_melee", "polearm", "bow", "staff_wand", "claw", "sword", "one_handed_melee", "melee_weapon" }) {
				if (key == weapon) return "weapon";
			}
			for (const auto* armor : { "armor", "heavy_armor", "helm", "shield" }) {
				if (key == armor) return "armor";
			}
			return "mixed";
		}

		bool FormulaMatches(const json& a_recipe)
		{
			switch (g.formulaFilter) {
			case 1: return RecommendationGroup(a_recipe) == "weapon";
			case 2: return RecommendationGroup(a_recipe) == "armor";
			case 3: return RecommendationGroup(a_recipe) == "mixed";
			default: return true;
			}
		}

		std::vector<std::uint8_t> FormulaGlyphs(const json& a_recipe, std::size_t* a_unknown = nullptr)
		{
			return PP::GlyphIndices(RuneforgeMenu::Operations::GlyphSequence(Str(a_recipe, "id")), a_unknown);
		}

		std::size_t GlyphCount(const json& a_recipe)
		{
			return FormulaGlyphs(a_recipe).size();
		}

		std::vector<RP::InventoryEntry> RuneInventory()
		{
			std::vector<RP::InventoryEntry> out;
			const auto& p = Panel();
			if (!p.contains("runeInventory") || !p["runeInventory"].is_array()) return out;
			for (const auto& e : p["runeInventory"]) {
				const auto owned = e.contains("owned") && e["owned"].is_number_unsigned() ? e["owned"].get<std::uint32_t>() : 0u;
				out.push_back({ ParseKey(Str(e, "token")), owned });
			}
			return out;
		}

		std::string RuneName(std::uint64_t a_token)
		{
			const auto& p = Panel();
			if (p.contains("runeInventory") && p["runeInventory"].is_array()) {
				for (const auto& e : p["runeInventory"]) {
					if (ParseKey(Str(e, "token")) == a_token) return Str(e, "name", "Rune");
				}
			}
			return "Rune";
		}

		// PRES-D31: one readiness per listed (learned) formula, from the runes the player owns and the selected item.
		void RecomputeReadiness()
		{
			const auto& p = Panel();
			const bool known = Flag(p, "runeInventoryKnown");
			const auto inventory = RuneInventory();
			const bool hasItem = !g.selectedKey.empty();
			g.readiness.clear();
			for (const auto& f : g.formulas) {
				std::vector<std::uint64_t> tokens;
				if (f.contains("runeTokens") && f["runeTokens"].is_array()) {
					for (const auto& t : f["runeTokens"]) tokens.push_back(t.is_string() ? ParseKey(t.get<std::string>()) : 0u);
				}
				std::size_t unknown = 0;
				const auto glyphs = FormulaGlyphs(f, &unknown);
				const auto it = g.verdicts.find(Str(f, "id"));
				const auto verdict = it == g.verdicts.end() ? RP::ItemVerdict::kUnknown : it->second.verdict;
				g.readiness.push_back(RP::Evaluate(tokens, unknown ? std::vector<std::uint8_t>{} : glyphs, known, inventory, hasItem, verdict));
			}
		}

		const RP::Readiness* ReadinessAt(std::size_t a_index)
		{
			return a_index < g.readiness.size() ? &g.readiness[a_index] : nullptr;
		}

		std::string NeedsText(const RP::Readiness& a_r, bool a_missingOnly)
		{
			std::string out;
			for (const auto& need : a_r.needs) {
				if (a_missingOnly && need.owned >= need.required) continue;
				if (!out.empty()) out += a_missingOnly ? ", " : " | ";
				if (a_missingOnly) out += RuneName(need.token) + " x" + std::to_string(need.required - need.owned);
				else out += RuneName(need.token) + " " + std::to_string(need.owned) + "/" + std::to_string(need.required);
			}
			return out;
		}

		std::string ReadinessMessage(const json& a_formula, const RP::Readiness& a_r)
		{
			switch (a_r.state) {
			case RP::State::kReady: return {};
			case RP::State::kMissingRunes: return "Missing runes: " + NeedsText(a_r, true) + ".";
			case RP::State::kNoItem: return "Select an item first.";
			case RP::State::kInventoryUnknown: return "The rune inventory is not available yet.";
			case RP::State::kCompatibilityUnknown: return "Compatibility is checked when you explicitly select this formula.";
			case RP::State::kItemIneligible:
			case RP::State::kAlreadyApplied:
			case RP::State::kRefused: {
				const auto it = g.verdicts.find(Str(a_formula, "id"));
				return it != g.verdicts.end() && !it->second.message.empty() ? it->second.message : std::string(RP::Name(a_r.state));
			}
			}
			return {};
		}

		void RebuildOps();

		void RefreshSnapshot()
		{
			const auto keepItem = g.items.empty() || g.itemFocus >= g.items.size() ? std::string{} : Str(g.items[g.itemFocus], "instanceKey");
			const auto keepFormula = g.formulas.empty() || g.formulaFocus >= g.formulas.size() ? std::string{} : Str(g.formulas[g.formulaFocus], "id");
			g.snapshot = RuneforgeMenu::Operations::Snapshot();
			g.items.clear();
			g.formulas.clear();
			if (g.snapshot.contains("bases") && g.snapshot["bases"].is_array()) {
				for (const auto& b : g.snapshot["bases"]) {
					if (ItemMatches(b)) g.items.push_back(b);
				}
			}
			std::stable_sort(g.items.begin(), g.items.end(), [](const json& a, const json& b) { return Str(a, "displayName") < Str(b, "displayName"); });
			if (g.snapshot.contains("recipes") && g.snapshot["recipes"].is_array()) {
				for (const auto& r : g.snapshot["recipes"]) {
					if (FormulaMatches(r)) g.formulas.push_back(r);
				}
			}
			g.selectedKey = Str(g.snapshot, SC::kSelectedInstanceKey);
			g.selectedRecipe = Str(Panel(), "recipeId");
			const auto find = [](const std::vector<json>& a_list, const char* a_field, const std::string& a_value, std::size_t a_fallback) {
				for (std::size_t i = 0; i < a_list.size(); ++i) {
					if (!a_value.empty() && Str(a_list[i], a_field) == a_value) return i;
				}
				return a_list.empty() ? std::size_t{ 0 } : std::min(a_fallback, a_list.size() - 1);
			};
			g.itemFocus = find(g.items, "instanceKey", keepItem.empty() ? g.selectedKey : keepItem, g.itemFocus);
			RecomputeReadiness();
			if (g.formulas.size() == g.readiness.size() && g.formulas.size() > 1) {
				std::vector<std::size_t> order(g.formulas.size());
				for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
				std::stable_sort(order.begin(), order.end(), [](std::size_t a, std::size_t b) {
					const auto aReady = g.readiness[a].state == RP::State::kReady;
					const auto bReady = g.readiness[b].state == RP::State::kReady;
					return aReady != bReady && aReady;
				});
				std::vector<json> sortedFormulas;
				std::vector<RP::Readiness> sortedReadiness;
				sortedFormulas.reserve(order.size());
				sortedReadiness.reserve(order.size());
				for (const auto index : order) {
					sortedFormulas.push_back(std::move(g.formulas[index]));
					sortedReadiness.push_back(std::move(g.readiness[index]));
				}
				g.formulas = std::move(sortedFormulas);
				g.readiness = std::move(sortedReadiness);
			}
			g.formulaFocus = find(g.formulas, "id", keepFormula.empty() ? g.selectedRecipe : keepFormula, g.formulaFocus);
			RebuildOps();
		}

		void RebuildOps()
		{
			g.ops.clear();
			const auto& p = Panel();
			const bool known = Flag(g.snapshot, "inventoryKnown") && Flag(g.snapshot, "runtimeEnabled") && !g.selectedKey.empty();
			const auto num = [&](const char* a_key) { return p.contains(a_key) && p[a_key].is_number() ? p[a_key].get<double>() : 0.0; };
			const auto count = [&](const char* a_known, const char* a_owned) {
				return Flag(p, a_known) ? std::to_string(static_cast<long long>(num(a_owned))) : std::string("?");
			};
			// Labels reuse the flat panel's operation labels (ui/runeforge/as2/src/RuneforgeMenu.as).
			g.ops.push_back({ "IDENTIFY (" + count("identifyScrollsKnown", "identifyScrollsOwned") + "/" + std::to_string(static_cast<long long>(num("identifyScrollCost"))) + " SCROLLS)",
				"identify", 0, known && Flag(p, "identifyEligible") });
			if (p.contains("reforgeLockCandidates") && p["reforgeLockCandidates"].is_array()) {
				std::size_t ordinal = 1;
				for (const auto& c : p["reforgeLockCandidates"]) {
					g.ops.push_back({ "REFORGE OPTION " + std::to_string(ordinal++) + " | " + Str(c, "slotKind", "AFFIX") + " | " +
										  std::to_string(static_cast<long long>(num("lockedReforgeCost"))) + " ORBS: " + Str(c, "name", "Unknown affix"),
						"reforge", ParseKey(Str(c, "affixToken")), known && Flag(p, "reforgeEligible") });
				}
			}
			g.ops.push_back({ "EXPAND AFFIX (" + std::to_string(static_cast<long long>(num("expandAffixCost"))) + " ORBS)", "expand", 0,
				known && Flag(p, "expandEligible") && Flag(p, "canExpandAffix") });
			g.ops.push_back({ std::string(g.scourArmed ? "CONFIRM SCOUR (ALL)" : "SCOUR (" + count("scouringOrbsKnown", "scouringOrbsOwned") + "/" +
																					  std::to_string(static_cast<long long>(num("scouringOrbCost"))) + " ORBS, ALL)"),
				"scour", 0, known && Flag(p, "scourEligible") });
			g.ops.push_back({ "TRADE " + std::to_string(static_cast<long long>(num("exchangeReforgeScrollCost"))) + " SCROLLS > 1 REFORGE ORB", "exchangeReforge", 0,
				Flag(p, "exchangeReforgeEligible") });
			g.ops.push_back({ "TRADE " + std::to_string(static_cast<long long>(num("exchangeScourScrollCost"))) + " SCROLLS > 1 SCOUR ORB", "exchangeScour", 0,
				Flag(p, "exchangeScourEligible") });
			if (g.opsFocus >= g.ops.size()) g.opsFocus = g.ops.empty() ? 0 : g.ops.size() - 1;
		}

		// ---------------------------------------------------------------- render (native -> AS2)

		// R8.7.4.18 (PromptPolicy): key caps always name the live keyboard/controller binding of the last non-mouse device.
		std::string KeyName(IP::Button a_button)
		{
			auto* controls = RE::ControlMap::GetSingleton();
			const auto userEvent = std::string(PRP::UserEventFor(a_button));
			const auto device = g.promptDevice == IP::Device::kGamepad ? RE::INPUT_DEVICE::kGamepad : RE::INPUT_DEVICE::kKeyboard;
			const std::uint32_t mapped = controls && !userEvent.empty() ?
				controls->GetMappedKey(userEvent, device, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode) : 0xFFu;
			return PRP::PromptKeyName(a_button, g.promptDevice, mapped, g.settings.promptFamily);
		}

		json Prompt(const char* a_action, IP::Button a_button, const char* a_label)
		{
			return { { "action", a_action }, { "key", KeyName(a_button) }, { "label", a_label } };
		}

		// The footer of a state, from the pure plan the parity tests check (PromptPolicy::PlanFooter).
		void AddFooter(json& a_prompts, SP::State a_state)
		{
			const auto plan = PRP::PlanFooter(a_state, g.detail);
			for (std::size_t i = 0; i < plan.count; ++i) {
				const auto& p = plan.prompts[i];
				a_prompts.push_back({ { "action", std::string(p.action) }, { "key", KeyName(p.button) }, { "label", std::string(p.label) } });
			}
		}

		// PRES-D29: the physical row is never assumed; the panel says what the slab actually holds.
		std::string RowLine()
		{
			switch (g.session.row) {
			case SP::Ack::kPending: return "Rune row: laying the tablets on the slab...";
			case SP::Ack::kOk: return g.rowStatus.empty() ? std::string("Rune row: laid out.") : g.rowStatus;
			case SP::Ack::kBad: return g.rowStatus.empty() ? std::string("Rune row: INCOMPLETE. Go Back and select the formula again.") : g.rowStatus;
			case SP::Ack::kNone: break;
			}
			return {};
		}

		// R8.7.4.21 I3: RunewordEffectCard v1 JSON helpers (lines are already budgeted ASCII; see RunewordEffectCardPolicy).
		std::vector<std::string> CardLines(const json& a_card)
		{
			std::vector<std::string> out;
			if (a_card.is_object() && a_card.contains("lines") && a_card["lines"].is_array()) {
				for (const auto& line : a_card["lines"]) {
					if (line.is_string()) out.push_back(line.get<std::string>());
				}
			}
			return out;
		}

		std::size_t CardCompact(const json& a_card)
		{
			return a_card.is_object() && a_card.contains("compact") && a_card["compact"].is_number_unsigned() ? a_card["compact"].get<std::size_t>() : 0u;
		}

		json FormulaCard(const json& a_formula)
		{
			return a_formula.is_object() && a_formula.contains("effectCard") && a_formula["effectCard"].is_object() ? a_formula["effectCard"] : json::object();
		}

		const json& FocusedItemCard(const json& a_item)
		{
			const auto key = Str(a_item, "instanceKey");
			const auto revision = g.snapshot.is_object() && g.snapshot.contains("snapshotRevision") && g.snapshot["snapshotRevision"].is_number_unsigned() ?
				g.snapshot["snapshotRevision"].get<std::uint64_t>() : 0ull;
			const bool equipped = Flag(a_item, "equipped");
			if (key != g.itemCardKey || revision != g.itemCardRevision || equipped != g.itemCardEquipped) {
				g.itemCardKey = key;
				g.itemCardRevision = revision;
				g.itemCardEquipped = equipped;
				g.itemCard = Str(a_item, "runewordId").empty() ? json::object() : RuneforgeMenu::Operations::ItemEffectCard(ParseKey(key), equipped);
			}
			return g.itemCard;
		}

		json BuildRender()
		{
			const auto state = g.session.state;
			json r = {
				{ "v", 1 }, { "state", std::string(SP::Name(state)) }, { "token", g.session.token },
				{ "device", std::string(PRP::DeviceName(g.promptDevice)) }, { "pointer", g.device == IP::Device::kMouse },
				{ "family", g.settings.promptFamily }, { "status", g.status }, { "detail", g.detail },
				{ "hold", g.session.hold >= 0 ? std::min(1.0, static_cast<double>(g.session.hold) / 800.0) : 0.0 },
				{ "rows", json::array() }, { "focus", 0 }, { "lines", json::array() }, { "glyphs", "" }, { "prompts", json::array() },
			};
			auto& rows = r["rows"];
			auto& lines = r["lines"];
			auto& prompts = r["prompts"];
			static const char* kItemFilters[] = { "ALL", "EQUIPPED", "WEAPONS", "ARMOR" };
			static const char* kFormulaFilters[] = { "ALL", "WEAPON", "ARMOR", "MIXED" };
			switch (state) {
			case SP::State::kEntering:
				r["title"] = "Runeforge";
				lines.push_back("Taking your place at the slab...");
				break;
			case SP::State::kSelectItem:
				r["title"] = "Select Item";
				r["filter"] = std::string("ITEMS: ") + kItemFilters[g.itemFilter];
				for (const auto& item : g.items) {
					std::string label = Str(item, "displayName", "Item");
					if (Flag(item, "equipped")) label += " [Equipped]";
					if (Flag(item, "favorited")) label += " [Favorited]";
					rows.push_back({ { "label", label }, { "enabled", true }, { "selected", Str(item, "instanceKey") == g.selectedKey } });
				}
				r["focus"] = g.itemFocus;
				if (g.items.empty()) lines.push_back("No eligible unique item instances found.");
				else {
					const auto& item = g.items[std::min(g.itemFocus, g.items.size() - 1)];
					std::string kind = Str(item, "itemKind", "item");
					std::transform(kind.begin(), kind.end(), kind.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
					lines.push_back(Str(item, "baseName", "Item") + " (" + kind + ")");
					std::string itemState = Flag(item, "equipped") ? "Equipped" : "In inventory";
					if (Flag(item, "favorited")) itemState += " | Favorited";
					lines.push_back(itemState);
					const auto runewordId = Str(item, "runewordId");
					json itemCard = { { "kind", "item" }, { "present", false } };
					if (!runewordId.empty()) {
						// R8.7.4.21 I3 (B5 G2): the engraved Runeword's effect card for the focused item, by instance key.
						const auto& card = FocusedItemCard(item);
						const auto cardLines = CardLines(card);
						if (!cardLines.empty()) {
							lines.push_back("Existing Runeword: " + cardLines.front());
							for (auto& line : EC::PanelBody(EC::Panel::kSelectItem, cardLines, CardCompact(card), g.detail)) lines.push_back(std::move(line));
							itemCard = card;
							itemCard["kind"] = "item";
							itemCard["present"] = true;
						} else {
							std::string attached = "Existing Runeword: engraved";
							for (const auto& formula : g.formulas) {
								if (Str(formula, "id") == runewordId) {
									attached = "Existing Runeword: " + Str(formula, "name", "engraved");
									break;
								}
							}
							lines.push_back(attached);
						}
					}
					r["card"] = std::move(itemCard);
					std::size_t runeReadyCount = 0;
					std::string availableShortlist;
					for (std::size_t i = 0; i < g.formulas.size(); ++i) {
						const auto* readiness = ReadinessAt(i);
						if (!readiness || !readiness->runesAvailable) continue;
						++runeReadyCount;
						if (runeReadyCount <= 3) {
							if (!availableShortlist.empty()) availableShortlist += ", ";
							availableShortlist += Str(g.formulas[i], "name", "Runeword");
						}
					}
					lines.push_back("Discovered formulas: " + std::to_string(g.formulas.size()) + " | rune-ready: " + std::to_string(runeReadyCount));
					if (!availableShortlist.empty()) lines.push_back("Rune-ready: " + availableShortlist);
					if (runewordId.empty()) lines.push_back("Recipe fit is verified when you select a formula.");
					if (!g.itemStatus.empty()) lines.push_back(g.itemStatus);
				}
				AddFooter(prompts, state);
				break;
			case SP::State::kSelectFormula:
			case SP::State::kConfirm:
			case SP::State::kResultFailure: {
				r["title"] = state == SP::State::kSelectFormula ? "Select Runeword" : "Engrave";
				r["filter"] = std::string("RECOMMENDATION: ") + kFormulaFilters[g.formulaFilter];
				if (state == SP::State::kSelectFormula) {
					for (std::size_t i = 0; i < g.formulas.size(); ++i) {
						const auto& f = g.formulas[i];
						const auto* rd = ReadinessAt(i);
						const std::string tag = rd ? std::string(RP::Name(rd->state)) : std::string("?");
						rows.push_back({ { "label", Str(f, "name", "Runeword") + "  [" + tag + "]" }, { "enabled", true }, { "selected", Str(f, "id") == g.selectedRecipe } });
					}
					r["focus"] = g.formulaFocus;
				}
				if (g.formulas.empty()) {
					lines.push_back("No discovered recipes are available.");
				} else {
					const auto index = std::min(g.formulaFocus, g.formulas.size() - 1);
					const auto& f = g.formulas[index];
					r["glyphs"] = Str(f, "glyphSequence");
					lines.push_back(Str(f, "name", "Runeword"));
					lines.push_back("RUNES: " + Str(f, "runeSequence", "?"));
					// R8.7.4.21 I3 (B5 G1): the formula's effect card. SELECT_FORMULA: the EFFECT column carries the summary
					// and the trigger/chance/cooldown line; CONFIRM shows trigger and exact effect lines without Details.
					auto card = FormulaCard(f);
					const auto cardLines = CardLines(card);
					const auto panel = state == SP::State::kSelectFormula ? EC::Panel::kSelectFormula : EC::Panel::kConfirm;
					auto cardBody = EC::PanelBody(panel, cardLines, CardCompact(card), g.detail);
					if (state == SP::State::kSelectFormula) {
						std::string effect = Str(f, "effect");
						if (!cardBody.empty()) {
							effect += (effect.empty() ? "" : "\n") + cardBody.front();
							cardBody.erase(cardBody.begin());
						}
						lines.push_back(effect);
					} else {
						lines.push_back(Str(f, "effect"));
						for (auto& line : cardBody) lines.push_back(std::move(line));
						cardBody.clear();
					}
					card["kind"] = "formula";
					r["card"] = std::move(card);
					if (const auto* rd = ReadinessAt(index)) {
						const char* compatibility = "UNKNOWN";
						switch (rd->itemVerdict) {
						case RP::ItemVerdict::kEligible: compatibility = "VERIFIED"; break;
						case RP::ItemVerdict::kIneligible: compatibility = "INCOMPATIBLE"; break;
						case RP::ItemVerdict::kAlreadyApplied: compatibility = "ALREADY APPLIED"; break;
						case RP::ItemVerdict::kRefused: compatibility = "REFUSED"; break;
						case RP::ItemVerdict::kUnknown: break;
						}
						if (state == SP::State::kConfirm || (state == SP::State::kSelectFormula && g.detail)) {
							lines.push_back(std::string("RUNES AVAILABLE: ") + (rd->runesAvailable ? "YES" : "NO") +
								"  |  ITEM COMPATIBILITY: " + compatibility + "  |  READY TO ENGRAVE: " +
								(rd->state == RP::State::kReady ? "YES" : "NO"));
						} else {
							lines.push_back(std::string("RUNES AVAILABLE: ") + (rd->runesAvailable ? "YES" : "NO"));
							lines.push_back(std::string("ITEM COMPATIBILITY: ") + compatibility);
							lines.push_back(std::string("READY TO ENGRAVE: ") + (rd->state == RP::State::kReady ? "YES" : "NO"));
						}
						const auto needs = NeedsText(*rd, false);
						if (!needs.empty() && !(state == SP::State::kSelectFormula && g.detail)) lines.push_back("Runes owned/required: " + needs);
						if (rd->state != RP::State::kReady && state != SP::State::kConfirm && !(state == SP::State::kSelectFormula && g.detail)) lines.push_back(ReadinessMessage(f, *rd));
					}
					// SELECT_FORMULA lower block: readiness, then the effect lines (Details: compact readiness + full card).
					for (auto& line : cardBody) lines.push_back(std::move(line));
					const auto row = RowLine();
					if (!row.empty()) lines.push_back(row);
				}
				AddFooter(prompts, state);
				break;
			}
			case SP::State::kOperations:
				r["title"] = "Operations";
				for (const auto& op : g.ops) rows.push_back({ { "label", op.label }, { "enabled", op.enabled }, { "selected", false } });
				r["focus"] = g.opsFocus;
				AddFooter(prompts, state);
				break;
			case SP::State::kCommitting:
			case SP::State::kPerforming:
				r["title"] = "Engrave";
				AddFooter(prompts, state);
				break;
			case SP::State::kResultSuccess:
				r["title"] = "Runeword Complete";
				for (const auto& line : CompletionPolicy::BuildLines(g.result)) lines.push_back(line);
				if (g.resultCard.is_object() && !g.resultCard.empty()) {
					r["card"] = g.resultCard;
					r["card"]["kind"] = "result";
				}
				AddFooter(prompts, state);
				break;
			default:
				r["title"] = "Runeforge";
				break;
			}
			return r;
		}

		void PushRender()
		{
			if (!g.menuOpen) return;
			auto* ui = RE::UI::GetSingleton();
			auto movie = ui ? ui->GetMovieView(kMenuName) : nullptr;
			if (!movie) return;
			const auto payload = BuildRender().dump();
			// run 1: the panel was pushed (and AS2 rebuilt) every frame; push only a changed model
			if (!GP::ShouldPush(g.render, payload)) return;
			RE::GFxValue arg(payload.c_str());
			movie->InvokeNoReturn("_root.runeforgeWsUpdate", &arg, 1);
		}

		// ---------------------------------------------------------------- camera director (CR-1..CR-8)

		RE::ThirdPersonState* ThirdPerson(RE::PlayerCamera* a_camera)
		{
			if (!a_camera) return nullptr;
			auto& state = a_camera->cameraStates[RE::CameraState::kThirdPerson];
			return state ? skyrim_cast<RE::ThirdPersonState*>(state.get()) : nullptr;
		}

		bool ThirdPartyCameraPresent()
		{
			for (const auto* dll : { L"SmoothCam.dll", L"TrueDirectionalMovement.dll", L"ImprovedCameraSE.dll", L"ImprovedCamera.dll" }) {
				if (::GetModuleHandleW(dll)) return true;
			}
			return false;
		}

		void SnapshotCamera()
		{
			g.camera = {};
			g.camVerified = false;
			g.camFramesWaited = 0;
			g.camWrites = 0;
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera) return;
			g.camera.valid = true;
			g.camera.firstPerson = camera->IsInFirstPerson();
			g.camera.worldFov = camera->worldFOV;
			if (auto* tps = ThirdPerson(camera)) {
				g.camera.thirdPersonValid = true;
				g.camera.targetZoom = tps->targetZoomOffset;
				g.camera.currentZoom = tps->currentZoomOffset;
				g.camera.savedZoom = tps->savedZoomOffset;
			}
			// WS-D05 / WS-D18: free camera only when SP-2 enabled it and no camera mod owns the camera.
			g.backend = g.settings.freeCamera && !ThirdPartyCameraPresent() ? Backend::kFree : Backend::kFurniture;
			SKSE::log::info("[FEASR-RF] WS camera snapshot firstPerson={} fov={} backend={}", g.camera.firstPerson, g.camera.worldFov,
				g.backend == Backend::kFree ? "free" : "furniture");
		}

		RE::TESCameraState* FreeState()
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera || !camera->currentState || camera->currentState->id != RE::CameraState::kFree) return nullptr;
			const auto vtable = *reinterpret_cast<const std::uintptr_t*>(camera->currentState.get());
			return vtable == RE::VTABLE_FreeCameraState[0].address() ? camera->currentState.get() : nullptr;
		}

		// x64 MSVC RTTICompleteObjectLocator, stored just before the vtable: does it name FreeCameraState at this offset?
		bool RttiNamesFreeCamera(std::uintptr_t a_vtable, std::uint32_t a_expectedOffset)
		{
			if (!a_vtable) return false;
			struct CompleteObjectLocator
			{
				std::uint32_t signature;
				std::uint32_t offset;
				std::uint32_t cdOffset;
				std::int32_t typeDescriptor;
				std::int32_t classDescriptor;
				std::int32_t self;
			};
			const auto* col = *reinterpret_cast<const CompleteObjectLocator* const*>(a_vtable - sizeof(void*));
			if (!col || col->signature != 1) return false;
			const auto typeDescriptor = REL::Module::get().base() + static_cast<std::uintptr_t>(col->typeDescriptor);
			return col->offset == a_expectedOffset && typeDescriptor == RE::RTTI_FreeCameraState.address();
		}

		bool AllocationSane(const void* a_base)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (::VirtualQuery(a_base, &info, sizeof(info)) != sizeof(info)) return false;
			const auto start = reinterpret_cast<std::uintptr_t>(a_base);
			const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
			const DWORD writable = PAGE_READWRITE | PAGE_EXECUTE_READWRITE;
			return info.State == MEM_COMMIT && (info.Protect & writable) != 0 && (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 &&
			       end >= start + FC::kMinimumSize;
		}

		FC::CameraCheckInput ReadFreeCamera(RE::PlayerCamera* a_camera)
		{
			FC::CameraCheckInput in{};
			in.expectedPrimaryVtable = RE::VTABLE_FreeCameraState[0].address();
			in.expectedSecondaryVtable = RE::VTABLE_FreeCameraState[1].address();
			auto* state = a_camera ? a_camera->currentState.get() : nullptr;
			if (!state) return in;
			in.stateIsFree = state->id == RE::CameraState::kFree;
			const auto* base = reinterpret_cast<const std::uint8_t*>(state);
			in.primaryVtable = *reinterpret_cast<const std::uintptr_t*>(base);
			in.allocationSane = AllocationSane(base);
			if (in.allocationSane) {
				in.secondaryAt20 = *reinterpret_cast<const std::uintptr_t*>(base + FC::kSecondaryVtableOffset);
				std::memcpy(in.translationAt30.data(), base + FC::kTranslationOffset, sizeof(float) * 3);
				std::memcpy(in.rotationAt3C.data(), base + FC::kRotationOffset, sizeof(float) * 2);
			}
			in.rttiMatches = in.primaryVtable == in.expectedPrimaryVtable && in.secondaryAt20 == in.expectedSecondaryVtable &&
			                 RttiNamesFreeCamera(in.primaryVtable, 0) && RttiNamesFreeCamera(in.secondaryAt20, static_cast<std::uint32_t>(FC::kSecondaryVtableOffset));
			if (a_camera->cameraRoot) {
				const auto& p = a_camera->cameraRoot->world.translate;
				in.liveCameraPositionValid = true;
				in.liveCameraPosition = { p.x, p.y, p.z };
			}
			return in;
		}

		void FailClosed(std::string_view a_when, std::uint32_t a_failures)
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (camera && camera->IsInFreeCameraMode()) camera->ToggleFreeCameraMode(false);
			g.backend = Backend::kFurniture;
			g.camVerified = false;
			SKSE::log::warn("[FEASR-RF] WS CAM fail-closed when={} failures={:#x}: furniture camera for the rest of this session ({})", a_when, a_failures,
				g.camWrites ? "writes stopped" : "nothing was written");
		}

		// PRES-D23 self-check: every condition passes before the first write; any failure falls back to the furniture camera.
		bool CheckFreeCamera(const char* a_when)
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			const auto in = ReadFreeCamera(camera);
			const auto result = FC::CheckFreeCamera(in);
			const auto verdict = FC::Judge(result, g.camFramesWaited);
			SKSE::log::info(
				"[FEASR-RF] WS CAM check when={} frame={} stateId={} free={} vt0={:#x} expected={:#x} vt20={:#x} expected={:#x} rtti={} alloc={} "
				"t30=({:.2f},{:.2f},{:.2f}) cameraRoot=({:.2f},{:.2f},{:.2f}) error={:.2f} tolerance={:.0f} rot3C=({:.4f},{:.4f}) failures={:#x} verdict={}",
				a_when, g.camFramesWaited, camera && camera->currentState ? static_cast<int>(camera->currentState->id) : -1, in.stateIsFree, in.primaryVtable,
				in.expectedPrimaryVtable, in.secondaryAt20, in.expectedSecondaryVtable, in.rttiMatches, in.allocationSane, in.translationAt30[0],
				in.translationAt30[1], in.translationAt30[2], in.liveCameraPosition[0], in.liveCameraPosition[1], in.liveCameraPosition[2], result.positionError,
				FC::kPositionTolerance, in.rotationAt3C[0], in.rotationAt3C[1], result.failures, FC::Name(verdict));
			switch (verdict) {
			case FC::CameraVerdict::kVerified:
				g.camVerified = true;
				return true;
			case FC::CameraVerdict::kWait:
				return true;
			case FC::CameraVerdict::kFailClosed:
				FailClosed(a_when, result.failures);
				return false;
			}
			return false;
		}

		bool MayWriteFree(RE::TESCameraState* a_state)
		{
			if (g.backend != Backend::kFree || !a_state) return false;
			if (!g.camVerified) {
				g.camFramesWaited += 1;
				if (!CheckFreeCamera("frame") || !g.camVerified) return false;
				SKSE::log::info("[FEASR-RF] WS CAM first write after {} frame(s); input isolated (lease held, thumbsticks consumed)", g.camFramesWaited);
				return true;
			}
			// structural re-check before every write
			const auto* base = reinterpret_cast<const std::uint8_t*>(a_state);
			const auto vt0 = *reinterpret_cast<const std::uintptr_t*>(base);
			const auto vt20 = *reinterpret_cast<const std::uintptr_t*>(base + FC::kSecondaryVtableOffset);
			if (a_state->id != RE::CameraState::kFree || vt0 != RE::VTABLE_FreeCameraState[0].address() || vt20 != RE::VTABLE_FreeCameraState[1].address()) {
				FailClosed("pre-write", FC::kCamNotFreeState | FC::kCamPrimaryVtable | FC::kCamSecondaryVtable);
				return false;
			}
			return true;
		}

		void EnterFreeCamera()
		{
			if (g.backend != Backend::kFree) return;
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (camera && !camera->IsInFreeCameraMode()) camera->ToggleFreeCameraMode(false);
			if (!CheckFreeCamera("enter")) {
				SKSE::log::warn("[FEASR-RF] WS free camera unavailable (self-check failed); furniture camera for this session");
				if (camera && camera->IsInFreeCameraMode()) camera->ToggleFreeCameraMode(false);
				g.backend = Backend::kFurniture;
			}
		}

		void WriteFreePose(const ShotPolicy::Vec3& a_position, const ShotPolicy::Vec3& a_lookAt)
		{
			auto* state = FreeState();
			if (!state) {
				if (g.backend == Backend::kFree) {
					// CR-5: another system changed the camera state; back off for the rest of the session.
					SKSE::log::warn("[FEASR-RF] WS camera state changed by another system; furniture camera from now on");
					g.backend = Backend::kFurniture;
				}
				return;
			}
			if (!MayWriteFree(state)) return;
			auto* base = reinterpret_cast<std::uint8_t*>(state);
			// The corrected 1.6.1170 layout (PRES-D23); the RC1 0x20/0x34 layout is rejected at compile time.
			auto* translation = reinterpret_cast<RE::NiPoint3*>(base + FC::kTranslationOffset);
			auto* rotation = reinterpret_cast<RE::NiPoint2*>(base + FC::kRotationOffset);
			const auto angles = ShotPolicy::LookAngles(a_position, a_lookAt);
			*translation = { a_position.x, a_position.y, a_position.z };
			rotation->x = angles.pitch;
			rotation->y = angles.yaw;
			g.camWrites += 1;
		}

		ShotPolicy::Frame SlabFrame()
		{
			if (auto* slab = Slab()) {
				const auto pos = slab->GetPosition();
				return { { pos.x, pos.y, pos.z }, slab->GetAngleZ() * 180.0f / ShotPolicy::kPi };
			}
			return ShotPolicy::kThirdForgeSlab;
		}

		void MoveToShot(SP::Shot a_shot)
		{
			const auto* def = ShotPolicy::Find(a_shot);
			if (!def) return;
			// the right-button pan is kept until the next authored shot
			if (!VP::PanIsZero(g.pan)) SKSE::log::info("[FEASR-RF] WS PAN reset at the next shot (was yaw={:.1f} pitch={:.1f})", g.pan.yawDegrees, g.pan.pitchDegrees);
			g.pan = {};
			if (g.panWritten) g.panResetPending = true;
			const auto target = ShotPolicy::WorldPose(SlabFrame(), a_shot);
			const float k = g.moveDuration > 0.0f ? std::min(g.moveElapsed / g.moveDuration, 1.0f) : 1.0f;
			g.from = ShotPolicy::Ease(g.from, g.to, k * g.moveDuration, g.moveDuration);
			if (g.from.position.x == 0.0f && g.from.position.y == 0.0f && g.from.position.z == 0.0f) g.from = target;
			g.to = target;
			g.moveElapsed = 0.0f;
			g.moveDuration = def->moveSeconds;
			if (a_shot == SP::Shot::kEnter) EnterFreeCamera();
		}

		void UpdateCamera(float a_dt)
		{
			if (g.backend != Backend::kFree || g.session.state == SP::State::kIdle || g.session.state == SP::State::kExiting) return;
			g.moveElapsed += a_dt;
			// PRES-D35: during the engraving/bind beat the authored camera holds; the orbit springs back
			const bool beat = g.session.state == SP::State::kPerforming;
			g.orbit = ShotPolicy::UpdateOrbit(g.orbit, beat ? 0.0f : g.rightX, beat ? 0.0f : g.rightY, a_dt);
			const auto pose = ShotPolicy::Ease(g.from, g.to, g.moveElapsed, g.moveDuration, g.settings.reducedMotion);
			// the right-button pan (kept until the next shot) on top of the right-stick orbit, clamped together
			const auto view = VP::CombineView(g.orbit.yawDegrees, g.orbit.pitchDegrees, g.pan);
			WriteFreePose(ShotPolicy::ApplyOrbit(pose, { view.yawDegrees, view.pitchDegrees }), pose.lookAt);
		}

		// ---------------------------------------------------------------- R8.7.4.18 furniture framing (FramingPolicy)

		// Measured pose of the rendered camera (the camera root's world transform, whichever camera state drives it); a_tps is the
		// third-person state only while it is the current state (the only state the framing writes to).
		bool MeasureCamera(RE::ThirdPersonState*& a_tps, std::uint32_t& a_stateId, RE::NiPoint3& a_pos, FRP::Mat3& a_rot)
		{
			a_tps = nullptr;
			a_stateId = 0xFFFFFFFFu;
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera || !camera->cameraRoot || !camera->currentState) return false;
			a_stateId = static_cast<std::uint32_t>(camera->currentState->id);
			if (auto* tps = ThirdPerson(camera); tps && camera->currentState.get() == tps) a_tps = tps;
			const auto& w = camera->cameraRoot->world;
			a_pos = w.translate;
			for (int r = 0; r < 3; ++r)
				for (int c = 0; c < 3; ++c) a_rot.m[r][c] = w.rotate.entry[r][c];
			return std::isfinite(a_pos.x) && std::isfinite(a_pos.y) && std::isfinite(a_pos.z);
		}

		RE::NiPoint3 FramingPivot()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			return player ? player->GetLookingAtLocation() : RE::NiPoint3{};
		}

		// Undo every framing write (free rotation is reset with the pan base; look pitch and zoom to their base values).
		void ReleaseFraming(std::string_view a_why)
		{
			auto& f = g.framing;
			if (!f.active && !f.lookPitchWritten && !f.zoomWritten) return;
			auto* camera = RE::PlayerCamera::GetSingleton();
			auto* tps = ThirdPerson(camera);
			if (f.lookPitchWritten) {
				if (auto* player = RE::PlayerCharacter::GetSingleton()) player->SetRotationX(f.baseLookPitch);
			}
			if (f.zoomWritten && tps) tps->targetZoomOffset = f.baseZoom;
			if (tps && g.panBaseValid) {
				tps->freeRotation = g.panBase;
				g.panBaseValid = false;
			}
			SKSE::log::info("[FEASR-RF] WS FRAME released ({}) channel={} commandedPitch={:.1f} zoom={:.3f}->{:.3f}", a_why, FRP::Name(f.channel),
				f.commandedPitch, f.baseZoom, f.zoom);
			const bool frFailed = f.freeRotationFailed, lpFailed = f.lookPitchFailed, zFailed = f.zoomFailed;
			f = {};
			// a failed channel stays failed for the rest of this session (fail-closed)
			f.freeRotationFailed = frFailed;
			f.lookPitchFailed = lpFailed;
			f.zoomFailed = zFailed;
		}

		// Furniture backend view, every frame: the authored framing (pitch + pull-back) plus the player's right-button pan,
		// both through the furniture camera's own controls, measured after every write. Replaces the pan-only writer.
		void ApplyFurnitureView(float a_dt)
		{
			if (g.backend != Backend::kFurniture) return;
			auto& f = g.framing;
			const bool wanted = g.settings.furnitureFraming && FRP::AppliesTo(g.session.state) && g.seatWatch;
			if (!wanted) {
				if (f.active) ReleaseFraming(g.session.state == SP::State::kExiting ? "exit" : "state");
			}
			RE::ThirdPersonState* tps = nullptr;
			std::uint32_t stateId = 0;
			RE::NiPoint3 pos{};
			FRP::Mat3 rot{};
			const bool measured = MeasureCamera(tps, stateId, pos, rot);
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (wanted && !f.active && measured && player) {
				// start: identify the forward axis against the seated heading, snapshot the bases, choose the channel
				const float h = player->GetAngleZ();
				f.forwardAxis = FRP::ForwardAxis(rot, { std::sin(h), std::cos(h), 0.0f });
				const auto fwd = FRP::AxisOf(rot, f.forwardAxis);
				f.startPitch = FRP::PitchDegreesOf(fwd);
				const auto pivot = FramingPivot();
				f.startDistance = pos.GetDistance(pivot);
				f.baseLookPitch = player->GetAngleX();
				f.baseZoom = tps ? tps->targetZoomOffset : 0.0f;
				f.zoom = f.baseZoom;
				if (!tps) f.zoomFailed = true;   // zoom is a third-person control: not available in this camera state
				if (tps && !g.panBaseValid) {
					g.panBase = tps->freeRotation;
					g.panBaseValid = true;
				}
				f.channel = tps && tps->freeRotationEnabled && !f.freeRotationFailed ? FRP::Channel::kFreeRotation :
				            (!f.lookPitchFailed ? FRP::Channel::kLookPitch : FRP::Channel::kNone);
				// PC1.1: start on the engine sign (free rotation looks down with a negative pitch) - one monotonic ramp, no up/down probe
				f.pitchSign = FRP::InitialSign(f.channel);
				f.startElevation = FRP::ElevationDegrees({ pos.x, pos.y, pos.z }, { pivot.x, pivot.y, pivot.z });
				f.measuredElevation = f.startElevation;
				f.nextTraceAt = 0.0f;
				f.active = true;
				f.elapsed = 0.0f;
				SKSE::log::info("[FEASR-RF] WS FRAME begin state={} cameraState={} channel={} sign={:+.0f} freeRotationEnabled={} axis={:#x} startPitch={:.1f} startElevation={:.1f} "
				                "startDistance={:.1f} zoom={:.3f} cam=({:.1f},{:.1f},{:.1f}) target pitch={:.1f} pullBack={:.1f}",
					SP::Name(g.session.state), stateId, FRP::Name(f.channel), f.pitchSign, tps && tps->freeRotationEnabled, f.forwardAxis, f.startPitch,
					f.startElevation, f.startDistance, f.baseZoom, pos.x, pos.y, pos.z, g.settings.framingPitch, g.settings.framingPullBack);
			}
			const float pitchTarget = f.active ? g.settings.framingPitch * FRP::Ramp(f.elapsed, g.settings.reducedMotion) : 0.0f;
			if (f.active) {
				f.elapsed += a_dt;
				f.commandedPitch = pitchTarget;
			}
			// write rotation: free rotation carries framing + pan; the look-pitch channel carries framing only (pan needs free rotation)
			const bool tpsActive = measured && tps != nullptr;
			if (tpsActive && tps->freeRotationEnabled && (f.channel == FRP::Channel::kFreeRotation || !VP::PanIsZero(g.pan) || g.panResetPending)) {
				if (!g.panBaseValid) {
					g.panBase = tps->freeRotation;
					g.panBaseValid = true;
				}
				const float framingPitch = f.channel == FRP::Channel::kFreeRotation ? f.pitchSign * f.commandedPitch : 0.0f;
				tps->freeRotation.x = g.panBase.x + g.pan.yawDegrees * VP::kPi / 180.0f;
				tps->freeRotation.y = g.panBase.y + (framingPitch + g.pan.pitchDegrees) * VP::kPi / 180.0f;
				g.panWritten = !VP::PanIsZero(g.pan);
				if (g.panResetPending) g.panResetPending = false;
			} else if (!VP::PanIsZero(g.pan) && !g.panUnavailableLogged) {
				SKSE::log::info("[FEASR-RF] WS PAN the furniture camera does not allow it here (state={} thirdPersonActive={} freeRotation={})",
					measured ? static_cast<int>(stateId) : -1, tpsActive, tpsActive && tps->freeRotationEnabled);
				g.panUnavailableLogged = true;
				g.panResetPending = false;
			}
			if (f.active && f.channel == FRP::Channel::kLookPitch && player) {
				player->SetRotationX(f.baseLookPitch + f.pitchSign * f.commandedPitch * VP::kPi / 180.0f);
				f.lookPitchWritten = true;
			}
			if (!f.active || !measured) return;
			// measure and judge
			const auto fwd = FRP::AxisOf(rot, f.forwardAxis);
			f.forward = fwd;
			f.cameraPos = pos;
			f.measuredPitch = FRP::PitchDegreesOf(fwd);
			const auto pivotNow = FramingPivot();
			f.measuredDistance = pos.GetDistance(pivotNow);
			f.measuredElevation = FRP::ElevationDegrees({ pos.x, pos.y, pos.z }, { pivotNow.x, pivotNow.y, pivotNow.z });
			// PC1.1: bounded activation trace (every 0.1 s until the frame has settled) - the evidence for a single controlled transition
			if (!f.settledLogged && f.elapsed >= f.nextTraceAt) {
				f.nextTraceAt = f.elapsed + 0.1f;
				SKSE::log::info("[FEASR-RF] WS FRAME trace t={:.2f} channel={} sign={:+.0f} commanded={:.1f} elevation={:.1f} pitch={:.1f} distance={:.1f} "
				                "cam=({:.1f},{:.1f},{:.1f}) state={}",
					f.elapsed, FRP::Name(f.channel), f.pitchSign, f.commandedPitch, f.measuredElevation, f.measuredPitch, f.measuredDistance, pos.x, pos.y, pos.z,
					SP::Name(g.session.state));
			}
			if (!f.signJudged && f.channel != FRP::Channel::kNone && f.elapsed > FRP::kJudgeAfterSeconds) {
				switch (FRP::JudgeChannel(f.channel, f.commandedPitch, f.measuredPitch - f.startPitch, f.measuredElevation - f.startElevation)) {
				case FRP::SignVerdict::kPending: break;
				case FRP::SignVerdict::kSame: f.signJudged = true; break;
				case FRP::SignVerdict::kFlipped:
					f.pitchSign = -f.pitchSign;
					f.signJudged = true;
					SKSE::log::warn("[FEASR-RF] WS FRAME channel={} pitches the other way (elevation {:.1f}->{:.1f}, pitch {:.1f}->{:.1f}): sign flipped",
						FRP::Name(f.channel), f.startElevation, f.measuredElevation, f.startPitch, f.measuredPitch);
					break;
				case FRP::SignVerdict::kNoEffect: {
					SKSE::log::warn("[FEASR-RF] WS FRAME channel={} has no measurable effect (commanded {:.1f}, measured {:.1f}): reverted, fail-closed for this session",
						FRP::Name(f.channel), f.commandedPitch, f.measuredPitch - f.startPitch);
					if (f.channel == FRP::Channel::kFreeRotation) {
						f.freeRotationFailed = true;
						if (g.panBaseValid && tps) tps->freeRotation.y = g.panBase.y + g.pan.pitchDegrees * VP::kPi / 180.0f;
						f.channel = f.lookPitchFailed ? FRP::Channel::kNone : FRP::Channel::kLookPitch;
						f.pitchSign = FRP::InitialSign(f.channel);   // PC1.1: the look-pitch channel keeps its r87418 sign
					} else {
						f.lookPitchFailed = true;
						if (f.lookPitchWritten) player->SetRotationX(f.baseLookPitch);
						f.lookPitchWritten = false;
						f.channel = FRP::Channel::kNone;
					}
					f.elapsed = 0.0f;
					f.startPitch = f.measuredPitch;
					f.startElevation = f.measuredElevation;
					break;
				}
				}
			}
			// closed-loop zoom toward the pull-back distance
			if (!f.zoomFailed && tps && g.settings.framingPullBack > 0.0f) {
				const float target = f.startDistance + g.settings.framingPullBack;
				const float next = FRP::ZoomStep(f.zoom, f.baseZoom, f.measuredDistance, target, a_dt);
				if (next > f.zoom) f.zoomFramesIncreasing += 1;
				f.zoom = next;
				tps->targetZoomOffset = f.zoom;
				f.zoomWritten = true;
				if (FRP::ZoomHasNoEffect(f.zoomFramesIncreasing, f.measuredDistance - f.startDistance)) {
					SKSE::log::warn("[FEASR-RF] WS FRAME zoom has no measurable effect (distance {:.1f} -> {:.1f}): reverted, fail-closed for this session",
						f.startDistance, f.measuredDistance);
					tps->targetZoomOffset = f.baseZoom;
					f.zoom = f.baseZoom;
					f.zoomFailed = true;
				}
			}
			if (!f.settledLogged && f.elapsed > FRP::kRampSeconds + 0.6f) {
				f.settledLogged = true;
				SKSE::log::info("[FEASR-RF] WS FRAME settled state={} channel={} sign={:+.0f} elevation {:.1f}->{:.1f} pitch {:.1f}->{:.1f} (commanded {:.1f}) distance {:.1f}->{:.1f} "
				                "(target {:.1f}) zoom {:.3f}->{:.3f} cam=({:.1f},{:.1f},{:.1f}) fwd=({:.3f},{:.3f},{:.3f}) zoomFailed={} freeRotationFailed={} lookPitchFailed={}",
					SP::Name(g.session.state), FRP::Name(f.channel), f.pitchSign, f.startElevation, f.measuredElevation, f.startPitch, f.measuredPitch, f.commandedPitch, f.startDistance,
					f.measuredDistance, f.startDistance + g.settings.framingPullBack, f.baseZoom, f.zoom, pos.x, pos.y, pos.z, fwd.x, fwd.y, fwd.z,
					f.zoomFailed, f.freeRotationFailed, f.lookPitchFailed);
			}
		}

		void RestoreCamera()
		{
			ReleaseFraming("restore");
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera || !g.camera.valid) return;
			if (camera->IsInFreeCameraMode()) camera->ToggleFreeCameraMode(false);
			if (g.camera.firstPerson && !camera->IsInFirstPerson()) camera->ForceFirstPerson();
			if (!g.camera.firstPerson && g.camera.thirdPersonValid) {
				if (auto* tps = ThirdPerson(camera)) {
					tps->targetZoomOffset = g.camera.targetZoom;
					tps->currentZoomOffset = g.camera.currentZoom;
					tps->savedZoomOffset = g.camera.savedZoom;
				}
			}
			camera->worldFOV = g.camera.worldFov;
			g.camera.valid = false;
			g.backend = Backend::kFurniture;
			g.orbit = {};
			g.from = {};
			g.to = {};
			g.pan = {};
			g.panning = false;
			SKSE::log::info("[FEASR-RF] WS camera restored firstPerson={}", camera->IsInFirstPerson());
		}

		void SetHudHidden(bool a_hidden)
		{
			auto* ui = RE::UI::GetSingleton();
			auto movie = ui ? ui->GetMovieView(RE::HUDMenu::MENU_NAME) : nullptr;
			if (!movie) return;
			constexpr const char* kCrosshair = "_root.HUDMovieBaseInstance.Crosshair._alpha";
			constexpr const char* kCompass = "_root.HUDMovieBaseInstance.CompassShoutMeterHolder._alpha";
			if (a_hidden) {
				RE::GFxValue value;
				g.hud = {};
				if (movie->GetVariable(&value, kCrosshair) && value.IsNumber()) g.hud.crosshairAlpha = value.GetNumber();
				if (movie->GetVariable(&value, kCompass) && value.IsNumber()) g.hud.compassAlpha = value.GetNumber();
				g.hud.valid = true;
				movie->SetVariable(kCrosshair, RE::GFxValue(0.0));
				movie->SetVariable(kCompass, RE::GFxValue(0.0));
			} else if (g.hud.valid) {
				movie->SetVariable(kCrosshair, RE::GFxValue(g.hud.crosshairAlpha));
				movie->SetVariable(kCompass, RE::GFxValue(g.hud.compassAlpha));
				g.hud.valid = false;
			}
		}

		// ---------------------------------------------------------------- stance (PRES-D27: seat-or-abort)

		bool InStance()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* stance = StanceRef();
			if (!player || !stance) return false;
			const auto occupied = player->GetOccupiedFurniture().get();
			return occupied && occupied.get() == stance;
		}

		bool StanceOccupiedByOther(RE::TESObjectREFR* a_stance)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* processLists = RE::ProcessLists::GetSingleton();
			if (!a_stance || !processLists) return false;
			for (auto& handle : processLists->highActorHandles) {
				auto actor = handle.get();
				if (actor && actor.get() != player && actor->GetOccupiedFurniture().get().get() == a_stance) return true;
			}
			return false;
		}

		std::string RefText(const RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) return "none";
			const auto* base = a_ref->GetBaseObject();
			return fmt::format("{:08X}/{:08X}", a_ref->GetFormID(), base ? base->GetFormID() : 0u);
		}

		constexpr float kRadToDeg = 57.29577951308232f;

		// The authored work datum (PRES-D33, feasr-core/alpha/workstation/data/workstation-stance-datum.v1.json): the stance ref
		// transform composed with HammerTableMarker's SIT marker (offset 0, heading 0). Skyrim heading: positive Z turns clockwise
		// seen from above, so local (x, y) maps to world (x cos + y sin, -x sin + y cos).
		struct SeatGeometry
		{
			float planar{ 0.0f };
			float vertical{ 0.0f };
			float headingErrorDeg{ 0.0f };
			float headingDeg{ 0.0f };
			float distance{ 0.0f };
			RE::NiPoint3 slabLocal{};
		};

		SeatGeometry MeasureSeat(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_stance)
		{
			SeatGeometry geo{};
			const auto p = a_player->GetPosition();
			const auto s = a_stance->GetPosition();
			const float yaw = a_stance->GetAngleZ();
			const float c = std::cos(yaw);
			const float sn = std::sin(yaw);
			const RE::NiPoint3 datum{ s.x + SeatP::kMarkerOffsetX * c + SeatP::kMarkerOffsetY * sn, s.y - SeatP::kMarkerOffsetX * sn + SeatP::kMarkerOffsetY * c,
				s.z + SeatP::kMarkerOffsetZ };
			geo.planar = std::hypot(p.x - datum.x, p.y - datum.y);
			geo.vertical = p.z - datum.z;
			geo.headingDeg = a_player->GetAngleZ() * kRadToDeg;
			geo.headingErrorDeg = SeatP::HeadingErrorDeg(geo.headingDeg, yaw * kRadToDeg + SeatP::kMarkerHeadingDeg);
			geo.distance = p.GetDistance(s);
			if (auto* slab = Slab()) {
				const auto o = slab->GetPosition();
				const float a = slab->GetAngleZ();
				const float dx = p.x - o.x;
				const float dy = p.y - o.y;
				geo.slabLocal = { dx * std::cos(a) - dy * std::sin(a), dx * std::sin(a) + dy * std::cos(a), p.z - o.z };
			}
			return geo;
		}

		SeatP::Observation ObserveSeat(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_stance, bool a_checkOthers, std::string* a_occupied, SeatGeometry* a_geo = nullptr)
		{
			SeatP::Observation o{};
			o.nowMs = NowMs();
			const auto occupied = a_player->GetOccupiedFurniture().get();
			o.occupiesStance = occupied && occupied.get() == a_stance;
			if (a_occupied) *a_occupied = RefText(occupied.get());
			const auto* state = a_player->AsActorState();
			o.sitState = state ? static_cast<std::uint32_t>(state->GetSitSleepState()) : 0u;
			o.weaponDrawn = state && state->IsWeaponDrawn();
			auto* camera = RE::PlayerCamera::GetSingleton();
			o.firstPerson = camera && camera->IsInFirstPerson();
			const auto geo = MeasureSeat(a_player, a_stance);
			o.planarToDatum = geo.planar;
			o.verticalToDatum = geo.vertical;
			o.headingErrorDeg = geo.headingErrorDeg;
			const auto pos = a_player->GetPosition();
			o.movedSinceLastPoll = g.lastPlayerPosValid ? pos.GetDistance(g.lastPlayerPos) : 0.0f;
			g.lastPlayerPos = pos;
			g.lastPlayerPosValid = true;
			o.player3DLoaded = a_player->Is3DLoaded();
			o.stanceOccupiedByOther = a_checkOthers && StanceOccupiedByOther(a_stance);
			if (a_geo) *a_geo = geo;
			return o;
		}

		void LogSeat(std::string_view a_what, const SeatP::Observation& a_o, const std::string& a_occupied)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* stance = StanceRef();
			const auto geo = player && stance ? MeasureSeat(player, stance) : SeatGeometry{};
			SKSE::log::info(
				"[FEASR-RF] WS SEAT {} t={}ms phase={} expected={} occupied={} sit={}({}) slabLocal=({:.1f},{:.1f},{:.1f}) heading={:.1f} datumPlanar={:.1f} "
				"datumDz={:.1f} headingErr={:.1f} moved={:.1f} onDatum={} places={} attempts={} weaponDrawn={} sneaking={} mounted={} inCombat={} firstPerson={} "
				"distance={:.1f} otherOccupant={} state={}",
				a_what, a_o.nowMs - g.seatEnterMs, SeatP::Name(g.seat.phase), RefText(stance), a_occupied, SeatP::SitName(a_o.sitState), a_o.sitState,
				geo.slabLocal.x, geo.slabLocal.y, geo.slabLocal.z, geo.headingDeg, a_o.planarToDatum, a_o.verticalToDatum, a_o.headingErrorDeg,
				a_o.movedSinceLastPoll, SeatP::OnDatum(a_o), g.seat.placeAttempts, g.seat.attempts, a_o.weaponDrawn, player && player->IsSneaking(),
				player && player->IsOnMount(), player && player->IsInCombat(), a_o.firstPerson, geo.distance, a_o.stanceOccupiedByOther, SP::Name(g.session.state));
		}

		// kStanceEnter: the acquisition starts; Tick polls it every frame (SeatPolicy) until SEATED or the bounded window ends.
		void EnterStance()
		{
			g.seat = {};
			g.seatPhaseReported = SeatP::Phase::kPlace;
			g.lastPlayerPosValid = false;
			g.seatWatch = true;
			g.seatEnterMs = NowMs();
			g.seatStatusMs = 0;
			g.seatForcedThirdPerson = false;
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* stance = StanceRef();
			if (!player || !stance) {
				SKSE::log::error("[FEASR-RF] WS SEAT pre-check failed: player={} stance={} (the acquisition will time out)", player != nullptr, stance != nullptr);
				return;
			}
			std::string occupied;
			const auto o = ObserveSeat(player, stance, true, &occupied);
			LogSeat("pre-check", o, occupied);
			if (HandleStaleStanceOnEnter(o, occupied)) return;
		}

		void PollSeat()
		{
			if (!g.seatWatch) return;
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* stance = StanceRef();
			if (!player || !stance) return;
			const bool entering = g.session.state == SP::State::kEntering;
			std::string occupied;
			const auto o = ObserveSeat(player, stance, entering, &occupied);
			const auto action = SeatP::Next(g.seat, o);
			// PRES-D33: the acquisition phase goes to the state machine (kAcq) before the action it leads to
			if (entering && g.seat.phase != g.seatPhaseReported) {
				g.seatPhaseReported = g.seat.phase;
				LogSeat(fmt::format("acq={}", SeatP::Name(g.seat.phase)), o, occupied);
				Dispatch({ SP::EventKind::kAcq, static_cast<std::int64_t>(g.seat.phase), 0, {} });
			}
			switch (action) {
			case SeatP::Action::kNone:
				if (entering && o.nowMs - g.seatStatusMs >= kSeatStatusLogMs) {
					g.seatStatusMs = o.nowMs;
					LogSeat("poll", o, occupied);
				}
				return;
			case SeatP::Action::kForceThirdPerson:
				if (auto* camera = RE::PlayerCamera::GetSingleton()) camera->ForceThirdPerson();
				g.seatForcedThirdPerson = true;
				LogSeat("action=force-third-person (the stance furniture needs third person)", o, occupied);
				return;
			case SeatP::Action::kSheathe:
				player->DrawWeaponMagicHands(false);
				LogSeat("action=sheathe", o, occupied);
				return;
			case SeatP::Action::kPlace:
				// the datum is the stance ref transform: MoveTo passes its position and angle to the engine's MoveTo_Impl
				player->MoveTo(stance);
				LogSeat(fmt::format("action=place-on-datum attempt={}", g.seat.placeAttempts), o, occupied);
				return;
			case SeatP::Action::kActivate:
				LogSeat(fmt::format("action=activate attempt={}", g.seat.attempts), o, occupied);
				stance->ActivateRef(player, 0, nullptr, 0, false);
				return;
			case SeatP::Action::kReportSeated:
				LogSeat(fmt::format("verdict=SEATED elapsedMs={}", o.nowMs - g.seatEnterMs), o, occupied);
				Dispatch({ SP::EventKind::kSeated, 1, 0, {} });
				return;
			case SeatP::Action::kReportUnseated:
				LogSeat(entering ? "lost before the acquisition completed" : "verdict=STANCE_LOST (the player left the stance)", o, occupied);
				Dispatch({ SP::EventKind::kSeated, 0, 0, {} });
				return;
			case SeatP::Action::kAbortNormalOccupied:
				LogSeat(fmt::format("verdict=ABORT_STANCE_FAILED reason=expected-occupancy-kNormal graceMs={} no-reactivation", SeatP::kNormalOccupiedGraceMs), o, occupied);
				Dispatch({ SP::EventKind::kInterrupt, 0, 0, SP::Cause::kStanceFailed });
				return;
			}
		}

		void ExitStance()
		{
			g.seatWatch = false;
			if (InStance()) {
				if (auto* player = RE::PlayerCharacter::GetSingleton()) player->StopInteractingQuick(false);
			}
		}

		bool SessionNeedsSettlement()
		{
			return g.session.state == SP::State::kExiting && (g.session.seated || InStance() || g.exit.settlementRequired);
		}

		void CapturePreSessionControls()
		{
			g.exit = {};
			const auto raw = ReadControls();
			const auto check = CP::SelfCheck(raw, ControlsLayout(), false);
			g.exit.preSessionControlsValid = check.ok;
			g.exit.preSessionControls = raw.flags;
			SKSE::log::info("[FEASR-RF] WS CTRL pre-session baseline={:#010x} selfcheck={} faults={:#04x} {}", raw.flags,
				check.ok ? "OK" : "REJECT", check.faults, ControlsText(raw));
		}

		void OnLeaseBaseline(std::uint32_t a_controls)
		{
			if (!g.exit.preSessionControlsValid) {
				g.exit.furnitureSuppressed = 0;
				SKSE::log::error("[FEASR-RF] WS CTRL furnitureSuppressed=UNRESOLVED (pre-session baseline invalid)");
				return;
			}
			g.exit.furnitureSuppressed = (g.exit.preSessionControls & ~a_controls) & LP::kLeasedFlags;
			SKSE::log::info("[FEASR-RF] WS CTRL lease-baseline preSession={:#010x} leaseSnapshot={:#010x} furnitureSuppressed={:#05x}",
				g.exit.preSessionControls, a_controls, g.exit.furnitureSuppressed);
		}

		bool QueueCameraRestore()
		{
			if (!SessionNeedsSettlement()) return false;
			g.exit.cameraRestoreQueued = true;
			SKSE::log::info("[FEASR-RF] WS STANCE EXIT defer camera restore until furniture settlement");
			return true;
		}

		bool QueueHudRestore()
		{
			if (!SessionNeedsSettlement()) return false;
			g.exit.hudRestoreQueued = true;
			SKSE::log::info("[FEASR-RF] WS STANCE EXIT defer HUD restore until furniture settlement");
			return true;
		}

		bool QueueLeaseRelease(std::uint32_t a_token)
		{
			if (!SessionNeedsSettlement()) return false;
			g.exit.leaseReleaseQueued = true;
			g.exit.leaseReleaseToken = a_token;
			SKSE::log::info("[FEASR-RF] WS STANCE EXIT defer lease release token={} until furniture settlement", a_token);
			return true;
		}

		XP::Observation ExitObservation(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_stance, std::string& a_occupied)
		{
			XP::Observation out{};
			if (!a_player || !a_stance) return out;
			const auto occupied = a_player->GetOccupiedFurniture().get();
			out.stanceOccupied = occupied && occupied.get() == a_stance;
			a_occupied = RefText(occupied.get());
			if (const auto* state = a_player->AsActorState()) out.sitState = static_cast<std::uint32_t>(state->GetSitSleepState());
			const auto raw = ReadControls();
			out.controls = raw.flags;
			out.furnitureSuppressed = g.exit.furnitureSuppressed;
			return out;
		}

		void IssueFurnitureExit(std::string_view a_why)
		{
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				player->StopInteractingQuick(false);
				SKSE::log::info("[FEASR-RF] WS STANCE EXIT StopInteractingQuick(false) reason={}", a_why);
			}
		}

		bool BeginExitSettlement()
		{
			g.seatWatch = false;
			const bool required = g.session.seated || InStance();
			if (!required) return false;
			g.exit.settlementRequired = true;
			g.exit.settlementReached = false;
			g.exit.settlementFailed = false;
			g.exit.entryRecovery = false;
			g.exit.lastPollLogMs = 0;
			g.exit.finalBaselineLogged = false;
			g.exit.heldMs = 0;
			g.exit.holdBypassLogged = false;
			XP::Start(g.exit.tracker);
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* stance = StanceRef();
			std::string occupied;
			const auto obs = ExitObservation(player, stance, occupied);
			SKSE::log::info("[FEASR-RF] WS STANCE EXIT begin occupied={} sit={}({}) controls={:#010x} furnitureSuppressed={:#05x}", occupied,
				SeatP::SitName(obs.sitState), obs.sitState, obs.controls, obs.furnitureSuppressed);
			IssueFurnitureExit("session-exit");
			UpdateActive();
			return true;
		}

		void RestartSeatAfterStaleRecovery()
		{
			g.seat = {};
			g.seatPhaseReported = SeatP::Phase::kPlace;
			g.lastPlayerPosValid = false;
			g.seatWatch = true;
			g.seatEnterMs = NowMs();
			g.seatStatusMs = 0;
			g.seatForcedThirdPerson = false;
			g.session.t = 0;
			g.session.acq = SP::AcqPhase::kPlace;
			SKSE::log::info("[FEASR-RF] WS STALE_STANCE recovery=EXITED -> fresh seat acquisition");
		}

		bool HandleStaleStanceOnEnter(const SeatP::Observation& a_observed, const std::string& a_occupied)
		{
			const bool stale = a_observed.occupiesStance &&
				(a_observed.sitState != SeatP::kSitIsSitting || !SeatP::OnDatum(a_observed));
			if (!stale) return false;
			g.seatWatch = false;
			g.exit.entryRecovery = true;
			g.exit.settlementRequired = false;
			g.exit.settlementReached = false;
			g.exit.settlementFailed = false;
			g.exit.furnitureSuppressed = 0;
			g.exit.lastPollLogMs = 0;
			g.exit.heldMs = 0;
			g.exit.holdBypassLogged = false;
			XP::Start(g.exit.tracker);
			SKSE::log::warn("[FEASR-RF] WS STALE_STANCE_ON_ENTER occupied={} sit={}({}) datumPlanar={:.1f} onDatum={} controls={:#010x}; bounded recovery first",
				a_occupied, SeatP::SitName(a_observed.sitState), a_observed.sitState, a_observed.planarToDatum, SeatP::OnDatum(a_observed), EnabledControls());
			IssueFurnitureExit("stale-enter-recovery");
			UpdateActive();
			return true;
		}

		void FinishSettledExit()
		{
			g.exit.settlementReached = true;
			SKSE::log::info("[FEASR-RF] WS STANCE EXIT verdict=EXITED elapsedMs={} occupied=none sit=kNormal controls={:#010x} furnitureSuppressed={:#05x}",
				g.exit.tracker.elapsedMs, EnabledControls(), g.exit.furnitureSuppressed);
			if (g.exit.cameraRestoreQueued) {
				RestoreCamera();
				g.exit.cameraRestoreQueued = false;
			}
			if (g.exit.hudRestoreQueued) {
				SetHudHidden(false);
				g.exit.hudRestoreQueued = false;
			}
			if (g.exit.leaseReleaseQueued) {
				const auto token = g.exit.leaseReleaseToken;
				g.exit.leaseReleaseQueued = false;
				ReleaseLease(token, "r3 stance settled");
			}
		}

		void RecoverFailedExit(std::string_view a_why)
		{
			SKSE::log::error("[FEASR-RF] WS STANCE EXIT hard recovery reason={}; restoring presentation and releasing Runeforge-owned lease without claiming clean settlement", a_why);
			if (g.exit.cameraRestoreQueued) {
				RestoreCamera();
				g.exit.cameraRestoreQueued = false;
			}
			if (g.exit.hudRestoreQueued) {
				SetHudHidden(false);
				g.exit.hudRestoreQueued = false;
			} else {
				SetHudHidden(false);
			}
			if (g.exit.leaseReleaseQueued) {
				const auto token = g.exit.leaseReleaseToken;
				g.exit.leaseReleaseQueued = false;
				ReleaseLease(token, a_why);
			}
		}

		// PC1.1: the player stands in a loaded cell that is not the forge cell (coc, teleport, load). Null cells (mid-load) are not "left".
		bool PlayerLeftWorkstationCell(RE::PlayerCharacter* a_player)
		{
			auto* forge = ForgeCell();
			auto* cell = a_player ? a_player->GetParentCell() : nullptr;
			return forge && cell && cell != forge;
		}

		bool GamePaused()
		{
			auto* ui = RE::UI::GetSingleton();
			return ui && ui->GameIsPaused();
		}

		// PC1.1: drop tracked foreign menus the UI no longer reports open (a close the sink never saw, e.g. across a cell load).
		void PruneClosedForeignMenus()
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui || g.uiForeignMenus.empty()) return;
			std::erase_if(g.uiForeignMenus, [ui](const std::string& a_name) {
				if (ui->IsMenuOpen(a_name)) return false;
				SKSE::log::info("[FEASR-RF] WS UI foreign menu '{}' is no longer open: dropped from ownership tracking (close not observed)", a_name);
				return true;
			});
		}

		void PollExitSettlement(std::int64_t a_deltaMs)
		{
			if (!g.exit.tracker.active) return;
			auto* player = RE::PlayerCharacter::GetSingleton();
			PruneClosedForeignMenus();
			const bool paused = GamePaused();
			const bool leftCell = PlayerLeftWorkstationCell(player);
			const UIO::ExitHoldInput hold{ !g.uiForeignMenus.empty(), paused, leftCell, g.exit.heldMs };
			if (UIO::HoldsExitSettlement(hold)) {
				g.exit.heldMs += std::max<std::int64_t>(0, a_deltaMs);
				const auto now = NowMs();
				if (g.exit.lastPollLogMs == 0 || now - g.exit.lastPollLogMs >= 500) {
					g.exit.lastPollLogMs = now;
					SKSE::log::info("[FEASR-RF] WS STANCE EXIT paused for foreign UI menu='{}' openCount={} elapsedMs={} heldMs={} capMs={} (game paused; timeout budget held)",
						g.uiForeignMenus.front(), g.uiForeignMenus.size(), g.exit.tracker.elapsedMs, g.exit.heldMs, UIO::kExitHoldCapMs);
				}
				return;
			}
			if (!g.uiForeignMenus.empty() && !g.exit.holdBypassLogged) {
				g.exit.holdBypassLogged = true;
				SKSE::log::warn("[FEASR-RF] WS STANCE EXIT foreign UI menu='{}' openCount={} does not hold the exit (gamePaused={} leftWorkstationCell={} heldMs={} capMs={})",
					g.uiForeignMenus.front(), g.uiForeignMenus.size(), paused, leftCell, g.exit.heldMs, UIO::kExitHoldCapMs);
			}
			auto* stance = StanceRef();
			if (!player || !stance) {
				if (a_deltaMs > 0) g.exit.tracker.elapsedMs += a_deltaMs;
				if (g.exit.tracker.elapsedMs >= XP::kHardTimeoutMs) {
					g.exit.tracker.active = false;
					g.exit.settlementFailed = true;
					SKSE::log::error("[FEASR-RF] WS STANCE EXIT verdict=FAILSAFE_STANCE_EXIT reason=missing-player-or-stance hardTimeoutMs={}", XP::kHardTimeoutMs);
					if (!g.exit.entryRecovery) RecoverFailedExit("r3.4 hard-timeout missing-player-or-stance");
				}
				return;
			}
			std::string occupied;
			const auto obs = ExitObservation(player, stance, occupied);
			auto action = XP::Action::kNone;
			if (leftCell) {
				// PC1.1: the furniture cannot be occupied from another cell - finish now (stale-entry recovery always takes the failsafe path)
				action = XP::LeaveCell(g.exit.tracker, obs);
				if (g.exit.entryRecovery && action == XP::Action::kSettled) action = XP::Action::kHardTimeout;
				auto* cell = player->GetParentCell();
				SKSE::log::warn("[FEASR-RF] WS STANCE EXIT player left the workstation cell (now {:08X}) elapsedMs={} occupied={} sit={}({}) controls={:#010x} action={}",
					cell ? cell->GetFormID() : 0u, g.exit.tracker.elapsedMs, occupied, SeatP::SitName(obs.sitState), obs.sitState, obs.controls, XP::Name(action));
			} else {
				action = XP::Next(g.exit.tracker, obs, a_deltaMs);
			}
			const auto now = NowMs();
			if (g.exit.lastPollLogMs == 0 || now - g.exit.lastPollLogMs >= 500 || action != XP::Action::kNone) {
				g.exit.lastPollLogMs = now;
				SKSE::log::info("[FEASR-RF] WS STANCE EXIT poll elapsedMs={} occupied={} sit={}({}) controls={:#010x} furnitureRestored={} stableMs={} action={}",
					g.exit.tracker.elapsedMs, occupied, SeatP::SitName(obs.sitState), obs.sitState, obs.controls,
					XP::FurnitureControlsRestored(obs), g.exit.tracker.stableMs, XP::Name(action));
			}
			switch (action) {
			case XP::Action::kNone:
				return;
			case XP::Action::kReissueStop:
				IssueFurnitureExit(g.exit.entryRecovery ? "stale-enter-reissue" : "session-exit-reissue");
				return;
			case XP::Action::kSoftTimeout:
				SKSE::log::warn("[FEASR-RF] WS STANCE EXIT verdict=SLOW_EXIT elapsedMs={} occupied={} sit={}({}) controls={:#010x}; continuing bounded observation to hardTimeoutMs={}",
					g.exit.tracker.elapsedMs, occupied, SeatP::SitName(obs.sitState), obs.sitState, obs.controls, XP::kHardTimeoutMs);
				IssueFurnitureExit(g.exit.entryRecovery ? "stale-enter-soft-timeout" : "session-exit-soft-timeout");
				UpdateActive();
				return;
			case XP::Action::kSettled:
				if (g.exit.entryRecovery) {
					g.exit.entryRecovery = false;
					RestartSeatAfterStaleRecovery();
				} else {
					FinishSettledExit();
				}
				UpdateActive();
				return;
			case XP::Action::kHardTimeout:
				g.exit.settlementFailed = true;
				IssueFurnitureExit(g.exit.entryRecovery ? "stale-enter-hard-timeout" : "session-exit-hard-timeout");
				if (g.exit.entryRecovery) {
					g.exit.entryRecovery = false;
					SKSE::log::error("[FEASR-RF] WS STALE_STANCE recovery=FAILSAFE_STANCE_EXIT elapsedMs={} occupied={} sit={}({}); refusing fresh acquisition",
						g.exit.tracker.elapsedMs, occupied, SeatP::SitName(obs.sitState), obs.sitState);
					g.deferred.push_back({ SP::EventKind::kInterrupt, 0, 0, SP::Cause::kStanceFailed });
				} else {
					RecoverFailedExit("r3.4 hard-timeout recovery");
					SKSE::log::error("[FEASR-RF] WS STANCE EXIT verdict=FAILSAFE_STANCE_EXIT elapsedMs={} occupied={} sit={}({}) controls={:#010x}; Runeforge-owned controls released, clean IDLE withheld",
						g.exit.tracker.elapsedMs, occupied, SeatP::SitName(obs.sitState), obs.sitState, obs.controls);
				}
				UpdateActive();
				return;
			}
		}

		bool FinalBaselinePending()
		{
			return g.exit.settlementRequired && g.exit.settlementReached && !g.exit.settlementFailed && !g.exit.finalBaselineLogged;
		}

		void RememberFinalRestoreCode(std::int64_t a_code)
		{
			g.exit.finalRestoreCode = a_code == 2 ? 2 : 1;
		}

		std::int64_t FinalRestoreCode()
		{
			return g.exit.finalRestoreCode;
		}

		bool FinalGameplayBaselineReady(std::uint32_t a_controls)
		{
			if (!g.exit.settlementRequired) return true;
			if (g.exit.settlementFailed || !g.exit.settlementReached || !g.exit.preSessionControlsValid) return false;
			const auto relevant = LP::kLeasedFlags | LP::kConsole;
			const auto expected = g.exit.preSessionControls & relevant;
			const auto actual = a_controls & relevant;
			const bool ready = expected == actual;
			if (ready) {
				if (!g.exit.finalBaselineLogged) {
					g.exit.finalBaselineLogged = true;
					SKSE::log::info("[FEASR-RF] WS FINAL gameplay baseline expected={:#05x} actual={:#05x} fullControls={:#010x} verdict=VERIFIED", expected, actual, a_controls);
				}
				return true;
			}
			const auto now = NowMs();
			if (g.exit.lastPollLogMs == 0 || now - g.exit.lastPollLogMs >= 500) {
				g.exit.lastPollLogMs = now;
				SKSE::log::warn("[FEASR-RF] WS FINAL gameplay baseline expected={:#05x} actual={:#05x} missing={:#05x} fullControls={:#010x} verdict=WAIT",
					expected, actual, expected & ~actual, a_controls);
			}
			return false;
		}

		// ---------------------------------------------------------------- native sweep (T6 / T10)

		// R8.7.4.21 (A3): the facts SweepPolicy decides on. The cradle test is slab-local (LayoutPolicy::WorldToSlab / CradleContains).
		bool IsCradleAllowListed(const RE::TESObjectREFR& a_ref)
		{
			auto* data = Data();
			if (!data) return false;
			const auto id = a_ref.GetFormID();
			for (const auto& [local, plugin] : { std::pair{ kSlabId, kSites }, std::pair{ kGearAnchorId, kSites }, std::pair{ kStageHostId, kPresentation },
					 std::pair{ kSlabActivatorRefId, kPresentation }, std::pair{ kWorkpieceGlowRefId, kWorkstationR1 } }) {
				if (const auto full = data->LookupFormID(local, plugin); full != 0u && full == id) return true;
			}
			const auto* base = a_ref.GetBaseObject();
			return base && base->Is(RE::FormType::Light);  // presentation lights are allowed anywhere on the slab
		}

		SweepP::RefFacts SweepFacts(const RE::TESObjectREFR& a_ref)
		{
			SweepP::RefFacts f{};
			f.deleted = a_ref.IsDeleted();
			f.disabled = a_ref.IsDisabled();
			f.dynamic = a_ref.IsDynamicForm();
			const auto* base = a_ref.GetBaseObject();
			f.displayBase = IsDisplayBase(base);
			f.inventoryBase = base && (base->Is(RE::FormType::Weapon) || base->Is(RE::FormType::Armor) || base->Is(RE::FormType::Ammo) || base->Is(RE::FormType::Misc));
			f.activationBlocked = a_ref.IsActivationBlocked();
			if (const auto* slab = Slab()) {
				const auto p = a_ref.GetPosition();
				const auto local = LayoutPolicy::WorldToSlab({ slab->GetPositionX(), slab->GetPositionY(), slab->GetPositionZ() }, slab->GetAngleZ(), { p.x, p.y, p.z });
				f.inCradle = LayoutPolicy::CradleContains(local.x, local.y, local.z);
			}
			f.allowListed = IsCradleAllowListed(a_ref);
			return f;
		}

		// T6 / T10, hardened in R8.7.4.21 (A3 G1/G2/G3): only runtime-created references are deleted, plugin references only disabled;
		// an unledgered item clone in the cradle is found by its facts; placed dressing in the cradle is reported, never touched.
		// Every caller runs it with no session (post-load, slab cell attach, the exit watchdog), so the live preview is never a target.
		std::uint32_t NativeSweep(bool a_scanForgeCell)
		{
			std::uint32_t count = 0;
			std::uint32_t disabledOnly = 0;
			const auto apply = [&](RE::TESObjectREFR* a_ref, SweepP::Action a_action) {
				if (!a_ref || a_action == SweepP::Action::kKeep) return;
				a_ref->Disable();
				if (a_action == SweepP::Action::kDisableAndDelete) {
					a_ref->SetDelete(true);
				} else {
					++disabledOnly;
				}
				++count;
			};
			for (const auto& handle : g.spawned) {
				if (auto ref = handle.get()) apply(ref.get(), SweepP::ForTracked(SweepFacts(*ref)));
			}
			g.spawned.clear();
			ForgetClones();
			if (auto* ledger = Ledger(); ledger && ledger->scriptAddedTempForms) {
				for (const auto id : *ledger->scriptAddedTempForms) {
					if (auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(id)) apply(ref, SweepP::ForTracked(SweepFacts(*ref)));
				}
				ledger->scriptAddedTempForms->clear();
				ledger->scriptAddedFormCount = 0;
			}
			if (a_scanForgeCell) {
				if (auto* cell = ForgeCell(); cell && cell->IsAttached()) {
					std::vector<std::pair<RE::TESObjectREFR*, SweepP::Action>> stray;
					std::vector<RE::FormID> intruders;
					std::uint32_t orphanItems = 0;
					cell->ForEachReference([&](RE::TESObjectREFR& a_ref) {
						const auto facts = SweepFacts(a_ref);
						const auto action = SweepP::ForCellScan(facts, true);
						if (action != SweepP::Action::kKeep) {
							stray.emplace_back(&a_ref, action);
							if (SweepP::IsOrphanItemClone(facts)) ++orphanItems;
						} else if (SweepP::IsCradleIntruder(facts)) {
							intruders.push_back(a_ref.GetFormID());
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
					for (const auto& [ref, action] : stray) apply(ref, action);
					if (orphanItems) SKSE::log::info("[FEASR-RF] WS native sweep removed {} unledgered item clone(s) from the cradle", orphanItems);
					for (const auto id : intruders) SKSE::log::warn("[FEASR-RF] WS CRADLE_INTRUDER {:08X}: an enabled placed reference lies in the item cradle (not touched)", id);
				}
			}
			if (count) SKSE::log::info("[FEASR-RF] WS native sweep removed {} refs ({} plugin-placed refs only disabled)", count, disabledOnly);
			return count;
		}

		// ---------------------------------------------------------------- the session

		std::string ItemIdentity(std::int64_t a_index)
		{
			return a_index >= 0 && static_cast<std::size_t>(a_index) < g.items.size() ? Str(g.items[static_cast<std::size_t>(a_index)], "instanceKey") : std::string{};
		}

		std::string RowIdentity(std::int64_t a_index)
		{
			if (a_index < 0 || static_cast<std::size_t>(a_index) >= g.formulas.size()) return {};
			std::string identity = PP::WireFormulaId(Str(g.formulas[static_cast<std::size_t>(a_index)], "id")) + "|";
			if (const auto* rd = ReadinessAt(static_cast<std::size_t>(a_index))) {
				for (std::size_t i = 0; i < rd->slots.size(); ++i) identity += (i ? "," : "") + std::to_string(rd->slots[i]);
			}
			return identity;
		}

		void SendItem(std::int64_t a_seq, std::int64_t a_index)
		{
			const auto token = g.session.token;
			if (a_index < 0 || static_cast<std::size_t>(a_index) >= g.items.size()) {
				Send(PP::StageItem(token, a_seq, {}));
				return;
			}
			auto* base = RuneforgeMenu::Operations::ResolveBase(ParseKey(Str(g.items[static_cast<std::size_t>(a_index)], "instanceKey")));
			LayoutPolicy::ItemClass cls = LayoutPolicy::ItemClass::kNeutral;
			if (auto* weapon = base ? base->As<RE::TESObjectWEAP>() : nullptr) {
				cls = LayoutPolicy::ClassifyWeapon(static_cast<std::uint32_t>(weapon->GetWeaponType()));
			} else if (auto* armor = base ? base->As<RE::TESObjectARMO>() : nullptr) {
				cls = LayoutPolicy::ClassifyArmor(static_cast<std::uint32_t>(armor->GetSlotMask()));
			}
			LayoutPolicy::ItemPose pose{};
			LayoutPolicy::Bounds bounds{};
			if (base && cls != LayoutPolicy::ItemClass::kNeutral) {
				const auto& b = base->boundData;
				bounds = { b.boundMin.x, b.boundMin.y, b.boundMin.z, b.boundMax.x, b.boundMax.y, b.boundMax.z };
				pose = LayoutPolicy::Pose(cls, bounds);
			}
			if (pose.itemClass == LayoutPolicy::ItemClass::kNeutral) {
				SKSE::log::info("[FEASR-RF] WS item has no supported display pose (neutral fallback)");
			}
			Send(PP::StageItem(token, a_seq, pose), base);
			if (pose.itemClass != LayoutPolicy::ItemClass::kNeutral) {
				// R8.7.4.23: presentation-only; the host keeps the inscription clear of the displayed item
				Send(PP::ItemBounds(token, a_seq, LayoutPolicy::DisplayBounds(pose, bounds)));
			}
		}

		// The row of formula j: every slot in formula order, glyph or 0 (PRES-D31: a missing rune's slot stays empty).
		void SendRunes(std::int64_t a_seq, std::int64_t a_index)
		{
			const auto& f = g.formulas[static_cast<std::size_t>(a_index)];
			const auto* rd = ReadinessAt(static_cast<std::size_t>(a_index));
			const auto slots = rd ? rd->slots : std::vector<std::uint8_t>{};
			g.row = { a_seq, PP::WireFormulaId(Str(f, "id")), rd ? rd->ownedSlots : 0u, static_cast<std::uint32_t>(slots.size()) };
			g.rowStatus.clear();
			Send(PP::StageRunes(g.session.token, a_seq, Str(f, "id"), slots));
		}

		void StageRequest(GP::Kind a_kind, std::int64_t a_seq, std::int64_t a_index)
		{
			const bool runes = a_kind == GP::Kind::kRunes;
			const auto token = g.session.token;
			if (runes) {
				const auto* rd = a_index >= 0 ? ReadinessAt(static_cast<std::size_t>(a_index)) : nullptr;
				if (!rd || rd->slots.empty()) {
					// nothing can be laid out (no formula, or its glyphs are unknown): the row is answered BAD here, the old row cleared
					SKSE::log::warn("[FEASR-RF] WS STAGE runes seq={} index={} has no slot to lay out; row answered INCOMPLETE without a request", a_seq, a_index);
					GP::Cleared(g.stage, GP::Kind::kRunes);
					Send(PP::TokenOnly(PP::kClearRunes, token));
					g.row = { a_seq, {}, 0u, 0u };
					g.rowStatus = "Rune row: this formula cannot be laid out (unknown glyphs).";
					g.deferred.push_back({ SP::EventKind::kAckRunes, token, a_seq, {}, 0 });
					return;
				}
			}
			GP::Request request{ false, a_seq, a_index, runes ? RowIdentity(a_index) : ItemIdentity(a_index) };
			const auto identity = request.identity;
			const auto decision = GP::RequestStage(g.stage, a_kind, std::move(request), NowMs());
			SKSE::log::info("[FEASR-RF] WS STAGE {} seq={} index={} id='{}' -> {}", runes ? "runes" : "item", a_seq, a_index, identity, GP::Name(decision));
			switch (decision) {
			case GP::Decision::kSend:
				if (runes) SendRunes(a_seq, a_index);
				else SendItem(a_seq, a_index);
				break;
			case GP::Decision::kReuse:
				// the same physical content, already positively acknowledged: answer this seq from that acknowledgement
				if (runes) {
					const auto* rd = ReadinessAt(static_cast<std::size_t>(a_index));
					g.row = { a_seq, PP::WireFormulaId(Str(g.formulas[static_cast<std::size_t>(a_index)], "id")), rd ? rd->ownedSlots : 0u,
						rd ? static_cast<std::uint32_t>(rd->slots.size()) : 0u };
				}
				g.deferred.push_back({ runes ? SP::EventKind::kAckRunes : SP::EventKind::kAckItem, token, a_seq, {}, 1 });
				break;
			case GP::Decision::kQueue:
				break;
			}
		}

		void PollStage()
		{
			auto release = GP::Poll(g.stage, NowMs());
			if (!release) return;
			const bool runes = release->kind == GP::Kind::kRunes;
			auto index = release->request.index;
			const auto& identity = release->request.identity;
			const auto current = [&](std::int64_t i) { return runes ? RowIdentity(i) : ItemIdentity(i); };
			if (current(index) != identity) {
				index = -1;
				const auto size = static_cast<std::int64_t>(runes ? g.formulas.size() : g.items.size());
				for (std::int64_t i = 0; i < size; ++i) {
					if (current(i) == identity) {
						index = i;
						break;
					}
				}
			}
			if (index < 0) {
				SKSE::log::info("[FEASR-RF] WS STAGE {} seq={} id='{}' left the list while waiting; dropped (the row times out visibly)", runes ? "runes" : "item",
					release->request.seq, identity);
				return;
			}
			SKSE::log::info("[FEASR-RF] WS STAGE {} seq={} index={} released after the flight", runes ? "runes" : "item", release->request.seq, index);
			StageRequest(release->kind, release->request.seq, index);
		}

		void Commit()
		{
			RefreshSnapshot();
			const auto& p = Panel();
			const auto key = ParseKey(g.selectedKey);
			const auto recipe = Str(p, "recipeId");
			const auto inserted = p.contains("insertedRunes") && p["insertedRunes"].is_number() ? p["insertedRunes"].get<std::uint32_t>() : 0u;
			const auto outcome = RuneforgeMenu::Operations::Transmute(key, recipe, inserted);
			g.result = {};
			g.resultCard = json::object();
			if (outcome.ok) {
				for (const auto& formula : g.formulas) {
					if (Str(formula, "id") == recipe) {
						g.result.recipeName = Str(formula, "name");
						g.result.effect = Str(formula, "effect");
						g.result.runePattern = Str(formula, "runeSequence");
						// R8.7.4.21 I3: the effect card captured before RefreshSnapshot (presentation only)
						g.resultCard = FormulaCard(formula);
						const auto cardLines = CardLines(g.resultCard);
						g.result.effectLines = EC::PanelBody(EC::Panel::kResult, cardLines, CardCompact(g.resultCard), true);
						break;
					}
				}
				for (const auto& item : g.items) {
					if (Str(item, "instanceKey") == g.selectedKey) {
						g.result.itemName = Str(item, "displayName", Str(item, "baseName", ""));
						break;
					}
				}
			}
			g.status = outcome.ok ? std::string{} : outcome.message;
			RefreshSnapshot();
			SP::Event result{ SP::EventKind::kCommitResult, outcome.ok ? 1 : 0, 0, {} };
			Dispatch(result);
		}

		void Apply(const SP::Effect& a_effect)
		{
			auto* ui = RE::UI::GetSingleton();
			auto* queue = RE::UIMessageQueue::GetSingleton();
			switch (a_effect.kind) {
			case SP::EffectKind::kSnapshot:
				CapturePreSessionControls();
				SnapshotCamera();
				if (auto* player = RE::PlayerCharacter::GetSingleton()) g.race = player->GetRace();
				break;
			case SP::EffectKind::kLeaseAcquire: AcquireLease(a_effect.token); break;
			case SP::EffectKind::kLeaseRelease:
				if (!QueueLeaseRelease(a_effect.token)) ReleaseLease(a_effect.token, "session exit");
				break;
			case SP::EffectKind::kMenuOpen:
				g.closeRequested = false;
				if (queue) queue->AddMessage(RE::BSFixedString(kMenuName), RE::UI_MESSAGE_TYPE::kShow, nullptr);
				break;
			case SP::EffectKind::kMenuClose:
				g.closeRequested = true;
				if (queue && ui && ui->IsMenuOpen(kMenuName)) queue->AddMessage(RE::BSFixedString(kMenuName), RE::UI_MESSAGE_TYPE::kHide, nullptr);
				break;
			case SP::EffectKind::kHudHide: SetHudHidden(true); break;
			case SP::EffectKind::kHudRestore:
				if (!QueueHudRestore()) SetHudHidden(false);
				break;
			case SP::EffectKind::kStanceEnter: EnterStance(); break;
			case SP::EffectKind::kStanceExit:
				if (!BeginExitSettlement()) ExitStance();
				break;
			case SP::EffectKind::kShot: MoveToShot(a_effect.shot); break;
			case SP::EffectKind::kRestoreEase:
			case SP::EffectKind::kRestoreNow:
				if (!QueueCameraRestore()) RestoreCamera();
				break;
			case SP::EffectKind::kBegin:
				ReleaseSpatialSounds();
				Send(PP::TokenOnly(PP::kBegin, a_effect.token), Slab());
				PlayWakeCue();
				break;
			// run 1/2: one staging in flight, the latest request wins, acknowledged content is reused; clears go out at once
			case SP::EffectKind::kStageItem: StageRequest(GP::Kind::kItem, a_effect.seq, a_effect.a); break;
			case SP::EffectKind::kClearItem:
				GP::Cleared(g.stage, GP::Kind::kItem);
				Send(PP::TokenOnly(PP::kClearItem, a_effect.token));
				break;
			case SP::EffectKind::kStageRunes: StageRequest(GP::Kind::kRunes, a_effect.seq, a_effect.a); break;
			case SP::EffectKind::kClearRunes:
				GP::Cleared(g.stage, GP::Kind::kRunes);
				g.rowStatus.clear();
				Send(PP::TokenOnly(PP::kClearRunes, a_effect.token));
				break;
			case SP::EffectKind::kItemTimeout:
				SKSE::log::warn("[FEASR-RF] WS ITEM_ACK token={} seq={} verdict=TIMEOUT (no acknowledgement)", a_effect.token, a_effect.seq);
				g.itemStatus = "The item could not be laid on the slab (no answer from the staging host).";
				break;
			case SP::EffectKind::kRowTimeout:
				SKSE::log::warn("[FEASR-RF] WS RUNES_ACK token={} seq={} formula={} verdict=TIMEOUT (no acknowledgement)", a_effect.token, a_effect.seq, g.row.formula);
				g.rowStatus = "Rune row: INCOMPLETE (no answer from the slab). Go Back and select the formula again.";
				break;
			case SP::EffectKind::kBeatStrike:
				Send(PP::BeatStrike(a_effect.token, a_effect.a));
				GP::Forget(g.stage, GP::Kind::kRunes);  // the performance consumes the staged row: never reuse its acknowledgement
				PlayStrikeCue();
				break;
			case SP::EffectKind::kBeatBind:
				Send(PP::BeatBind(a_effect.token));
				PlayBindCue();
				break;
			case SP::EffectKind::kBeatSkip:
				Send(PP::BeatSkip(a_effect.token));
				GP::Cleared(g.stage, GP::Kind::kRunes);  // the host cleans the rune row on SKIP
				PlayBindCue();
				break;
			case SP::EffectKind::kEnd:
				Send(PP::End(a_effect.token, a_effect.reason, a_effect.cause));
				ReleaseSpatialSounds();
				break;
			case SP::EffectKind::kReset:
				RestoreCheckedOutEquipment();
				ReleaseSpatialSounds();
				Send(PP::Reset());
				g.stage = {};
				ReleaseLease(0, "post-load reset");
				break;
			case SP::EffectKind::kCommit: Commit(); break;
			case SP::EffectKind::kDecline: PlayDeclineCue(); break;
			case SP::EffectKind::kSweep:
				RestoreCheckedOutEquipment();
				ReleaseSpatialSounds();
				NativeSweep(true);
				ReleaseLease(0, "watchdog sweep");
				break;
			case SP::EffectKind::kRefuse:
				if (a_effect.refusal == SP::Refusal::kRowIncomplete) {
					g.status = g.session.row == SP::Ack::kBad && !g.rowStatus.empty() ? g.rowStatus :
						"The rune row is not complete on the slab. Go Back and select the formula again.";
				}
				if (a_effect.refusal != SP::Refusal::kAlreadyOpen) PlayDeclineCue();
				break;
			case SP::EffectKind::kOp:
			case SP::EffectKind::kFlat:
				break;
			}
		}

		void Dispatch(const SP::Event& a_event)
		{
			const auto before = g.session.state;
			const auto session = g.session;
			SP::Effects fx;
			// R8.7.4.21: runtime timing v3 with LANDED / SEAL gating (bounded by beatAckTimeoutMs); the vectors pin the v2 defaults
			SP::Step(g.session, a_event, fx, SP::kPerformanceV3);
			for (const auto& effect : fx) {
				SKSE::log::info("[FEASR-RF] WS {} -> {}", SP::Name(g.session.state), SP::Format(effect));
				Apply(effect);
			}
			const auto after = g.session.state;
			if (after != SP::State::kIdle || before != SP::State::kIdle) {
				ActivationIntentPolicy::Clear(g_activationIntent);
			}
			UpdateActive();
			if (before != after) {
				// Inform the staging/presentation host after the authoritative transition. This is fire-and-forget and
				// deliberately carries no acknowledgement or gameplay decision back into the session policy.
				const auto visualToken = g.session.token != 0 ? g.session.token : session.token;
				if (visualToken != 0) Send(PP::VisualState(visualToken, SP::Name(after)), Slab());
				if (before == SP::State::kEntering && after == SP::State::kExiting && g.session.reason == SP::ExitReason::kAbort) {
					SKSE::log::warn("[FEASR-RF] WS state ENTERING -> ABORT_{} -> EXITING (seated={} leased={} ready={} acq={} elapsedMs={} attempts={} places={})",
						SP::Name(g.session.cause), session.seated, session.leased, session.ready, SP::Name(session.acq), NowMs() - g.seatEnterMs, g.seat.attempts,
						g.seat.placeAttempts);
				}
				SKSE::log::info("[FEASR-RF] WS state {} -> {}", SP::Name(before), SP::Name(after));
				g.scourArmed = false;
				if (after == SP::State::kSelectItem && before != SP::State::kEntering) {
					RestoreCheckedOutEquipment();
				}
				if (after == SP::State::kExiting) {
					RestoreCheckedOutEquipment();
					SKSE::log::info("[FEASR-RF] WS session totals STAGE sent={} reused={} replacedWhileWaiting={} timedOut={}; RENDER pushes={} suppressedDuplicates={}; "
									"PAN drags={} stanceForcedThirdPerson={}",
						g.stage.sent, g.stage.reused, g.stage.replaced, g.stage.timedOut, g.render.pushed, g.render.suppressed, g.panDrags, g.seatForcedThirdPerson);
				}
				if (after == SP::State::kSelectItem && before == SP::State::kEntering && !g.items.empty()) {
					Dispatch({ SP::EventKind::kFocusItem, static_cast<std::int64_t>(g.itemFocus), 0, {} });
				} else if (after == SP::State::kSelectFormula && (before == SP::State::kSelectItem || before == SP::State::kOperations) && !g.formulas.empty()) {
					Dispatch({ SP::EventKind::kFocusFormula, static_cast<std::int64_t>(g.formulaFocus),
						static_cast<std::int64_t>(GlyphCount(g.formulas[g.formulaFocus])), {} });
				} else if (after == SP::State::kIdle) {
					g.spawned.clear();
					ForgetClones();
					g.status.clear();
					g.seatWatch = false;
					g.stage = {};
				}
			}
			PushRender();
			// events produced while the effects ran (lease result, reused acknowledgements) run after this transition
			while (!g.deferred.empty()) {
				const auto next = g.deferred.front();
				g.deferred.erase(g.deferred.begin());
				Dispatch(next);
			}
		}

		void Interrupt(SP::Cause a_cause)
		{
			if (g.session.state == SP::State::kIdle || g.session.state == SP::State::kExiting) return;
			SKSE::log::info("[FEASR-RF] WS interrupt {}", SP::Name(a_cause));
			Dispatch({ SP::EventKind::kInterrupt, 0, 0, a_cause });
		}

		// ---------------------------------------------------------------- commands (input policy / AS2 clicks)

		void Focus(std::size_t a_index, bool a_playSound = true)
		{
			switch (g.session.state) {
			case SP::State::kSelectItem:
				if (g.items.empty()) return;
				g.itemFocus = std::min(a_index, g.items.size() - 1);
				Dispatch({ SP::EventKind::kFocusItem, static_cast<std::int64_t>(g.itemFocus), 0, {} });
				break;
			case SP::State::kSelectFormula:
				if (g.formulas.empty()) return;
				g.formulaFocus = std::min(a_index, g.formulas.size() - 1);
				Dispatch({ SP::EventKind::kFocusFormula, static_cast<std::int64_t>(g.formulaFocus),
					static_cast<std::int64_t>(GlyphCount(g.formulas[g.formulaFocus])), {} });
				break;
			case SP::State::kOperations:
				if (g.ops.empty()) return;
				g.opsFocus = std::min(a_index, g.ops.size() - 1);
				g.scourArmed = false;
				RebuildOps();
				PushRender();
				break;
			default:
				return;
			}
			if (a_playSound) PlaySound(kSoundFocus);
		}

		std::size_t ListSize()
		{
			switch (g.session.state) {
			case SP::State::kSelectItem: return g.items.size();
			case SP::State::kSelectFormula: return g.formulas.size();
			case SP::State::kOperations: return g.ops.size();
			default: return 0;
			}
		}

		std::size_t CurrentFocus()
		{
			switch (g.session.state) {
			case SP::State::kSelectItem: return g.itemFocus;
			case SP::State::kSelectFormula: return g.formulaFocus;
			case SP::State::kOperations: return g.opsFocus;
			default: return 0;
			}
		}

		void Navigate(long long a_delta, bool a_wrap = false, bool a_playSound = true)
		{
			const auto size = ListSize();
			if (size == 0) return;
			const auto current = static_cast<long long>(CurrentFocus());
			long long next = current + a_delta;
			if (a_wrap && size > 1) {
				const auto span = static_cast<long long>(size);
				next = ((next % span) + span) % span;
			} else {
				next = std::clamp(next, 0LL, static_cast<long long>(size) - 1);
			}
			if (next != current) Focus(static_cast<std::size_t>(next), a_playSound);
		}

		void FocusFirstReadyFormula()
		{
			if (g.formulas.empty() || g.readiness.size() != g.formulas.size()) {
				g.formulaFocus = 0;
				return;
			}
			const auto it = std::find_if(g.readiness.begin(), g.readiness.end(), [](const RP::Readiness& a_readiness) {
				return a_readiness.state == RP::State::kReady;
			});
			g.formulaFocus = it == g.readiness.end() ? 0 : static_cast<std::size_t>(std::distance(g.readiness.begin(), it));
		}

		void CycleFilter(int a_delta)
		{
			if (g.session.state == SP::State::kSelectItem) {
				g.itemFilter = (g.itemFilter + 4 + a_delta) % 4;
				g.itemFocus = 0;
				RefreshSnapshot();
				if (!g.items.empty()) Focus(0);
			} else if (g.session.state == SP::State::kSelectFormula) {
				g.formulaFilter = (g.formulaFilter + 4 + a_delta) % 4;
				g.formulaFocus = 0;
				RefreshSnapshot();
				if (!g.formulas.empty()) Focus(0);
			}
			PushRender();
		}

		void RunOperation()
		{
			if (g.ops.empty()) return;
			const auto op = g.ops[std::min(g.opsFocus, g.ops.size() - 1)];
			if (!op.enabled) {
				g.status = "Not available for the selected item.";
				Dispatch({ SP::EventKind::kAccept, 0, 0, {} });
				return;
			}
			const auto& p = Panel();
			const auto key = ParseKey(g.selectedKey);
			const auto signature = Str(p, "affixSignature", "-");
			const auto affixCount = p.contains("regularAffixCount") && p["regularAffixCount"].is_number() ? p["regularAffixCount"].get<std::uint32_t>() : 0u;
			using RuneforgeMenu::Operations::Craft;
			RuneforgeMenu::Operations::Outcome outcome{};
			if (op.kind == "scour" && !g.scourArmed) {
				g.scourArmed = true;
				g.status = "Scouring replaces every regular affix. Activate again to confirm.";
				RebuildOps();
				PushRender();
				return;
			}
			if (op.kind == "identify") outcome = RuneforgeMenu::Operations::CraftAffixes(Craft::kIdentify, key, 0, signature, affixCount);
			else if (op.kind == "reforge") outcome = RuneforgeMenu::Operations::CraftAffixes(Craft::kReforge, key, op.token, signature, affixCount);
			else if (op.kind == "scour") outcome = RuneforgeMenu::Operations::CraftAffixes(Craft::kScour, key, 0, signature, affixCount);
			else if (op.kind == "expand") outcome = RuneforgeMenu::Operations::ExpandAffix(key, affixCount, signature);
			else if (op.kind == "exchangeReforge") outcome = RuneforgeMenu::Operations::ExchangeCurrency(false);
			else if (op.kind == "exchangeScour") outcome = RuneforgeMenu::Operations::ExchangeCurrency(true);
			if (outcome.ok && (op.kind == "identify" || op.kind == "reforge" || op.kind == "expand" || op.kind == "scour")) {
				const char* cue = op.kind == "identify" ? "IDENTIFY" : op.kind == "reforge" ? "REFORGE" : op.kind == "expand" ? "EXPAND" : "SCOUR";
				Send(PP::OperationCue(g.session.token, cue), Slab());
			}
			g.scourArmed = false;
			g.status = outcome.message;
			RefreshSnapshot();
			Dispatch({ SP::EventKind::kAccept, outcome.ok ? 1 : 0, 0, {} });
		}

		void Accept()
		{
			switch (g.session.state) {
			case SP::State::kSelectItem: {
				if (g.items.empty()) {
					Dispatch({ SP::EventKind::kAccept, 0, 0, {} });
					return;
				}
				const auto key = Str(g.items[g.itemFocus], "instanceKey");
				const auto outcome = RuneforgeMenu::Operations::SelectBase(ParseKey(key));
				g.status = outcome.ok ? std::string{} : (outcome.message.empty() ? "Base selection failed." : outcome.message);
				g.verdicts.clear();  // the item/recipe preconditions belong to the selected item
				if (outcome.ok) CheckoutEquippedItemForPresentation(ParseKey(key));
				RefreshSnapshot();
				if (outcome.ok) FocusFirstReadyFormula();
				Dispatch({ SP::EventKind::kAccept, outcome.ok ? 1 : 0, 0, {} });
				break;
			}
			case SP::State::kSelectFormula: {
				if (g.formulas.empty()) {
					Dispatch({ SP::EventKind::kAccept, 0, 0, {} });
					return;
				}
				// Copy: RefreshSnapshot() below rebuilds g.formulas, so no reference into it may survive the call.
				const json f = g.formulas[g.formulaFocus];
				const auto recipe = Str(f, "id");
				const auto requestedItem = g.selectedKey;
				// PRES-D31: a learned formula without its rune pieces stays browseable and is refused here with the exact counts
				const auto* readiness = ReadinessAt(g.formulaFocus);
				if (!readiness || !RP::CanAttemptExplicitSelection(*readiness)) {
					g.status = readiness ? ReadinessMessage(f, *readiness) : "Not ready.";
					SKSE::log::info("[FEASR-RF] WS READINESS formula={} state={} detail='{}'", recipe, readiness ? RP::Name(readiness->state) : "?", g.status);
					Dispatch({ SP::EventKind::kAccept, 0, 0, {} });
					return;
				}
				const auto outcome = RuneforgeMenu::Operations::SelectRecipe(ParseKey(requestedItem), recipe);
				RefreshSnapshot();
				// R8.7.4.6: read the refreshed snapshot through the shared producer/consumer contract. The selected
				// instance key is a TOP-LEVEL snapshot field; R8.7.4.2 looked it up inside `panel`, got "", and refused
				// every legitimate selection. Exact item, exact recipe, listed rows, canInsert and the authoritative
				// SelectRecipe result all still have to agree before CONFIRM.
				const auto view = SC::ReadSelection(g.snapshot);
				const auto focusedRecipe = g.formulaFocus < g.formulas.size() ? Str(g.formulas[g.formulaFocus], "id") : std::string{};
				const auto decision = SC::DecideFormulaSelection(requestedItem, g.selectedKey, recipe, focusedRecipe,
					outcome.ok, outcome.message, view);
				if (decision.stickyVerdict) {
					g.verdicts[recipe] = { decision.verdict, decision.message };
				} else {
					g.verdicts.erase(recipe);  // identity drift is not a verdict about this pair; the player may select again
				}
				g.status = decision.message;
				// Recompute after the verdict is stored and before Accept can enter CONFIRM.
				RecomputeReadiness();
				const auto stateAfter = g.readiness.size() > g.formulaFocus ? g.readiness[g.formulaFocus].state : RP::State::kRefused;
				const bool ready = decision.ready && stateAfter == RP::State::kReady;
				SKSE::log::info("[FEASR-RF] WS READINESS formula={} state={} outcome={} identityMatched={} ready={} requestedItem={} snapshotItem={} panelRecipe={} focused={} listed={}/{} rows={} canInsert={} opOk={} detail='{}'",
					recipe, RP::Name(stateAfter), SC::Name(decision.outcome), decision.identityMatched, ready, requestedItem,
					view.selectedInstanceKey, view.panelRecipeId, focusedRecipe, view.selectedItemListed, view.panelRecipeListed,
					view.selectedItemRows, view.canInsert, outcome.ok, g.status);
				Dispatch({ SP::EventKind::kAccept, ready ? 1 : 0, 0, {} });
				break;
			}
			case SP::State::kOperations: RunOperation(); break;
			default: Dispatch({ SP::EventKind::kAccept, 1, 0, {} }); break;
			}
		}

		void Command(IP::Command a_command, SP::Cause a_emergencyCause = SP::Cause::kEmergency)
		{
			switch (a_command) {
			case IP::Command::kNavUp: Navigate(-1, g.session.state == SP::State::kSelectFormula); break;
			case IP::Command::kNavDown: Navigate(1, g.session.state == SP::State::kSelectFormula); break;
			case IP::Command::kPageUp: Navigate(-static_cast<long long>(kPageRows)); break;
			case IP::Command::kPageDown: Navigate(static_cast<long long>(kPageRows)); break;
			case IP::Command::kFilterPrev: CycleFilter(-1); break;
			case IP::Command::kFilterNext: CycleFilter(1); break;
			case IP::Command::kAccept: Accept(); break;
			case IP::Command::kDetail:
				// R8.7.4.21 I3: only where Details has a visible effect, and never while the engrave hold is filling
				if (PRP::DetailToggleAllowed(g.session.state, g.session.hold >= 0)) {
					g.detail = !g.detail;
					PushRender();
				}
				break;
			case IP::Command::kEmergency: Interrupt(a_emergencyCause); break;
			case IP::Command::kNone: break;
			default: {
				SP::Event event{};
				if (IP::ToStateEvent(a_command, event)) Dispatch(event);
				break;
			}
			}
		}

		// ---------------------------------------------------------------- clock (menu frames; worker after the menu closes)

		void Tick(float a_seconds)
		{
			std::scoped_lock lock(g.mutex);
			// PRES-D34 / RC1-WS r3: settle furniture first, then verify deferred control restoration.
			g.frame += 1;
			PollExitSettlement(std::max<std::int64_t>(1, static_cast<std::int64_t>(a_seconds * 1000.0f)));
			VerifyControls();
			if (g.session.state == SP::State::kIdle) return;
			auto* player = RE::PlayerCharacter::GetSingleton();
			// interrupts polled per frame (T8 cell change, combat, death, race change)
			if (player && g.session.state != SP::State::kExiting) {
				if (player->IsDead()) return Interrupt(SP::Cause::kDeath);
				if (player->IsInCombat()) return Interrupt(SP::Cause::kCombat);
				if (auto* cell = ForgeCell(); cell && player->GetParentCell() != cell) return Interrupt(SP::Cause::kCell);
				if (g.race && player->GetRace() != g.race) return Interrupt(SP::Cause::kRace);
			}
			// PC1.1: bounded backstop for a committed session yielded to foreign UI (an overlay that never closes, a close never seen)
			if (g.session.state != SP::State::kExiting && g.session.commits > 0 && g_uiYield.load(std::memory_order_acquire) &&
				!g_uiTeardownQueued.load(std::memory_order_acquire)) {
				PruneClosedForeignMenus();
				if (!GamePaused()) g.uiYieldUnpausedMs += std::max<std::int64_t>(1, static_cast<std::int64_t>(a_seconds * 1000.0f));
				const auto yield = UIO::ResolveCommittedYield(!g.uiForeignMenus.empty(), g.uiYieldUnpausedMs);
				if (yield == UIO::YieldAction::kResume) {
					g_uiYield.store(false, std::memory_order_release);
					g.uiYieldUnpausedMs = 0;
					SKSE::log::info("[FEASR-RF] WS UI no tracked foreign menu is open: committed workstation input resumed (backstop)");
				} else if (yield == UIO::YieldAction::kEndSession) {
					SKSE::log::warn("[FEASR-RF] WS UI committed session yielded to menu='{}' for {} ms of unpaused play (cap {}): ending the session; the committed result is kept",
						g.uiForeignMenus.front(), g.uiYieldUnpausedMs, UIO::kYieldUnpausedCapMs);
					g.uiYieldUnpausedMs = 0;
					return Interrupt(SP::Cause::kForced);
				}
			} else if (!g_uiYield.load(std::memory_order_acquire)) {
				g.uiYieldUnpausedMs = 0;
			}
			// PRES-D27: the seat is acquired, then watched for the whole session (a loss is ABORT_STANCE_LOST)
			if (g.session.state != SP::State::kExiting) PollSeat();
			if (g.session.state == SP::State::kIdle) return;
			// held left stick repeats like a held D-pad
			if (g.stickDirection != IP::Button::kOther) {
				g.stickHeld += a_seconds;
				if (IP::RepeatsDue(g.stickHeld) > g.stickRepeats) {
					g.stickRepeats += 1;
					Command(IP::NavigationCommand(g.session.state, g.stickDirection));
				}
			}
			UpdateCamera(a_seconds);
			ApplyFurnitureView(a_seconds);
			PollStage();
			g.msRemainder += static_cast<double>(a_seconds) * 1000.0;
			const auto ms = static_cast<std::int64_t>(g.msRemainder);
			if (ms > 0) {
				g.msRemainder -= static_cast<double>(ms);
				const bool holding = g.session.hold >= 0;
				const auto state = g.session.state;
				Dispatch({ SP::EventKind::kTick, ms, 0, {} });
				if (holding && g.session.state == state) PushRender();
			}
		}

		// ---------------------------------------------------------------- input (raw sink while the workstation is up)

		bool RightButton(bool a_down, bool a_up)
		{
			const auto route = VP::RouteRightButton(a_down, a_up, g.pointerInPanel, g.panning);
			// PRES-D35: no pan during the engraving/bind beat (S_STRIKE resets it); the button stays consumed
			if (route == VP::RightButton::kPanBegin && g.session.state == SP::State::kPerforming) {
				SKSE::log::info("[FEASR-RF] WS PAN held during the bind beat (authored camera)");
				return true;
			}
			if (route == VP::RightButton::kPanBegin) {
				g.panning = true;
				g.panDrags += 1;
				SKSE::log::info("[FEASR-RF] WS PAN begin backend={} yaw={:.1f} pitch={:.1f} state={}", g.backend == Backend::kFree ? "free" : "furniture",
					g.pan.yawDegrees, g.pan.pitchDegrees, SP::Name(g.session.state));
			} else if (route == VP::RightButton::kPanEnd) {
				g.panning = false;
				SKSE::log::info("[FEASR-RF] WS PAN end yaw={:.1f} pitch={:.1f} (kept until the next shot)", g.pan.yawDegrees, g.pan.pitchDegrees);
			} else if (a_down) {
				SKSE::log::info("[FEASR-RF] WS PAN right button inside the GUI box: ignored (no Back)");
			}
			return true;  // the right button never exits; the lease keeps it from blocking or attacking
		}


		bool MayConsumeInput()
		{
			return UIO::MayConsumeWorkstationInput(
				g_active.load(std::memory_order_acquire),
				false,
				g_uiYield.load(std::memory_order_acquire));
		}

		class MenuOwnershipSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event) return RE::BSEventNotifyControl::kContinue;
				g_activationMenuEpoch.fetch_add(1, std::memory_order_relaxed);
				const char* rawName = a_event->menuName.c_str();
				const std::string_view menuName = rawName ? std::string_view(rawName) : std::string_view{};
				const auto menuClass = UIO::Classify(menuName, kMenuName);
				SP::State state = SP::State::kIdle;
				bool committed = false;
				bool active = false;
				bool resumeAfterForeignClose = false;
				{
					std::scoped_lock lock(g.mutex);
					state = g.session.state;
					const bool sessionPresent = state != SP::State::kIdle;
					active = sessionPresent && state != SP::State::kExiting;
					committed = g.session.commits > 0;
					if (menuClass == UIO::MenuClass::kForeign && sessionPresent) {
						if (a_event->opening) {
							if (std::find(g.uiForeignMenus.begin(), g.uiForeignMenus.end(), std::string(menuName)) == g.uiForeignMenus.end()) {
								g.uiForeignMenus.emplace_back(menuName);
							}
						} else {
							g.uiForeignMenus.erase(std::remove(g.uiForeignMenus.begin(), g.uiForeignMenus.end(), std::string(menuName)), g.uiForeignMenus.end());
							resumeAfterForeignClose = committed && g.uiForeignMenus.empty() &&
								state != SP::State::kIdle && state != SP::State::kExiting &&
								!g_uiTeardownQueued.load(std::memory_order_acquire);
						}
					}
				}
				if (resumeAfterForeignClose) {
					g_uiYield.store(false, std::memory_order_release);
					SKSE::log::info("[FEASR-RF] WS UI foreign-close menu='{}' state={} committed=true; all foreign UI closed, workstation input resumed",
						menuName, SP::Name(state));
				}
				const auto decision = UIO::ResolveOpen(menuName, kMenuName, a_event->opening, active,
					committed ? UIO::CommitState::kPostCommit : UIO::CommitState::kPreCommit);
				if (!decision.yieldInput) return RE::BSEventNotifyControl::kContinue;
				g_uiYield.store(true, std::memory_order_release);
				SKSE::log::warn("[FEASR-RF] WS UI foreign-open menu='{}' state={} committed={} class={} action={} preserveCommittedResult={}; workstation input yielded immediately",
					menuName, SP::Name(state), committed, UIO::Name(decision.menuClass), UIO::Name(decision.action), decision.preserveCommittedResult);
				if (!decision.endWorkstation) return RE::BSEventNotifyControl::kContinue;
				bool expected = false;
				if (!g_uiTeardownQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return RE::BSEventNotifyControl::kContinue;
				if (auto* tasks = SKSE::GetTaskInterface()) {
					tasks->AddTask([menu = std::string(menuName), preserve = decision.preserveCommittedResult]() {
						g_uiTeardownQueued.store(false, std::memory_order_release);
						std::scoped_lock lock(g.mutex);
						if (g.session.state == SP::State::kIdle || g.session.state == SP::State::kExiting) return;
						SKSE::log::warn("[FEASR-RF] WS UI arbitration teardown menu='{}' state={} preserveCommittedResult={}; presentation-only interrupt",
							menu, SP::Name(g.session.state), preserve);
						Interrupt(SP::Cause::kForced);
					});
				} else {
					SKSE::log::critical("[FEASR-RF] WS UI arbitration FAIL: task interface unavailable for foreign menu '{}'; input remains yielded", menuName);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		MenuOwnershipSink g_menuOwnershipSink;

		IP::Button ResolveWorkstationFallbackButton(const RE::ButtonEvent& a_button, RE::INPUT_DEVICE a_device)
		{
			// Custom-menu ButtonEvents do not always carry QUserEvent on every phase.
			// Resolve the physical ID back through the live MenuMode ControlMap so a
			// remapped Accept/Activate still produces DOWN/HELD/UP for the hold gate.
			if (auto* controls = RE::ControlMap::GetSingleton()) {
				using Context = RE::UserEvents::INPUT_CONTEXT_ID;
				const auto id = a_button.GetIDCode();
				const auto matches = [&](const char* a_userEvent) {
					const auto mapped = controls->GetMappedKey(a_userEvent, a_device, Context::kMenuMode);
					return mapped != 0xFFu && mapped == id;
				};
				if (matches("Accept") || matches("Activate")) return IP::Button::kAccept;
				if (matches("Up")) return IP::Button::kUp;
				if (matches("Down")) return IP::Button::kDown;
				if (matches("Left")) return IP::Button::kLeft;
				if (matches("Right")) return IP::Button::kRight;
				if (matches("PrevPage") || matches("LeftShoulder")) return IP::Button::kPageUp;
				if (matches("NextPage") || matches("RightShoulder")) return IP::Button::kPageDown;
				if (matches("XButton")) return IP::Button::kOps;
				if (matches("YButton")) return IP::Button::kDetail;
			}

			// Last-resort defaults keep the shipped keyboard/controller interaction
			// usable if MenuMode has no semantic mapping entry at all. Fixed Back
			// controls are resolved earlier by ButtonForInputEvent and still win.
			if (a_device == RE::INPUT_DEVICE::kKeyboard && a_button.GetIDCode() == RE::BSKeyboardDevice::Keys::kE) {
				return IP::Button::kAccept;
			}
			if (a_device == RE::INPUT_DEVICE::kGamepad && a_button.GetIDCode() == RE::BSWin32GamepadDevice::Keys::kA) {
				return IP::Button::kAccept;
			}
			return IP::Button::kOther;
		}

		class InputSink final : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!a_event || !MayConsumeInput()) return RE::BSEventNotifyControl::kContinue;
				std::scoped_lock lock(g.mutex);
				if (g.session.state == SP::State::kIdle || g.session.state == SP::State::kExiting) return RE::BSEventNotifyControl::kContinue;
				bool consumed = false;
				for (auto* current = *a_event; current; current = current->next) {
					const auto device = current->GetDevice();
					if (current->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
						const auto* stick = static_cast<const RE::ThumbstickEvent*>(current);
						g.device = PRP::NextDevice(g.device, PRP::InputKind::kGamepadStick, std::max(std::fabs(stick->xValue), std::fabs(stick->yValue)));
						g.promptDevice = PRP::NextPromptDevice(g.promptDevice, g.device);
						if (stick->IsRight()) {
							g.rightX = stick->xValue;
							g.rightY = stick->yValue;
						} else if (stick->IsLeft()) {
							const auto dir = IP::StickDirection(stick->xValue, stick->yValue);
							if (dir != g.stickDirection) {
								g.stickDirection = dir;
								g.stickHeld = 0.0f;
								g.stickRepeats = 0;
								if (dir != IP::Button::kOther) Command(IP::NavigationCommand(g.session.state, dir));
							}
						}
						consumed = true;
						continue;
					}
					if (current->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove) {
						const auto* move = static_cast<const RE::MouseMoveEvent*>(current);
						// R8.7.4.18: motion alone never changes the device (the key caps stay truthful)
						g.device = PRP::NextDevice(g.device, PRP::InputKind::kMouseMove);
						if (g.panning) g.pan = VP::AccumulatePan(g.pan, move->mouseInputX, move->mouseInputY);
						continue;
					}
					const auto* button = current->AsButtonEvent();
					if (!button) continue;
					if (device == RE::INPUT_DEVICE::kMouse) {
						g.device = PRP::NextDevice(g.device, PRP::InputKind::kMouseButton);
						// left clicks belong to the movie. The right button pans the camera outside the GUI box and does nothing
						// inside it; it never exits (Back is Esc, Tab or gamepad B).
						if (button->GetIDCode() == 1) consumed = RightButton(button->IsDown(), button->IsUp()) || consumed;
						continue;
					}
					if (device != RE::INPUT_DEVICE::kKeyboard && device != RE::INPUT_DEVICE::kGamepad) continue;
					g.device = PRP::NextDevice(g.device, device == RE::INPUT_DEVICE::kGamepad ? PRP::InputKind::kGamepadButton : PRP::InputKind::kKeyboard);
					g.promptDevice = PRP::NextPromptDevice(g.promptDevice, g.device);
					const auto inputDevice = g.device;
					IP::Button which = IP::ButtonForInputEvent(button->QUserEvent().c_str(), inputDevice, button->GetIDCode());
					if (which == IP::Button::kOther) which = ResolveWorkstationFallbackButton(*button, device);
					if (device == RE::INPUT_DEVICE::kKeyboard && button->GetIDCode() == RE::BSKeyboardDevice::Keys::kF10) which = IP::Button::kF10;
					if (which == IP::Button::kOther) continue;
					const auto phase = button->IsDown() ? IP::Phase::kDown : (button->IsUp() ? IP::Phase::kUp : IP::Phase::kHeld);
					const auto decision = IP::Map(g.session.state, which, phase, button->HeldDuration(), g.tracker);
					Command(decision.command, which == IP::Button::kF10 ? SP::Cause::kF10 : SP::Cause::kEmergency);
					consumed = consumed || decision.consume;
				}
				return consumed ? RE::BSEventNotifyControl::kStop : RE::BSEventNotifyControl::kContinue;
			}
		};
		InputSink g_inputSink;

		ActivationIntentPolicy::Identity CurrentActivationIdentity(bool* a_exactTarget = nullptr)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* cell = player ? player->GetParentCell() : nullptr;
			auto* forgeCell = ForgeCell();
			auto* actorState = player ? player->AsActorState() : nullptr;
			auto* ui = RE::UI::GetSingleton();
			auto* pick = RE::CrosshairPickData::GetSingleton();
			RE::NiPointer<RE::TESObjectREFR> target = pick ? pick->target.get() : RE::NiPointer<RE::TESObjectREFR>{};
			auto* proxy = Lookup<RE::TESObjectREFR>(kSlabActivatorRefId, kPresentation);
			auto* forge = Lookup<RE::TESObjectREFR>(0x832, kSites);
			const bool exactTarget = target && (target.get() == proxy || target.get() == forge);
			if (a_exactTarget) *a_exactTarget = exactTarget;
			const bool available = player && cell && forgeCell && cell == forgeCell && actorState &&
				actorState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && ui && !ui->IsModalMenuOpen() &&
				!ui->IsApplicationMenuOpen() && !ui->IsMenuOpen("CustomMenu") && !ui->IsMenuOpen(kMenuName);
			return {
				.player = player ? player->GetFormID() : 0,
				.cell = cell ? cell->GetFormID() : 0,
				.target = target ? target->GetFormID() : 0,
				.session = g_activationSession.load(std::memory_order_relaxed),
				.cellEpoch = g_activationCellEpoch.load(std::memory_order_relaxed),
				.menuEpoch = g_activationMenuEpoch.load(std::memory_order_relaxed),
				.gameplayAvailable = available && exactTarget
			};
		}

		class ActivationIntentSink final : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!a_event) return RE::BSEventNotifyControl::kContinue;
				for (auto* current = *a_event; current; current = current->next) {
					const auto* button = current->AsButtonEvent();
					if (!button || !button->IsDown() || std::string_view(button->QUserEvent().c_str()) != "Activate") continue;

					bool exactTarget = false;
					const auto identity = CurrentActivationIdentity(&exactTarget);

					std::scoped_lock lock(g.mutex);
					if (ActivationIntentPolicy::ObserveActivateDown(g_activationIntent, identity, exactTarget,
							identity.gameplayAvailable && g.session.state == SP::State::kIdle, std::chrono::steady_clock::now())) {
						SKSE::log::info("RuneforgeIntent: captured seq={} target={:08X} device={} state=IDLE", g_activationIntent.pending.sequence,
							identity.target, static_cast<int>(current->GetDevice()));
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		ActivationIntentSink g_activationIntentSink;

		// ---------------------------------------------------------------- Ack listener (Papyrus -> native, protocol v3)

		void OnAck(const PP::Ack& a_ack)
		{
			const auto token = g.session.token;
			const bool live = token != 0u && a_ack.token == token;
			switch (a_ack.kind) {
			case PP::AckKind::kReady:
				Dispatch({ SP::EventKind::kAckReady, a_ack.token, 0, {} });
				break;
			case PP::AckKind::kCleared:
				SKSE::log::info("[FEASR-RF] WS staging cleared {} refs for token {}", a_ack.value, a_ack.token);
				Dispatch({ SP::EventKind::kAckCleared, a_ack.token, 0, {} });
				break;
			case PP::AckKind::kItemSpawned:
			case PP::AckKind::kItemLoaded:
			case PP::AckKind::kItemFail: {
				const char* verb = a_ack.kind == PP::AckKind::kItemSpawned ? "SPAWNED" : a_ack.kind == PP::AckKind::kItemLoaded ? "LOADED" : "FAIL";
				if (!live) {
					SKSE::log::info("[FEASR-RF] WS ITEM_ACK token={} seq={} {} verdict=STALE_TOKEN (session token {}); dropped", a_ack.token, a_ack.seq, verb, token);
					break;
				}
				if (a_ack.kind != PP::AckKind::kItemFail) {
					if (auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(a_ack.value)) {
						const auto handle = ref->GetHandle();
						if (std::find(g.spawned.begin(), g.spawned.end(), handle) == g.spawned.end()) g.spawned.push_back(handle);
						RememberClone(a_ack.value);
					}
				}
				if (a_ack.kind == PP::AckKind::kItemSpawned) {
					SKSE::log::info("[FEASR-RF] WS ITEM_ACK token={} seq={} SPAWNED ref={:08X} (waiting for 3D)", a_ack.token, a_ack.seq, a_ack.value);
					break;
				}
				if (a_ack.code == "SUPERSEDED") {
					SKSE::log::info("[FEASR-RF] WS ITEM_ACK token={} seq={} FAIL SUPERSEDED (a newer request or clear replaced it); dropped", a_ack.token, a_ack.seq);
					GP::Acked(g.stage, GP::Kind::kItem, a_ack.seq, false);
					break;
				}
				// a NEUTRAL fallback is a deliberate empty slab, not a failure
				const bool ok = a_ack.kind == PP::AckKind::kItemLoaded || a_ack.code == "NEUTRAL";
				const bool current = a_ack.seq == g.session.itemSeq;
				SKSE::log::info("[FEASR-RF] WS ITEM_ACK token={} seq={} {} {} verdict={}", a_ack.token, a_ack.seq, verb,
					a_ack.kind == PP::AckKind::kItemLoaded ? fmt::format("ref={:08X}", a_ack.value) : a_ack.code, !current ? "STALE_SEQ" : (ok ? "OK" : "FAILED"));
				GP::Acked(g.stage, GP::Kind::kItem, a_ack.seq, ok);
				if (current) {
					g.itemStatus = ok ? std::string{} :
					                    "The item could not be laid on the slab (" + a_ack.code + "). Select it again to retry.";
				}
				Dispatch({ SP::EventKind::kAckItem, a_ack.token, a_ack.seq, {}, ok ? 1 : 0 });
				break;
			}
			case PP::AckKind::kRunes: {
				if (!live) {
					SKSE::log::info("[FEASR-RF] WS RUNES_ACK token={} seq={} formula={} verdict=STALE_TOKEN (session token {}); dropped", a_ack.token, a_ack.seq,
						a_ack.formula, token);
					break;
				}
				if (a_ack.code == "SUPERSEDED") {
					SKSE::log::info("[FEASR-RF] WS RUNES_ACK token={} seq={} formula={} code=SUPERSEDED (a newer request, clear or beat replaced it); dropped",
						a_ack.token, a_ack.seq, a_ack.formula);
					GP::Acked(g.stage, GP::Kind::kRunes, a_ack.seq, false);
					break;
				}
				const bool current = a_ack.seq == g.row.seq;
				const bool ok = current && a_ack.RowComplete() && a_ack.formula == g.row.formula && a_ack.requested == g.row.requested &&
				                a_ack.requested == g.row.slotCount;
				const char* verdict = !current ? "STALE_SEQ" : ok ? "OK" : (a_ack.RowComplete() && a_ack.requested < g.row.slotCount ? "INCOMPLETE_MISSING_RUNES" : "INCOMPLETE");
				SKSE::log::info("[FEASR-RF] WS RUNES_ACK token={} seq={} formula={} requested={} staged={} slots={} code={} expected={}/{} verdict={}", a_ack.token,
					a_ack.seq, a_ack.formula, a_ack.requested, a_ack.staged, PP::SlotsText(a_ack.slots), a_ack.code, g.row.requested, g.row.slotCount, verdict);
				GP::Acked(g.stage, GP::Kind::kRunes, a_ack.seq, a_ack.RowComplete() && current && a_ack.formula == g.row.formula);
				if (current) {
					if (ok) g.rowStatus = "Rune row: " + std::to_string(a_ack.staged) + " of " + std::to_string(g.row.slotCount) + " tablets on the slab.";
					else if (a_ack.RowComplete()) g.rowStatus = "Rune row: " + std::to_string(a_ack.staged) + " of " + std::to_string(g.row.slotCount) + " tablets (missing runes leave their slots empty).";
					else g.rowStatus = "Rune row: INCOMPLETE, " + std::to_string(a_ack.staged) + " of " + std::to_string(a_ack.requested) + " tablets (" + a_ack.code + "). Go Back and select the formula again.";
				}
				Dispatch({ SP::EventKind::kAckRunes, a_ack.token, a_ack.seq, {}, ok ? 1 : 0 });
				break;
			}
			case PP::AckKind::kError:
				SKSE::log::warn("[FEASR-RF] WS staging error {} for token {}", a_ack.code, a_ack.token);
				break;
			case PP::AckKind::kBeatLanded:
			case PP::AckKind::kBeatFail:
				// R8.7.4.21: presentation only; it lets the next beat come at its schedule instead of after the timeout
				SKSE::log::info("[FEASR-RF] WS BEAT_ACK token={} strike={} {}{} t={} beat={} verdict={}", a_ack.token, a_ack.seq,
					a_ack.kind == PP::AckKind::kBeatLanded ? "LANDED" : "FAIL ", a_ack.code, g.session.t, g.session.beat,
					!live ? "STALE_TOKEN" : (g.session.state == SP::State::kPerforming ? "OK" : "NOT_PERFORMING"));
				if (live) Dispatch({ SP::EventKind::kAckBeat, a_ack.token, a_ack.seq, {}, a_ack.kind == PP::AckKind::kBeatLanded ? 1 : 0 });
				break;
			case PP::AckKind::kSealDone:
				SKSE::log::info("[FEASR-RF] WS SEAL_ACK token={} t={} bound={} verdict={}", a_ack.token, g.session.t, g.session.bound,
					!live ? "STALE_TOKEN" : (g.session.state == SP::State::kPerforming ? "OK" : "NOT_PERFORMING"));
				if (live) Dispatch({ SP::EventKind::kAckSeal, a_ack.token, 0, {} });
				break;
			case PP::AckKind::kInvalid:
				break;
			}
		}

		class AckSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (!a_event || std::string_view(a_event->eventName.c_str()) != PP::kAck) return RE::BSEventNotifyControl::kContinue;
				const std::string str = a_event->strArg.c_str();
				auto ack = PP::ParseAck(str, a_event->numArg);
				if (ack.kind == PP::AckKind::kInvalid) {
					SKSE::log::warn("[FEASR-RF] WS invalid ack '{}' {}", str, a_event->numArg);
					return RE::BSEventNotifyControl::kContinue;
				}
				// The Papyrus emitter runs on a VM thread; the session only changes on the game thread.
				if (auto* tasks = SKSE::GetTaskInterface()) {
					tasks->AddTask([ack = std::move(ack)]() {
						std::scoped_lock lock(g.mutex);
						OnAck(ack);
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		AckSink g_ackSink;

		// ---------------------------------------------------------------- world events (T5 hit, T7/T6 cell attach)

		class WorldSink final :
			public RE::BSTEventSink<RE::TESHitEvent>,
			public RE::BSTEventSink<RE::TESCellAttachDetachEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (!g_active.load(std::memory_order_acquire) || !a_event || !a_event->target) return RE::BSEventNotifyControl::kContinue;
				if (a_event->target.get() == RE::PlayerCharacter::GetSingleton() && a_event->cause && a_event->cause.get() != a_event->target.get()) {
					if (auto* tasks = SKSE::GetTaskInterface()) {
						tasks->AddTask([]() {
							std::scoped_lock lock(g.mutex);
							Interrupt(SP::Cause::kHit);
						});
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent* a_event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override
			{
				if (a_event && a_event->reference == RE::PlayerCharacter::GetSingleton()) {
					g_activationCellEpoch.fetch_add(1, std::memory_order_relaxed);
					std::scoped_lock lock(g.mutex);
					ReleaseSpatialSounds(0);
					ActivationIntentPolicy::Clear(g_activationIntent);
				}
				if (!a_event || !a_event->attached || !a_event->reference || g_active.load(std::memory_order_acquire)) return RE::BSEventNotifyControl::kContinue;
				if (a_event->reference == Slab()) {
					if (auto* tasks = SKSE::GetTaskInterface()) {
						tasks->AddTask([]() {
							std::scoped_lock lock(g.mutex);
							if (g.session.state == SP::State::kIdle) NativeSweep(true);
						});
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		WorldSink g_worldSink;

		// ---------------------------------------------------------------- AS2 callbacks (GameDelegate)

		void ReadyCallback(const RE::FxDelegateArgs&)
		{
			std::scoped_lock lock(g.mutex);
			GP::ForcePush(g.render);
			PushRender();
		}

		void CommandCallback(const RE::FxDelegateArgs& a_args)
		{
			if (a_args.GetArgCount() < 1 || a_args[0].GetType() != RE::GFxValue::ValueType::kString) return;
			const std::string command = a_args[0].GetString();
			const double number = a_args.GetArgCount() > 1 && a_args[1].GetType() == RE::GFxValue::ValueType::kNumber ? a_args[1].GetNumber() : -1.0;
			std::scoped_lock lock(g.mutex);
			// R8.7.4.18: only a deliberate click or the wheel makes this a mouse session; pointer enter/leave and hover do not
			if (PRP::CallbackIsDeliberateClick(command)) g.device = PRP::NextDevice(g.device, PRP::InputKind::kUiClick);
			else if (PRP::CallbackIsWheel(command)) g.device = PRP::NextDevice(g.device, PRP::InputKind::kMouseWheel);
			const auto state = g.session.state;
			if (command == "preview" && IP::SelectionState(state) && number >= 0.0 && number < 4096.0) {
				Focus(static_cast<std::size_t>(number), false);
			} else if (command == "wheelUp" && IP::SelectionState(state)) {
				Navigate(-1, false, false);
			} else if (command == "wheelDown" && IP::SelectionState(state)) {
				Navigate(1, false, false);
			} else if (command == "focus" && number >= 0.0 && number < 4096.0) {
				Focus(static_cast<std::size_t>(number));
			} else if (command == "accept") {
				Command(state == SP::State::kPerforming ? IP::Command::kSkip : IP::Command::kAccept);
			} else if (command == "back") {
				Command(state == SP::State::kPerforming ? IP::Command::kSkip : IP::Command::kBack);
			} else if (command == "holdBegin" && state == SP::State::kConfirm) {
				Command(IP::Command::kHoldBegin);
			} else if (command == "holdEnd" && state == SP::State::kConfirm) {
				Command(IP::Command::kHoldEnd);
			} else if (command == "ops" && (state == SP::State::kSelectFormula || state == SP::State::kOperations)) {
				Command(state == SP::State::kSelectFormula ? IP::Command::kOps : IP::Command::kBack);
			} else if (command == "filterNext") {
				Command(IP::Command::kFilterNext);
			} else if (command == "detail") {
				Command(IP::Command::kDetail);
			} else if (command == "pageUp") {
				Command(IP::Command::kPageUp);
			} else if (command == "pageDown") {
				Command(IP::Command::kPageDown);
			} else if (command == "pointer") {
				// the movie reports when the cursor enters (1) or leaves (0) the GUI box: the right button pans only outside it
				g.pointerInPanel = number > 0.5;
			}
		}

		void DiagnosticCallback(const RE::FxDelegateArgs& a_args)
		{
			if (a_args.GetArgCount() == 1 && a_args[0].GetType() == RE::GFxValue::ValueType::kString) {
				std::string message = a_args[0].GetString();
				if (message.size() > 256) message.resize(256);
				SKSE::log::info("[FEASR-RF] WS AS2 {}", message);
			}
		}

		// ---------------------------------------------------------------- the workstation menu (WS-D04)

		class WorkstationMenu final : public RE::IMenu
		{
		public:
			WorkstationMenu()
			{
				// Non-pausing, modal, menu input context, no kAllowSaving: the engine refuses saves for the whole session (SR-2).
				// Gameplay handlers still see input under a non-pausing menu (run 2): the control lease keeps them silent.
				auto flags = static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kUsesMenuContext) |
					static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kModal) |
					static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kDisablePauseMenu) |
					static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kRequiresUpdate);
				if (g.device != IP::Device::kGamepad) flags |= static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kUsesCursor);
				menuFlags = static_cast<RE::UI_MENU_FLAGS>(flags);
				inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode;
				depthPriority = 3;
			}

			void PostCreate() override
			{
				auto* scaleform = RE::BSScaleformManager::GetSingleton();
				const bool loaded = scaleform && scaleform->LoadMovie(this, uiMovie, "RuneforgeMenu");
				SKSE::log::info("[FEASR-RF] WS menu movie loaded={}", loaded);
				if (loaded && uiMovie) {
					uiMovie->SetVariable("_root.runeforgeMode", RE::GFxValue("workstation"));
				} else {
					std::scoped_lock lock(g.mutex);
					Interrupt(SP::Cause::kForced);
				}
			}

			void Accept(RE::FxDelegateHandler::CallbackProcessor* a_processor) override
			{
				if (!a_processor) return;
				a_processor->Process("runeforgeWsReady", ReadyCallback);
				a_processor->Process("runeforgeWsCommand", CommandCallback);
				a_processor->Process("diagnosticRuneforgeUi", DiagnosticCallback);
			}

			void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override
			{
				RE::IMenu::AdvanceMovie(a_interval, a_currentTime);
				Tick(a_interval);
			}

			RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override
			{
				if (a_message.type == RE::UI_MESSAGE_TYPE::kShow) {
					std::scoped_lock lock(g.mutex);
					g.menuOpen = true;
				} else if (a_message.type == RE::UI_MESSAGE_TYPE::kHide || a_message.type == RE::UI_MESSAGE_TYPE::kForceHide) {
					std::scoped_lock lock(g.mutex);
					g.menuOpen = false;
					if (!g.closeRequested) Interrupt(SP::Cause::kForced);
					g.closeRequested = false;
				}
				return RE::IMenu::ProcessMessage(a_message);
			}
		};

		RE::IMenu* CreateMenu()
		{
			return new WorkstationMenu();
		}

		// After the menu closes (EXITING) the session clock comes from this worker: the watchdog bound must hold
		// even when Papyrus never answers (INV-7, T10).
		void StartWorker()
		{
			g_worker = std::jthread([](std::stop_token a_stop) {
				auto last = std::chrono::steady_clock::now();
				while (!a_stop.stop_requested()) {
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					const auto now = std::chrono::steady_clock::now();
					const float seconds = std::chrono::duration<float>(now - last).count();
					last = now;
					if (!g_active.load(std::memory_order_acquire)) continue;
					bool menuOpen = false;
					{
						std::scoped_lock lock(g.mutex);
						menuOpen = g.menuOpen;
					}
					if (menuOpen) continue;
					auto* tasks = SKSE::GetTaskInterface();
					bool expected = false;
					if (!tasks || !g_tickPending.compare_exchange_strong(expected, true)) continue;
					tasks->AddTask([seconds]() {
						g_tickPending.store(false);
						Tick(seconds);
					});
				}
			});
		}
	}

	void Install()
	{
		std::scoped_lock lock(g.mutex);
		LoadSettings();
		bool menuSinkRegistered = false;
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->Register(kMenuName, CreateMenu);
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuOwnershipSink);
			g.menuRegistered = true;
			menuSinkRegistered = true;
		}
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) input->AddEventSink(&g_inputSink);
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) input->AddEventSink(&g_activationIntentSink);
		if (auto* source = SKSE::GetModCallbackEventSource()) source->AddEventSink(&g_ackSink);
		if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESHitEvent>(&g_worldSink);
			holder->AddEventSink<RE::TESCellAttachDetachEvent>(&g_worldSink);
		}
		StartWorker();
		SKSE::log::info("[FEASR-RF] WS installed (menu {} registered={}; RC1-WS protocol v3 r3.5-authfix: back input + stance guard + one-use activation authorization + foreign-menu arbitration; "
						"controls layout={} uiSink={})",
			kMenuName, g.menuRegistered, ControlsLayout().name, menuSinkRegistered);
	}

	bool TryOpen()
	{
		std::scoped_lock lock(g.mutex);
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* slab = Slab();
		auto* cell = ForgeCell();
		bool anchors = Lookup<RE::TESObjectREFR>(kGearAnchorId, kSites) != nullptr;
		for (RE::FormID id = kFirstSlotId; id < kFirstSlotId + 6; ++id) anchors = anchors && Lookup<RE::TESObjectREFR>(id, kSites) != nullptr;
		auto* stance = StanceRef();
		const bool stanceOccupied = stance && player && StanceOccupiedByOther(stance);
		const ContextPolicy::Context context{
			.flatModePreferred = g.settings.flatMode,
			.presentationLoaded = PluginLoaded(kPresentation),
			.playerInForgeCell = player && cell && player->GetParentCell() == cell,
			.slabResolved = slab != nullptr,
			.slab3DLoaded = slab && slab->Is3DLoaded(),
			.anchorsResolved = anchors,
			.stanceResolved = stance != nullptr,
			.stanceOccupiedByOther = stanceOccupied,
			.workstationMenuRegistered = g.menuRegistered,
		};
		const auto resolution = ContextPolicy::Resolve(context);
		if (resolution.mode != ContextPolicy::Mode::kWorkstation) {
			SKSE::log::info("[FEASR-RF] WS flat mode: {}", ContextPolicy::Name(resolution.reason));
			return false;
		}
		if (g.session.state == SP::State::kIdle) RestoreCheckedOutEquipment();
		g.itemFilter = 0;
		g.formulaFilter = 0;
		g.itemFocus = 0;
		g.formulaFocus = 0;
		g.opsFocus = 0;
		g.detail = false;
		g.status.clear();
		g.result = {};
		g.resultCard = json::object();
		g.itemCardKey.clear();
		g.itemCardRevision = ~0ull;
		g.itemCard = json::object();
		g.spawned.clear();
		g.tracker = {};
		g.stickDirection = IP::Button::kOther;
		g.verdicts.clear();
		g.stage = {};
		g.render = {};
		g.row = {};
		g.rowStatus.clear();
		g.itemStatus.clear();
		g.deferred.clear();
		g.pointerInPanel = false;
		g.panning = false;
		g.pan = {};
		g.panBaseValid = false;
		g.panWritten = false;
		g.panResetPending = false;
		g.panUnavailableLogged = false;
		g.panDrags = 0;
		if (g.session.state == SP::State::kIdle) {
			g.uiForeignMenus.clear();
			g.uiYieldUnpausedMs = 0;
			g_uiYield.store(false, std::memory_order_release);
			g_uiTeardownQueued.store(false, std::memory_order_release);
		}
		// R8.7.4.18: every session starts on the detected device (a connected, enabled gamepad, otherwise the keyboard)
		{
			auto* inputManager = RE::BSInputDeviceManager::GetSingleton();
			g.device = inputManager && inputManager->IsGamepadEnabled() ? IP::Device::kGamepad : IP::Device::kKeyboard;
			g.promptDevice = g.device;
			SKSE::log::info("[FEASR-RF] WS input start device={} family={}", PRP::DeviceName(g.device), g.settings.promptFamily);
		}
		RuneforgeMenu::Operations::SetDebugMode(false);
		if (auto* bridge = EventBridge::GetSingleton()) {
			const auto materialized = bridge->PrepareRuneforgeSingletonInventoryIdentities();
			if (materialized > 0u) {
				SKSE::log::info("[FEASR-RF] WS prepared {} singleton inventory instance(s) for Runeforge selection", materialized);
			}
		}
		RefreshSnapshot();
		Dispatch({ SP::EventKind::kOpen, 1, 0, {} });
		return g.session.state != SP::State::kIdle;
	}

	bool IsActive()
	{
		return g_active.load(std::memory_order_acquire);
	}

	std::uint64_t ClaimActivationIntent()
	{
		std::scoped_lock lock(g.mutex);
		const auto sequence = ActivationIntentPolicy::Claim(g_activationIntent, CurrentActivationIdentity(), std::chrono::steady_clock::now());
		if (sequence == 0) {
			SKSE::log::warn("RuneforgeIntent: Claim rejected reason=no_valid_unclaimed_physical_edge");
			return 0;
		}
		SKSE::log::info("RuneforgeIntent: claimed seq={} before Papyrus RequestOpen side effects", sequence);
		return sequence;
	}

	bool ConsumeClaimedActivationIntent(std::uint64_t a_sequence)
	{
		std::scoped_lock lock(g.mutex);
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* cell = player ? player->GetParentCell() : nullptr;
		const ActivationIntentPolicy::OpenIdentity identity{
			.player = player ? player->GetFormID() : 0,
			.cell = cell ? cell->GetFormID() : 0,
			.session = g_activationSession.load(std::memory_order_relaxed),
			.cellEpoch = g_activationCellEpoch.load(std::memory_order_relaxed)
		};
		if (!ActivationIntentPolicy::ConsumeClaim(g_activationIntent, a_sequence, identity)) {
			SKSE::log::warn("RuneforgeIntent: rejected Open seq={} reason=claim_missing_stale_or_context_changed", a_sequence);
			return false;
		}
		SKSE::log::info("RuneforgeIntent: consumed claimed seq={} for Open", a_sequence);
		return true;
	}

	void OnPreLoadGame()
	{
		g_activationSession.fetch_add(1, std::memory_order_relaxed);
		std::scoped_lock lock(g.mutex);
		ReleaseSpatialSounds(0);
		ActivationIntentPolicy::Clear(g_activationIntent);
		if (g.session.state != SP::State::kIdle) {
			Interrupt(SP::Cause::kPreload);
			RestoreCamera();
		}
		ReleaseLease(0, "pre-load");
	}

	void OnPostLoadGame()
	{
		g_activationSession.fetch_add(1, std::memory_order_relaxed);
		std::scoped_lock lock(g.mutex);
		ReleaseSpatialSounds(0);
		ActivationIntentPolicy::Clear(g_activationIntent);
		Dispatch({ SP::EventKind::kPostLoad, 0, 0, {} });
		g.menuOpen = false;
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([]() {
				std::scoped_lock inner(g.mutex);
				if (g.session.state == SP::State::kIdle) NativeSweep(true);
			});
		}
	}

	bool IsDisplayClone(const RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) return false;
		const auto id = a_ref->GetFormID();
		for (const auto& slot : g_cloneIds) {
			if (slot.load() == id) return true;
		}
		// EventBridge loot/container events may be delivered from engine worker threads
		// during startup/load reconstruction. Do not dereference the reference's base
		// object here: the atomic clone-ID mirror is the thread-safe authority for
		// temporary Runeforge presentation clones.
		return false;
	}

	bool IsFeasrRuneforgeActive()
	{
		auto* bridge = EventBridge::GetSingleton();
		return bridge && bridge->IsFeasrRuneforgeContractCompatible() && PluginLoaded(kCore);
	}
}
