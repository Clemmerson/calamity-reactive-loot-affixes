# Calamity - Reactive Loot & Affixes

[![Release](https://img.shields.io/badge/Release-v2.2.2-2ea44f)](https://github.com/servaltullius/calamity-reactive-loot-affixes/releases/tag/v2.2.2)
[![Download (MO2 ZIP)](https://img.shields.io/badge/Download-MO2%20ZIP-2ea44f)](https://github.com/servaltullius/calamity-reactive-loot-affixes/releases/download/v2.2.2/CalamityAffixes_MO2_v2.2.2_2026-09-28.zip)
[![Nexus Mods](https://img.shields.io/badge/Nexus%20Mods-Calamity%202-d98f40)](https://www.nexusmods.com/skyrimspecialedition/mods/193193)

## 어떤 모드인가요?

**Calamity - Reactive Loot & Affixes**는 Skyrim SE/AE의 기존 전투와 장비 파밍 위에 Diablo/PoE 스타일의 **어픽스·룬워드·통화 성장 루프**를 추가하는 SKSE 장비 빌드 모드입니다. 새 던전이나 퀘스트를 추가하는 대신, 플레이어가 가진 무기·방어구를 개별 인스턴스로 추적해 전투 효과와 성장 상태를 부여합니다.

**핵심 플레이 루프:** `적 처치 → 시체에서 제작 재료 수집 → 원하는 장비 선택 → 확인·선택 재련·슬롯 확장·룬워드 제작 → 나만의 전투 빌드 완성`

장비를 획득하는 순간 무작위 어픽스를 자동으로 붙이지 않습니다. 플레이어가 Prisma UI에서 원하는 베이스 장비를 직접 선택해 성장시키며, 완성된 장비는 적중·피격·처치 같은 조건에 따라 주문, 함정, 원소 전환, 소환, 시체 폭발 등의 효과를 발동합니다. 통화는 일반 상자나 월드에 뿌리지 않고 적격 적대 대상의 시체에서 획득합니다.

현재 적용 범위는 **플레이어 장비와 플레이어가 지휘하는 소환체의 처치** 중심입니다. 일반 NPC·팔로워 장비 전체를 독립적으로 강화하는 NPC 장비 오버홀 모드는 아닙니다.

## 다운로드 (플레이어)

> **현재 정식 릴리스:** [v2.2.2](https://github.com/servaltullius/calamity-reactive-loot-affixes/releases/tag/v2.2.2)

- 넥서스: [Calamity 2 - Reactive Loot and Affixes](https://www.nexusmods.com/skyrimspecialedition/mods/193193) (2.0부터의 새 페이지. 옛 페이지 [mods/172116](https://www.nexusmods.com/skyrimspecialedition/mods/172116)은 1.2.21에서 멈춘 구버전입니다)
- MO2 ZIP: [CalamityAffixes_MO2_v2.2.2_2026-09-28.zip](https://github.com/servaltullius/calamity-reactive-loot-affixes/releases/download/v2.2.2/CalamityAffixes_MO2_v2.2.2_2026-09-28.zip)
- SHA-256: [CalamityAffixes_MO2_v2.2.2_2026-09-28.zip.sha256](https://github.com/servaltullius/calamity-reactive-loot-affixes/releases/download/v2.2.2/CalamityAffixes_MO2_v2.2.2_2026-09-28.zip.sha256)
- 전체 릴리스: https://github.com/servaltullius/calamity-reactive-loot-affixes/releases
- 변경 이력: [CHANGELOG.md](CHANGELOG.md)

## 주요 기능

스카이림 SE/AE의 개별 장비 인스턴스에 Diablo/PoE 스타일 어픽스와 룬워드를 부여하고, 전투 발동·ICD·통화 수급을 SKSE 런타임에서 처리합니다.

- 일반 접두 73개, 일반 접미 66개(22계열), 룬워드 94개와 룬 33종을 데이터 기반으로 제공합니다.
- 적중·피격·처치·저체력 조건의 주문, 전환, 함정, 소환, 시체 폭발 등 다양한 전투 효과를 지원합니다.
- 장비는 owner FormID + UID 단위로 추적하며, 획득 시 자동 롤 대신 플레이어가 원하는 베이스를 골라 확인 스크롤로 감정하고 재련·정제로 다듬습니다.
- 룬 조각·재련 오브·확인 스크롤·정제 오브는 적격 적대 대상의 시체에서만 획득하며, 일반 확률·피티와 고유·보스·드래곤 확정 보상을 함께 지원합니다.
- Prisma UI에서 선택 장비 툴팁, 어픽스 슬롯 진행, 룬워드 검색·제작, 보유 통화와 피티 현황을 한국어·영어로 확인할 수 있습니다.

## v2.2.0 새 기능

- **선택 재련은 장비마다 6회까지입니다.** 다 쓰면 정제 오브로 어픽스를 전부 다시 굴리면서 6회를 되돌려 받습니다. 슬롯 확장은 횟수에 들어가지 않고, 패널 비용 줄에 남은 횟수(예: `4/6`)가 나옵니다.
- 정제 오브가 리셋 수단이 되면서 기본 드랍률을 2%에서 4%로, 확인 스크롤 교환을 15:1에서 10:1로 조정했습니다.
- 게임 안 한국어 표기를 "칼라미티"로 통일했습니다.
- 장비별 재련 횟수는 새 코세이브 레코드에 저장됩니다. 기존 세이브를 그대로 쓸 수 있고, 기존 장비는 0회부터 시작합니다. 자세한 내용은 [2.2.0 릴리스 노트](docs/releases/2026-09-28-github-release-body-v2.2.0.md)를 확인하세요.
- 2.2.1과 2.2.2는 영어 UI에서도 룬워드 이름이 한국어로 나오던 문제(레시피 목록, 베이스 이름, HUD 알림)를 고친 업데이트입니다.
- 2.1.x에서 추가된 것: 확인 스크롤 교환(2.1.0), 선택 베이스 어픽스 상자 수정(2.1.1), 룬워드 이름 통일(2.1.2), 서드파티 라이선스 고지(2.1.3).

## v2.0 핵심 변경

- **어픽스 제작을 세 재화로 나눴습니다.** 확인 스크롤로 처음 부여하고(1~3개), 재련 오브로 원하는 어픽스 하나만 바꾸고, 희귀한 정제 오브로 전부를 한 번에 다시 굴립니다. 아래 표를 참고하세요.
- 새 재화 두 종은 적격 적대 시체에서 떨어지며(확인 20%, 정제 2% — 2.2.0부터 4%), MCM에서 확률을 조정할 수 있습니다.
- 패널에 제작 재료 대시보드를 추가하고 제작 버튼을 흐름 순서로 정리했습니다. 넓은 레이아웃의 룬워드 탭에서 레시피 목록이 거의 보이지 않던 문제, 룬 그리드 칸이 잘리던 문제, 작은 패널에서 베이스 목록이 넘치던 문제도 고쳤습니다.
- 기존 세이브와 호환되며 기존 장비의 어픽스·룬워드는 그대로 유지됩니다. 새 재화가 ESP에 추가되므로 **전체 ZIP으로 설치**하세요.
- 자세한 규칙과 업데이트 방법은 [2.0.0 릴리스 노트](docs/releases/2026-09-26-github-release-body-v2.0.0.md)를 확인하세요. 2.0.1은 패널 표시를 다듬은 업데이트입니다.

## 어픽스 제작: 확인·선택 재련·정제

일반 어픽스는 `접두 1 + 접미 2`, 룬워드는 별도 1개 구조입니다.

| 작업 | 결과 | 비용 |
| --- | --- | ---: |
| 확인 | 일반 어픽스가 없는 장비에 1~3개 부여(60%/30%/10%) | 확인 스크롤 1개 |
| 선택 재련 | 선택한 일반 어픽스 하나만 같은 슬롯 종류로 교체(장비마다 6회, 정제하면 다시 6회) | 재련 오브 2개 |
| 슬롯 확장 1 → 2 | 기존 어픽스를 유지하고 접미 추가 | 재련 오브 2개 |
| 슬롯 확장 2 → 3 | 기존 접미와 다른 계열의 접미 추가 | 재련 오브 4개 |
| 정제 | 슬롯 수를 유지한 채 일반 어픽스 전부를 한 번에 다시 굴리고, 선택 재련 6회를 되돌림 | 정제 오브 1개 |
| 스크롤 교환 | 재련 오브 1개 / 정제 오브 1개 획득(한 방향, 되돌릴 수 없음) | 확인 스크롤 3개 / 10개 |

- 모든 작업은 완성된 룬워드와 그 성장 상태를 보존합니다. 선택 재련은 선택하지 않은 일반 어픽스의 성장 상태도 보존합니다.
- 선택 재련은 **장비마다 6회**까지입니다. 다 쓰면 정제(또는 어픽스가 없는 장비의 확인)로 어픽스를 다시 굴리면서 6회를 되돌려 받습니다. 슬롯 확장은 이 횟수에 들어가지 않습니다. 횟수는 코세이브에 장비별로 저장됩니다.
- 정제는 6초 안에 한 번 더 눌러 확정합니다. 기존 일반 어픽스와 그 성장 상태는 사라지고, 같은 슬롯 수로 새 어픽스가 붙습니다. 선택 재련이 하나씩 조준하는 방법이라면, 정제는 전부를 통째로 다시 굴리고 재련 기회를 새로 받는 방법입니다.
- 장비를 한 번 확인하면 확인 스크롤은 다시 쓰지 않으므로, 남는 스크롤은 패널 "제작 재료"에서 재련 오브(3:1)나 정제 오브(10:1)로 바꿀 수 있습니다.
- 구버전 세이브의 비정상 어픽스 구성(알 수 없는 어픽스, 접두/접미 배치 오류)은 선택 재련·확장이 막히며, 정제로 정상 구성으로 되돌릴 수 있습니다.
- 적격 적대 시체에서 확인 스크롤은 기본 20%, 정제 오브는 기본 4%로 독립 판정합니다(MCM에서 조정 가능). 기존 룬 조각·재련 오브 보상은 유지됩니다. 새 재화에는 별도 보스 확정 보상이나 천장이 없습니다.
- 이전 장비의 어픽스와 슬롯을 자동 변경하지 않습니다. 새 재화는 기존 ESP 레코드 뒤에 추가하며 DLL·ESP·패널을 함께 업데이트해야 합니다.
- 상세 규칙과 검증 기록: [제작 시스템 안내](docs/testing/2026-09-22-affix-crafting.md).

## 필수/권장 모드

- 필수: SKSE64, Address Library for SKSE Plugins, Prisma UI
- 권장: SkyUI, powerofthree's Tweaks
- 선택: KID, MCM Helper, I4

## 5분 설치 (MO2 기준)

1. 위 “필수” 모드를 설치합니다.
2. GitHub Releases에서 `CalamityAffixes_MO2_vX.Y.Z_YYYY-MM-DD.zip`을 다운로드합니다.
3. MO2에서 “Install a new mod from an archive”로 ZIP을 설치하고 Enable 합니다. 기존 버전 업데이트라면 같은 모드에 **Replace**로 설치하고, Merge하거나 구·신 버전을 동시에 활성화하지 않습니다.
4. SummonFix DLL 테스트 모드를 사용했다면 비활성화해 정식 DLL을 덮어쓰지 않게 합니다. 업데이트 전에 이미 소환된 개체는 사라진 뒤 새로 소환하세요.
5. SKSE로 실행합니다.
6. 정상 동작 확인: `Documents/My Games/Skyrim Special Edition/SKSE/CalamityAffixes.log`가 생성됩니다.

## 사용법 (한 줄 요약)

- 플레이어 또는 player-owned summon/proxy가 적대 대상을 처치하면 해당 **시체 인벤토리**에만 일반 적 확률 보상 또는 고유·보스 확정 보상이 적용됩니다. (어픽스 자동 부여, 일반 상자/컨테이너/월드 드랍 없음)
- 인벤/루팅/상점에서 아이템을 “선택”하면 Prisma UI 툴팁에 어픽스 설명이 표시됩니다.
- Prisma 조작 패널 토글: 기본 `F11` (MCM에서 변경 가능)
- Prisma 패널에서 **재련 오브(Reforge Orb)** 를 사용해 선택 장비를 재련할 수 있습니다. 최초 부여는 확인 스크롤 1개, 선택 재련은 재련 오브 2개, 슬롯 확장은 2/4개를 사용합니다.
- 일반 적은 룬 조각 8%와 재련 오브 12% 독립 판정을 사용하며, 적격 드래곤은 두 통화를 각각 1개씩 확정 지급합니다. 보상은 플레이어에게 바로 들어오지 않고 처치한 대상의 시체에 추가됩니다.
- 잘못 연결된 어픽스 상태를 지우려면 디버그 모드를 켜고 장비를 착용·선택한 뒤 고급 탭 **복구 및 초기화**의 **Reset State / 상태 초기화**를 두 번 누릅니다. 이 섹션은 디버그 모드에서만 보입니다. 일반 어픽스만 다시 굴리려면 정제 오브를 사용하세요. Calamity 어픽스/룬워드/런타임 상태가 제거되며 재료는 환불되지 않습니다.
- 룬워드 패널에서 레시피를 선택하면 **효과/권장 베이스/룬 순서/상세**가 한국어/영어 설정에 맞춰 상태 영역에 표시되며, 레시피 항목 hover로도 상세 효과를 확인할 수 있습니다.
- 패널은 화면 경계 안에 자동 배치되고, 레시피 목록은 wheel·스크롤바 드래그·키보드 스크롤과 ARIA 접근성을 함께 지원합니다.

## 문제 해결 (가장 흔한 것)

- 툴팁이 안 뜸: Prisma UI 설치 확인, `Data/PrismaUI/views/CalamityAffixes/index.html`가 있는지 확인, 로그 확인
- 실행/크래시: 런타임과 맞는 SKSE/Address Library인지 확인, `Data/SKSE/Plugins/*.dll` 충돌 여부 확인
- 판매/상자 이동 뒤 다른 장비에 예전 어픽스가 보임: 잘못 표시된 장비를 착용·선택하고 디버그 모드에서 고급 탭의 **Reset State / 상태 초기화**를 두 번 누릅니다. v1.2.22부터 새 소유권 이동은 owner+UID 기준으로 추적되며 상태끼리 병합되지 않습니다.
- 업데이트 뒤 구 SPID 분배가 계속 보임: MO2에서 새 버전을 별도 모드로 겹쳐 켜면 충돌 우선순위에 따라 구 `CalamityAffixes_DISTR.ini`가 새 빈 파일보다 우선할 수 있습니다. 기존 Calamity 모드를 교체/덮어쓰거나 구 DISTR 파일을 비활성화하세요.
- 설정 변경:
  - MCM Helper가 있으면 MCM에서 조정
  - 없으면 `Data/MCM/Config/CalamityAffixes/settings.ini`를 `Data/MCM/Settings/CalamityAffixes.ini`로 복사해서 오버라이드
  - 런타임/MCM UI 값은 `Data/SKSE/Plugins/CalamityAffixes/user_settings.json`에도 저장되어, 새 게임/재시작 후에도 마지막 설정을 복원합니다.
  - 런타임(MCM) 값 저장은 짧은 디바운스(약 250ms)로 묶어 디스크 쓰기 빈도를 줄입니다.
  - `user_settings.json` 파싱/IO 실패 시 기존 파일을 빈 값으로 덮어쓰지 않고 보존합니다.

## 이 레포는 무엇인가요?

- 플레이어용 배포 ZIP과 함께, 소스 코드(SKSE 플러그인), 생성기(dotnet), 문서를 같이 관리하는 개발 레포입니다.
- 개발/설계/배포 자동화 상세는 아래 “펼치기” 섹션을 참고하세요.

<details>
<summary>개발/설계 상세 (펼치기)</summary>

### Maintainer 빠른 링크

- GitHub Releases(전체): https://github.com/servaltullius/calamity-reactive-loot-affixes/releases
- 체인지로그: `CHANGELOG.md`
- 넥서스 업로드 실무 가이드: `docs/releases/2026-02-09-nexus-upload-playbook.md`
- 넥서스 붙여넣기 완성본(v0.1.0-beta.1): `docs/releases/2026-02-09-nexus-publish-copy-v0.1.0-beta.1.md`

스카이림(SE/AE)용 “Diablo/PoE 스타일 어픽스 + 프로크(확률 발동) + ICD” 시스템을 만들기 위한 개발 레포입니다.

## 목표(요약)

- 아이템(무기/아머)에 **어픽스(확률 발동 효과)**를 붙이고
- SKSE 플러그인의 EventBridge가 **프로크/ICD/가드**와 무기·마법·소환수·DoT 적용(=틱 X) 트리거를 처리합니다.

## 적용 범위(현재)

- 기본 런타임 대상은 **플레이어 장비/플레이어 인벤토리**입니다.
- 전투 트리거는 **플레이어 본인 + 플레이어가 지휘하는 소환체(player-owned summon/proxy)** 까지 지원합니다.
- **일반 팔로워/NPC가 착용한 장비를 독립 주체로 추적/발동**하는 모드는 현재 지원하지 않습니다.

## 어픽스 적용 방식(인스턴스 / Reforge 중심)

- SKSE 플러그인은 아이템 **인스턴스(ExtraUniqueID의 owner FormID + UID)** 상태를 코세이브로 추적/저장합니다.
- 현재 기본 정책은 **획득/제작 시 자동 어픽스 롤링 비활성화**이며, 어픽스는 확인 스크롤로 최초 부여하고 선택 재련(Reforge)으로 하나씩 교체합니다.
- 일반 어픽스가 없는 장비는 확인 스크롤 1개로 접두 1개와 접미 0~2개를 얻고, 부족한 슬롯은 확장으로 `접두 1 + 접미 2`까지 성장합니다. `1→2`는 오브 2개, `2→3`은 오브 4개를 사용합니다.
- 선택 재련은 일반 어픽스 하나만 교체하고, 슬롯 확장은 기존 어픽스와 완성 룬워드를 보존한 채 새 접미만 추가합니다.
- 장기 세이브 안정성을 위해 인스턴스 어픽스는 SKSE 코세이브에 **문자열이 아닌 64-bit 토큰(FNV-1a)**로 저장합니다(`IAXF v7`). *(v1~v6 레코드는 로드 시 호환 처리)*
  - 따라서 `affix.id`는 **리네임 금지**(기존 세이브 매칭이 깨짐). 변경이 필요하면 “새 id 추가”로 버전업합니다.
- v1.2.22는 구버전이 플레이어 인벤토리에 잘못 만든 `(item FormID, UID)` 키를 현재 인벤토리에서 정확히 확인되는 경우에만 `(player FormID, UID)`로 보정합니다. 이미 다른 아이템에 잘못 붙은 상태는 자동 추정 이동하지 않으며 위 초기화 버튼으로 제거합니다.
- (안전한 프루닝) 플레이어가 월드에 **드랍한 아이템 레퍼런스가 삭제될 때만** 해당 인스턴스 엔트리를 정리해 코세이브 누적을 완화합니다(재획득 시엔 프루닝 안 함).
- (기본 UX) Prisma UI 오버레이로 **선택 중인 아이템의 어픽스 효과 설명을 표시**합니다. (**Prisma UI 필수**, Norden UI 등 UI 테마와 무관)
  - 아이템 메뉴(인벤/루팅/상점)에서 “선택된 아이템”이 어픽스 인스턴스면 표시되고, 아니면 자동으로 숨겨집니다.
  - 합성 프리뷰(획득 전 가상 롤) 경로는 기본 비활성화되어 “프리뷰-획득 불일치”를 줄입니다.
- (기본값) `loot.renameItem=true` : 아이템 이름에 **짧은 어픽스 라벨**을 붙여(좌측 리스트) 빠르게 식별합니다.
  - `loot.nameMarkerPosition=trailing`이면 이름 마커를 뒤에 붙입니다. 예: `철검*` (정렬 안정화)

기본값: `loot.runewordFragmentChancePercent=8`, `loot.reforgeOrbChancePercent=12`, `loot.uniqueActorGuaranteedRunewordChancePercent=40`, `loot.currencyDropMode=hybrid` (레거시 설정 호환 토큰; 실제 드랍 권한은 SKSE death-event corpse-only), `loot.renameItem=true`, `loot.nameMarkerPosition=trailing`, `loot.sharedPool=true`
참고: `loot.chancePercent`는 현재 기본 정책에서 실질적으로 사용되지 않는 레거시 호환 필드입니다.
추가 안전장치(기본값·권장): `loot.trapGlobalMaxActive=48` (전역 논리 함정 상한). 물리 월드 마커도 안전상 최대 48개이므로 `0=무제한`은 논리 함정만 늘리고 48개를 넘는 마커 표시를 보장하지 않아 권장하지 않습니다.

## 제작 아이템(대장간/제작)에서는 어픽스가 어떻게 붙나요?

- 제작 완료 아이템도 기본적으로 **자동 어픽스 롤링이 적용되지 않습니다**.
- 룬워드/재련 플로우를 통해 인스턴스 효과를 부여하는 설계입니다.

## 팔로워/NPC에게 아이템을 주면 적용되나요?

- 현재 설계는 플레이어 중심이라, **팔로워/NPC 장비를 별도 인스턴스로 굴리는 방식은 미지원**입니다.
- 따라서 “NPC가 착용한 같은 아이템도 동일한 어픽스 시스템으로 발동”을 기대하면 현재 빌드에서는 동작이 제한적입니다.
- 안정성을 우선해 범위를 좁힌 상태이며, 필요 시 추후 `팔로워 한정` 단계부터 확장하는 것을 권장합니다.

## 레포 구조

- `docs/design/` : 개발 명세서(컨셉/제약/설계)
- `docs/AFFIX_CATALOG.md` : 현재 구현된 효과 목록(한국어 요약)
- `docs/RUNEWORD_EFFECTS.md` : JSON에서 개별 정의한 룬워드 94개 레시피별 효과 설명
- `docs/EFFECTS_EN.md` : 영어권 플레이어용 전체 효과 목록(접두·접미·룬워드, 게임 내 영문 문구, 넥서스 링크용)
- `THIRD_PARTY_NOTICES.txt` : DLL에 들어가는 오픈소스(CommonLibSSE-NG, spdlog, fmt, nlohmann/json, rapidcsv, Xbyak)의 MIT·BSD 라이선스 고지. 배포 ZIP의 `Docs/`에도 들어갑니다.
- `Data/` : 게임 `Data/`에 그대로 설치 가능한 “스테이징” 폴더 (MO2 테스트용)
  - `Data/CalamityAffixes.esp` : 키워드/스펠/MGEF + MCM Helper Quest(자동 생성, 단일 ESP)
  - `Data/Scripts/Source/` : Papyrus 소스(.psc)
    - 활성 컴파일 대상: `CalamityAffixes_ModeControl.psc`, `CalamityAffixes_ModEventEmitter.psc`, `CalamityAffixes_MCMConfig.psc`
    - `CalamityAffixes_MCMConfig.psc` : MCM Helper용 config 스크립트(Quest에 바인딩)
    - `CalamityAffixes_ModeControl.psc` : Prisma/MCM 공용 글로벌 브리지(수동 모드 + 룬워드 + Prisma 패널 토글)
    - `CalamityAffixes_AffixManager.psc` / `CalamityAffixes_PlayerAlias.psc` / `CalamityAffixes_SkseBridge.psc`는 레거시 참고용 소스(현재 배포 빌드에서 .pex 미생성)
  - `Data/Scripts/*.pex` : MCM/런타임에 필요한 컴파일 결과물(빌드 시 자동 생성)
  - `Data/MCM/Config/CalamityAffixes/` : MCM Helper 설정
    - `General / 일반` 페이지: 런타임 설정 + DotApply 안전 자동비활성 토글 + Prisma 패널 열기 토글 + 패널 언어(EN/KO/Both)
    - `keybinds.json`: `prisma_panel_toggle` 포함(Prisma 패널 빠른 토글)
    - MCM `Panel Language / 패널 언어`에서 Prisma 패널 텍스트를 영어/한국어/병행 표기로 전환
  - `Data/CalamityAffixes_KID.ini` : KID 템플릿(태그/옵션 분배)
  - `Data/CalamityAffixes_DISTR.ini` : 빈 SPID 호환 산출물(새 통화 분배 규칙 비활성)
- `skse/CalamityAffixes/` : CommonLibSSE-NG 기반 SKSE 플러그인(CMake/vcpkg)

## 설치 전 준비물(플레이어)

### 게임/런타임

- 단일 CommonLibSSE-NG/Address Library 빌드로 Skyrim SE 1.5.97 및 AE 1.6.x·1.7.x를 대상으로 합니다. 자동 테스트는 Skyrim 실행 파일을 구동하지 않으므로, 실제 인게임 호환성은 사용 중인 런타임·SKSE·Address Library 조합에서 확인해야 합니다.
- SKSE64: 런타임에 맞는 버전 설치 (SE/AE 빌드가 분리됨) ([skse.silverlock.org](https://skse.silverlock.org/))
- Address Library for SKSE Plugins: 런타임(1.5.x vs 1.6.x·1.7.x)에 맞는 파일 설치 ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/32444))

### 의존성(기본 UX 기준)

- Prisma UI (**필수**, 툴팁 오버레이) ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/148718))
  - 뷰 파일 위치: `Data/PrismaUI/views/CalamityAffixes/index.html`
  - 메인 조작 UI(수동 모드/룬워드/디버그)도 Prisma 패널에서 처리
  - 인벤토리에서는 기본적으로 “툴팁은 클릭-스루(비상호작용)”입니다. 패널은 **단일 단축키**(`prisma_panel_toggle`, MCM에서 설정)로 열고/닫습니다. 키가 미설정이면 기본 `F11`로 토글됩니다(인벤 밖에서도 가능).
  - 패널 표시 언어는 MCM `Panel Language / 패널 언어` 옵션에서 즉시 변경할 수 있습니다.
  - 레시피의 효과·권장 베이스·룬 순서·상세는 선택 언어에 맞춰 표시되고, 검색 결과·hover 상세도 같은 설명 계약을 사용합니다.
  - 패널 위치는 현재 viewport 안으로 제한되며, 레시피 목록은 wheel과 네이티브 scrollbar 조작을 경쟁시키지 않고 키보드·ARIA 상태도 제공합니다.
  - 패널 단축키/언어 값은 `Data/SKSE/Plugins/CalamityAffixes/user_settings.json`에 동기화되며, MCM 설정 파일이 없는 상태에서도 해당 값으로 복원됩니다.
  - 패널이 열리면 입력/커서는 패널이 잡아 “클릭-스루”로 인벤이 함께 눌리는 현상을 방지합니다(ESC 또는 Close로 닫기).
  - 패널이 열리면 툴팁은 패널 안으로 이동합니다(중복 표시 방지).

#### (권장) SkyUI

- SkyUI (인벤/루팅 UI 품질 향상; 본 모드 핵심 로직에는 필수 아님) ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/12604))

#### (권장) powerofthree's Tweaks

- powerofthree's Tweaks ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/51073))
- 2.2.7까지는 사실상 필수였습니다. 이 모드는 자기 주문·재화·이펙트를 이름(에디터 ID)으로 찾는데, 스카이림은 이 이름을 버리기 때문에 powerofthree's Tweaks의 "Load EditorIDs"(기본 켜짐)가 있어야 찾을 수 있었습니다.
- 2.2.8부터는 DLL이 `CalamityAffixes.esp`를 직접 읽어 이름표를 만들므로 없어도 동작합니다. 있으면 다른 모드·바닐라 폼의 이름까지 읽을 수 있어서, 방어구 제외 목록(`rewardbox`·`lootbox` 등)과 보스 상자 이름 판정이 함께 적용됩니다.

#### (권장) KID

- KID (태그 분배; DoT 태그 `CAFF_TAG_DOT` 포함) ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/55728))
- 주의: `CAFF_TAG_DOT`을 **필터 없이(Magic Effect + `NONE`) 전체 분배**하면, 키워드 기반 디스펠/정화 효과와 상호작용해 “적용중인 효과”가 사라지는 등 **심각한 부작용**이 날 수 있습니다. 그래서 기본 KID 규칙은 `*Alch*|H`(바닐라 독/알케미 해로운 MGEF)로 **좁게 제한**하고, 위험한 무필터 규칙은 생성 단계에서 출력 자체를 건너뜁니다.

#### SPID 호환 참고

- 현재 통화 드랍은 SKSE death event가 처리하므로 SPID가 필요하지 않습니다.
- `Data/CalamityAffixes_DISTR.ini`는 설치·업데이트 호환을 위해 남기는 **빈 산출물**이며 새 룬 조각/재련 오브 분배 규칙을 등록하지 않습니다.
- SPID를 별도로 설치해도 본 모드의 어픽스 런타임 대상 범위를 NPC 전체로 확장하거나, 일반 상자·컨테이너에 통화를 추가하지 않습니다.

### 의존성(선택)

- MCM Helper ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/53000))
  - 런타임 확률, 패널 단축키·언어와 안전 옵션을 MCM에서 조정할 때 사용합니다.
- Inventory Interface Information Injector (I4) ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/85702))
  - 본 모드는 `Data/SKSE/Plugins/InventoryInjector/CalamityAffixes.json`를 **빈 placeholder**로 동봉합니다(`rules: []`).
  - 툴팁/효과 표시 자체에 필수는 아닙니다.

## DoT 정책 (PoE식 “저발동”)

- DoT는 **hit로 취급하지 않음**
- “틱마다 발동” 금지
- 대신 **DoT 적용/갱신 시 1회**만 트리거를 흘림
  - C++ 플러그인에서 `TESMagicEffectApplyEvent`를 받고
  - `CAFF_TAG_DOT` 키워드가 붙은 MGEF만 선별하여
  - 대상+이펙트 단위로 강한 ICD(기본 1.5초)로 레이트리밋 후 Papyrus로 전달
- `CAFF_TAG_DOT`은 `DotApply` 트리거용 DoT 태그입니다. 기본 배포본에서는 **비활성(chance=0)** 이며, 필요하면 `Data/CalamityAffixes_KID.ini`에서 바닐라 독 규칙(`*Alch*|H`)의 chance를 `100`으로 바꿔 활성화하세요. 다른 DoT(모드/커스텀 MGEF)에 반응시키려면 `affixes/modules/*.json`(조합 결과는 `affixes/affixes.json`)의 `keywords.kidRules`에 **좁은 필터 규칙**을 추가해 생성기로 다시 빌드하세요.

관련 명세는 `docs/design/개발명세서.md`의 `5.4` 참고.

## 개발/테스트 준비물

- .NET SDK 8.0+ (`tools/CalamityAffixes.Generator`, `tools/CalamityAffixes.Generator.Tests`)
- Python 3 (lint/JSON 스모크 체크)
- CMake + Ninja + C++ toolchain (SKSE DLL 빌드)
- Papyrus Compiler 환경 (`tools/compile_papyrus.sh`, `tools/build_mo2_zip.sh`에서 사용)

상세 절차는 아래 `SKSE 플러그인 빌드(개발자용)` / `MO2 배포 ZIP 생성` 섹션을 따릅니다.

## SKSE 플러그인 빌드(개발자용)

> 이 레포는 Linux에서도 편집 가능하지만, SKSE 플러그인 빌드는 보통 Windows + Visual Studio 2022 환경이 필요합니다.

참고(입문/환경 구성): [skyrim.dev - Your first SKSE plugin in C++](https://skyrim.dev/skse/first-plugin)

### 개요

- 프로젝트 위치: `skse/CalamityAffixes/`
- CMake + vcpkg로 `commonlibsse-ng-flatrim` 의존성을 사용합니다.
- 빌드 산출물: `CalamityAffixes.dll`
- 설치 위치(스테이징): `Data/SKSE/Plugins/CalamityAffixes.dll`

### 빌드 경로 선택 (실수 방지)

- `linux-release-commonlibsse` 프리셋: 테스트/정적체크 용도(g++).
- `build.linux-clangcl-rel`: 실제 Windows 타깃 DLL 산출용(WSL cross, `clang-cl + lld-link + xwin`).
- Windows 환경에서는 Visual Studio Developer Command Prompt에서 빌드하는 경로를 권장합니다.

`fatal error: Windows.h: No such file or directory` 발생 시:
- 대부분 `g++` 경로(테스트 프리셋)로 DLL 빌드를 시도한 경우입니다.
- DLL 빌드 경로를 `build.linux-clangcl-rel` 또는 Windows VS 경로로 전환하세요.

### WSL/Linux cross로 DLL 빌드

```bash
cd skse/CalamityAffixes
python3 ../../tools/ensure_skse_build.py --lane plugin --lane runtime-gate
cmake --build build.linux-clangcl-rel --target CalamityAffixes
ctest --test-dir build.linux-clangcl-rel --no-tests=error --output-on-failure
cp -f build.linux-clangcl-rel/CalamityAffixes.dll ../../Data/SKSE/Plugins/CalamityAffixes.dll
```

최신 DLL 반영 확인:

```bash
sha256sum build.linux-clangcl-rel/CalamityAffixes.dll ../../Data/SKSE/Plugins/CalamityAffixes.dll
```

두 해시가 동일하면 `Data/SKSE/Plugins/CalamityAffixes.dll`이 최신 산출물입니다.

### 최소 스모크 체크(레포 내에서 가능한 것)

```bash
python3 tools/compose_affixes.py --check
python3 -m pip install --user jsonschema
python3 tools/lint_affixes.py --spec affixes/affixes.json --schema affixes/affixes.schema.json --manifest affixes/affixes.modules.json --generated Data/SKSE/Plugins/CalamityAffixes/affixes.json
python3 -m json.tool Data/MCM/Config/CalamityAffixes/config.json >/dev/null
python3 -m json.tool Data/MCM/Config/CalamityAffixes/keybinds.json >/dev/null
```

## Papyrus ↔ SKSE 브리지

  - 이벤트 시그니처 근거 요약: `docs/references/2026-02-09-event-signatures.md`
  - C++ 이벤트 싱크: `skse/CalamityAffixes/src/EventBridge.cpp`
  - `TESHitEvent` → `CalamityAffixes_Hit` (sender = 대상 Actor)
  - `TESDeathEvent` → `CalamityAffixes_Kill` (sender = 사망한 Actor) *(현재 Papyrus로는 미연결 — 추후 확장용)*
  - `TESMagicEffectApplyEvent`(+ `CAFF_TAG_DOT`) → `CalamityAffixes_DotApply` (sender = 대상 Actor)
  - `ModCallbackEvent` 수신:
    - `CalamityAffixes_MCM_SetEnabled` / `CalamityAffixes_MCM_SetDebugNotifications` / `CalamityAffixes_MCM_SetValidationInterval` / `CalamityAffixes_MCM_SetProcChanceMult` / `CalamityAffixes_MCM_SpawnTestItem`
      → MCM General 설정을 런타임에 즉시 반영
    - `CalamityAffixes_UI_SetPanel` / `CalamityAffixes_UI_TogglePanel`
      → Prisma 조작 패널 열기/닫기
    - `CalamityAffixes_ModeCycle_Next` / `CalamityAffixes_ModeCycle_Prev` → 장착 중 `modeCycle.manualOnly=true` affix 모드 수동 전환
    - `CalamityAffixes_Runeword_*` → 룬워드 베이스 순환/레시피 순환/룬 삽입/상태 조회/테스트용 조각 지급
  - 활성 Papyrus 브리지(컴파일 대상):
    - `Data/Scripts/Source/CalamityAffixes_ModeControl.psc`
    - `Data/Scripts/Source/CalamityAffixes_ModEventEmitter.psc`
    - `Data/Scripts/Source/CalamityAffixes_MCMConfig.psc`
  - `CalamityAffixes_SkseBridge.psc` / `AffixManager` 기반 경로는 레거시 참고용이며, 현재 기본 빌드 파이프라인에서는 컴파일하지 않습니다.

## 룬워드 제작(실험)

- 현재는 **유저 자유 베이스 지정**을 지원합니다. (장착 중 무기/방어구를 순환 선택)
- 레시피는 D2/D2R 기준 **94개 룬워드 카탈로그**(2차 확장)를 제공합니다. 레시피별 **권장 베이스 타입(무기/방어구)**은 안내만 하고 강제하지 않습니다.
- 룬워드 94개는 `affixes/affixes.json`의 JSON 정의로 각각 명시되며, 생성기는 정확히 **94개 레시피 + 33개 룬 가중치** 계약이 아니거나 ID/레코드가 충돌하면 실패합니다.
- Prisma 룬워드 패널은 **검색 + 총/검색결과 카운트**와 한국어/영어 상세 설명을 표시하며, 화면 경계 제한과 wheel·스크롤바·키보드/ARIA 탐색을 지원합니다.
- 조각 수급:
  - 설정 파일의 `hybrid` 값은 구버전 호환 토큰입니다. 실제 권한은 **SKSE death event corpse-only** 경로 하나입니다.
  - 플레이어 또는 player-owned summon/proxy가 처치한 적대 대상만 판정하며, 성공한 통화는 바닥에 생성하지 않고 해당 시체 인벤토리에 조용히 직접 추가합니다.
  - 일반 상자/컨테이너 활성화, 아이템 픽업, 월드 생성과 새 SPID 통화 분배는 모두 사용하지 않습니다.
  - 피해자가 팔로워/동료, 소환·지휘된 액터, 아동, player-owned 또는 비적대 대상이면 제외합니다. 환경 오브젝트나 player-owned가 아닌 독립 NPC/팔로워가 낸 처치도 제외합니다.
  - 룬 종류는 저급 `El-Amn=4`, 중급 `Sol-Um=3`, 고급 `Mal-Lo=2`, 최상급 `Sur-Zod=1`의 4단계 가중치로 선택됩니다(최고:최저 4:1).
  - 기본 파편 판정은 `8%`이고 99회 연속 실패 뒤 보장하는 피티를 유지합니다. 피티 카운터와 시체별 카테고리 ledger는 `CCRT` 코세이브 레코드에 저장됩니다.
  - 업데이트 전에 SPID로 통화 또는 해당 레벨드 리스트가 이미 들어간 전환 시체는 룬 조각/재련 오브 카테고리별로 감지해 같은 카테고리의 런타임 중복 롤을 건너뜁니다.
  - MCM 확률 변경은 다음 eligible hostile death 판정부터 적용됩니다.
  - Prisma 디버그 버튼: `+1 next fragment`, `+1 recipe set`
- 재련 오브(리포지):
  - 룬 조각과 같은 eligible hostile corpse-only 경로로 획득합니다.
  - 기본 확률은 `loot.reforgeOrbChancePercent=12`입니다.
  - Prisma 패널 `Reforge / 재련` 버튼으로 **선택된 장착 장비**에 2개를 소모해 선택한 일반 어픽스 하나를 교체합니다.
  - 아이템 어픽스 탭의 슬롯 확장은 일반 어픽스 `1→2`에 2개, `2→3`에 4개를 사용합니다. 기존 어픽스와 완성 룬워드는 보존됩니다.
  - 일반 장비 재련: 선택한 일반 어픽스 하나를 재굴림합니다.
  - 완성 룬워드 장비 재련: **룬워드 효과는 보존**하고, 선택한 일반 어픽스 하나만 재롤합니다.
  - 룬워드 재변환: 이미 룬워드가 완성된 장비에 새 룬워드를 적용하면 기존 룬워드를 교체합니다.
- 완성 시 룬워드 결과 토큰은 인스턴스 슬롯의 최우선 토큰으로 반영됩니다. (일반 어픽스와 공존 가능)

## 다음 단계(구현 시작 체크리스트)

1) (권장) `affixes/modules/*.json` 편집 후 `python3 tools/compose_affixes.py`로 `affixes/affixes.json` 조합 → 생성기 실행  
2) 두 가지 시작 방식 중 선택
   - **CK 0 (SKSE 런타임만 사용)**: 생성기 + KID + SKSE DLL만으로 동작(Quest/Alias 없음)
   - **CK 최소 (Papyrus 매니저 사용)**: Quest + PlayerAlias 세팅 후 스크립트 부착/프로퍼티 연결
3) DoT 태그(`CAFF_TAG_DOT`) 및 어픽스 키워드 구성  
4) 테스트 아이템/스펠로 “장비 스캔 → 적중/DoT 적용 → 프로크/ICD” 흐름 확인

### 생성기(키워드/ini 자동 생성)

```bash
python3 tools/compose_affixes.py
dotnet run --project tools/CalamityAffixes.Generator -- --spec affixes/affixes.json --data Data
```

생성기는 아래도 함께 갱신합니다:
- `Data/SKSE/Plugins/CalamityAffixes/affixes.json` (SKSE 런타임 설정)
- `Data/SKSE/Plugins/CalamityAffixes/runtime_contract.json` (Generator↔SKSE 트리거/액션 + 런워드 카탈로그/룬 가중치 계약 스냅샷)
- `Data/SKSE/Plugins/InventoryInjector/CalamityAffixes.json` (InventoryInjector placeholder: `rules: []`)
- `Data/CalamityAffixes.esp` (키워드 + **옵션: records 기반 Spell/MGEF 자동 생성** + MCM Quest + `CalamityAffixes_MCMConfig` 바인딩)

런워드 계약 데이터 SSOT:
- `affixes/runeword.contract.json` (런워드 카탈로그 + 룬 가중치)
- Generator는 이 파일을 우선 로드하고, 결과를 `runtime_contract.json`으로 스냅샷 배포합니다.

런타임에서 생성/유지되는 사용자 설정 파일:
- `Data/SKSE/Plugins/CalamityAffixes/user_settings.json` (MCM 런타임 값 + Prisma 패널 단축키/언어)
  - 저장 방식: 임시 파일 기록 + flush + 원자적 교체(중간 손상 위험 완화)

### 드랍 경로 정책

- 현재 정책 메모: `docs/references/2026-07-14-corpse-runtime-currency-policy.md`
- `loot.currencyDropMode=hybrid`는 기존 설정 파일을 깨지 않기 위한 레거시 토큰일 뿐이며, 실제 통화 판정 권한은 **플레이어 또는 player-owned summon/proxy의 적대 처치를 받는 SKSE death event**에만 있습니다.
- 성공한 룬 조각/재련 오브는 사망한 대상의 인벤토리에 직접 추가됩니다. `PlaceAtMe` 월드 생성, 플레이어 인벤토리 직행, 일반 컨테이너 활성화, 픽업 롤과 새 SPID 분배는 사용하지 않습니다.
- 피해자가 팔로워/동료, 소환·지휘 액터, 아동, player-owned/비적대 대상이면 제외하며, 환경 오브젝트와 player-owned가 아닌 독립 NPC/팔로워의 처치도 제외합니다.
- 일반 적은 룬 조각 `8%`와 재련 오브 `12%` 독립 판정을 사용합니다.
- Actor Base가 `Unique`인 고유·네임드 적은 두 통화 중 1개를 확정 지급합니다. 기본 선택 비율은 룬 조각 `40%`, 재련 오브 `60%`입니다.
- `LocRefTypeBoss`가 지정된 보스와 `ActorTypeDragon` 드래곤은 룬 조각 1개와 재련 오브 1개를 각각 확정 지급하며, 보스·드래곤 판정이 `Unique` 판정보다 우선합니다.
- 고유·보스 확정 보상은 일반 확률 판정을 추가로 실행하거나 일반 피티를 증가·초기화하지 않습니다.
- 시체 FormID·날짜·통화 카테고리 mask와 룬/오브 피티를 새 `CCRT` 코세이브 레코드에 저장해 재로드 후 중복 롤과 피티 초기화를 막습니다.
- 기존 SPID 분배가 남아 있는 전환 시체는 보유 통화/레벨드 리스트를 카테고리별로 확인하고 해당 카테고리만 건너뜁니다.
- 기존 `IAXF` 등 직렬화 레코드, 어픽스 ID, 장비, 보유 룬 조각과 완성 룬워드는 그대로 호환됩니다. `CCRT`는 추가 레코드이므로 새 게임이 필요하지 않습니다.
- `Data/CalamityAffixes_DISTR.ini`는 빈 호환 산출물이며 레벨리스트(LVLI) 주입/오버라이드, `DeathItem`, `Perk + AddLeveledListOnDeath`는 사용하지 않습니다.

### MO2 배포 ZIP 생성

```bash
tools/build_mo2_zip.sh
```

위 스크립트는 ZIP 생성 전에 자동으로:
1) 생성기 실행(esp/json/ini 갱신)
2) Papyrus 컴파일(`Data/Scripts/*.pex`)
를 수행합니다.

상세 CK 체크리스트: `docs/design/CK_MVP_셋업_체크리스트.md`

자동화(B 방식) 워크플로우: `docs/design/데이터주도_생성기_워크플로우.md`

</details>
