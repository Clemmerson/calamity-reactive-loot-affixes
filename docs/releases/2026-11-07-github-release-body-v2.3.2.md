# Calamity - Reactive Loot & Affixes v2.3.2

v2.3.2는 **NPC에게 장비 강화를 맡기면 어픽스가 사라지던 문제**를 고친 핫픽스입니다. ESP와 코세이브 형식은 2.3.1과 같고, DLL만 바뀝니다.

*English summary: paying a smith NPC to temper an item (for example with Temper and Recharge Services) erased its affixes. The service drops the item, tempers the world reference and adds it back; the container events for that round trip name no reference, so the affix state could not follow the item and was pruned as orphaned. The runtime now follows the unique-ID change that turns the player's item into the world reference, keeps the state through the early delete signal the engine sends for that reference, and gives the returned item a player unique ID with its affixes back. Tempering at a grindstone or workbench was never affected. Affixes already lost cannot be restored. ESP and co-save are unchanged; install over 2.3.x with Replace.*

## 수정: NPC 강화 후 어픽스 소실

- 'Temper and Recharge Services'처럼 대장장이에게 돈을 내고 강화를 맡기는 모드는 장비를 잠깐 바닥에 떨어뜨려 강화한 뒤 다시 인벤토리에 넣습니다.
- 이때 게임이 보내는 아이템 이동 정보에 떨어진 아이템이 빠져 있어, 칼라미티가 어픽스 기록을 옮길 곳을 찾지 못했습니다. 그 기록은 주인 없는 기록으로 정리돼 사라졌습니다.
- 이제 플레이어 장비가 바닥 아이템이 되는 순간의 고유 ID 변경으로 기록을 옮겨 두고, 다시 들어온 장비에 고유 ID를 붙여 어픽스를 되돌립니다.
- 게임이 그 바닥 아이템을 곧바로 '지워짐'으로 알리는 경우가 있어, 30초 안에 돌아오는 아이템은 그 신호로 기록을 지우지 않습니다. 끝내 돌아오지 않으면 기록을 정리합니다.
- 어느 장비가 돌아왔는지 확실할 때만 어픽스를 붙입니다. 같은 종류 장비가 여럿이라 구분할 수 없으면 엉뚱한 장비에 붙이지 않습니다.
- 숫돌·작업대에서 직접 강화하는 것은 원래 문제가 없었습니다.
- **이미 사라진 어픽스는 되돌릴 수 없습니다.**

## 확인한 범위

- 떼껄룩 환경(AE 1.6.1170, Temper and Recharge Services)에서 에오를룬드에게 강화를 맡겨, 로그와 툴팁으로 어픽스가 유지되는 것을 확인했습니다.
- 착용한 장비 두 개를 맡긴 경우도 확인했습니다. 맡기는 동안 빠졌던 어픽스가, 돌아온 장비를 다시 끼자 모두 다시 동작했습니다.
- 판정 규칙(바닥 아이템이 되는 고유 ID 변경, 돌아온 장비 선택, 30초 기준)은 컴파일 시점 테스트로 고정했습니다.
- Python·생성기·SKSE 검사와 배포 패키지 검증을 통과했습니다.

## 설치와 업데이트

1. **`CalamityAffixes_MO2_v2.3.2_2026-11-07.zip`** 전체를 설치하고, MO2에서 기존 Calamity 모드를 **Replace**하세요. DLL만 바꾸지 마세요.
2. 새 게임은 필요하지 않습니다.

Prisma UI는 이 ZIP에 포함되지 않습니다. 공개 자산은 **MO2 ZIP, ZIP SHA256, DLL, ESP** 네 개입니다.
