# Sveta Assistant

Windows Desktop AI Companion — 화면 위에 상주하며 사용자를 바라보고, 반응하고,
대화하고, 필요할 때 도와주는 인터랙티브 캐릭터 플랫폼.

> 전체 기획서: [`docs/Sveta_Assistant_Integrated_Project_Plan.docx`](docs/Sveta_Assistant_Integrated_Project_Plan.docx)

## 목차

- [진행 상황](#진행-상황)
- [시작하기](#시작하기)
- [기능](#기능)
  - [캐릭터 창 & 렌더링](#캐릭터-창--렌더링)
  - [마우스 상호작용](#마우스-상호작용)
  - [감정 · 행동 · 성격](#감정--행동--성격)
  - [AI 대화](#ai-대화)
  - [음성 (TTS)](#음성-tts)
  - [데스크톱 인식](#데스크톱-인식)
  - [게임 인식과 반응](#게임-인식과-반응)
  - [리그 오브 레전드 실시간 컨텍스트](#리그-오브-레전드-실시간-컨텍스트)
  - [선제적 발화](#선제적-발화)
  - [기억 시스템](#기억-시스템)
  - [트레이 아이콘 & 설정 창](#트레이-아이콘--설정-창)
- [프로젝트 구조](#프로젝트-구조)
- [알려진 제한사항](#알려진-제한사항)

## 진행 상황

기획서 52장 로드맵(Phase 0~13) 기준. Phase 8까지 완료했고, Phase 6 위에
정식 Phase는 아닌 확장(게임 인식/반응, 리그 오브 레전드 연동)도 얹었다.

| Phase | 내용 | 상태 |
| --- | --- | --- |
| 0 | Foundation | 완료 |
| 1 | Desktop Character | 완료 |
| 2 | Interaction | 완료 |
| 3 | Character Life | 완료 |
| 4 | AI Conversation | 완료 |
| 5 | Voice | 부분 완료 — TTS만, STT/Push-to-Talk 보류 |
| 6 | Desktop Context | 부분 완료 — Screen Capture/Vision 제외 |
| 7 | Proactive AI | 부분 완료 — 트리거 1개(동일 오류 반복)만 |
| 8 | Memory | 부분 완료 — Daily/Long-Term만, "중요 대화" 추출은 보류 |
| 9~13 | Item / Furniture / Multi-Character / Content Platform / Polish | 예정 |

STT가 보류된 이유는 이 개발 머신에 한국어/영어 음성 *인식* 엔진이 설치되어
있지 않기 때문이다(TTS 목소리는 있음). 다른 머신에서 Windows 언어팩을
설치한 뒤 이어서 할 예정.

## 시작하기

### 사전 준비물

- Visual Studio 2022 이상, "C++를 사용한 데스크톱 개발" 워크로드 (MSVC 툴셋 + Windows SDK)
- CMake 3.28 이상
- Git

### 빌드

```
cmake -B build -S .
cmake --build build --config Debug
```

또는 Visual Studio에서 폴더 열기(File > Open > Folder)로 열면 CMake를
네이티브로 통합해서 사용할 수 있다.

### AI 채팅 설정 (선택)

`config/ai_config.json`의 `model`과 `config/secrets.local.json`의 `api_key`를
채워야 실제로 응답이 온다. 안 채워도 앱은 정상 작동하고, 더블클릭하면
설정이 안 됐다는 안내 말풍선만 뜬다.

```json
// config/ai_config.json (커밋됨 — 민감 정보 없음)
{ "endpoint": "https://factchat-cloud.mindlogic.ai/v1/gateway/chat/completions/", "model": "실제 모델명으로 교체" }

// config/secrets.local.json (.gitignore 처리됨 — 직접 만들어서 채우기)
{ "api_key": "실제 키로 교체" }
```

`endpoint`는 실제 completions 라우트까지 정확히 가리켜야 한다 —
`/v1/gateway`만 쓰면 404가 나거나(게이트웨이마다 다름) 응답이 안 온다.
`ai/ChatClient`는 OpenAI Chat Completions 호환 스키마
(`{model, messages}` 요청 / `choices[0].message.content` 응답)를 쓰는
서비스면 어디든 붙는다.

## 기능

### 캐릭터 창 & 렌더링

- 테두리 없음·항상 위·픽셀 단위 투명 창(레이어드 윈도우)에 PNG 스프라이트를
  WIC로 디코딩해 렌더링. 원본 아트는 화면에 보일 최대 크기(240px)로 자동
  축소한다 (`rendering/Sprite`)
- 드래그로 이동, 위치는 `%LOCALAPPDATA%\SvetaAssistant\`에 저장(`window/WindowPosition`)
  하고 다음 실행 시 복원한다
  - 저장된 위치가 현재 연결된 모니터 중 어디에도 없으면(모니터 구성이
    바뀐 경우 등) 기본 위치로 안전하게 대체한다 — `MainWindow::IsPositionOnAnyMonitor`
- 커서가 캐릭터 위에 들어오고 나가는 것을 추적(Hover/Leave)한다. 레이어드
  창의 알파 채널 기반 히트테스트 덕분에 실루엣 바깥은 클릭도 자연스럽게
  통과된다

### 마우스 상호작용

- 머리(헬멧) 영역에 비례 기반 히트박스 계산 (`interaction/HeadHitbox`) —
  현재는 스프라이트 크기에 비례한 근사치(상단 50%, 가운데 72% 너비)이고,
  캐릭터별 정확한 히트박스는 추후 Character Package에서 다룰 예정
- 히트박스 안에서 좌우로 반복 스와이프하면 쓰다듬기로 인식
  (`interaction/PettingDetector`) — 적당한 속도, 짧은 시간(1.2초) 내 방향
  전환 5회 이상 조건 (기획서 8장 원안은 3회였는데, 이마 쪽에서 살짝
  2번만 왔다갔다 해도 인식돼버린다는 피드백을 받고 좀 더 확실하게
  쓰다듬어야 인식되도록 올렸다)

### 감정 · 행동 · 성격

`character/CharacterState`가 Emotion(10종)·Action(16종 — 기획서 11장의
15종 + 게임 반응용 `PlayingGame`)·Personality(기획서 12장 수치 그대로:
Affection 0.90 등)를 결합해 캐릭터 행동을 관리한다 (기획서 9장
"Emotion + Action + Personality + Environment = Character Behavior").

- 호버 → Curious + LookingAtCursor, 쓰다듬기 → Happy + BeingPetted, 드래그
  시작 → Surprised + Dragged. 진행 중이던 리액션은 드래그가 가로챈다
- 5분간 상호작용이 없으면 Sleeping/Sleepy로 전환, 커서가 다가오면 기상한다
  (기획서 6장). *구현 메모: 처음엔 60초였는데 정상적으로 작업하다가도
  계속 잠들어서 5분으로 늘렸다*
- 1초 간격 타이머로 `behavior/IdleBehavior`가 성격 가중치를 반영해 확률적으로
  Idle 행동을 고른다(LookAround/Blink/Yawn/Stretch/Move/SitAtBottom/Read/
  Drink/Doze/PlayWithItem, 기획서 13장) — Playfulness가 높을수록
  Move/PlayWithItem이, Curiosity가 높을수록 LookAround가 더 잘 나온다

**감정별 스프라이트**: 실제 원화가 `calm.png` 하나뿐이라 나머지 9개
감정(happy/excited/curious/sad/annoyed/embarrassed/sleepy/concerned/surprised)은
`calm.png` 위에 절차적 오버레이(블러시, 눈물, 땀방울, 반짝임, Z, ?, !,
화남 표시)를 합성해서 만들었다. `character/Emotion::SpriteFileName()`이
파일명을 매핑하고, `MainWindow::SyncSpriteToEmotion()`이 현재 감정/액션이
바뀌었을 때만 다시 로드한다(파일이 없으면 calm.png로 폴백). 오버레이
생성 스크립트는 [`tools/generate_emotion_sprites.py`](tools/generate_emotion_sprites.py)
(Python + Pillow) — `calm.png`가 바뀌거나 아이콘을 조정하고 싶으면 이
스크립트를 고쳐서 다시 실행하면 된다.

### AI 대화

Idle → Listening → Thinking → Talking → Idle 흐름 (기획서 19장, 20장 AI
Engine, 54장 두 번째 MVP).

- 캐릭터를 더블클릭하면 말풍선 모양 입력창이 뜬다. Enter로 전송, Esc로 취소
- `ai/ChatClient`가 WinHTTP로 설정된 엔드포인트에 POST. JSON은
  `third_party/nlohmann/json.hpp`(벤더링, MIT) 사용
- 네트워크 호출은 백그라운드 스레드(`MainWindow::SendChatRequestAsync`)에서
  실행되고 결과는 `PostMessage`로 UI 스레드에 돌려준다 — 응답을 기다리는
  동안에도 Idle 애니메이션, 드래그, 쓰다듬기가 그대로 동작한다
- 성격 수치를 반영한 시스템 프롬프트(`ai/Persona`)와 세션 내 대화 기록(최근
  20턴, 재시작하면 사라짐 — 기획서의 Session 메모리 계층. 재시작을 버텨야
  하는 Daily/Long-Term 계층은 [기억 시스템](#기억-시스템) 참고)
- **개인화**: [설정 창](#트레이-아이콘--설정-창)에서 지정한 사용자 이름,
  "어떻게 대해줬으면 좋겠는지" 메모, Sveta의 주 사용 언어가 매번
  시스템 프롬프트에 그대로 들어간다(`ai/UserProfile`, `config/user_profile.json`)
  — 셋 다 비워두면 프롬프트에서 조용히 생략된다(주 언어는 기본값
  "한국어"로 항상 명시)

**말풍선 UI**: GDI+로 직접 그린 둥근 말풍선(꼬리, 그림자, Segoe UI)이
레이어드 창으로 뜬다. 입력 중일 때만 예외적으로 진짜 Edit 컨트롤을 쓰는데
— `UpdateLayeredWindow`로 그리는 창은 일반 자식 컨트롤을 위에 합성하지
못한다는 Win32 제약 때문에, 그 순간만 레이어드를 끄고 둥근 리전
(`SetWindowRgn`)으로 대체한다. GDI+ 폰트 서브시스템은 프로세스당 최초 1회
초기화가 느려서(이 환경에서 ~5초, 이후 매번 ~4ms) 앱 시작 시 백그라운드
스레드에서 미리 예열해둔다.

- **입력창 자동 확장**: 예전엔 높이가 46px 고정이라 긴 문장을 쓰면
  자동 스크롤로 앞부분이 화면 밖으로 밀려나 안 보였다. 지금은 입력하는
  동안 텍스트 양에 맞춰 캐릭터 쪽 꼬리는 고정한 채 위쪽으로 커진다
  (`ChatBubble::ResizeInputBox()`, 최대 200px)
- **응답 말풍선 이모지**: GDI+가 쓰는 "Segoe UI" 폰트엔 컬러 이모지
  글리프가 없어서, AI 응답에 이모지가 섞이면 깨진 네모(tofu box)로
  보였다. 표시용 텍스트에서만 이모지를 걸러낸다
  (`audio::StripUnrenderableSymbols()`) — 마크다운은 그대로 두고, TTS용
  필터(`MakeSpeakable()`)와는 별개 함수다

### 음성 (TTS)

기획서 21장 Voice System. STT/Push-to-Talk는 이 개발 머신에 음성 인식
엔진이 없어서 보류 중. TTS는 두 백엔드 중 하나를 고를 수 있고, 둘 다
`audio::ITextToSpeech`를 구현해서 `MainWindow`는 어느 쪽이 말하는지
신경 쓰지 않는다(`audio/TextToSpeechFactory`가 아래 설정에 따라 고른다).

- **로컬 SAPI 보이스** (`audio/TextToSpeech`, 기본값): 무료, 네트워크
  불필요, Windows에 이미 설치된 보이스만 씀. 음질은 다소 기계적
- **Google Cloud TTS — Chirp 3: HD** (`audio/GoogleTextToSpeech`, 권장):
  훨씬 자연스러운 신경망 음성. 요청마다 네트워크 호출이 생기지만, 이미
  AI 채팅도 네트워크 호출이라 구조적으로는 자연스럽게 들어맞는다

`config/tts_config.json`의 `"provider"`를 `"google"` 또는 `"sapi"`로
설정한다. `"google"`인데 인증 정보가 없으면 자동으로 SAPI로 폴백한다
(경고 로그만 남기고 계속 동작). 이 파일의 모든 필드는
[설정 창](#트레이-아이콘--설정-창)에서 직접 편집할 수도 있다 — 손으로
JSON을 고칠 필요 없음.

```json
// config/tts_config.json (커밋됨 — 민감 정보 없음)
{
    "provider": "google",
    "volume_percent": 100,
    "google_voices": {
        "Korean": "ko-KR-Chirp3-HD-Leda",
        "English": "en-US-Chirp3-HD-Kore"
        // ... 나머지 5개 언어도 마찬가지 형태
    }
}
```

- `volume_percent`(1~100)는 Google 쪽은 `audioConfig.volumeGainDb`로
  변환해서(50% ≈ -6dB, 진폭-데시벨 표준 공식) 보내고, SAPI 쪽은
  `ISpVoice::SetVolume`에 그대로 넘긴다 — 어느 제공자를 쓰든 같은
  설정 하나로 조절된다
- `google_voices`의 값은 Chirp 3: HD 보이스 30종 중 아무거나 언어별로
  자유롭게 바꿔 끼울 수 있다. 이 30종 이름은 추측이 아니라 Cloud TTS의
  `voices.list` 엔드포인트를 실제로 호출해서(`GET
  /v1/voices?languageCode=ko-KR`) 성별까지 확인한 것 — 설정 창의 언어별
  음성 칸이 이 목록을 드롭다운으로 보여준다. 한국어 기본값은 "귀여운
  느낌"을 요청받아 Google 문서 기준 "Youthful" 톤인 **Leda**(여성)로
  맞춰뒀다

**인증은 API 키가 아니라 서비스 계정(ADC)이다** — 이 프로젝트의 Google
Cloud 프로젝트는 조직 정책으로 API 키 발급 자체가 막혀 있어서, 정적
키 하나 붙이는 방식이 애초에 안 된다. 대신:

1. Google Cloud Console → IAM 및 관리자 → 서비스 계정 → 서비스 계정
   만들기(또는 기존 것 사용)
2. 해당 계정의 "키" 탭 → 키 추가 → 새 키 만들기 → **JSON** 선택 →
   다운로드
3. 다운로드한 파일을 `config/google_service_account.json`으로 저장
   (`.gitignore` 처리되어 있어 커밋되지 않는다)

**"서비스 계정 키 생성 사용 중지됨" 에러가 뜨면**: `iam.disableServiceAccountKeyCreation`
조직 정책이 걸려 있는 것이다 — 신규 프로젝트에 Google이 기본으로 까는
"보안 우선 기본 설정"의 일부일 수 있다. 개인(비-Workspace) Gmail
계정이면 대개 소유자 권한으로 직접 풀 수 있다:

```bash
gcloud organizations list   # 조직 숫자 ID 확인
gcloud resource-manager org-policies disable-enforce \
    constraints/iam.disableServiceAccountKeyCreation --organization=<숫자ID>
```

(`gcloud org-policies delete`는 "명시적으로 설정된 정책이 없다"며
`NOT_FOUND`를 내는데, 이건 지우려는 게 아니라 기본값을 명시적으로
덮어써야 하는 상황이라 그렇다 — 위처럼 `disable-enforce`를 써야 한다.)

앱은 이 JSON의 `private_key`로 JWT를 직접 서명해서(RS256) OAuth2 액세스
토큰을 발급받고, 그걸로 Text-to-Speech API를 호출한다
(`audio/GoogleOAuthTokenProvider`) — API 키 방식보다 코드는 복잡하지만
사용자 쪽에서 할 일은 파일 하나 내려받아 저장하는 것뿐이다.

공통 부분:

- `audio/LanguageDetection`이 응답 텍스트를 7개 목표 언어(한국어/영어/
  일본어/중국어/스페인어/독일어/러시아어 — 기획서 21장 원래 목표 4개에
  중국어/스페인어/독일어 추가) 중 하나로 분류해서 매번 그 언어에 맞는
  보이스를 다시 고른다(SAPI/Google 둘 다 이 결과를 그대로 씀). 한글/
  가나/한자/키릴 문자는 스크립트로 구분되고, 독일어/스페인어/영어는
  같은 라틴 문자라 특수문자(ä/ö/ü/ß, ñ/¿/¡)로만 구분한다 — 완벽하진
  않아서 어느 쪽에도 안 걸리면 영어로 취급
- **입 애니메이션**은 마우스 아트가 아직 없어서(기획서 14장 "중기" 단계
  예정), 말하는 동안 얼굴 근처에 사운드바가 깜빡이는 것으로 대체했다
  (`rendering::WithTalkingIndicator`, 180ms 간격)

**SAPI 언어별 검증 상태** (이 개발 머신 기준 — 다른 머신은 설치된 SAPI
보이스에 따라 다름):

| 언어 | 감지 | 실제 음성 전환 |
| --- | --- | --- |
| 한국어 | O | O (Microsoft Heami) |
| 영어 | O | O (Microsoft Zira) |
| 독일어 | O | O (Microsoft Hedda) |
| 러시아어 | O | O (Microsoft Irina) |
| 일본어 | O | O (Microsoft Haruka) |
| 중국어 | O | O (Microsoft Huihui) |
| 스페인어 | O | O (Microsoft Helena) |

일본어/중국어/스페인어 보이스는 설정 → 시간 및 언어 → 음성 → 음성 관리 →
음성 추가에서 설치한다.

**구현 메모**:
- 마크다운은 TTS 직전에만 제거된다(`audio/SpeakableText::MakeSpeakable()`) —
  말풍선에 보이는 텍스트는 마크다운까지 원문 그대로. 이모지는 [위에서
  설명한 대로](#ai-대화) TTS용과 말풍선 표시용, 두 군데 모두에서 각자
  걸러진다(전자는 `MakeSpeakable()`, 후자는 `StripUnrenderableSymbols()`)
- 말풍선은 `WM_MOVE`마다 위치를 다시 계산해 캐릭터를 드래그해도 따라온다
  (`ChatBubble::Reposition()`)
- 말풍선 유지 시간은 실제 TTS 종료 이벤트로 결정된다
  (`ChatBubble::RescheduleDismiss()`, 종료 후 2.5초 유예) — 글자 수 기반
  추측 타이머는 TTS를 못 쓸 때(또는 실패했을 때)만 쓰는 안전장치다.
  *구현 메모: 이 안전장치 타이머가 원래 `Speak()` 호출 "직후" 짧게
  잡혀 있어서, Google TTS의 네트워크 왕복(토큰 발급+합성 요청)이 끝나기도
  전에 먼저 만료돼 `Stop()`을 호출 — 재생이 문장 중간에 끊기는 버그가
  있었다. TTS로 말할 예정이면 이 타이머를 훨씬 넉넉하게(최대 2분) 잡도록
  고쳐서 항상 실제 종료 이벤트가 먼저 이기게 했다*
- 감지 후 걸러내는 이모지/기호 목록에 `~`(물결표) 계열(전각 물결,
  웨이브 대시 등)도 추가했다 — TTS가 이걸 "물결표"라고 그대로 읽던
  문제. 다만 통째로 지우면 "3~5개"가 "35개"로 붙어버려서, 지우는 대신
  공백으로 치환한다
- GDI+ `Bitmap`이 외부에서 감싼 premultiplied 메모리에 직접
  `FillRectangle`을 그리면 에러 없이 조용히 무시된다 — 별도의 GDI+ 소유
  비트맵에 그린 뒤 `LockBits`로 수동 알파 합성하는 방식으로 우회
- **Google Cloud TTS 재생**: SAPI와 달리 OS가 재생 완료를 알려주지
  않아서, `LINEAR16`으로 요청해 응답에 포함된 WAV 헤더에서 재생 시간을
  정확히 계산하고(추측이 아니라 헤더의 데이터 크기/샘플레이트로 계산한
  정확한 값) 그 시간만큼 백그라운드 스레드에서 대기한 뒤 "종료" 신호를
  보낸다. 재생 자체는 `PlaySoundW(SND_MEMORY)`로 메모리에서 바로 재생
  — 임시 파일도 안 만들고 MP3 디코더도 필요 없다
- **서비스 계정 인증**: PKCS#8 PEM 개인키를 `CryptStringToBinaryA` →
  `CryptDecodeObjectEx`(PKCS#8 해제 → PKCS#1 파싱)로 벗겨서 레거시
  CAPI `PRIVATEKEYBLOB`을 얻고, 필드별로 바이트 순서를 뒤집어(CAPI는
  리틀엔디언, BCrypt는 빅엔디언 — 필드 순서 자체는 동일) CNG용
  `BCRYPT_RSAFULLPRIVATE_BLOB`으로 변환한 뒤 `BCryptSignHash`로
  RS256 서명한다. 이 변환 로직은 개발 중 .NET의 `RSA.SignData`로
  독립적으로 만든 서명과 바이트 단위로 완전히 일치하는 것까지
  확인했다. 실제 Google OAuth 서버(`oauth2.googleapis.com/token`)에도
  요청을 보내봐서, 존재하지 않는 테스트용 서비스 계정으로도
  `"invalid_grant: account not found"`라는(형식 오류가 아니라 계정
  조회 단계까지 도달했다는 뜻) 정상적인 응답을 받는 것까지 확인했다.
  이후 실제 등록된 서비스 계정으로 전체 흐름(토큰 발급 → 합성 요청 →
  재생)이 끊김 없이 도는 것까지 검증 완료

### 데스크톱 인식

기획서 15~17장 Desktop Awareness / UI Automation. 지금 사용자가 어떤
창을 보고 있는지를 AI 채팅 컨텍스트에 한 줄로 얹는다.

- 원화면 스크린샷/Vision은 이번 스코프에서 제외했다 — "가능하면
  Screenshot보다 구조화된 UI 데이터를 우선한다"(기획서 17장) 원칙에 따라
  UI Automation 텍스트만 얕게(자식 요소까지만, 최대 12개, 800자 캡)
  읽어온다
- `context/ActiveWindowTracker`가 `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`로
  포그라운드 창이 바뀔 때마다 이벤트 기반(폴링 없이)으로 알림을 받는다
- `context/UiAutomationReader`가 그 창의 UI Automation 트리에서 자식
  요소들의 `Name`을 얕게 훑어서 요약 텍스트를 만든다(화면 밖
  `IsOffscreen` 요소는 제외). COM 호출은 백그라운드 스레드에서 실행하고,
  끝나기 전에 창이 또 바뀌면 세대(generation) 번호로 낡은 결과를 버린다
- `context/ContextEngine::BuildContextLine()`이 이걸 모아 한 줄로 만들고,
  매 채팅 메시지마다 시스템 메시지로 끼워 넣는다

**프라이버시**: `config/privacy_config.json`(`screen_awareness_enabled`,
`excluded_processes`)으로 기능 자체를 끄거나 특정 프로세스를 제외할 수
있다. `excluded_processes`는 기본값이 빈 배열이다 — 창 제목만으로도
민감한 정보(메모장에 띄운 API 키 메모 등)가 로그와 AI 컨텍스트에 그대로
들어갈 수 있으니, 메모장·비밀번호 관리자처럼 창 제목에 민감한 게 뜰 수
있는 프로그램은 이 목록에 직접 추가하는 걸 권장한다. (문서 *내용*까지
읽지는 않는다 — UI Automation의 `Name` 속성만 읽고 `Value`/문서 텍스트는
안 읽는 설계.)

**구현 메모**:
- `<UIAutomationClient.h>`는 COM의 `interface` 매크로(`struct`로 치환)가
  이미 정의돼 있다고 가정하고 자기 자신은 그걸 정의하지 않는다 —
  `<objbase.h>`를 먼저 include해야 한다(안 그러면 C2146/C2371 연쇄 오류)
- 앱을 켠 직후 창을 한 번도 안 바꾸면 첫 채팅에 컨텍스트가 비어있던 문제를
  고쳐서, `ContextEngine::Create()`가 현재 포그라운드 창을 즉시 한 번
  읽어와 시드한다

### 게임 인식과 반응

정식 Phase는 아니고, Desktop Context를 캐릭터 행동에 직접 연결한 확장이다
— 기획서 40장 Game Integration처럼 게임 SDK 훅을 붙이는 건 아니고,
지금 하고 있는 게 게임인지 "추측"만 해서 캐릭터가 같이 놀아준다.

**감지 방식** (`context/GameDetector`, 세 가지를 함께 사용):

1. `config/games_config.json`에 등록된 프로세스 이름과 일치하는지 (사용자가
   직접 추가 가능)
2. 설치된 게임 런처 라이브러리를 앱 시작 시 백그라운드 스레드에서 훑어서
   실행 파일명을 자동으로 목록에 추가
   - `context/SteamLibraryScanner`: 레지스트리로 설치 경로를 찾고
     `libraryfolders.vdf` + `appmanifest_*.acf`를 파싱한다. 매니페스트가
     실제 실행 파일명까진 안 알려줘서 설치 폴더를 깊이 3단계까지 훑고,
     설치 파일류(`.NET` 재배포판, 크래시 핸들러 등)는 필터로 제외한다
   - `context/EpicLibraryScanner`: `%ProgramData%\Epic\EpicGamesLauncher\Data\Manifests\*.item`
     (JSON)을 읽는다. `LaunchExecutable` 필드가 실제 실행 파일 경로를
     직접 알려줘서 스팀과 달리 폴더를 뒤질 필요가 없다
   - 둘 다 사용자 컴퓨터에 실제로 설치된 것만 읽으므로 크라우드소싱
     데이터베이스 같은 라이선스 문제가 없다. 결과는 `shared_ptr` + `mutex`로
     공유해서 스캔이 끝나기 전에 `GameDetector`가 파괴돼도 안전하다
3. 위 두 방법에도 없는 게임을 위해, 창이 모니터 전체(작업표시줄 영역
   포함)를 덮으면 전체화면/전체화면형 무테두리로 간주한다 — 일반 창을
   최대화해도 작업표시줄 영역까지는 안 덮으므로 오탐이 거의 없다
   (작업표시줄을 자동 숨김으로 해둔 경우는 예외). 이 앱이 아직 DPI 인식
   매니페스트가 없어서 좌표가 실제 창과 몇 픽셀 어긋날 수 있어 8px
   허용 오차를 뒀다. 다만 이 휴리스틱은 "모니터를 꽉 채운 창"과 "게임"을
   구분 못 해서 동영상 플레이어·브라우저 전체화면·시스템 유틸리티도
   똑같이 걸릴 수 있다 — `games_config.json`의
   `fullscreen_heuristic_exclusions`에 등록된 프로세스는 이 휴리스틱만
   건너뛴다(명시적으로 등록된 게임 목록/라이브러리 스캔 결과는 그대로
   유효). 브라우저·미디어 플레이어·IDE 등 흔한 경우는 기본으로 이미
   빠져 있고, 사용자가 직접 추가할 수도 있다. *구현 메모: 앱 시작 직후
   한 번 낯선 프로세스가 잠깐 포그라운드를 차지해서 게임으로 잘못
   판정된 사례가 있었는데, 로그로 실행 파일 경로까지 추적해보니
   실제로는 "산나비(SANABI)" 게임 자체의 실행 파일(`SNB.exe`)이었다 —
   Steam 라이브러리 스캔이 정상적으로 게임으로 잡아낸 것이었지 오탐이
   아니었다. 낯선 프로세스명만 보고 바로 제외 목록에 넣기 전에 실제
   경로를 확인해야 한다는 교훈*

**게임 중 캐릭터 행동**: `character/Action::PlayingGame` + `Emotion::Excited`로
표시되고, 전용 스프라이트 `playing_game.png`(calm.png 위에 게임패드 아이콘
합성)로 바뀐다. 게임 중엔 Idle-to-sleep 타임아웃과 랜덤 Idle 행동이
멈추고 이 포즈를 유지하지만, 호버/쓰다듬기/드래그/채팅은 그대로 우선권을
가진다 — 게임 중에 캐릭터를 건드리면 평소처럼 반응하고 손을 떼면 다시
게임 포즈로 돌아간다.

**구현 메모**: 전체화면 휴리스틱이 처음엔 `WS_CAPTION` 없음도 같이
요구했는데, 실제 게임(HELLDIVERS 2)이 시각적으론 전체화면인데도 그
스타일 비트를 갖고 있어서 계속 놓치는 걸 확인하고 그 조건을 뺐다.

### 리그 오브 레전드 실시간 컨텍스트

게임 인식이 "무슨 게임인지"는 알아도 "지금 상황에 뭐가 좋을지" 같은
실시간 정보까진 모른다. 이건 게임마다 사정이 완전히 달라서, 공식 실시간
데이터 소스가 있는 리그 오브 레전드로 범위를 좁혔다.

- **리그 오브 레전드**: 라이엇 클라이언트가 매치 진행 중에만 로컬로 여는
  공식 API가 있다(`https://127.0.0.1:2999/liveclientdata/*`, 인증 없음,
  자체 서명 인증서). `context/LeagueLiveClient`가 진행 시간, 내 챔피언·
  레벨·골드·아이템, 양 팀 전원의 챔피언·킬/데스/어시스트·CS를 가져와서
  매 채팅 요청마다 시스템 메시지로 끼워 넣는다 — "지금 뭐 사야 돼?"
  같은 질문에 실제 상황을 보고 답할 수 있다. 클라이언트 버전에 따라
  다른 플레이어 식별 필드(구버전 `summonerName` / 신버전
  `riotIdGameName`+`riotIdTagLine`)를 둘 다 처리하고, "우리 팀"/"상대 팀"은
  고정된 ORDER/CHAOS가 아니라 내 챔피언이 실제로 속한 팀 기준으로 나눈다
- **레인보우 식스 시즈 등 다른 게임**: 공식 실시간 데이터 API가 없다.
  화면을 OCR로 읽는 방법은 패치마다 깨지는 유지보수 부담이 커서 Vision과
  함께 보류했고, 게임 메모리를 직접 읽는 방법은 안티치트 정책 위반(계정
  정지 위험)이라 시도하지 않는다. 그래서 이런 게임은 일반 채팅으로
  물어보는 것 이상은 지원하지 않는다

**구현 메모**: 매치가 진행 중이 아닐 때 이 API에 요청을 보내면 실패하기까지
~2초가 걸려서(닫힌 로컬 포트치고 이례적으로 느림), 리그 오브 레전드를
안 하고 있어도 채팅을 보낼 때마다 2초씩 늦어지는 문제가 있었다. 포그라운드
프로세스명이 실제로 리그 오브 레전드일 때만 API를 두드리도록 게이트를
걸어서(`IsForegroundProcessLeagueOfLegends`), 무관한 경우엔 네트워크
요청 자체를 하지 않도록 고쳤다(지연 2.1초 → 11ms).

> 이 개발 머신엔 실행 중인 매치가 없어서 파싱 로직은 공식 문서 기준
> 샘플 데이터로만 검증했다. 실제 매치로 확인 시 필드가 안 맞으면 알려줄 것.

### 선제적 발화

기획서 18장 Proactive Assistant. 캐릭터가 질문을 기다리지 않고 특정
상황에서 먼저 말을 걸 수 있다. 방해를 최소화하기 위해 기획서가 정의한
Interruption Score를 그대로 가져왔다(`proactive/InterruptionScore`):

| 이벤트 | 점수 |
| --- | --- |
| 일반 화면 변경 | 0.05 |
| 앱 실행 | 0.10 |
| 작업 성공 | 0.30 |
| 새 오류 | 0.60 |
| 동일 오류 반복 | 0.82 |
| 중대한 문제 | 0.95 |

기본 Threshold는 0.75. 지금 인프라(창 제목 + 얕은 UI Automation 텍스트,
빌드 시스템/IDE 연동 없음)로 신뢰성 있게 구분할 수 있는 건 **동일 오류
반복**뿐이라 이번엔 그 트리거 하나만 구현했다 — 나머지는 표만 코드에
남겨서 다음 트리거를 추가할 때 참고하게 해뒀다.

- `proactive/ErrorRepeatDetector`가 활성 창의 제목+얕은 UI 텍스트에서
  오류 키워드(error/exception/failed/fatal/crash/오류/실패/에러)를 찾고,
  같은 신호가 "화면에서 사라졌다가 다시 나타나면" 반복으로 판정한다 —
  같은 오류 창이 계속 떠 있는 것과는 구분됨
- 반복이 감지되면 `MainWindow::StartProactiveSpeech()`가 사용자 메시지
  없이(페르소나 프롬프트 + 상황 설명만으로) 실제 AI를 호출한다. 응답은
  평소 채팅 응답과 동일하게 말풍선 + TTS로 나온다
- 스팸 방지로 10분 쿨다운(`kProactiveCooldown`, UX 튜닝 전 임시값)을 뒀고,
  이미 채팅 중이거나 말풍선이 떠 있으면 트리거를 건너뛴다

### 기억 시스템

기획서 22장 Memory. Session 메모리(현재 대화, `MainWindow::conversationHistory_`,
재시작하면 사라짐)는 기존에 있었고, 이번에 재시작을 버텨내는 두 계층을
새로 추가했다 — 둘 다 AI 요약 없이, 앱이 이미 감지하고 있는 신호(게임/
개발 도구 포커스, Idle→Sleeping 전환, 오류 반복 감지)만으로 만든 사실
기반 기록이다:

- **Daily Memory** (`memory/DailyMemory`): 하루치 타임스탬프 활동 로그,
  `%LOCALAPPDATA%\SvetaAssistant\memory\daily\YYYY-MM-DD.json`에 저장.
  예: `13:20 프로그램 시작`, `14:05 게임 시작 (VALORANT-Win64-Shipping.exe)`,
  `15:32 게임 종료 (1시간 27분)`, `15:40 같은 오류 반복 감지`,
  `18:00 휴식 시작`, `18:22 활동 재개`. 로컬 벽시계 시각 기준
  (`GetLocalTime()` — `core::Logger`의 UTC 타임스탬프와 다름). 30일 지난
  파일은 시작 시 자동 삭제.
- **Long-Term Memory** (`memory/LongTermMemory`): 누적 통계 하나
  (`...\memory\long_term.json`) — 최초 실행일, 프로그램별/게임별 누적
  사용 시간(초). "중요한 대화"나 습관 추론은 AI 요약이 필요해서 v1
  범위 밖으로 미뤘다(코드에 `TODO(Phase 8.x)`로 남겨둠).
- **`memory/MemoryEngine`**가 이 둘을 오케스트레이션한다. 게임/개발 도구/
  휴식 상태는 30초(`kDebounceTicks`) 연속으로 유지돼야 로그에 커밋되도록
  디바운스했다 — 몇 초짜리 Alt-Tab에 "시작/종료" 쌍이 스팸처럼 찍히는 걸
  막기 위함. 개발 도구는 `GameDetector`의 풀스크린 예외 목록과 같은
  방식으로 하드코딩된 목록(devenv.exe, Code.exe, 각종 JetBrains IDE 등)을
  씀 — v1은 사용자 편집 불가, 다음 버전에서 설정 파일로 뺄 예정.
- 매 채팅 요청마다 오늘의 기록 + 누적 통계를 한 줄 다이제스트로 압축해
  시스템 프롬프트에 얹는다(`BuildMemoryDigestLine()`) — 예:
  `(함께한 지 12일째. 오늘: 14:05 게임 시작(발로란트), 15:40 같은 오류
  반복 감지. 자주 쓰는 프로그램: devenv.exe, chrome.exe.)`. "자주 쓰는
  프로그램/게임"은 일별 로그 파일이 3개 이상 쌓이기 전엔 생략한다 — 쓴 지
  5분 만에 "자주 쓰는 프로그램" 운운하면 어색하니까.
- 설정 창의 "자동 동작" 섹션에서 메모리 자체를 켜고 끌 수 있다
  (`PrivacyConfig::memoryEnabled`, 기획서 51장 프라이버시 토글 요구사항).
  끄면 새 기록이 멈추고 다이제스트도 사라지지만 이미 쌓인 기록은
  삭제되지 않는다 — 다시 켜면 이어서 기록.

### 트레이 아이콘 & 설정 창

캐릭터를 우클릭할 수 없는 대신(드래그 제스처와 겹침), 작업표시줄
트레이 아이콘(`window/TrayIcon`)이 진입점이다. 아이콘 자체는
`content/face.png`를 GDI+로 불러와 32px로 리사이즈해서 만든다 —
별도 `.ico` 리소스가 필요 없다.

**우클릭 메뉴**: 설정 / 일시정지(다시 보이기 토글) / 종료.
일시정지는 프로세스를 끄지 않고 캐릭터 창만 숨기고 Idle 틱 타이머를
멈춘다. 탐색기가 재시작돼도(`WM_TASKBARCREATED`) 아이콘을 자동으로
다시 등록한다.

**설정 창** (`window/SettingsWindow`): 일반 데코레이션 창(모덜리스,
한 번 만들고 재사용)으로, 다음을 한 곳에서 다룬다:

- **개인화**: 사용자 이름, "어떻게 대해줬으면 좋겠는지" 자유 텍스트
- **언어**: Sveta의 주 대화 언어(7종), 그리고 이 설정 창 자체의 표시
  언어(한국어/English — 콤보박스 바꾸는 즉시 라벨이 실시간으로
  바뀐다). 앱의 나머지 부분(트레이 메뉴, 말풍선)은 아직 한국어 전용
- **음성**: 제공자(Google/SAPI), 볼륨 슬라이더, 언어별 Chirp 3: HD
  보이스 드롭다운(위 [음성 (TTS)](#음성-tts) 참고)
- **자동 동작**: 프로액티브 음성, 게임 감지, 메모리(위 [기억
  시스템](#기억-시스템) 참고) 각각 켜고 끌 수 있음 (`context::PrivacyConfig`에
  필드 추가 — `SetProactiveSpeechEnabled()`/`SetGameDetectionEnabled()`/
  `SetMemoryEnabled()`로 `ContextEngine`/`MemoryEngine`을 재생성하지 않고
  즉시 반영)
- **캐릭터**: 위치 초기화 버튼

저장하면 해당 JSON 파일들에 바로 기록되고(TTS 엔진 재생성 등) 재시작 없이
바로 적용된다.

**디자인**: 기본 Win32 다이얼로그 스타일(회색 배경, 각진 버튼)이 "너무
올드하다"는 피드백을 받고 다크 테마로 전면 교체했다.

- `DwmSetWindowAttribute`로 다크 타이틀바 + 둥근 모서리(Windows 11
  네이티브 API, 수동 그리기 없이 OS가 처리)
- 저장/취소/위치초기화 버튼은 `BS_OWNERDRAW` + `WM_DRAWITEM`으로 직접
  그린다 — 테마 적용된(v6 comctl32) 네이티브 버튼은 `WM_CTLCOLORBTN`의
  배경 브러시를 무시하기 때문에 커스텀 색을 입히려면 직접 그리는 수밖에
  없다
- 콤보박스/트랙바는 `SetWindowTheme(..., L"DarkMode_CFD"/"DarkMode_Explorer", ...)`로
  다크 스타일을 입힌다(비공식이지만 Windows 탐색기 자신도 쓰는 방식)
- *구현 메모*: 체크박스/라디오 버튼은 테마가 걸리면 `WM_CTLCOLORBTN`이
  설정한 글자색을 무시하고 무조건 검정으로 그려서, 다크 배경 위에서
  글자가 안 보이는 문제가 있었다. `SetWindowTheme(control, L"", L"")`로
  이 두 컨트롤만 테마를 꺼서(클래식 렌더링으로 되돌려서) 해결 — 체크박스
  모양은 살짝 예전 스타일이 되지만 글자는 확실히 읽힌다

## 프로젝트 구조

```
src/
  app/          진입점 (WinMain)
  core/         로깅, 파일 경로, 공용 유틸리티
  window/       Win32 창 관리, 드래그, 위치 저장/복원, 트레이 아이콘, 설정 창
  rendering/    PNG 스프라이트 로딩 (WIC); Direct2D/Direct3D는 이후 단계
  character/    Emotion/Action/Personality, CharacterState 오케스트레이션
  behavior/     Idle 상태에서의 확률 기반 행동 선택
  interaction/  마우스 상호작용, 쓰다듬기, 히트박스
  content/      캐릭터/아이템/가구 패키지 로딩
  items/        인터랙티브 소품
  context/      데스크톱 인식, 게임 감지(Steam/Epic 라이브러리, 리그 오브 레전드)
  proactive/    선제적 발화(Interruption Score, 동일 오류 반복 감지)
  ai/           AI 엔진 연동 (대화, 비전, 페르소나, 사용자 개인화 프로필)
  memory/       세션/일간/장기 기억
  audio/        TTS (SAPI, Google Cloud Chirp 3: HD); STT는 이 머신에 언어 인식 모델이 없어서 보류
content/        캐릭터 및 아이템 패키지 (코드 아닌 데이터)
assets/         공용 에셋
config/         런타임 설정
tests/          테스트
tools/          에셋 생성 등 개발용 스크립트 (빌드에 포함 안 됨)
third_party/    벤더링한 헤더 전용 라이브러리 (nlohmann/json)
```

구조를 정한 근거는 기획서 47장, 전체 Phase 로드맵은 52장 참고.

## 알려진 제한사항

- **DPI 인식 매니페스트 없음**: 앱 자신이 보는 창 좌표와 DPI를 인식하는
  외부 도구가 보는 좌표가 몇 픽셀 어긋날 수 있다. 앱 자체의 위치
  저장/복원 왕복에는 영향 없지만, 다른 창(게임 등)의 좌표를 판단할 때는
  허용 오차로 대응 중. Phase 13 폴리싱 전에는 근본적으로 고쳐야 한다
- **머리 히트박스**: 캐릭터마다 다른 정확한 히트박스가 아니라 스프라이트
  크기에 비례한 근사치. Character Package(기획서 24~25장)에서 다룰 예정
- **Idle-to-sleep 타임아웃(5분)**, **선제적 발화 쿨다운(10분)**: 둘 다
  정식 UX 튜닝 전의 placeholder 값
- **STT/Push-to-Talk 보류**: 이 개발 머신에 한국어/영어 음성 인식 엔진이
  없음. 다른 머신에서 언어팩 설치 후 진행 예정
- **UI 언어 설정은 설정 창 자체에만 적용**: 트레이 메뉴, 말풍선, 로그는
  여전히 한국어 전용. 앱 전체 다국어화는 아직 스코프 밖
- **체크박스/라디오 버튼은 의도적으로 언테마드**: 다크 설정 창에서 글자
  색이 하얗게 보이게 하려고 `SetWindowTheme(control, L"", L"")`로
  껐다(위 [트레이 아이콘 & 설정 창](#트레이-아이콘--설정-창) 구현 메모
  참고) — 체크 모양이 Windows 11 네이티브 스타일보다 살짝 예전 느낌
- **게임 실시간 컨텍스트는 리그 오브 레전드 한정**: 다른 게임은 공식
  데이터 소스가 없는 한 지원 계획 없음(위 [리그 오브 레전드 실시간
  컨텍스트](#리그-오브-레전드-실시간-컨텍스트) 참고)
- **대화 기록은 세션 한정**: 최근 20턴까지만 유지, 재시작하면 사라짐 —
  의도된 설계(Session 메모리 계층). 재시작을 버티는 활동 기록은 [기억
  시스템](#기억-시스템)의 Daily/Long-Term Memory 참고
- **Long-Term Memory는 "중요한 대화"를 기억하지 않음**: v1은 프로그램/게임
  사용 시간 같은 기계적 통계만 쌓는다 — 습관이나 대화 내용 요약은 AI
  호출이 필요해서 다음 버전으로 미뤘다(`memory/LongTermMemory.h`의
  `TODO(Phase 8.x)` 참고)
- **개발 도구 감지 목록은 하드코딩**: `memory/MemoryEngine.cpp`에 이름을
  직접 추가해야 함 — 사용자 편집 가능한 설정 파일은 아직 없음
