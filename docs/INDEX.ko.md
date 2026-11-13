# docs/ 디렉토리 인덱스

- [VFX/SFX Matrix / 시각·음향 피드백 매트릭스](VFX_MATRIX.md)

## 어픽스 효과 목록

| 파일 | 설명 | 생성 |
|------|------|------|
| `PREFIX_EFFECTS.md` | 공개용 프리픽스 73개 효과 정리, 인게임 표시 문자열 기준 | `tools/gen_prefix_doc.py` |
| `SUFFIX_EFFECTS.md` | 공개용 서픽스 22패밀리 66개 효과 정리, 인게임 표시 문자열 기준 | `tools/gen_suffix_doc.py` |
| `RUNEWORD_EFFECTS.md` | 공개용 룬워드 94개 효과 정리, 인게임 표시 문자열 기준 | `tools/gen_runeword_doc.py` |
| `AFFIX_CATALOG.md` | 공개용 어픽스 전체 입구 문서 | `tools/update_docs.py` |

## 루트 문서

| 파일 | 설명 |
|------|------|
| `NEXUS_DESCRIPTION.md` | Nexus Mods 배포 페이지 설명 (한국어/영어) |
| `NEXUS_DESCRIPTION_BBCODE.txt` | Nexus Mods BBCode 포맷 설명 |
| `스카이림 바닐라 효과 ARPG 변환 아이디어.md` | 바닐라 효과 → ARPG 어픽스 변환 기획 노트 |

## design/ — 설계 문서

프로젝트 기획, 아키텍처, 워크플로우 문서. (구 `doc/`에서 이동)

| 파일 | 설명 |
|------|------|
| `개발명세서.md` | 프로젝트 개발 명세서 |
| `CK_MVP_셋업_체크리스트.md` | Creation Kit MVP 셋업 체크리스트 |
| `데이터주도_생성기_워크플로우.md` | 데이터 주도 생성기 워크플로우 |
| `아키텍처_분석_리팩터링_로드맵.md` | 아키텍처 분석 및 리팩터링 로드맵 |

## adr/ — Architecture Decision Records

프로젝트 핵심 아키텍처 결정 기록.

| 파일 | 설명 |
|------|------|
| `0001-instance-affix-storage-extrauniqueid-serialization.md` | 인스턴스 어픽스 저장 방식 (ExtraUniqueID + 직렬화) |
| `0002-prisma-ui-tooltip-overlay.md` | Prisma UI 기반 툴팁 오버레이 결정 |
| `0003-trigger-pipeline-healthdamage-hook-hit-event-dot-apply.md` | 전투 트리거 파이프라인 (HealthDamage 훅 + HitEvent + DoT) |

## plans/ — 기획 및 설계 문서

기능 기획, 코드 리뷰, 리팩터링 계획 등.

주요 문서:
- `2026-02-12-prefix-suffix-system-v1.md` — Prefix/Suffix 분류 시스템
- `2026-02-07-runeword-physical-fragments.md` — 룬워드 물리 조각 시스템
- `2026-02-13-bilingual-i18n.md` — 한영 이중 언어 지원
- `2026-02-02-eventbridge-split-v1.md` — EventBridge 도메인 분할
- `2026-02-09-eventbridge-actions-decomposition-v1.md` — EventBridge 액션 분해
- `2026-01-31-autogen-spell-mgef.md` — ESP 자동 생성 (MagicEffect/Spell)
- `2026-03-06-eventbridge-state-ownership-extraction-design.md` — EventBridge 상태 소유권 분리 상위 설계
- `2026-03-06-combat-runtime-state-extraction-design.md` — 전투 runtime state ownership 분리 설계
- `2026-03-06-loot-runtime-state-extraction-design.md` — loot runtime state ownership 분리 설계
- `2026-03-06-runeword-runtime-state-extraction-design.md` — runeword runtime state ownership 분리 설계
- `2026-03-06-serialization-runtime-state-extraction-design.md` — serialization runtime state ownership 분리 설계
- `2026-07-27-eventbridge-decomposition-status-and-next-steps.md` — 위 상위 설계의 실측 달성도 감사와 수정된 단계 순서

## references/ — 참조 문서

외부 아키텍처 분석, 호환성 정책, LoreBox 예제.

| 파일 | 설명 |
|------|------|
| `2026-02-09-event-signatures.md` | SKSE 이벤트 시그니처 참조 |
| `2026-02-15-calamityaffixes-architecture-snapshot-v1.2.14.md` | v1.2.14 아키텍처 스냅샷 |
| `2026-02-22-corpse-drop-compat-policy.md` | 시체 드랍 호환성 정책 |
| `LoreBox-Example/` | LoreBox ESP 예제 (Nexus #156534) |
| `LoreBox-Tutorial/` | LoreBox 튜토리얼 (Nexus #156534) |

## releases/ — 릴리즈 기록

GitHub 릴리즈 본문, Nexus 배포 카피, QA 체크리스트 등.

주요 유형:
- `github-release-body-*.md` — GitHub 릴리즈 노트
- `ingame-qa-smoke-checklist.md` — 인게임 QA 스모크 테스트
- `nexus-*` — Nexus Mods 배포 관련

## reviews/ — 코드 리뷰 및 분석

| 파일 | 설명 |
|------|------|
| `2026-02-18-deep-review.md` | 심층 코드 리뷰 |
| `2026-02-21-runeword-94-diff-rc2-to-rc6.md` | 룬워드 94종 RC2→RC6 변경 diff |
| `2026-02-26-deep-analysis-comprehensive.md` | 종합 심층 분석 |
| `2026-02-26-deep-analysis-round2-supplement.md` | 심층 분석 2차 보충 |
| `2026-02-26-deep-analysis-round3-final-decision.md` | 심층 분석 3차 최종 결정 |
| `2026-03-06-architecture-hotspots-overview.md` | 현재판 아키텍처 요약 + hotspot 5개 + 우선 개선 순서 |
