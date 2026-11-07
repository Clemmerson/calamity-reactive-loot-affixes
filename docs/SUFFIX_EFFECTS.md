# 서픽스 효과 정리 (공개용)

> 업데이트: 2026-11-07
> 기준 버전: `v2.3.2`
> 기준 코드: `affixes/modules/keywords.affixes.suffixes.json`

- 총 서픽스 패밀리: **22개**
- 총 서픽스 엔트리: **66개**
- 각 티어 이름은 인게임 표시 문자열(`nameKo`/`nameEn`)을 그대로 사용
- 공개용 문서 기준으로 패밀리/핵심 수치/추천 빌드를 중심으로 정리
- 같은 패밀리는 장착한 모든 사본의 티어를 합산하고 최대 T3로 제한한 뒤, 결과 티어의 효과 하나만 적용
- 같은 티어를 여러 장비에 착용해도 합산에 포함: T1 + T1 → T2, T1 + T2 → T3, T2 + T2 → T3
- 능력치 숫자를 단순 합산하는 방식은 아님: guardian T1(+25) + T2(+50)는 T3(+80) 한 번으로 적용
- assassin의 치명타 피해 보너스도 합산 결과 티어의 값 한 번만 적용
- 장비 자체의 옵션 티어를 영구 변경하는 것은 아니며, 장착 구성이 바뀌면 유효 티어를 다시 계산
- 서로 다른 패밀리는 각각 계산하며, 패밀리가 없는 룬워드 패시브에는 이 티어 합산 규칙을 적용하지 않음

## 패밀리 요약

| # | 패밀리 | 영향 능력치 | T1 | T2 | T3 | 추천 빌드 |
|---|--------|-------------|----|----|----|-----------|
| 1 | **assassin** | CriticalChance | +10 | +20 | +30 | 치명타 |
| 2 | **brilliance** | Magicka | +20 | +40 | +60 | 마법사 |
| 3 | **bulwark** | BlockModifier | +5 | +10 | +15 | 방패 탱커 |
| 4 | **champion** | TwoHandedModifier | +5 | +10 | +15 | 양손 전사 |
| 5 | **conjurer** | ConjurationModifier | +5 | +10 | +15 | 소환 마법사 |
| 6 | **eagle_eye** | 활 당기기 속도 (런타임 적용) | +5 | +10 | +15 | 궁수 |
| 7 | **enchanter** | Enchanting | +5 | +10 | +15 | 부여 마법사 |
| 8 | **endurance** | Stamina | +20 | +40 | +60 | 근접/궁수 |
| 9 | **evasion** | 받는 피해 감소(경갑, %) | +5 | +10 | +15 | 경장/도적 |
| 10 | **fortitude** | 받는 피해 감소(중갑, %) | +5 | +10 | +15 | 중장/탱커 |
| 11 | **gladiator** | AttackDamageMult | +3 | +6 | +9 | 공격력 |
| 12 | **guardian** | DamageResist | +25 | +50 | +80 | 물리 방어 |
| 13 | **marksman** | MarksmanModifier | +5 | +10 | +15 | 궁수 |
| 14 | **meditation** | MagickaRateMult | +25 | +50 | +75 | 마법사 |
| 15 | **regeneration** | HealRateMult | +25 | +50 | +75 | 범용 |
| 16 | **shadow** | SneakingModifier | +5 | +10 | +15 | 스텔스/도적 |
| 17 | **spell_ward** | ResistMagic | +3 | +8 | +12 | 마법 저항 |
| 18 | **steed** | CarryWeight | +15 | +30 | +50 | 범용 유틸 |
| 19 | **swiftness** | SpeedMult | +4 | +7 | +10 | 범용 이동 |
| 20 | **swordsman** | OneHandedModifier | +5 | +10 | +15 | 한손 전사 |
| 21 | **tenacity** | StaminaRateMult | +25 | +50 | +75 | 근접 |
| 22 | **vitality** | Health | +25 | +50 | +75 | 범용 |

## 패밀리별 상세

### assassin

- 영향 능력치: `CriticalChance`
- 추천 빌드: 치명타

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_assassin_t1` | 약간의 암살자: 치명타 확률 +10%(바닐라 치명타 퍽과 중첩 안 됨), 치명타 피해 +10% | of Minor Assassin: Critical Chance +10% (does not stack with vanilla critical perks), Critical Damage +10% | 10 |
| T2 | `suffix_assassin_t2` | 암살자: 치명타 확률 +20%(바닐라 치명타 퍽과 중첩 안 됨), 치명타 피해 +20% | of the Assassin: Critical Chance +20% (does not stack with vanilla critical perks), Critical Damage +20% | 20 |
| T3 | `suffix_assassin_t3` | 위대한 암살자: 치명타 확률 +30%(바닐라 치명타 퍽과 중첩 안 됨), 치명타 피해 +30% | of Grand Assassin: Critical Chance +30% (does not stack with vanilla critical perks), Critical Damage +30% | 30 |

### brilliance

- 영향 능력치: `Magicka`
- 추천 빌드: 마법사

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_brilliance_t1` | 약간의 총명: 최대 매지카 +20 | of Minor Brilliance: Max Magicka +20 | 20 |
| T2 | `suffix_brilliance_t2` | 총명: 최대 매지카 +40 | of Brilliance: Max Magicka +40 | 40 |
| T3 | `suffix_brilliance_t3` | 위대한 총명: 최대 매지카 +60 | of Grand Brilliance: Max Magicka +60 | 60 |

### bulwark

- 영향 능력치: `BlockModifier`
- 추천 빌드: 방패 탱커

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_bulwark_t1` | 약간의 방벽: 방어 +5% | of Minor Bulwark: Block +5% | 5 |
| T2 | `suffix_bulwark_t2` | 방벽: 방어 +10% | of Bulwark: Block +10% | 10 |
| T3 | `suffix_bulwark_t3` | 위대한 방벽: 방어 +15% | of Grand Bulwark: Block +15% | 15 |

### champion

- 영향 능력치: `TwoHandedModifier`
- 추천 빌드: 양손 전사

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_champion_t1` | 약간의 투사: 양손 무기 +5% | of Minor Champion: Two-Handed +5% | 5 |
| T2 | `suffix_champion_t2` | 투사: 양손 무기 +10% | of Champion: Two-Handed +10% | 10 |
| T3 | `suffix_champion_t3` | 위대한 투사: 양손 무기 +15% | of Grand Champion: Two-Handed +15% | 15 |

### conjurer

- 영향 능력치: `ConjurationModifier`
- 추천 빌드: 소환 마법사

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_conjurer_t1` | 약간의 소환술사: 소환 마법 비용 -5% | of Minor Conjurer: Conjuration spell cost -5% | 5 |
| T2 | `suffix_conjurer_t2` | 소환술사: 소환 마법 비용 -10% | of the Conjurer: Conjuration spell cost -10% | 10 |
| T3 | `suffix_conjurer_t3` | 위대한 소환술사: 소환 마법 비용 -15% | of Grand Conjurer: Conjuration spell cost -15% | 15 |

### eagle_eye

- 영향 능력치: `활 당기기 속도 (런타임 적용)`
- 추천 빌드: 궁수

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_eagle_eye_t1` | 약간의 독수리눈: 활 속도 +5% | of Minor Eagle Eye: Bow Speed +5% | 5 |
| T2 | `suffix_eagle_eye_t2` | 독수리눈: 활 속도 +10% | of Eagle Eye: Bow Speed +10% | 10 |
| T3 | `suffix_eagle_eye_t3` | 위대한 독수리눈: 활 속도 +15% | of Grand Eagle Eye: Bow Speed +15% | 15 |

### enchanter

- 영향 능력치: `Enchanting`
- 추천 빌드: 부여 마법사

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_enchanter_t1` | 약간의 부여술사: 부여 위력 +5% | of Minor Enchanter: Items you enchant are 5% stronger | 5 |
| T2 | `suffix_enchanter_t2` | 부여술사: 부여 위력 +10% | of the Enchanter: Items you enchant are 10% stronger | 10 |
| T3 | `suffix_enchanter_t3` | 위대한 부여술사: 부여 위력 +15% | of Grand Enchanter: Items you enchant are 15% stronger | 15 |

### endurance

- 영향 능력치: `Stamina`
- 추천 빌드: 근접/궁수

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_endurance_t1` | 약간의 인내: 최대 스태미나 +20 | of Minor Endurance: Max Stamina +20 | 20 |
| T2 | `suffix_endurance_t2` | 인내: 최대 스태미나 +40 | of Endurance: Max Stamina +40 | 40 |
| T3 | `suffix_endurance_t3` | 위대한 인내: 최대 스태미나 +60 | of Grand Endurance: Max Stamina +60 | 60 |

### evasion

- 영향 능력치: `받는 피해 감소(경갑, %)`
- 추천 빌드: 경장/도적

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_evasion_t1` | 약간의 회피: 경갑 착용 시 받는 피해 -5% | of Minor Evasion: -5% damage taken while wearing light armor | 5 |
| T2 | `suffix_evasion_t2` | 회피: 경갑 착용 시 받는 피해 -10% | of Evasion: -10% damage taken while wearing light armor | 10 |
| T3 | `suffix_evasion_t3` | 위대한 회피: 경갑 착용 시 받는 피해 -15% | of Grand Evasion: -15% damage taken while wearing light armor | 15 |

### fortitude

- 영향 능력치: `받는 피해 감소(중갑, %)`
- 추천 빌드: 중장/탱커

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_fortitude_t1` | 약간의 견고: 중갑 착용 시 받는 피해 -5% | of Minor Fortitude: -5% damage taken while wearing heavy armor | 5 |
| T2 | `suffix_fortitude_t2` | 견고: 중갑 착용 시 받는 피해 -10% | of Fortitude: -10% damage taken while wearing heavy armor | 10 |
| T3 | `suffix_fortitude_t3` | 위대한 견고: 중갑 착용 시 받는 피해 -15% | of Grand Fortitude: -15% damage taken while wearing heavy armor | 15 |

### gladiator

- 영향 능력치: `AttackDamageMult`
- 추천 빌드: 공격력

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_gladiator_t1` | 약간의 검투사: 공격력 +3% | of Minor Gladiator: Attack Damage +3% | 3 |
| T2 | `suffix_gladiator_t2` | 검투사: 공격력 +6% | of the Gladiator: Attack Damage +6% | 6 |
| T3 | `suffix_gladiator_t3` | 위대한 검투사: 공격력 +9% | of Grand Gladiator: Attack Damage +9% | 9 |

### guardian

- 영향 능력치: `DamageResist`
- 추천 빌드: 물리 방어

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_guardian_t1` | 약간의 수호: 방어력 +25 | of Minor Guardian: Armor +25 | 25 |
| T2 | `suffix_guardian_t2` | 수호: 방어력 +50 | of the Guardian: Armor +50 | 50 |
| T3 | `suffix_guardian_t3` | 위대한 수호: 방어력 +80 | of Grand Guardian: Armor +80 | 80 |

### marksman

- 영향 능력치: `MarksmanModifier`
- 추천 빌드: 궁수

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_marksman_t1` | 약간의 명사수: 활/석궁 +5% | of Minor Marksman: Archery +5% | 5 |
| T2 | `suffix_marksman_t2` | 명사수: 활/석궁 +10% | of Marksman: Archery +10% | 10 |
| T3 | `suffix_marksman_t3` | 위대한 명사수: 활/석궁 +15% | of Grand Marksman: Archery +15% | 15 |

### meditation

- 영향 능력치: `MagickaRateMult`
- 추천 빌드: 마법사

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_meditation_t1` | 약간의 명상: 매지카 재생 +25% | of Minor Meditation: Magicka Regen +25% | 25 |
| T2 | `suffix_meditation_t2` | 명상: 매지카 재생 +50% | of Meditation: Magicka Regen +50% | 50 |
| T3 | `suffix_meditation_t3` | 위대한 명상: 매지카 재생 +75% | of Grand Meditation: Magicka Regen +75% | 75 |

### regeneration

- 영향 능력치: `HealRateMult`
- 추천 빌드: 범용

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_regeneration_t1` | 약간의 재생: 체력 재생 +25% | of Minor Regeneration: Health Regen +25% | 25 |
| T2 | `suffix_regeneration_t2` | 재생: 체력 재생 +50% | of Regeneration: Health Regen +50% | 50 |
| T3 | `suffix_regeneration_t3` | 위대한 재생: 체력 재생 +75% | of Grand Regeneration: Health Regen +75% | 75 |

### shadow

- 영향 능력치: `SneakingModifier`
- 추천 빌드: 스텔스/도적

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_shadow_t1` | 약간의 그림자: 은신 +5% | of Minor Shadow: Sneak +5% | 5 |
| T2 | `suffix_shadow_t2` | 그림자: 은신 +10% | of the Shadow: Sneak +10% | 10 |
| T3 | `suffix_shadow_t3` | 위대한 그림자: 은신 +15% | of Grand Shadow: Sneak +15% | 15 |

### spell_ward

- 영향 능력치: `ResistMagic`
- 추천 빌드: 마법 저항

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_spell_ward_t1` | 약간의 항마: 마법 저항 +3% | of Minor Spell Ward: Magic Resist +3% | 3 |
| T2 | `suffix_spell_ward_t2` | 항마: 마법 저항 +8% | of Spell Ward: Magic Resist +8% | 8 |
| T3 | `suffix_spell_ward_t3` | 위대한 항마: 마법 저항 +12% | of Grand Spell Ward: Magic Resist +12% | 12 |

### steed

- 영향 능력치: `CarryWeight`
- 추천 빌드: 범용 유틸

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_steed_t1` | 약간의 준마: 소지 무게 +15 | of Minor Steed: Carry Weight +15 | 15 |
| T2 | `suffix_steed_t2` | 준마: 소지 무게 +30 | of Steed: Carry Weight +30 | 30 |
| T3 | `suffix_steed_t3` | 위대한 준마: 소지 무게 +50 | of Grand Steed: Carry Weight +50 | 50 |

### swiftness

- 영향 능력치: `SpeedMult`
- 추천 빌드: 범용 이동

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_swiftness_t1` | 약간의 질풍: 이동 속도 +4% | of Minor Swiftness: Move Speed +4% | 4 |
| T2 | `suffix_swiftness_t2` | 질풍: 이동 속도 +7% | of Swiftness: Move Speed +7% | 7 |
| T3 | `suffix_swiftness_t3` | 위대한 질풍: 이동 속도 +10% | of Grand Swiftness: Move Speed +10% | 10 |

### swordsman

- 영향 능력치: `OneHandedModifier`
- 추천 빌드: 한손 전사

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_swordsman_t1` | 약간의 검술: 한손 무기 +5% | of Minor Swordsmanship: One-Handed +5% | 5 |
| T2 | `suffix_swordsman_t2` | 검술: 한손 무기 +10% | of Swordsmanship: One-Handed +10% | 10 |
| T3 | `suffix_swordsman_t3` | 위대한 검술: 한손 무기 +15% | of Grand Swordsmanship: One-Handed +15% | 15 |

### tenacity

- 영향 능력치: `StaminaRateMult`
- 추천 빌드: 근접

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_tenacity_t1` | 약간의 불굴: 스태미나 재생 +25% | of Minor Tenacity: Stamina Regen +25% | 25 |
| T2 | `suffix_tenacity_t2` | 불굴: 스태미나 재생 +50% | of Tenacity: Stamina Regen +50% | 50 |
| T3 | `suffix_tenacity_t3` | 위대한 불굴: 스태미나 재생 +75% | of Grand Tenacity: Stamina Regen +75% | 75 |

### vitality

- 영향 능력치: `Health`
- 추천 빌드: 범용

| 티어 | ID | 한국어 이름 | 영어 이름 | 수치 |
|------|----|-------------|-----------|-----:|
| T1 | `suffix_vitality_t1` | 약간의 활력: 최대 체력 +25 | of Minor Vitality: Max Health +25 | 25 |
| T2 | `suffix_vitality_t2` | 활력: 최대 체력 +50 | of Vitality: Max Health +50 | 50 |
| T3 | `suffix_vitality_t3` | 위대한 활력: 최대 체력 +75 | of Grand Vitality: Max Health +75 | 75 |
