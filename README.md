# Sveta Assistant

Windows Desktop AI Companion / Interactive Character Platform.
전체 기획서: `docs/Sveta_Assistant_Integrated_Project_Plan.docx`.

## 진행 상황

Phase 0(Foundation), Phase 1(Desktop Character), Phase 2(Interaction),
Phase 3(Character Life), Phase 4(AI Conversation), Phase 5(Voice — TTS만,
아래 참고) 완료:

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

**음성(TTS)**도 붙었다 (기획서 21장 Voice System). STT/Push-to-Talk는
이번엔 뺐다 — 이 개발 머신엔 한국어/영어 음성 인식 엔진이 설치되어
있지 않고(TTS 목소리는 있는데 인식 모델이 없음, Windows 언어팩 설치가
필요), 독일어로만 검증 가능해서 실사용 언어로 끝까지 확인할 수 없었기
때문이다. TTS/입 애니메이션까지만 먼저 끝내기로 함.

- `audio/TextToSpeech`가 SAPI(`ISpVoice`)로 AI 응답을 읽는다.
  `audio/LanguageDetection`이 응답 텍스트를 보고 목표 7개 언어
  (한국어/영어/일본어/중국어/스페인어/독일어/러시아어 — 기획서 21장
  원래 목표였던 4개에 중국어/스페인어/독일어 추가) 중 하나로 분류해서
  매번 그 언어에 맞는 SAPI 보이스를 다시 고른다(한글/가나/한자/키릴
  문자는 스크립트로 확실히 구분되고, 독일어/스페인어/영어는 같은
  라틴 문자라 ä/ö/ü/ß, ñ/¿/¡ 같은 특수문자로만 구분 — 이 셋을 완벽히
  가르는 방법은 아니라서 어느 쪽에도 안 걸리면 영어로 취급). 캐릭터별
  Voice Profile은 아직 없어서 이 언어 감지로 대신한다 (기획서 21/25장)
- SAPI 자체의 비동기 재생 + 알림 메시지(`SetNotifyWindowMessage`)를
  쓰기 때문에 재생용 스레드를 따로 만들 필요가 없다
- **입 애니메이션**은 레이어를 분리한 진짜 마우스 아트가 아직 없어서
  (기획서 14장 "중기" 단계 예정) 말하는 동안 얼굴 근처에 작은
  사운드바(막대 3개)가 깜빡이는 것으로 대체했다 — 실제 구현은
  `rendering::WithTalkingIndicator`가 매 180ms마다 현재 스프라이트
  복사본에 GDI+로 막대를 합성해서 보여준다
  - 여기서 실제 버그 하나 발견: GDI+ `Bitmap`이 **외부에서 감싼**
    premultiplied 메모리에 직접 `FillRectangle`을 그리면 아무것도
    그려지지 않는다(에러도 없이 조용히 무시됨). 별도의
    GDI+ 소유 비트맵에 그린 뒤 `LockBits`로 읽어서 수동으로
    알파 합성하는 방식으로 고쳤다
- 시작/종료 이벤트로 실제 TTS 재생 시간에 맞춰 사운드바가 뜨고 사라지는
  것까지 실제 API 응답으로 화면 캡처해서 확인했다

**언어별 검증 상태** (이 개발 머신 기준 — 다른 머신은 설치된 SAPI
보이스에 따라 다름):

| 언어 | 감지 | 실제 음성 전환 확인 |
| --- | --- | --- |
| 한국어 | O | O (Microsoft Heami) |
| 영어 | O | O (Microsoft Zira) |
| 독일어 | O | O (Microsoft Hedda) |
| 러시아어 | O | O (Microsoft Irina) |
| 일본어 | O | X — 이 머신에 보이스 없음, 경고 로그 남기고 마지막 보이스로 폴백 |
| 중국어 | O | X — 위와 동일 |
| 스페인어 | O | X — 위와 동일 |

7개 언어 각각 실제 API로 그 언어 응답을 받아서 로그로 확인했다 —
설치된 4개는 정확히 그 언어 보이스로 전환됐고, 없는 3개는 크래시 없이
경고만 남기고 직전 보이스로 계속 읽었다(예: 일본어 텍스트를 러시아어
보이스로 읽음 — 알아들을 수 없지만 안전하게 폴백은 됨). 일본어/중국어/
스페인어를 실제로 그 언어 음성으로 들으려면 Windows 설정에서 해당
언어팩(음성 인식이 아니라 음성 합성 쪽)을 설치해야 한다.

**사용해보고 나온 버그 세 개도 고쳤다:**

- **마크다운/이모지를 그대로 읽던 문제**: `audio/SpeakableText::MakeSpeakable()`가
  TTS로 보내기 직전에만 `**굵게**`, `` `코드` ``, `# 제목`, `[링크](url)`,
  이모지/기호(정규식 + 서로게이트 쌍 기반)를 제거한다. 말풍선에 보이는
  텍스트는 원문 그대로 — 실제 응답으로 "**좋아!** ... 😊"가
  "좋아! ..."로만 읽히는 것 확인
- **드래그 중 말풍선이 안 따라오던 문제**: 캐릭터 창이 움직여도 말풍선
  위치를 갱신하는 코드가 아예 없었다. `WM_MOVE`(캡션 드래그 루프 중에도
  계속 발생함)에서 `ChatBubble::Reposition()`을 호출하도록 추가 —
  캐릭터를 (-150,-100)만큼 옮기면 말풍선도 정확히 같은 만큼 따라가는 것
  확인
- **긴 대사 도중 말풍선이 먼저 사라지던 문제**: 기존엔 글자 수로 유지
  시간을 "추측"해서 최대 15초로 캡을 걸었는데, 실제 TTS는 21~45초까지도
  걸렸다. 이제 그 추측 타이머는 TTS를 못 쓸 때만 쓰는 안전장치(45초 캡)로
  남기고, 실제 발화 종료 이벤트가 오면 `ChatBubble::RescheduleDismiss()`로
  타이머를 다시 걸어 짧은 유예 시간(2.5초) 뒤에 닫히게 했다 — 실제 45초짜리
  응답으로 끝까지 떠 있다가 종료 이벤트 후 정확히 닫히는 것 확인. 새 메시지를
  보내거나 수동으로 닫으면 읽던 음성도 즉시 끊는다

다음은 Phase 6(Desktop Context)이나, 다른 머신에서 STT까지 마저 할지
선택.

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
{ "endpoint": "https://factchat-cloud.mindlogic.ai/v1/gateway/chat/completions/", "model": "실제 모델명으로 교체" }

// config/secrets.local.json (.gitignore 처리됨 — 직접 만들어서 채우기)
{ "api_key": "실제 키로 교체" }
```

`endpoint`는 실제 completions 라우트까지 정확히 가리켜야 한다 —
`/v1/gateway`만 쓰면 404가 나거나(게이트웨이마다 다름) 응답이 안 온다.

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
  audio/        TTS (SAPI); STT는 이 머신에 언어 인식 모델이 없어서 보류
content/        캐릭터 및 아이템 패키지 (코드 아닌 데이터)
assets/         공용 에셋
config/         런타임 설정
tests/          테스트
tools/          에셋 생성 등 개발용 스크립트 (빌드에 포함 안 됨)
third_party/    벤더링한 헤더 전용 라이브러리 (nlohmann/json)
```

이 구조를 정한 근거는 기획서 47장, 전체 Phase 로드맵(Phase 0~13)은
52장을 참고.
