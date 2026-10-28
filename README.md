# Auto-Type — 한영 전환 없이 쓰는 Windows 입력기

한/영 키를 누르지 않고 그냥 치면, 단어마다 한국어인지 영어인지 알아서 정합니다.
`dkssudgktpdy` → 안녕하세요, `APIfmf` → API를, `hello` → hello.

macOS 입력기 [Tavi](https://github.com/3x-haust/tavi)의 판단 로직을 Windows 정식 입력기(TSF)로 옮긴 것입니다.
Windows의 **한국어 키보드 목록에 별도 입력기로 등록**되기 때문에, 기존 Microsoft 입력기와 나란히 두고 언제든 오갈 수 있습니다.

## 설치

1. GitHub의 **Actions → build → 최신 실행 → Artifacts**에서 `AutoType-windows.zip`을 받아 압축을 풉니다.
2. `install.bat`을 **우클릭 → 관리자 권한으로 실행**합니다.
3. **설정 → 시간 및 언어 → 언어 및 지역 → 한국어 → … → 언어 옵션 → 키보드**에 `Auto-Type (한영 자동)`이 보이면 끝입니다.

> 서명되지 않은 DLL이라 SmartScreen이나 백신이 경고할 수 있습니다.

## 원래 방식으로 돌아가기

| 하고 싶은 것 | 방법 |
| --- | --- |
| 잠깐 기존 Microsoft 입력기로 | **Win + Space** (작업표시줄의 입력기 아이콘 클릭도 됨) |
| 다시 Auto-Type으로 | **Win + Space** 한 번 더 |
| 아예 지우기 | `uninstall.bat`을 관리자 권한으로 실행. Microsoft 입력기는 그대로 남습니다 |

## 쓰는 법

| 키 | 하는 일 |
| --- | --- |
| 그냥 입력 | 입력 중인 단어가 한국어/영어로 자동 판단돼 보입니다 |
| **한/영 키** 또는 **Shift + Space** | 입력 중인 단어를 한국어 ⇄ 영어로 뒤집습니다 |
| Space | 단어를 확정하고 띄어 씁니다 |
| Enter, Tab, Esc, 방향키, 문장부호 | 단어를 확정한 뒤 키를 앱에 그대로 보냅니다 |
| Backspace | 입력 중인 단어에서 한 글자(키)씩 지웁니다 |
| Ctrl/Alt 조합 | 단어를 확정한 뒤 단축키를 그대로 실행합니다 |

모든 판단은 PC 안의 작은 통계 모델(DLL에 포함, 약 0.2MB)로만 하며, 네트워크를 쓰지 않습니다.

## 직접 빌드

Visual Studio 2022(C++ 데스크톱 개발)와 CMake가 필요합니다.

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
build\Release\engine_test.exe resources\model-v2.dat
```

Linux에서도 판단 엔진 테스트와 MinGW 크로스 빌드가 됩니다.

```bash
g++ -std=c++17 -I src/core src/core/Engine.cpp tests/engine_test.cpp -o engine_test && ./engine_test resources/model-v2.dat
```

## 구조

- `src/core/` — OS와 무관한 한글 조합, 통계 모델, 후보 점수 계산 (Tavi에서 이식)
- `src/tsf/` — Windows Text Services Framework 입력기 (키 처리, 조합 문자열, COM 등록)
- `resources/model-v2.dat` — Tavi의 언어 모델 (MIT, `THIRD_PARTY_NOTICES.md` 참고)

## 아직 없는 것

- 개인 단어 학습(Tavi의 "같은 표기 세 번 확정하면 기억")
- 후보 목록 창, 트레이 메뉴, 설정 화면
- 코드 서명된 설치 프로그램
