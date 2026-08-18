# Sveta Assistant

Windows Desktop AI Companion / Interactive Character Platform.
전체 기획서: `docs/Sveta_Assistant_Integrated_Project_Plan.docx`.

## 진행 상황

Phase 0(Foundation)과 Phase 1(Desktop Character) 완료: 테두리 없음,
항상 위, 픽셀 단위로 투명한 창에 PNG 스프라이트를 (WIC로 디코딩해)
렌더링하고, 드래그로 이동할 수 있으며, 위치는
`%LOCALAPPDATA%\SvetaAssistant\`에 저장된다.

`content/characters/sveta/assets/calm.png`의 스프라이트는 플레이스홀더다
(실제 캐릭터 아트 아님, 단순 도형) — 실제 아트가 나오면 같은 경로에
교체하면 된다. 다음은 Phase 2(커서 트래킹, 히트박스, 쓰다듬기 감지).

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
