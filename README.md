# Sveta Assistant

Windows Desktop AI Companion / Interactive Character Platform.
전체 기획서: `docs/Sveta_Assistant_Integrated_Project_Plan.docx`.

## 진행 상황

Phase 0(Foundation), Phase 1(Desktop Character), Phase 2(Interaction),
Phase 3(Character Life), Phase 4(AI Conversation), Phase 5(Voice — TTS만,
아래 참고), Phase 6(Desktop Context — 아래 참고), Phase 7(Proactive AI —
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
  - 5분간 상호작용 없으면 Sleeping/Sleepy로 전환, 커서가 다시
    다가오면 기상 (기획서 6장 시나리오) — 처음엔 60초였는데 정상적으로
    작업하다가도 계속 잠들어서 5분으로 늘림
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
| 일본어 | O | O (Microsoft Haruka) — Windows 설정에서 언어팩 추가 설치 후 확인 |
| 중국어 | O | O (Microsoft Huihui) — 위와 동일 |
| 스페인어 | O | O (Microsoft Helena) — 위와 동일 |

목표 7개 언어 모두 실제 API로 그 언어 응답을 받아서 로그로
확인했다(요청마다 다른 언어로만 답해달라고 명시) — 매번 정확히 그
언어 보이스로 전환되고, "설치된 보이스 없음" 경고 없이 TTS가
끝까지 재생되는 것을 확인했다. 일본어/중국어/스페인어는 설정 → 시간 및 언어 → 음성 → 음성 관리 →
음성 추가에서 해당 언어 음성을 추가 설치해 해결했다.

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

**데스크톱 인식(Desktop Context)**도 붙었다 (기획서 17장 Perception):

- 지금 사용자가 어떤 창을 보고 있는지, 그 창에 뭐가 떠 있는지를 AI 채팅
  시스템 프롬프트에 한 줄로 얹는다. 원화면 스크린샷/Vision은 이번엔
  빼고, "가능하면 Screenshot보다 구조화된 UI 데이터를 우선한다"(기획서
  17장)는 원칙에 따라 UI Automation 텍스트만 얕게(자식 요소까지만,
  최대 12개, 800자 캡) 읽어오는 것으로 스코프를 줄였다 — 화면 전체를
  캡처하는 것보다 훨씬 가볍고, 화면에 뭐가 "보이는지"보다 뭐가
  "떠 있는지" 위주라 프라이버시 부담도 적다
- `context/ActiveWindowTracker`가 `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`로
  포그라운드 창이 바뀔 때마다 이벤트 기반으로(폴링 없이) 알림을 받는다
- `context/UiAutomationReader`가 그 창의 UI Automation 트리에서 자식
  요소들의 `Name`을 얕게 훑어서 요약 텍스트를 만든다(화면 밖
  `IsOffscreen` 요소는 제외). 느릴 수 있는 COM 호출이라 백그라운드
  스레드에서 실행하고, 다 끝나기 전에 창이 또 바뀌면 세대(generation)
  번호로 낡은 결과를 버린다 — AI 응답/TTS에서 이미 쓰던 것과 같은
  "백그라운드 스레드 → `PostMessage` → UI 스레드에서 재조립" 패턴
- `config/privacy_config.json`(`screen_awareness_enabled`,
  `excluded_processes`)으로 기능 자체를 끄거나 특정 프로세스(예:
  비밀번호 관리자)를 제외할 수 있다
  - 주의: `excluded_processes`는 기본값이 빈 배열이다. 실제로 메모장에
    민감한 내용(API 키 메모)을 띄워둔 상태로 테스트해보니 그 창 제목이
    그대로 로그와(대화 중이라면) AI 컨텍스트에 들어가는 것을 확인했다.
    메모장, 비밀번호 관리자 등 창 제목에 민감한 게 뜰 수 있는
    프로그램은 이 목록에 직접 추가해서 제외하는 걸 권장한다
- `context/ContextEngine::BuildContextLine()`이 이걸 모아
  `(사용자는 지금 "제목" (process.exe) 창을 보고 있다. 화면에 보이는
  내용: ...)` 같은 한 줄로 만들고, `MainWindow::OnMessageSubmitted()`가
  매 메시지마다 페르소나 시스템 프롬프트 뒤에 추가 시스템 메시지로
  끼워 넣는다
- 실제로 이 개발 머신에서 크롬으로 유튜브를 보고 있는 상태로 실행해서
  확인 — 로그에 실제 영상 제목("...근황 - YouTube - Chrome",
  chrome.exe)과 UI Automation으로 읽어온 화면 텍스트(106자)가 정확히
  찍히는 것을 확인했다
- 실제 버그 하나 발견: 앱을 켠 직후 사용자가 창을 한 번도 안 바꾸면
  `ContextEngine`이 아무 스냅샷도 못 만들어서(포그라운드 "변경" 이벤트만
  기다리고 있었음) 첫 채팅에 컨텍스트가 비어 있었다. `Create()`에서
  `ActiveWindowTracker::GetCurrentWindowInfo()`로 현재 포그라운드 창을
  즉시 한 번 읽어와 시드하도록 고쳤다
- 빌드 중 실제로 걸린 문제: `<UIAutomationClient.h>`는 COM의 `interface`
  매크로(`struct`로 치환되는)가 이미 정의돼 있다고 가정하고 자기
  자신은 그걸 정의하지 않는다 — 이 프로젝트는 `<windows.h>`만 두고
  `<objbase.h>`를 따로 안 걸어서, 매크로가 치환되지 않은 채
  `typedef interface X X;`가 그대로 파싱되며 수백 줄짜리 C2146/C2371
  연쇄 오류가 났다. `<objbase.h>`를 먼저 include해서 해결

STT/Push-to-Talk는 이번에도 뺐다 — Phase 5와 같은 이유(이 개발
머신엔 한국어/영어 음성 인식 엔진이 없음)로, 다른 머신에서 언어팩을
설치한 뒤 이어서 하기로 함.

**게임할 때 같이 놀아주기**도 Phase 6 위에 얹었다 (정식 Phase는 아니고,
Desktop Context를 캐릭터 행동에 직접 연결한 작은 확장 — 기획서 40장
Game Integration처럼 게임 SDK 훅을 붙이는 건 아니고, 지금 하고 있는
게 게임인지 "추측"만 해서 캐릭터 표정만 바꾼다):

- `context/GameDetector`가 두 가지 방법을 같이 쓴다: (1)
  `config/games_config.json`에 등록된 프로세스 이름과 일치하는지(리그
  오브 레전드, 발로란트, 마인크래프트 등 흔한 게임 몇 개를 기본으로
  넣어뒀고, 목록에 없는 게임은 사용자가 직접 추가하면 됨), (2) 창이
  모니터 전체를 정확히 덮고 `WS_CAPTION`이 없는(=제목표시줄 없는
  전체화면 독점 모드) 경우 — 목록에 없는 게임도 이 휴리스틱으로
  잡힐 수 있다. 브라우저/IDE를 최대화해도 제목표시줄은 남아있어서
  오탐은 안 남
- `ContextEngine::IsGaming()`이 활성 창이 바뀔 때마다 갱신되고,
  `MainWindow`가 1초 틱마다 `CharacterState::SetGamingContext()`로
  전달한다. 창 제목/화면 텍스트는 전혀 안 읽으니(프로세스명 + 창
  크기만 확인) `screen_awareness_enabled`를 꺼놔도 이 기능은 그대로
  동작한다
- `character/Action::PlayingGame`(원래 기획서 11장 15개 액션엔 없던
  것 — 이번에 추가)과 `Emotion::Excited`로 표시되고, 전용 스프라이트
  `playing_game.png`를 새로 만들었다 — 지금까지와 같은 방식으로
  `calm.png` 위에 게임패드 아이콘을 절차적으로 합성(240px로 축소돼도
  잘 읽히도록 디테일은 최소화: 알약 모양 몸체 + 큰 원 2개)
- 게임 중엔 5분 Idle-to-sleep 타임아웃과 랜덤 Idle 행동(책 읽기,
  마시기 등)이 멈추고 `PlayingGame` 포즈를 유지한다. 그래도 호버/
  쓰다듬기/드래그/채팅은 그대로 우선권을 가져서, 게임 중에 캐릭터를
  건드리면 평소처럼 반응하고 손을 떼면 다시 게임 포즈로 돌아간다
- 실제로 확인: `games_config.json`에 `notepad.exe`를 임시로 등록해서
  메모장을 포그라운드로 가져와봤더니(진행 중이던 Idle 행동이 끝난
  직후) `Action -> PlayingGame`, `Emotion -> Excited`,
  `playing_game.png` 로드까지 로그로 확인했고, 메모장을 닫으니
  `Idle`/`Calm`으로 정확히 돌아오는 것도 확인했다 — 확인 끝나고
  `notepad.exe`는 목록에서 다시 뺐다

**선제적 발화(Proactive AI)**도 붙었다 (기획서 18장 Proactive Assistant):

- 캐릭터가 사용자 질문을 기다리지 않고 특정 상황에서 먼저 말을 걸 수
  있다. 다만 방해를 최소화하려고 기획서가 정의한 Interruption Score
  표(`proactive/InterruptionScore`)를 그대로 가져왔다 — 일반 화면 변경
  0.05, 앱 실행 0.10, 작업 성공 0.30, 새 오류 0.60, 동일 오류 반복
  0.82, 중대한 문제 0.95, 기본 Threshold 0.75. 지금 인프라(창 제목 +
  얕은 UI Automation 텍스트, 빌드 시스템/IDE 연동 없음)로 신뢰성 있게
  구분할 수 있는 건 "동일 오류 반복"뿐이라, **이번엔 그 트리거 하나만
  구현**했다 — 나머지는 표만 코드에 남겨뒀고 다음에 트리거를 추가할 때
  참고하라고 남겨둔 것
- `proactive/ErrorRepeatDetector`가 활성 창의 제목+얕은 UI 텍스트에서
  오류 키워드(error/exception/failed/fatal/crash/오류/실패/에러)를
  찾고, 같은 신호(제목+텍스트 조합)가 "화면에서 사라졌다가 다시
  나타나면" 반복으로 판정한다 — 같은 오류 창이 그냥 계속 떠 있는 것과
  구분하기 위해 "떠난 뒤 재등장"만 카운트함
- 반복이 감지되면 `ContextEngine`이 상황 설명을 한 문장으로 만들고,
  `MainWindow::StartProactiveSpeech()`가 사용자 메시지 없이(페르소나
  시스템 프롬프트 + 상황 설명만으로) 실제 AI를 호출한다 — 채팅
  흐름(`OnMessageSubmitted`)과 같은 네트워크 코드를
  `SendChatRequestAsync()`로 공유. 응답은 평소 채팅 응답과 똑같이
  말풍선 + TTS로 나온다
- 스팸 방지로 10분 쿨다운(`kProactiveCooldown`, UX 튜닝 전 임시값)을
  뒀고, 이미 채팅 중이거나 말풍선이 떠 있으면 트리거 자체를 건너뛴다
- 검증: 실제 창 제목을 직접 지정할 수 있는 작은 테스트 창을 하나
  만들어서(메모장은 Windows 11에서 탭형이라 창 제목이 파일명을 안
  보여줘서 이 방법을 씀) "test error window"라는 제목으로 두 번
  띄웠더니(중간에 딴 창으로 전환), 로그에 `same error repeated
  (score=0.82 >= threshold=0.75)` → `Proactive speech triggered` →
  실제 AI POST(200) → `Action -> Talking` → 실제 TTS 재생 11초 →
  자동 종료까지 정확히 찍히는 것을 확인했다. 탐지 알고리즘 자체는
  별도로 4가지 시나리오(기본 반복, 서로 다른 오류가 오탐 안 나는지,
  일반 텍스트에서 오탐 안 나는지, 한글 키워드)로 독립 컴파일해서
  단위 테스트도 통과시켰다

다른 머신에서 STT까지 마저 할지가 다음 선택지.

머리 히트박스는 현재 스프라이트 크기에 비례한 근사치(상단 50%,
가운데 72% 너비)다. 캐릭터마다 다른 정확한 히트박스는 추후
Character Package(character.json, 기획서 24~25장)에서 다룰 예정.
Idle-to-sleep 타임아웃(현재 5분)도 정식 UX 튜닝 전 값이다 — 60초였을 땐
정상 작업 중에도 계속 잠들어서 5분으로 늘렸다.

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
  context/      데스크톱 인식(활성 창 추적, UI Automation 얕은 텍스트 읽기)
  proactive/    선제적 발화(Interruption Score, 동일 오류 반복 감지)
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
