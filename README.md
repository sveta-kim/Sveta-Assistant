# Sveta Assistant

Windows Desktop AI Companion / Interactive Character Platform.
전체 기획서: `docs/Sveta_Assistant_Integrated_Project_Plan.docx`.

## 진행 상황

Phase 0(Foundation), Phase 1(Desktop Character), Phase 2(Interaction),
Phase 3(Character Life), Phase 4(AI Conversation) 완료:

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

감정별 스프라이트도 추가했다. 실제 원화가 `calm.png` 하나뿐이라
happy/excited/curious/sad/annoyed/embarrassed/sleepy/concerned/
surprised는 전부 `calm.png` 위에 절차적으로 오버레이(블러시, 눈물,
땀방울, 반짝임, Z, ?, !, 화남 표시)를 합성해서 만들었다 — 새 원화는
아니지만 감정이 눈으로 구분된다. `character/Emotion::SpriteFileName()`이
파일명을 매핑하고, `MainWindow::SyncSpriteToEmotion()`이 매 틱/이벤트마다
`CharacterState`의 현재 감정과 마지막으로 반영한 감정을 비교해 바뀌었을
때만 다시 로드한다(파일이 없으면 calm.png로 폴백). Hover→Curious("?"),
쓰다듬기→Happy(블러시+별), 드래그→Surprised("!"), 방치→Sleepy(Z)까지
실제로 화면 캡처로 확인했다.

오버레이 생성 스크립트는 `tools/generate_emotion_sprites.py`(Python +
Pillow)에 있다. `calm.png`가 바뀌거나 아이콘 위치/색을 조정하고 싶으면
이 스크립트를 고쳐서 다시 실행하면 된다.

**텍스트 채팅**도 붙었다 (기획서 19장 "Idle -> Listening -> Thinking ->
Talking -> Idle", 20장 AI Engine, 54장 두 번째 MVP):

- 캐릭터를 더블클릭하면 말풍선 모양 입력창이 뜬다. Enter로 전송, Esc로
  취소
- `ai/ChatClient`가 WinHTTP로 OpenAI Chat Completions 호환 엔드포인트에
  POST — 어떤 서비스든 그 스키마(`{model, messages}` 요청 /
  `choices[0].message.content` 응답)를 따르면 붙는다. JSON은
  `third_party/nlohmann/json.hpp`(벤더링, MIT) 사용
- 네트워크 호출은 백그라운드 스레드에서 실행되고 결과는
  `PostMessage`로 UI 스레드에 돌려준다 (기획서 49장 "AI 응답이
  느리더라도 캐릭터는 계속 움직일 수 있어야 한다") — 응답을 기다리는
  동안에도 Idle 애니메이션, 드래그, 쓰다듬기 전부 그대로 동작
- 성격 수치를 반영한 간단한 시스템 프롬프트(`ai/Persona`)와 세션 내
  대화 기록(최근 20턴, 재시작하면 사라짐 — Phase 8 Memory System이
  대체할 자리표시자)
- API 키는 `config/secrets.local.json`(`.gitignore` 처리, 실제 값은
  본인이 직접 채워야 함)에, 엔드포인트/모델명은 `config/ai_config.json`
  (커밋됨, placeholder 값)에 있다. 둘 다 안 채워져 있으면 더블클릭 시
  "AI가 아직 설정되지 않았어요" 안내만 보여주고 네트워크 호출은
  시도하지 않는다

**말풍선 UI**: 처음엔 그냥 흰 사각형 Edit 박스였는데(구려서 다시 만듦),
지금은 GDI+로 직접 그린 둥근 말풍선(꼬리, 부드러운 그림자, Segoe UI)이
레이어드 창으로 뜬다. 입력 중일 때만 예외적으로 진짜 Edit 컨트롤을 쓰는데
— `UpdateLayeredWindow`로 그리는 창은 일반 자식 컨트롤을 위에 합성하지
못한다는 실제 Win32 제약 때문에, 그 순간만 레이어드를 끄고 둥근
리전(`SetWindowRgn`)으로 대체한다. 생각 중/응답 표시일 때는 텍스트까지
GDI+로 직접 그리므로 이 제약이 없다. GDI+ 폰트 서브시스템은 프로세스당
최초 1회 초기화가 느려서(이 환경에서 ~5초, 이후 매번 ~4ms) 앱 시작 시
백그라운드 스레드에서 미리 예열해둔다.

다음은 Phase 5(Voice)나 Phase 6(Desktop Context) 중 선택.

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

## AI 채팅 설정 (선택)

`config/ai_config.json`의 `model`과 `config/secrets.local.json`의
`api_key`를 채워야 실제로 응답이 온다. 안 채워도 앱은 정상 작동하고,
더블클릭하면 설정이 안 됐다는 안내 말풍선만 뜬다.

```json
// config/ai_config.json (커밋됨 — 민감 정보 없음)
{ "endpoint": "https://factchat-cloud.mindlogic.ai/v1/gateway", "model": "실제 모델명으로 교체" }

// config/secrets.local.json (.gitignore 처리됨 — 직접 만들어서 채우기)
{ "api_key": "실제 키로 교체" }
```

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
tools/          에셋 생성 등 개발용 스크립트 (빌드에 포함 안 됨)
third_party/    벤더링한 헤더 전용 라이브러리 (nlohmann/json)
```

이 구조를 정한 근거는 기획서 47장, 전체 Phase 로드맵(Phase 0~13)은
52장을 참고.
