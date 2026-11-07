# 프리픽스 효과 정리 (공개용)

> 업데이트: 2026-11-07
> 기준 버전: `v2.3.2`
> 기준 코드:
> - 효과 정의: `affixes/modules/keywords.affixes.core.json`, `affixes/modules/keywords.affixes.status.json`
> - 변환 스크립트: `tools/transform_prefixes.py`

- 총 공개 프리픽스: **73개**
- INTERNAL 항목은 공개 문서에서 숨김
- 루트 풀에서 빠진 접두 3개는 개수에서 빼고 문서 끝에 따로 적음
- 접두는 무기·방어구 공용 풀에서 뽑힘: 항목 옆 `[Weapon]`/`[Armor]`는 원래 의도한 장비 테마일 뿐, 모든 접두가 어느 장비에나 붙을 수 있음
- 스카이림 로어 기반 전면 리네이밍 (v1.2.21)
- 9개 freed slot → Thu'um 5종 + 마법학파 4종 신규 효과
- 각 효과 설명은 인게임 표시 문자열(`nameKo`/`nameEn`)을 그대로 사용
- 티어형 프리픽스는 같은 패밀리가 한 아이템에 중복되지 않으며, 각 `runtime.lootWeight`(없으면 `kid.chance`)를 실제 획득 가중치로 사용
- 같은 원소의 50%/100% 피해 주입도 하나의 배타적 패밀리로 취급

## 발동과 중복 장착 규칙 (RC6 반영)

- 곰 덫은 일반 무기 적중에서도 기본 40%, 치명 시전은 일반 근접 적중에서도 주문별 기본 35% 또는 45%로 판정. 개별 항목의 확률·ICD를 함께 확인
- 치명타·강공격에는 기존 판정만 사용하며 평타 확률을 추가로 이중 판정하지 않음. 주문·배시·폭발은 평타 발동 대상에서 제외
- 서로 다른 장비에 장착한 동일 표준 발동 어픽스는 장비별 확률 페널티를 반영한 강한 사본부터 최대 3개를 합산. 일반 주문 발동·적응형 원소 주문·덫에 적용하며, 치명 시전·피해 전환 등 별도 처리 효과에 일괄 적용하지 않음
- 합산 확률은 `1 - (1-p1)×(1-p2)×(1-p3)` 방식. 각 사본이 40%이고 추가 보정이 없으면 1/2/3개 장착 시 40% → 64% → 78.4%. 동일 효과의 피해량을 2배·3배로 만들거나 여러 번 실행하는 것은 아니며 발동 예산·ICD도 한 번만 소비
- 치명 시전은 한 번의 공격(치명타·강공격·일반 근접·활·석궁)에 주문 하나만 시전. 여러 개를 착용하면 차례로 돌아가며 발동하고, 발동 묶음당 전역 ICD 0.15초를 공유함
- 표기된 확률은 기본값이며 장착 구성·런타임 보정·발동 조건·ICD에 따라 실제 발동 빈도가 달라짐. 접미 슬롯 자체는 표준 발동의 다중 어픽스 확률 페널티를 늘리지 않음
- 접미의 티어 합산은 별도 규칙: [서픽스 효과 정리](SUFFIX_EFFECTS.md) 참조
- 같은 종류의 자기 버프는 겹쳐 쌓이지 않고 더 강한 쪽 하나만 남음. 새 버프가 같거나 더 강하면 옛것을 대체하고, 이미 더 강한 버프가 있으면 새 버프의 그 능력치 부분은 걸리지 않음. 모든 능력치에서 지고 즉시 회복 같은 다른 효과도 없으면 발동을 건너뛰어 확률·재사용 대기도 쓰지 않음. 상시 효과와 다른 모드 버프는 건드리지 않음

## 공용 상태 (v2.3.0)

상태를 거는 어픽스는 설명 끝에 상태 이름을 적어 둠. 다른 장비에서 건 상태끼리도 함께 작동함

- **빙결·출혈·감전 축적**: 발동할 때마다 대상의 미터가 참. 100이 되면 미터가 비면서 효과가 터짐. 보스·드래곤·고유 적은 150에서 시작하고 터질 때마다 50씩 올라감(최대 300). 마지막으로 쌓인 뒤 3초가 지나면 초당 15씩 줄어듦
  - **빙결**: 3초 동안 이동속도 -80%, 공격력 -30%
  - **출혈**: 대상 최대 체력의 8% 피해(강한 적은 3%, 최소 10·최대 400)
  - **감전**: 대상과 반경 300 안의 적 최대 2명에게 번개 피해 40
- **화상**: 1중첩당 초당 화염 피해 4, 최대 5중첩. 새 중첩이 붙으면 4초 지속이 다시 시작됨
- **노출**: 저항·방어도 감소(미스트는 마나 재생도 낮춤). 같은 능력치에는 가장 강한 칼라미티 노출 하나만 적용
- **파멸**: 표식 1.5초 뒤 폭발. 다시 걸면 시간만 갱신하고 더 큰 폭발을 유지
- 상태는 **약점 포착**(서로 다른 상태 2개 이상)과 **전염**(처치 시 주변으로 옮김)에 쓰임

### 카테고리별 수량

| 카테고리 | 수 |
|----------|---|
| 원소 타격 | 1 |
| 원소 취약 | 3 |
| 소환 | 7 |
| 피격 방어 | 6 |
| 두루마리 숙련 | 4 |
| 화염 DoT | 2 |
| CC / 디버프 | 3 |
| 유틸리티 | 4 |
| 덫 / 룬 | 3 |
| 원소 주입 (전환) | 6 |
| 대마법사 | 4 |
| 치명 시전 (Crit Cast) | 7 |
| 시체 소각 (죽음의 화장) | 3 |
| 소환 화장 | 2 |
| 역병 / 적응 | 4 |
| 성장형 | 2 |
| Thu'um (외침) — 신규 | 5 |
| 마법학파 — 신규 | 4 |
| 공용 상태 (v2.3.0) | 3 |
| **합계** | **73** |

## 원소 타격

- **`storm_call`** [Weapon]
  - 한글 표시: 폭풍 소환: 38% 확률로 번개 피해 10 + 적중의 10%. 1.5초마다 발동. 감전 축적 +30.
  - 영문 표시: Storm Call (Lucky Hit 38% / ICD 1.5s): 10 Lightning Damage + 10% of Hit Damage; Shock build-up +30
  - 대표 스펠: `CAFF_SPEL_ARC_LIGHTNING`

## 원소 취약

- **`flame_weakness`** [Weapon]
  - 한글 표시: 화염 취약: 36% 확률로 화염 저항 -15 (4초). 2.8초마다 발동. 노출.
  - 영문 표시: Flame Weakness (Lucky Hit 36% / ICD 2.8s): Fire Resist -15 (4s); Exposed
  - 대표 스펠: `CAFF_SPEL_FIRE_SHRED`

- **`frost_weakness`** [Weapon]
  - 한글 표시: 냉기 취약: 최근 2초 내 적중 유지 시 24% 확률로 냉기 저항 -15 (4초). 2.4초마다 발동. 노출.
  - 영문 표시: Frost Weakness (24% on hit while chaining hits in the last 2s / ICD 2.4s): Frost Resist -15 (4s); Exposed
  - 대표 스펠: `CAFF_SPEL_FROST_SHRED`

- **`shock_weakness`** [Weapon]
  - 한글 표시: 전격 취약: 35% 확률로 번개 저항 -25 (4초). 4초마다 발동. 노출.
  - 영문 표시: Shock Weakness (Lucky Hit 35% / ICD 4s): Shock Resist -25 (4s); Exposed
  - 대표 스펠: `CAFF_SPEL_SHOCK_SHRED`

## 소환

- **`wolf_spirit`** [Weapon]
  - 한글 표시: 늑대 혼령: 적중 시 30% 확률로 패밀리어 소환 (60초). 24초마다 발동. 직접 부른 소환수나 되살린 시체가 있으면 발동하지 않음(앞서 발동한 소환수는 교체).
  - 영문 표시: Wolf Spirit (30% on hit / ICD 24s): Summon Familiar (60s); skipped while your own summon or thrall is out (replaces an earlier proc summon)

- **`flame_sprite`** [Weapon]
  - 한글 표시: 화염 정령: 36% 확률로 폭발하는 불꽃 패밀리어 소환. 14초마다 발동. 직접 부른 소환수나 되살린 시체가 있으면 발동하지 않음(앞서 발동한 소환수는 교체).
  - 영문 표시: Flame Sprite (Lucky Hit 36% / ICD 14s): Summon Exploding Flame Familiar; skipped while your own summon or thrall is out (replaces an earlier proc summon)

- **`flame_atronach`** [Weapon]
  - 한글 표시: 화염 아트로나크: 30% 확률로 화염 아트로낙 소환 (60초). 32초마다 발동. 직접 부른 소환수나 되살린 시체가 있으면 발동하지 않음(앞서 발동한 소환수는 교체).
  - 영문 표시: Flame Atronach (Lucky Hit 30% / ICD 32s): Summon Flame Atronach (60s); skipped while your own summon or thrall is out (replaces an earlier proc summon)

- **`frost_atronach`** [Weapon]
  - 한글 표시: 냉기 아트로나크: 30% 확률로 냉기 아트로낙 소환 (60초). 32초마다 발동. 직접 부른 소환수나 되살린 시체가 있으면 발동하지 않음(앞서 발동한 소환수는 교체).
  - 영문 표시: Frost Atronach (Lucky Hit 30% / ICD 32s): Summon Frost Atronach (60s); skipped while your own summon or thrall is out (replaces an earlier proc summon)

- **`storm_atronach`** [Weapon]
  - 한글 표시: 폭풍 아트로나크: 26% 확률로 폭풍 아트로낙 소환 (60초). 40초마다 발동. 직접 부른 소환수나 되살린 시체가 있으면 발동하지 않음(앞서 발동한 소환수는 교체).
  - 영문 표시: Storm Atronach (Lucky Hit 26% / ICD 40s): Summon Storm Atronach (60s); skipped while your own summon or thrall is out (replaces an earlier proc summon)

- **`dremora_pact`** [Weapon]
  - 한글 표시: 드레모라 서약: 22% 확률로 드레모라 군주 소환 (60초). 52초마다 발동. 직접 부른 소환수나 되살린 시체가 있으면 발동하지 않음(앞서 발동한 소환수는 교체).
  - 영문 표시: Dremora Pact (Lucky Hit 22% / ICD 52s): Summon Dremora Lord (60s); skipped while your own summon or thrall is out (replaces an earlier proc summon)

- **`soul_snare`** [Weapon]
  - 한글 표시: 영혼 수확: 30% 확률로 마력·기력 각 8 + 물리 피해의 3% 회복 (최대 24). 1초마다 발동.
  - 영문 표시: Soul Harvest (Lucky Hit 30% / ICD 1s): Restore Magicka & Stamina 8 + 3% of Physical Damage (max 24)
  - 대표 스펠: `CAFF_SPEL_SOUL_EXPLOIT`

## 피격 방어

- **`shadow_veil`** [Weapon]
  - 한글 표시: 그림자 반격: 피격 시 투명화 3초 + 공격력 25% 증가 6초. 30초마다 발동.
  - 영문 표시: Shadow Counter (on hit taken / ICD 30s): Invisibility 3s + Attack Damage +25% 6s
  - 대표 스펠: `CAFF_SPEL_INVISIBILITY`

- **`stone_ward`** [Armor]
  - 한글 표시: 돌의 수호: 피격 시 10% 확률로 피해 저항 +120 (3초). 6초마다 발동.
  - 영문 표시: Stone Ward (10% on hit taken / ICD 6s): Damage Resist +120 (3s)
  - 대표 스펠: `CAFF_SPEL_INCOMING_WARDEN_SHELL`

- **`arcane_ward`** [Armor]
  - 한글 표시: 마법 방벽: 피격 시 18% 확률로 마법 저항 +25% (4초). 8초마다 발동.
  - 영문 표시: Arcane Ward (18% on hit taken / ICD 8s): Magic Resist +25% (4s)
  - 대표 스펠: `CAFF_SPEL_INCOMING_RESOLVE_BARRIER`

- **`healing_surge`** [Armor]
  - 한글 표시: 치유의 파도: 피격 시 20% 확률로 체력 재생 +140% (5초). 9초마다 발동.
  - 영문 표시: Healing Surge (20% on hit taken / ICD 9s): Health Regen +140% (5s)
  - 대표 스펠: `CAFF_SPEL_INCOMING_VITAL_REBOUND`

- **`mage_armor_t1`** [Armor]
  - 한글 표시: 마법사의 갑옷 I: 물리 피해 10%를 매지카로 전환 (타격당 최대 35)
  - 영문 표시: Mage Armor I: Redirect 10% of physical hit damage to Magicka (max 35 per hit)

- **`mage_armor_t2`** [Armor]
  - 한글 표시: 마법사의 갑옷 II: 물리 피해 15%를 매지카로 전환 (타격당 최대 50)
  - 영문 표시: Mage Armor II: Redirect 15% of physical hit damage to Magicka (max 50 per hit)

## 두루마리 숙련

- **`scroll_mastery_t1`** [Armor]
  - 한글 표시: 두루마리 숙련 I: 스크롤 비소모 +20%
  - 영문 표시: Scroll Mastery I: +20% chance to not consume scrolls

- **`scroll_mastery_t2`** [Armor]
  - 한글 표시: 두루마리 숙련 II: 스크롤 비소모 +25%
  - 영문 표시: Scroll Mastery II: +25% chance to not consume scrolls

- **`scroll_mastery_t3`** [Armor]
  - 한글 표시: 두루마리 숙련 III: 스크롤 비소모 +30%
  - 영문 표시: Scroll Mastery III: +30% chance to not consume scrolls

- **`scroll_mastery_t4`** [Armor]
  - 한글 표시: 두루마리 숙련 IV: 스크롤 비소모 +35%
  - 영문 표시: Scroll Mastery IV: +35% chance to not consume scrolls

## 화염 DoT

- **`ember_brand`** [Weapon]
  - 한글 표시: 잔불 낙인: 적중 시 40% 확률로 점화 피해 4 + 적중의 4% (4초). 0.8초마다 발동. 화상 1중첩.
  - 영문 표시: Ember Brand (40% on hit / ICD 0.8s): Ignite 4 + 4% of Hit Damage (4s); Burning +1 stack
  - 대표 스펠: `CAFF_SPEL_HIT_EMBERBRAND_IGNITE`

- **`ember_pyre`** [Weapon]
  - 한글 표시: 잔불 장작: 처치 시 18% 확률로 시체 폭발 (반경 320, 최대체력 8% + 14 화염). 1초마다 발동.
  - 영문 표시: Ember Pyre (18% on kill / ICD 1s): Corpse explosion (320 radius, 8% max health + 14 fire)
  - 대표 스펠: `CAFF_SPEL_DMG_FIRE_DYNAMIC`

## CC / 디버프

- **`ice_shackle`** [Weapon]
  - 한글 표시: 얼음 족쇄: 적중 시 24% 확률로 냉기 감속 -35% (2초). 2.5초마다 발동. 빙결 축적 +50.
  - 영문 표시: Ice Shackle (24% on hit / ICD 2.5s): Frost snare -35% (2s); Freeze build-up +50
  - 대표 스펠: `CAFF_SPEL_HIT_FROSTLOCK_SNARE`

- **`mana_burn`** [Weapon]
  - 한글 표시: 마나 소진: 35% 확률로 매지카 피해 60. 1.2초마다 발동.
  - 영문 표시: Mana Burn (Lucky Hit 35% / ICD 1.2s): Drain 60 Magicka
  - 대표 스펠: `CAFF_SPEL_HIT_SHOCK_OVERLOAD`

- **`life_drain`** [Weapon]
  - 한글 표시: 생명 흡수: 38% 확률로 체력 회복 4 + 물리 피해의 4.5% (최대 24). 0.7초마다 발동.
  - 영문 표시: Life Drain (Lucky Hit 38% / ICD 0.7s): Heal 4 + 4.5% of physical hit damage (max 24)
  - 대표 스펠: `CAFF_SPEL_HIT_BLOOD_SIPHON`

## 유틸리티

- **`soul_siphon`** [Weapon]
  - 한글 표시: 영혼 착취: 처치 시 30% 확률로 마나 재생 +35% (8초). 2초마다 발동.
  - 영문 표시: Soul Siphon (30% on kill / ICD 2s): Magicka Regen +35% (8s)
  - 대표 스펠: `CAFF_SPEL_KILL_SOUL_SURGE`

- **`shadow_stride`** [Weapon]
  - 한글 표시: 그림자 질주: 처치 시 공격력 +10% (4초), 발동 10/30회 후 +12%/+14%. 6초마다 발동.
  - 영문 표시: Shadow Stride (on kill / ICD 6s): Attack Damage +10% (4s), evolves to +12%/+14% after 10/30 procs
  - 대표 스펠: `CAFF_SPEL_SWAP_JACKPOT_HASTE`

- **`silent_step`** [Weapon]
  - 한글 표시: 은신 습격: 처치 시 투명화 3초 + 이동속도 30% 증가 6초. 8초마다 발동.
  - 영문 표시: Stealth Assault (on kill / ICD 8s): Invisibility 3s + Move Speed +30% 6s
  - 대표 스펠: `CAFF_SPEL_STEALTH_ASSAULT`

- **`battle_frenzy`** [Weapon]
  - 한글 표시: 전투 광란: 적중 시 20% 확률로 공격력 +10% (4초). 0.6초마다 발동. 같은 대상 12초.
  - 영문 표시: Battle Frenzy (20% on hit / ICD 0.6s / per-target ICD 12s): Attack Damage +10% (4s)
  - 대표 스펠: `CAFF_SPEL_SWAP_JACKPOT_HASTE`

## 덫 / 룬

- **`bear_trap`** [Weapon]
  - 한글 표시: 곰 덫: 치명타/강공 시 20%, 일반 무기 공격 시 40% 확률로 적 발밑에 덫 설치(재장전 2.5초) → 밟은 적 이동속도 -100% (2초) + 랜덤 추가 효과. 6초마다 발동.
  - 영문 표시: Bear Trap (20% crit/power attack; 40% normal weapon hit / ICD 6s): Deploy trap at enemy feet (2.5s reload) -> Snare -100% Move Speed (2s) + Random bonus effect
  - 대표 스펠: `CAFF_SPEL_TRAP_IRONJAW_SNARE`

- **`rune_trap`** [Weapon]
  - 한글 표시: 룬 함정: 적중 시 12% 확률로 대상 위치에 지뢰 설치(0.6초 후, 8초, 1회) → 반경 150 내 적 최대 2명 둔화(-40%, 2초). 1.5초마다 발동.
  - 영문 표시: Rune Trap (12% on hit / ICD 1.5s): Deploy mine at target (0.6s delay, 8s, 1 charge) -> Slow up to 2 enemies within radius 150 (-40%, 2s)
  - 대표 스펠: `CAFF_SPEL_MINEFIELD_SNARE`

- **`chaos_rune`** [Weapon]
  - 한글 표시: 혼돈의 룬: 적중 시 12% 확률로 대상 위치에 지뢰 설치(0.8초 후, 10초, 1회) → 반경 150 내 적 최대 2명 둔화 + 무작위 저주. 2초마다 발동. 같은 대상 8초.
  - 영문 표시: Chaos Rune (12% on hit / ICD 2s / per-target ICD 8s): Deploy mine at target (0.8s delay, 10s, 1 charge) -> Slow and curse up to 2 enemies within radius 150
  - 대표 스펠: `CAFF_SPEL_CHAOS_MINE_SNARE`

## 원소 주입 (전환)

- **`fire_infusion_50`** [Weapon]
  - 한글 표시: 화염 주입: 물리 피해 50%를 화염으로 전환
  - 영문 표시: Fire Infusion: Convert 50% Physical to Fire
  - 대표 스펠: `CAFF_SPEL_DMG_FIRE_DYNAMIC`

- **`fire_infusion_100`** [Weapon]
  - 한글 표시: 화염 주입: 물리 피해 100%를 화염으로 전환
  - 영문 표시: Fire Infusion: Convert 100% Physical to Fire
  - 대표 스펠: `CAFF_SPEL_DMG_FIRE_DYNAMIC`

- **`frost_infusion_50`** [Weapon]
  - 한글 표시: 냉기 주입: 물리 피해 50%를 냉기로 전환
  - 영문 표시: Frost Infusion: Convert 50% Physical to Frost
  - 대표 스펠: `CAFF_SPEL_DMG_FROST_DYNAMIC`

- **`frost_infusion_100`** [Weapon]
  - 한글 표시: 냉기 주입: 물리 피해 100%를 냉기로 전환
  - 영문 표시: Frost Infusion: Convert 100% Physical to Frost
  - 대표 스펠: `CAFF_SPEL_DMG_FROST_DYNAMIC`

- **`shock_infusion_50`** [Weapon]
  - 한글 표시: 전격 주입: 물리 피해 50%를 전격으로 전환
  - 영문 표시: Shock Infusion: Convert 50% Physical to Shock
  - 대표 스펠: `CAFF_SPEL_DMG_SHOCK_DYNAMIC`

- **`shock_infusion_100`** [Weapon]
  - 한글 표시: 전격 주입: 물리 피해 100%를 전격으로 전환
  - 영문 표시: Shock Infusion: Convert 100% Physical to Shock
  - 대표 스펠: `CAFF_SPEL_DMG_SHOCK_DYNAMIC`

## 대마법사

- **`archmage_t1`** [Armor]
  - 한글 표시: 대마법사: 주문 적중 시 최대 매지카의 10%를 번개 피해로 가하고, 5%를 추가 소모한다.
  - 영문 표시: Archmage (on spell hit): 10% Max Magicka as Lightning Damage / 5% Max Magicka extra cost
  - 대표 스펠: `CAFF_SPEL_DMG_SHOCK_DYNAMIC`

- **`archmage_t2`** [Armor]
  - 한글 표시: 대마법사: 주문 적중 시 최대 매지카의 12%를 번개 피해로 가하고, 5%를 추가 소모한다.
  - 영문 표시: Archmage (on spell hit): 12% Max Magicka as Lightning Damage / 5% Max Magicka extra cost
  - 대표 스펠: `CAFF_SPEL_DMG_SHOCK_DYNAMIC`

- **`archmage_t3`** [Armor]
  - 한글 표시: 대마법사: 주문 적중 시 최대 매지카의 14%를 번개 피해로 가하고, 5%를 추가 소모한다.
  - 영문 표시: Archmage (on spell hit): 14% Max Magicka as Lightning Damage / 5% Max Magicka extra cost
  - 대표 스펠: `CAFF_SPEL_DMG_SHOCK_DYNAMIC`

- **`archmage_t4`** [Armor]
  - 한글 표시: 대마법사: 주문 적중 시 최대 매지카의 16%를 번개 피해로 가하고, 5%를 추가 소모한다.
  - 영문 표시: Archmage (on spell hit): 16% Max Magicka as Lightning Damage / 5% Max Magicka extra cost
  - 대표 스펠: `CAFF_SPEL_DMG_SHOCK_DYNAMIC`

## 치명 시전 (Crit Cast)

- **`crit_cast_firebolt`** [Armor]
  - 한글 표시: 치명 시전: 파이어볼트: 근접 치명타/강공 시 100%, 근접 일반 공격 시 45%, 활·석궁은 일반 적중 시에도 100% 확률로 파이어볼트 시전. 피해는 주문 기본 피해와 물리 적중 피해의 30% 중 큰 값. 0.15초마다 발동.
  - 영문 표시: Crit Cast: Firebolt (100% Melee Crit/Power Attack; 45% Normal Melee Hit; any Bow/Crossbow Hit (100%); ICD 0.15s): Cast Firebolt; damage is the greater of the spell's base damage and 30% of physical hit damage.
  - 대표 스펠: `CAFF_SPEL_COC_FIREBOLT_HOSTILE_ONLY`

- **`crit_cast_ice_spike`** [Armor]
  - 한글 표시: 치명 시전: 아이스 스파이크: 근접 치명타/강공 시 100%, 근접 일반 공격 시 45%, 활·석궁은 일반 적중 시에도 100% 확률로 아이스 스파이크 시전. 피해는 주문 기본 피해와 물리 적중 피해의 30% 중 큰 값. 0.15초마다 발동.
  - 영문 표시: Crit Cast: Ice Spike (100% Melee Crit/Power Attack; 45% Normal Melee Hit; any Bow/Crossbow Hit (100%); ICD 0.15s): Cast Ice Spike; damage is the greater of the spell's base damage and 30% of physical hit damage.
  - 대표 스펠: `CAFF_SPEL_COC_ICE_SPIKE_HOSTILE_ONLY`

- **`crit_cast_lightning_bolt`** [Armor]
  - 한글 표시: 치명 시전: 라이트닝 볼트: 근접 치명타/강공 시 100%, 근접 일반 공격 시 45%, 활·석궁은 일반 적중 시에도 100% 확률로 라이트닝 볼트 시전. 피해는 주문 기본 피해와 물리 적중 피해의 30% 중 큰 값. 0.15초마다 발동.
  - 영문 표시: Crit Cast: Lightning Bolt (100% Melee Crit/Power Attack; 45% Normal Melee Hit; any Bow/Crossbow Hit (100%); ICD 0.15s): Cast Lightning Bolt; damage is the greater of the spell's base damage and 30% of physical hit damage.
  - 대표 스펠: `CAFF_SPEL_COC_LIGHTNING_BOLT_HOSTILE_ONLY`

- **`crit_cast_thunderbolt`** [Armor]
  - 한글 표시: 치명 시전: 썬더볼트: 근접 치명타/강공 시 100%, 근접 일반 공격 시 35%, 활·석궁은 일반 적중 시에도 100% 확률로 썬더볼트 시전. 피해는 주문 기본 피해와 물리 적중 피해의 30% 중 큰 값. 0.15초마다 발동.
  - 영문 표시: Crit Cast: Thunderbolt (100% Melee Crit/Power Attack; 35% Normal Melee Hit; any Bow/Crossbow Hit (100%); ICD 0.15s): Cast Thunderbolt; damage is the greater of the spell's base damage and 30% of physical hit damage.
  - 대표 스펠: `CAFF_SPEL_COC_THUNDERBOLT_HOSTILE_ONLY`

- **`crit_cast_icy_spear`** [Armor]
  - 한글 표시: 치명 시전: 아이시 스피어: 근접 치명타/강공 시 100%, 근접 일반 공격 시 35%, 활·석궁은 일반 적중 시에도 100% 확률로 아이시 스피어 시전. 피해는 주문 기본 피해와 물리 적중 피해의 30% 중 큰 값. 0.15초마다 발동.
  - 영문 표시: Crit Cast: Icy Spear (100% Melee Crit/Power Attack; 35% Normal Melee Hit; any Bow/Crossbow Hit (100%); ICD 0.15s): Cast Icy Spear; damage is the greater of the spell's base damage and 30% of physical hit damage.
  - 대표 스펠: `CAFF_SPEL_COC_ICY_SPEAR_HOSTILE_ONLY`

- **`crit_cast_chain_lightning`** [Armor]
  - 한글 표시: 치명 시전: 체인 라이트닝: 근접 치명타/강공 시 100%, 근접 일반 공격 시 35%, 활·석궁은 일반 적중 시에도 100% 확률로 체인 라이트닝 시전. 피해는 주문 기본 피해와 물리 적중 피해의 30% 중 큰 값. 0.15초마다 발동.
  - 영문 표시: Crit Cast: Chain Lightning (100% Melee Crit/Power Attack; 35% Normal Melee Hit; any Bow/Crossbow Hit (100%); ICD 0.15s): Cast Chain Lightning; damage is the greater of the spell's base damage and 30% of physical hit damage.
  - 대표 스펠: `CAFF_SPEL_COC_CHAIN_LIGHTNING_HOSTILE_ONLY`

- **`crit_cast_ice_storm`** [Armor]
  - 한글 표시: 치명 시전: 아이스 스톰: 근접 치명타/강공 시 100%, 근접 일반 공격 시 35%, 활·석궁은 일반 적중 시에도 100% 확률로 아이스 스톰 시전. 피해는 주문 기본 피해와 물리 적중 피해의 30% 중 큰 값. 0.15초마다 발동.
  - 영문 표시: Crit Cast: Ice Storm (100% Melee Crit/Power Attack; 35% Normal Melee Hit; any Bow/Crossbow Hit (100%); ICD 0.15s): Cast Ice Storm; damage is the greater of the spell's base damage and 30% of physical hit damage.
  - 대표 스펠: `CAFF_SPEL_COC_ICE_STORM_HOSTILE_ONLY`

## 시체 소각 (죽음의 화장)

- **`death_pyre_t1`** [Weapon]
  - 한글 표시: 죽음의 화장 T1: 처치 시 반경 600, (12 + 시체 최대체력 6%) 화염 피해. 0.9초 안에 이어진 폭발은 위력 30%씩 감소, 최대 5연속, 초당 4회 제한.
  - 영문 표시: Death Pyre T1 (on kill): Radius 600, (12 + 6% corpse max HP) Fire Damage. Successive explosions within 0.9s deal 0.7x each (max 5 in a row), 4/sec limit
  - 대표 스펠: `CAFF_SPEL_DMG_FIRE_DYNAMIC`

- **`death_pyre_t2`** [Weapon]
  - 한글 표시: 죽음의 화장 T2: 처치 시 반경 600, (14 + 시체 최대체력 7%) 화염 피해. 0.9초 안에 이어진 폭발은 위력 30%씩 감소, 최대 5연속, 초당 4회 제한.
  - 영문 표시: Death Pyre T2 (on kill): Radius 600, (14 + 7% corpse max HP) Fire Damage. Successive explosions within 0.9s deal 0.7x each (max 5 in a row), 4/sec limit
  - 대표 스펠: `CAFF_SPEL_DMG_FIRE_DYNAMIC`

- **`death_pyre_t3`** [Weapon]
  - 한글 표시: 죽음의 화장 T3: 처치 시 반경 600, (16 + 시체 최대체력 8%) 화염 피해. 0.9초 안에 이어진 폭발은 위력 30%씩 감소, 최대 5연속, 초당 4회 제한.
  - 영문 표시: Death Pyre T3 (on kill): Radius 600, (16 + 8% corpse max HP) Fire Damage. Successive explosions within 0.9s deal 0.7x each (max 5 in a row), 4/sec limit
  - 대표 스펠: `CAFF_SPEL_DMG_FIRE_DYNAMIC`

## 소환 화장

- **`conjured_pyre_t1`** [Armor]
  - 한글 표시: 소환 화장 T1: 소환물 사망 시 반경 600, (14 + 소환물 최대체력 7%) 화염 피해. 0.9초 안에 이어진 폭발은 위력 20%씩 감소, 최대 3연속, 초당 3회 제한.
  - 영문 표시: Conjured Pyre T1 (on summon death): Radius 600, (14 + 7% summon max HP) Fire Damage. Successive explosions within 0.9s deal 0.8x each (max 3 in a row), 3/sec limit
  - 대표 스펠: `CAFF_SPEL_DMG_FIRE_DYNAMIC`

- **`conjured_pyre_t2`** [Armor]
  - 한글 표시: 소환 화장 T2: 소환물 사망 시 반경 600, (18 + 소환물 최대체력 8.5%) 화염 피해. 0.9초 안에 이어진 폭발은 위력 20%씩 감소, 최대 3연속, 초당 3회 제한.
  - 영문 표시: Conjured Pyre T2 (on summon death): Radius 600, (18 + 8.5% summon max HP) Fire Damage. Successive explosions within 0.9s deal 0.8x each (max 3 in a row), 3/sec limit
  - 대표 스펠: `CAFF_SPEL_DMG_FIRE_DYNAMIC`

## 역병 / 적응

- **`elemental_bane`** [Weapon]
  - 한글 표시: 원소 저주: 적중 시 20% 확률로 가장 높은 원소 저항 -15 (4초). 1초마다 발동. 같은 대상 12초. 노출.
  - 영문 표시: Elemental Bane (20% on hit / ICD 1s / per-target ICD 12s): Highest elemental resist -15 (4s); Exposed

- **`plague_spore`** [Weapon]
  - 한글 표시: 역병 포자: 적중 시 20% 확률로 0.6초 후 포자 폭발(반경 150 내 적 최대 2명, 독 피해 3/s, 4초). 1.5초마다 발동.
  - 영문 표시: Plague Spore (20% on hit / ICD 1.5s): Spore burst after 0.6s (Up to 2 enemies within radius 150, Poison 3/s, 4s)
  - 대표 스펠: `CAFF_SPEL_DOT_BLOOM_POISON`

- **`tar_blight`** [Weapon]
  - 한글 표시: 역청 황폐: 적중 시 18% 확률로 0.6초 후 타르 폭발(반경 170 내 적 최대 2명, 둔화 -35% + 방어 -150, 4초). 2초마다 발동.
  - 영문 표시: Tar Blight (18% on hit / ICD 2s): Tar burst after 0.6s (Up to 2 enemies within radius 170, Slow -35% + Armor -150, 4s)
  - 대표 스펠: `CAFF_SPEL_DOT_BLOOM_TAR_SLOW`

- **`siphon_spore`** [Weapon]
  - 한글 표시: 흡수 포자: 적중 시 18% 확률로 0.6초 후 흡수 폭발(반경 170 내 적 최대 2명, 매지카/스태미나 재생 -60%, 6초). 2초마다 발동.
  - 영문 표시: Siphon Spore (18% on hit / ICD 2s): Siphon burst after 0.6s (Up to 2 enemies within radius 170, Magicka/Stamina Regen -60%, 6s)
  - 대표 스펠: `CAFF_SPEL_DOT_BLOOM_SIPHON_MAG`

## 성장형

- **`thunder_mastery`** [Weapon]
  - 한글 표시: 뇌격 숙련: 매 적중 시 번개 피해 누적 성장 (×1.0→1.25→1.6→2.1, 단계: 15/45/90회). 0.2초마다 발동. 감전 축적 +15.
  - 영문 표시: Thunder Mastery (100% on hit / ICD 0.2s): Lightning Damage Growth (x1.0→1.25→1.6→2.1, stages: 15/45/90 hits); Shock build-up +15
  - 대표 스펠: `CAFF_SPEL_SHOCK_DAMAGE`

- **`elemental_attunement`** [Weapon]
  - 한글 표시: 원소 조율: 매 적중 시 화염/냉기/번개 수동 전환 + 성장 (×1.0→1.3→1.7→2.2, 단계: 20/60/120회). 0.2초마다 발동.
  - 영문 표시: Elemental Attunement (100% on hit / ICD 0.2s): Fire/Frost/Shock manual switch + Growth (x1.0→1.3→1.7→2.2, stages: 20/60/120 hits)
  - 대표 스펠: `CAFF_SPEL_FIRE_DAMAGE`

## Thu'um (외침) — 신규

> v1.2.21 신규 추가 — freed slot 활용

- **`voice_of_power`** [Armor]
  - 한글 표시: 힘의 외침: 피격 시 25% 확률로 기력 30 즉시 회복 및 공격력 +15% (5초). 8초마다 발동.
  - 영문 표시: Voice of Power (25% on hit taken / ICD 8s): Restore 30 Stamina and gain +15% Attack Damage for 5s
  - 대표 스펠: `CAFF_SPEL_INCOMING_VOICE_POWER`

- **`death_mark`** [Weapon]
  - 한글 표시: 죽음의 표식: 25% 확률로 방어력 -200 (6초). 1초마다 발동. 같은 대상 10초. 노출.
  - 영문 표시: Death's Mark (Lucky Hit 25% / ICD 1s / per-target ICD 10s): Armor -200 (6s); Exposed
  - 대표 스펠: `CAFF_SPEL_CHAOS_CURSE_SUNDER`

- **`ice_form`** [Weapon]
  - 한글 표시: 얼음 형상: 15% 확률로 이동속도 -100% (2초). 10초마다 발동. 빙결 축적 +100.
  - 영문 표시: Ice Form (Lucky Hit 15% / ICD 10s): Move Speed -100% (2s); Freeze build-up +100
  - 대표 스펠: `CAFF_SPEL_TRAP_IRONJAW_SNARE`

- **`disarming_shout`** [Weapon]
  - 한글 표시: 무장 해제: 10% 확률로 적 공격력 -20% (6초). 10초마다 발동.
  - 영문 표시: Disarming Shout (Lucky Hit 10% / ICD 10s): Enemy Attack Damage -20% (6s)
  - 대표 스펠: `CAFF_SPEL_CHAOS_CURSE_SLOW_ATTACK`

- **`whirlwind_sprint`** [Weapon]
  - 한글 표시: 질풍 외침: 처치 시 이동속도 강화 (성장: ×1.0→1.25→1.55→2.0). 3초마다 발동.
  - 영문 표시: Whirlwind Sprint (100% on kill / ICD 3s): Move Speed Boost (Growth: x1.0→1.25→1.55→2.0)
  - 대표 스펠: `CAFF_SPEL_HIT_SHADOWSTEP_BURST`

## 마법학파 — 신규

> v1.2.21 신규 추가 — freed slot 활용

- **`spell_breach`** [Weapon]
  - 한글 표시: 마법 약화: 12% 확률로 마법 저항 감소. 10초마다 발동. 노출.
  - 영문 표시: Spell Breach (Lucky Hit 12% / ICD 10s): Magic Resist Reduction; Exposed
  - 대표 스펠: `CAFF_SPEL_CHAOS_CURSE_FRAGILE`

- **`stamina_drain`** [Weapon]
  - 한글 표시: 기력 고갈: 20% 확률로 스태미나 드레인. 4초마다 발동.
  - 영문 표시: Stamina Drain (Lucky Hit 20% / ICD 4s): Stamina Drain
  - 대표 스펠: `CAFF_SPEL_TRAP_DRAGONTEETH_DRAIN_STAMINA`

- **`nourishing_flame`** [Weapon]
  - 한글 표시: 자양의 불꽃: 최근 5초 내 처치 후 적중 시 체력 6 + 물리 피해의 10% 회복 (최대 50). 2초마다 발동.
  - 영문 표시: Nourishing Flame (on hit after a kill in the last 5s / ICD 2s): Heal 6 + 10% of Physical Hit Damage (max 50)
  - 대표 스펠: `CAFF_SPEL_HIT_BLOOD_SIPHON`

- **`mana_knot`** [Weapon]
  - 한글 표시: 마나 매듭: 35% 확률로 매지카 재생 -100% (6초). 5초마다 발동.
  - 영문 표시: Mana Knot (Lucky Hit 35% / ICD 5s): Magicka Regen -100% (6s)
  - 대표 스펠: `CAFF_SPEL_CHAOS_CURSE_DRAIN_MAGREGEN`

## 공용 상태 (v2.3.0)

- **`doom_brand`** [Weapon]
  - 한글 표시: 파멸의 낙인: 적중 시 25% 확률로 그림자 낙인. 1.5초 뒤 그 적중 피해의 60%가 마법 피해로 터짐(다시 찍으면 시간만 갱신하고 더 큰 폭발을 유지). 4초마다 발동.
  - 영문 표시: Doom Brand (25% on hit / ICD 4s): marks the target; 1.5s later it bursts for 60% of that hit's damage as magic damage (re-marking resets the timer and keeps the larger burst)
  - 대표 스펠: `CAFF_SPEL_STATUS_DOOM_BURST`

- **`exploit_weakness`** [Weapon]
  - 한글 표시: 약점 포착: 상태(노출·파멸 등)가 2가지 이상 걸린 적을 때리면 그 적중 피해의 15%를 추가 피해로 줌. 0.5초마다 발동.
  - 영문 표시: Exploit Weakness (on hit / ICD 0.5s): against an enemy with 2+ kinds of status (Exposed, Doom, ...), deal +15% of the hit's damage as extra damage
  - 대표 스펠: `CAFF_SPEL_STATUS_EXPLOIT_STRIKE`

- **`contagion`** [Armor]
  - 한글 표시: 전염: 상태(노출·파멸 등)가 걸린 적을 처치하면 반경 400 안의 적 최대 2명에게 같은 상태를 옮김. 3초마다 발동.
  - 영문 표시: Contagion (on kill / ICD 3s): a slain enemy's statuses (Exposed, Doom, ...) pass to up to 2 enemies within 400

## 루트 풀에서 빠진 접두

> 새 장비에는 붙지 않음(감정·정제·재련 포함). 이미 이 접두가 붙은 장비에서는 그대로 작동

- **`flame_strike`** [Weapon]
  - 한글 표시: 화염 일격: 매 적중 시 화염 피해 6 + 적중의 10%.
  - 영문 표시: Flame Strike (100% on hit): 6 Fire Damage + 10% of Hit Damage
  - 대표 스펠: `CAFF_SPEL_FIRE_DAMAGE`

- **`frost_strike`** [Weapon]
  - 한글 표시: 냉기 일격: 매 적중 시 냉기 피해 6 + 적중의 10%. 빙결 축적 +20.
  - 영문 표시: Frost Strike (100% on hit): 6 Frost Damage + 10% of Hit Damage; Freeze build-up +20
  - 대표 스펠: `CAFF_SPEL_FROST_DAMAGE`

- **`spark_strike`** [Weapon]
  - 한글 표시: 전격 일격: 매 적중 시 번개 피해 6 + 적중의 10%. 감전 축적 +20.
  - 영문 표시: Spark Strike (100% on hit): 6 Shock Damage + 10% of Hit Damage; Shock build-up +20
  - 대표 스펠: `CAFF_SPEL_SHOCK_DAMAGE`
