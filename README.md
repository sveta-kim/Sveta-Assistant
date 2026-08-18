# Sveta Assistant

Windows Desktop AI Companion / Interactive Character Platform.
전체 기획서: `docs/Sveta_Assistant_Integrated_Project_Plan.docx`.

## 진행 상황

Phase 0(Foundation), Phase 1(Desktop Character), Phase 2(Interaction),
Phase 3(Character Life) 완료:

- 테두리 없음, 항상 위, 픽셀 단위로 투명한 창에 PNG 스프라이트를
  (WIC로 디코딩해) 렌더링
- 원본 아트가 클 경우 화면에 보일 최대 크기(240px)로 자동 축소 —
  실제 `calm.png`(1254×1254)도 이 크기로 줄여서 표시
- 드래그로 이동, 위치는 `%LOCALAPPDATA%\SvetaAssistant\`에 저장
- 커서가 캐릭터 위에 들어오고 나가는 것 추적(Hover/Leave) — 레이어드
  창의 알파 채널 기반 히트테스트 덕분에 실루엣 바깥은 클릭도
  자연스럽게 통과됨
- 머리(헬멧) 영역에 비례 기반 히트박스 계산 (`interaction/HeadHitbox`)
- 히트박스 안에서 좌우로 반복 스와이프하면 쓰다듬기로 인식
  (`interaction/PettingDetector`) — 적당한 속도, 짧은 시간 내 방향
  전환 3회 이상 조건 (기획서 8장 규칙)
- `character/CharacterState`가 Emotion(10종)·Action(15종)·Personality
  (기획서 12장 수치 그대로: Affection 0.90 등)를 결합해 캐릭터
  행동을 관리 (기획서 9장 "Emotion + Action + Personality +
  Environment = Character Behavior")
  - 호버 → Curious + LookingAtCursor, 쓰다듬기 → Happy + BeingPetted
    (Sveta는 쓰다듬으면 기뻐한다는 기획서 8장 캐릭터별 반응 반영),
    드래그 시작 → Surprised + Dragged, 진행 중이던 리액션은 드래그가
    가로챔
  - 60초간 상호작용 없으면 Sleeping/Sleepy로 전환, 커서가 다시
    다가오면 기상 (기획서 6장 시나리오)
  - 1초 간격 타이머로 `behavior/IdleBehavior`가 성격 가중치를 반영해
    Idle 상태에서 확률적으로 행동 선택(LookAround/Blink/Yawn/
    Stretch/Move/SitAtBottom/Read/Drink/Doze/PlayWithItem, 기획서
    13장) — Playfulness가 높을수록 Move/PlayWithItem이, Curiosity가
    높을수록 LookAround가 더 잘 나옴

지금은 실제 그림(에셋)이 `calm.png` 하나뿐이라 감정/행동이 바뀌어도
겉모습은 그대로다 — 상태 전이는 전부 로그(`sveta.log`)로 관찰 가능하고
직접 확인도 했지만(호버/쓰다듬기/드래그/Sleep/기상/Idle 전부), 감정별
스프라이트(happy.png 등, 기획서 14장 "초기: PNG Sprite 기반")가 없어
시각적으로는 아직 안 드러난다. 다음은 Phase 4(AI Conversation)나 감정별
스프라이트 추가 중 선택.

머리 히트박스는 현재 스프라이트 크기에 비례한 근사치(상단 50%,
가운데 72% 너비)다. 캐릭터마다 다른 정확한 히트박스는 추후
Character Package(character.json, 기획서 24~25장)에서 다룰 예정.
Idle-to-sleep 60초 타임아웃도 UX 튜닝 전 임시값이다.

남은 과제: 아직 DPI 인식 매니페스트가 없어서, 앱 자신이 보는 창 좌표와
DPI를 인식하는 외부 도구가 보는 좌표가 다를 수 있다. 앱 자체의
저장/복원 왕복에는 영향 없지만, Phase 13 폴리싱 전에는 고쳐야 한다.

## 사전 준비물

- Visual Studio 2022 이상, "C++를 사용한 데스크톱 개발" 워크로드
  (MSVC 툴셋과 Windows SDK 포함)
- CMake 3.28 이상
- Git

이 컴퓨터에는 둘 다 설치되어 있다 (VS 2026 Community, CMake 4.4.2).

## 빌드

```
cmake -B build -S .
cmake --build build --config Debug
```

또는 Visual Studio에서 폴더 열기(File > Open > Folder)로 열면 CMake를
네이티브로 통합해서 사용할 수 있다.

## 구조

```
src/
  app/          진입점 (WinMain)
  core/         로깅, 파일 경로, 공용 유틸리티
  window/       Win32 창 관리, 드래그, 위치 저장/복원
  rendering/    PNG 스프라이트 로딩 (WIC); Direct2D/Direct3D는 이후 단계
  character/    Emotion/Action/Personality, CharacterState 오케스트레이션
  behavior/     Idle 상태에서의 확률 기반 행동 선택
  interaction/  마우스 상호작용, 쓰다듬기, 히트박스
  content/      캐릭터/아이템/가구 패키지 로딩
  items/        인터랙티브 소품
  context/      데스크톱 인식, 화면 이해
  ai/           AI 엔진 연동 (대화, 비전)
  memory/       세션/일간/장기 기억
  audio/        STT/TTS
content/        캐릭터 및 아이템 패키지 (코드 아닌 데이터)
assets/         공용 에셋
config/         런타임 설정
tests/          테스트
```

이 구조를 정한 근거는 기획서 47장, 전체 Phase 로드맵(Phase 0~13)은
52장을 참고.
