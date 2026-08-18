# Sveta Assistant

Windows Desktop AI Companion / Interactive Character Platform.
전체 기획서: `docs/Sveta_Assistant_Integrated_Project_Plan.docx`.

## 진행 상황

Phase 0(Foundation), Phase 1(Desktop Character), Phase 2(Interaction)
완료:

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

지금은 감지된 이벤트를 로그로만 남긴다(`CharacterHovered`,
`CharacterPetted`) — Emotion/Action 시스템이 아직 없어서 캐릭터가
실제로 반응하지는 않는다. 다음은 Phase 3(Character Life: 감정, 행동,
성격, Idle Behavior).

머리 히트박스는 현재 스프라이트 크기에 비례한 근사치(상단 50%,
가운데 72% 너비)다. 캐릭터마다 다른 정확한 히트박스는 추후
Character Package(character.json, 기획서 24~25장)에서 다룰 예정.

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
  character/    캐릭터 상태 (감정, 행동, 성격)
  behavior/     Idle/행동 선택 로직
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
