# Auto-Type v1.0.0

Windows에서 단어를 입력할 때 한글과 영어를 자동으로 판단하는 단독 실행형 도우미입니다. 입력 중인 단어는 별도 미리보기 창에 표시되고 스페이스 또는 Enter 등을 누르면 현재 입력창에 전달됩니다.

## 실행

1. GitHub Releases의 `Auto-Type-1.0.0-Windows-x64.exe`를 내려받아 실행합니다.
2. **자동 입력 켜기**를 누른 뒤 원하는 앱의 입력창에서 입력합니다.
3. 앱을 종료하면 기능이 꺼집니다. Windows에 키보드 입력기로 설치하거나 시작 프로그램에 등록하지 않습니다.

| 입력 | 기능 |
| --- | --- |
| 영문 자판 | 한글과 영어를 단어별로 자동 판단합니다 |
| 한/영 키 또는 Shift+Space | 입력 전에는 시작 언어, 입력 중에는 현재 단어의 언어를 바꿉니다 |
| Space, Enter, Tab 등 | 대기 중인 단어를 대상 창에 전달한 뒤 해당 키를 처리합니다 |
| Backspace | 입력 중인 단어에서 마지막 키를 지웁니다 |
| Esc | 입력 중인 단어를 취소합니다 |

예: `dkssudgktpdy` → `안녕하세요`, `hello` → `hello`, `APIfmf` → `API를`.

입력 중 다른 곳을 클릭하거나 포커스가 바뀌면 잘못된 위치에 입력하지 않도록 대기 단어의 전달이 중단됩니다. 이때 앱 창에서 **대기 입력 취소**를 누르고 다시 입력하세요. 단축키를 쓰기 전에는 단어를 확정하거나 취소해야 합니다.

비밀번호 입력창으로 표시된 곳은 처리하지 않습니다. 입력창의 접근성 정보나 관리자 권한 차이 때문에 지원하지 못하는 앱이 있을 수 있습니다. 엑셀에서는 선택한 셀 근처에 미리보기를 표시합니다. 모든 앱에서 한영 판단이 항상 정확하다고 보장하지는 않습니다.

자세한 사용법은 [사용 안내](USAGE.md)를 참고하세요. 입력 내용은 별도로 저장하거나 네트워크로 보내지 않습니다.

## 개발 및 테스트

Visual Studio 2022의 C++ 도구와 CMake가 필요합니다.

```powershell
cmake -S . -B build-x64 -A x64
cmake --build build-x64 --config Release
build-x64/Release/engine_test.exe resources/model-v2.dat
build-x64/Release/pending_word_test.exe resources/model-v2.dat
```

`AutoTypePreview.exe`가 단독 실행 앱입니다. 저장소에 남아 있는 `src/tsf`는 이전 버전의 Windows 입력기 실험 코드이며, v1.0.0 배포 파일에는 포함하지 않습니다.

## 라이선스

Auto-Type은 MIT 라이선스입니다. 한영 판별 로직과 모델은 Tavi에서 가져왔으며 상세 내용은 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에 있습니다.
