<h1 align="center">Auto-Type</h1>

<p align="center"><b>한/영 키 없이 그냥 치세요. 단어마다 한국어인지 영어인지 알아서 정하는 Windows 입력기</b></p>

<p align="center">
  <a href="https://github.com/cybereun/Auto-Type/actions/workflows/build.yml"><img alt="build" src="https://github.com/cybereun/Auto-Type/actions/workflows/build.yml/badge.svg"></a>
  <img alt="platform" src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6?logo=windows&logoColor=white">
  <img alt="language" src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white">
  <img alt="TSF" src="https://img.shields.io/badge/Text%20Services%20Framework-IME-5C2D91">
  <img alt="offline" src="https://img.shields.io/badge/network-offline%20only-2ea44f">
  <a href="https://github.com/cybereun/Auto-Type/releases/latest"><img alt="release" src="https://img.shields.io/github/v/release/cybereun/Auto-Type?include_prereleases"></a>
  <a href="LICENSE"><img alt="license" src="https://img.shields.io/badge/license-MIT-yellow"></a>
</p>

<p align="center">
  <a href="https://cybereun.github.io/Auto-Type/"><b>홈페이지에서 바로 체험하기 →</b></a>
</p>

<p align="center">
  <a href="https://github.com/cybereun"><img alt="developer" src="https://img.shields.io/badge/developer-cybereun-181717?logo=github"></a>
</p>

```
dkssudgktpdy  →  안녕하세요
gksdudwjsghks →  한영전환
APIfmf        →  API를
hello world   →  hello world
```

---

## 목차

1. [소개](#소개)
2. [주요 기능](#주요-기능)
3. [설치](#설치)
4. [사용 방법](#사용-방법)
5. [원래 입력기로 돌아가기](#원래-입력기로-돌아가기)
6. [동작 원리](#동작-원리)
7. [직접 빌드](#직접-빌드)
8. [프로젝트 구조](#프로젝트-구조)
9. [알려진 한계와 로드맵](#알려진-한계와-로드맵)
10. [개발자 정보](#개발자-정보)
11. [라이선스와 출처](#라이선스와-출처)

---

## 소개

한국어와 영어를 섞어 쓰다 보면 한/영 키를 잘못 눌러 `dkssud`나 `ㅗ디ㅣㅐ`를 지우고 다시 치는 일이 잦습니다.
Auto-Type은 **키 입력을 단어 단위로 보고, 한국어(두벌식)로 읽은 결과와 영어로 읽은 결과 중 더 자연스러운 쪽을 고릅니다.**

macOS 입력기 [Tavi](https://github.com/3x-haust/tavi)의 판단 로직을 C++로 옮기고, Windows 공식 입력기 구조인 **TSF(Text Services Framework)** 위에 새로 만들었습니다.
그래서 키를 가로채는 꼼수가 아니라 Windows 설정의 **한국어 키보드 목록에 정식 입력기로 등록**되고, 기존 Microsoft 입력기와 나란히 쓸 수 있습니다.

## 주요 기능

| 기능 | 설명 |
| --- | --- |
| **단어별 자동 한영 판단** | 영어 3글자 통계(트라이그램), 한글 음절 빈도, 영어·한국어 단어 사전으로 점수를 매겨 고릅니다 |
| **섞어 쓴 단어 처리** | `APIfmf` → API를, `githubdp` → github에 처럼 영어 + 조사를 한 단어로 이해합니다 |
| **구조적 영어 인식** | URL, 이메일, 경로, `snake_case`, `camelCase`, 숫자가 섞인 단어(`test123`)는 영어로 둡니다 |
| **약어 인식** | `API`, `HTML`처럼 대문자로만 된 단어는 영어로 둡니다 |
| **쌍자음·겹모음** | Shift + Q/W/E/R/T/O/P는 대문자가 아니라 ㅃ ㅉ ㄸ ㄲ ㅆ ㅒ ㅖ로 읽습니다 (`ehoTsk` → 됐나) |
| **문맥 반영** | 바로 앞 단어가 한국어면 한국어 쪽, 영어면 영어 쪽에 가산점을 줍니다 |
| **한 단어만 뒤집기** | 판단이 틀렸으면 한/영 키(또는 Shift+Space)로 입력 중인 단어만 뒤집습니다 |
| **한글 먼저** | 아는 영어 단어가 아니면 첫 키부터 한글로 보여 줍니다 (`d` → ㅇ, `dk` → 아) |
| **끊긴 단어 이어 받기** | 엑셀·카카오톡처럼 앱이 입력 중인 단어를 중간에 확정해도, 다음 키에서 그 단어를 다시 가져와 한 단어로 판단합니다 (되어야 + go → 되어야해) |
| **흔들림 방지** | 한국어로 보이기 시작한 단어는 애매한 키 하나로 영어로 튀지 않습니다 (안 → 아ss 방지) |
| **완전 오프라인** | 판단은 DLL 안의 약 0.2MB 모델로만 합니다. 입력한 글을 저장하거나 보내지 않습니다 |
| **정식 입력기 등록** | Win+Space로 Microsoft 입력기와 오갈 수 있고, 제거해도 기존 입력기는 그대로입니다 |

## 설치

### 필요한 것
- Windows 10 또는 11 (64비트)
- 한국어 언어 팩 (설정 → 시간 및 언어 → 언어 및 지역에 "한국어"가 있으면 됩니다)
- 관리자 권한

### 가장 쉬운 방법: 설치 파일(EXE)
1. [Releases](https://github.com/cybereun/Auto-Type/releases/latest)에서 `AutoType-Setup-<버전>.exe`를 받습니다.
2. 더블클릭해 실행하고, 관리자 권한 요청이 뜨면 **예**를 누릅니다.
3. 끝나면 **Win + Space**로 `Auto-Type (한영 자동)`을 고릅니다. 이미 열려 있던 앱은 다시 실행해야 인식합니다.

제거는 **설정 → 앱 → 설치된 앱 → Auto-Type → 제거**입니다.

### 수동 설치 (portable.zip)
1. 같은 Releases 페이지에서 `AutoType-<버전>-portable.zip`을 받아 압축을 풉니다.
   ```
   ├─ x64/AutoType.dll      64비트 앱용 (대부분의 앱)
   ├─ x86/AutoType.dll      32비트 앱용
   ├─ install.bat
   └─ uninstall.bat
   ```
2. `install.bat`을 **우클릭 → 관리자 권한으로 실행**합니다. 파일은 `C:\Program Files\AutoType`에 복사되고 `regsvr32`로 등록됩니다.
3. **설정 → 시간 및 언어 → 언어 및 지역 → 한국어 옆 ⋯ → 언어 옵션 → 키보드**에서 `Auto-Type (한영 자동)`이 보이는지 확인합니다.

> 코드 서명이 아직 없어서 SmartScreen이 막으면 **추가 정보 → 실행**을 누르세요. 백신이 경고할 수도 있습니다. 소스가 모두 이 저장소에 있으니 직접 빌드해서 쓸 수도 있습니다.

## 사용 방법

### 1. Auto-Type 켜기
**Win + Space**를 눌러 목록에서 `Auto-Type (한영 자동)`을 고릅니다. 작업표시줄 오른쪽 입력기 아이콘을 눌러 골라도 됩니다.

### 2. 그냥 치기
한/영 상태를 신경 쓰지 말고 영문 자판 위치 그대로 칩니다. 입력 중인 단어는 키를 누를 때마다 다시 판단해서 보여 줍니다.

```
키 입력          화면
d               ㅇ      ← 첫 키부터 한글
dk              아
dks             안
dkss            안ㄴ
dkssud          안녕
dkssud ␣        안녕 (확정)
```

### 3. 키별 동작

| 키 | 동작 |
| --- | --- |
| 영문자 A–Z | 입력 중인 단어에 더하고 다시 판단합니다 |
| 숫자 0–9 | 단어를 입력하는 중이면 단어에 더하고(`test123`), 아니면 그대로 입력합니다 |
| **Space** | 단어를 확정하고 띄어 씁니다 |
| **한/영 키** 또는 **Shift + Space** | 입력 중인 단어를 한국어 ⇄ 영어로 뒤집습니다. 그 단어가 끝날 때까지 유지됩니다. 입력 중인 단어가 없을 때 누른 한/영 키는 무시합니다 (Auto-Type이 꺼지지 않도록) |
| Backspace | 입력 중인 단어에서 마지막 키를 지웁니다 (`안녕` → `안녀`) |
| Enter, Tab, Esc, 방향키, 문장부호 | 단어를 확정한 뒤 그 키를 앱에 그대로 보냅니다 |
| Ctrl / Alt / Win 조합 | 단어를 확정한 뒤 단축키를 그대로 실행합니다 (Ctrl+C, Alt+Tab 등) |
| Caps Lock | 영어 대문자로 칩니다. 대문자로만 된 단어는 약어로 봐서 영어로 둡니다 |

### 4. 예시

| 친 키 | 결과 | 이유 |
| --- | --- | --- |
| `dkssudgktpdy` | 안녕하세요 | 한국어로 읽은 쪽이 훨씬 자연스러움 |
| `hello` | hello | 흔한 영어 단어 |
| `APIfmf` | API를 | 약어 + 조사 |
| `rkskek` | 가나다 | 영어로는 있을 수 없는 철자 |
| `ehoTsk` | 됐나 | Shift+T는 ㅆ |
| `test123` | test123 | 숫자가 섞이면 영어 |
| `tkfkd` → 한/영 키 | tkfkd | 판단을 직접 뒤집음 |

### 5. 판단이 틀렸을 때
단어를 확정하기 전(Space 누르기 전)에 **한/영 키**를 누르면 그 단어만 반대 언어로 바뀝니다. 이미 확정한 단어는 Backspace로 지우고 다시 쳐야 합니다.

## 원래 입력기로 돌아가기

| 하고 싶은 것 | 방법 |
| --- | --- |
| 잠깐 Microsoft 입력기로 | **Win + Space** → `Microsoft 입력기` |
| 다시 Auto-Type으로 | **Win + Space** → `Auto-Type (한영 자동)` |
| 목록에서만 숨기기 | 설정 → 언어 옵션 → 키보드에서 `Auto-Type` 제거 (나중에 다시 추가 가능) |
| 완전히 지우기 | 설정 → 앱 → `Auto-Type` → 제거 (수동 설치는 `uninstall.bat`). Microsoft 입력기는 그대로 남습니다 |

업데이트와 제거 모두 재부팅이 필요 없습니다. 열려 있던 앱은 다시 열면 새 상태를 따릅니다. 그 순간 지우지 못한 파일은 다음 로그인 때 자동으로 정리됩니다.

## 동작 원리

```
키 입력 ──► TextService (src/tsf)            Windows TSF와 대화
              │  글자 키면 Engine에 전달, 확정 키면 커밋
              ▼
           Engine (src/core)                  OS와 무관한 순수 C++
              ├─ hangul::compose   키 → 두벌식 한글 조합
              ├─ Model             영어 점수 / 한국어 점수 계산
              └─ score()           후보(영어, 한국어, 영어+조사, 한국어+영어) 점수 비교
              ▼
           가장 점수가 높은 후보를 조합 문자열로 화면에 표시
```

1. **두 가지로 읽기:** 같은 키를 영어 그대로(`dkssud`)와 두벌식 한글(`안녕`)로 각각 읽습니다.
2. **영어 점수:** 3글자 연속 확률(트라이그램)의 평균 로그 확률에, 흔한 영어 단어나 알려진 영어 단어면 최소 점수를 보장합니다.
3. **한국어 점수:** 음절마다 실제 사용 빈도를 보고, 완성되지 않은 낱자(ㅇ, ㅏ)가 남으면 크게 감점합니다.
4. **보정:** URL·숫자·camelCase는 영어 가산점, 앞 단어의 언어에 따라 가산점을 줍니다.
5. **섞인 단어:** `API`+`를`처럼 영어 앞부분과 한국어 조사를 나눈 후보도 만들어 함께 비교합니다.
6. 점수를 확률로 바꿔 가장 높은 후보를 보여 줍니다.

## 직접 빌드

### Windows (Visual Studio 2022)
"C++를 사용한 데스크톱 개발" 워크로드와 CMake가 필요합니다.

```bat
cmake -S . -B build-x64 -A x64
cmake --build build-x64 --config Release
build-x64\Release\engine_test.exe resources\model-v2.dat
```

32비트 DLL은 `-A Win32`로 같은 과정을 한 번 더 하면 됩니다.

### Linux / macOS (엔진 테스트만)

```bash
g++ -std=c++17 -I src/core src/core/Engine.cpp tests/engine_test.cpp -o engine_test
```

```bash
./engine_test resources/model-v2.dat
```

### Linux에서 Windows DLL 크로스 빌드 (MinGW)

```bash
cmake -S . -B build-win -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-posix -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build-win
```

## 릴리스 만들기 (개발자용)

1. `VERSION` 파일의 숫자를 올리고 커밋합니다 (예: `0.2.0`).
2. 같은 버전의 태그를 붙여 푸시합니다.

```bash
git tag v0.2.0
```

```bash
git push origin v0.2.0
```

태그를 푸시할 수 없으면 **Actions → build → Run workflow**(release 체크)를 누르면 `VERSION` 값으로 태그와 릴리스가 만들어집니다.

GitHub Actions가 x64·x86 DLL을 빌드하고 테스트를 돌린 뒤, `AutoType-Setup-0.2.0.exe`와 `portable.zip`을 Releases에 올립니다.

## 프로젝트 구조

```
Auto-Type/
├─ src/
│  ├─ core/
│  │  ├─ Engine.h          엔진 공개 인터페이스
│  │  └─ Engine.cpp        한글 조합, 통계 모델, 후보 점수 (Tavi에서 이식)
│  └─ tsf/
│     ├─ Globals.h         CLSID, 프로필 GUID, 공용 선언
│     ├─ TextService.*     TSF 입력기: 키 처리, 조합 문자열, 커밋
│     ├─ Dll.cpp           COM 진입점, 입력기 등록/해제
│     ├─ AutoType.rc       언어 모델을 DLL에 포함
│     └─ AutoType.def      DLL 내보내기 목록
├─ resources/model-v2.dat  언어 모델 (Tavi, MIT)
├─ tests/engine_test.cpp   엔진 테스트
├─ docs/                   홍보 페이지 (GitHub Pages) + 브라우저용 엔진 engine.js
├─ scripts/                install.bat, uninstall.bat
├─ installer/AutoType.iss  EXE 설치 파일 (Inno Setup)
├─ VERSION                 현재 버전
└─ .github/workflows/      Windows x64·x86 빌드, 테스트, 설치 파일, 릴리스
```

## 변경 기록

**0.2.3**
- 카카오톡에서 자음·모음이 따로 찍히던 문제를 고쳤습니다. 키마다 글자를 확정해 버리는 앱에서는, 확정된 글자를 지우고 단어 전체를 다시 입력해 한 단어로 조합합니다.

**0.2.2**
- 업데이트·제거할 때 재부팅이 필요 없습니다. 버전마다 다른 폴더에 설치해서, 열려 있는 앱이 쥐고 있는 파일을 덮어쓰지 않습니다. 지금 못 지운 파일은 다음 로그인 때 정리됩니다.

**0.2.1**
- 카카오톡처럼 앱이 IME를 끄거나 영문 모드로 바꾸면 즉시 한글 모드로 되돌립니다. 키가 Auto-Type을 거치지 않고 영어 그대로 들어가던 문제를 고칩니다.
- 진단 기록 추가: 시작 메뉴 → Auto-Type → **진단 기록 켜기**. 문제가 생기면 **진단 기록 끄고 보기**로 열리는 `log.txt` 내용을 Issues에 올려 주세요.

**0.2.0**
- 단어를 첫 키부터 한글로 보여 줍니다. 엑셀에서 첫 글자가 영어로 찍히던 문제를 고쳤습니다.
- Windows에 "IME 켜짐, 한글 모드"를 알립니다. 엑셀·카카오톡이 Auto-Type을 IME 꺼짐으로 보던 문제를 고쳤습니다.
- 앱이 입력 중인 단어를 중간에 끊으면 다음 키에서 다시 이어 받습니다 (`되어야해`가 `되어야go`로 되던 문제).
- 입력 중인 단어가 없을 때 한/영 키를 눌러도 Auto-Type이 꺼지지 않습니다.

**0.1.0** — 첫 릴리스

## 알려진 한계와 로드맵

**현재 한계**
- 입력 중인 단어에 밑줄 같은 표시가 없어 확정 여부가 잘 안 보일 수 있습니다.
- Enter·Tab·방향키는 단어를 확정한 뒤 키를 다시 보내는 방식이라, 일부 앱에서 다르게 동작할 수 있습니다.
- 관리자 권한으로 실행한 앱에는 32/64비트에 맞는 DLL이 모두 등록돼 있어야 합니다.
- 실제 Windows 환경에서 아직 충분히 시험하지 않았습니다. 문제가 생기면 [Issues](https://github.com/cybereun/Auto-Type/issues)에 앱 이름과 친 키를 남겨 주세요.

**로드맵**
- [ ] 입력 중인 단어 밑줄 표시 (Display Attribute)
- [ ] 개인 단어 학습: 같은 표기를 세 번 확정하면 기억
- [ ] 작업표시줄 모드 아이콘과 설정 메뉴
- [x] EXE 설치 파일과 버전별 릴리스
- [ ] 코드 서명
- [ ] 후보 목록 창

## 개발자 정보

| | |
| --- | --- |
| 개발자 | **cybereun** |
| GitHub | [@cybereun](https://github.com/cybereun) |
| 저장소 | [cybereun/Auto-Type](https://github.com/cybereun/Auto-Type) |
| 문의·버그 제보 | [Issues](https://github.com/cybereun/Auto-Type/issues) |

## 라이선스와 출처

- Auto-Type: [MIT License](LICENSE)
- 한영 판단 로직과 언어 모델은 [Tavi](https://github.com/3x-haust/tavi) (© 2026 3x-haust, MIT)에서 가져왔습니다. 전문은 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에 있습니다.
