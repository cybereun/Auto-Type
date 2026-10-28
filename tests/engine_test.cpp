// Runs on any OS: g++ -std=c++17 -I src/core src/core/Engine.cpp tests/engine_test.cpp && ./a.out resources/model-v2.dat
#include <fstream>
#include <iostream>
#include <iterator>

#include "Engine.h"

using namespace autotype;

static int failures = 0;

static std::u32string type(Engine& engine, const std::string& keys) {
    for (char k : keys) engine.append(k);
    return engine.commit();
}

static void expect(const Model& model, const std::string& keys, const std::u32string& want) {
    Engine engine(model);
    std::u32string got = type(engine, keys);
    bool ok = got == want;
    if (!ok) ++failures;
    std::cout << (ok ? "ok   " : "FAIL ") << keys << " -> " << toUtf8(got) << (ok ? "" : "  (want " + toUtf8(want) + ")") << "\n";
}

int main(int argc, char** argv) {
    std::ifstream file(argc > 1 ? argv[1] : "resources/model-v2.dat", std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Model model;
    if (!model.load(reinterpret_cast<const uint8_t*>(data.data()), data.size())) {
        std::cout << "model failed to load\n";
        return 1;
    }

    Engine startEnglish(model);
    startEnglish.toggleLanguage();
    startEnglish.append('d');
    if (startEnglish.text() != U"d") { ++failures; std::cout << "FAIL idle toggle starts in English\n"; }
    for (char k : std::string("kssud")) startEnglish.append(k);
    if (startEnglish.commit() != U"안녕") { ++failures; std::cout << "FAIL English start still detects Korean\n"; }
    startEnglish.append('h');
    if (startEnglish.text() != U"h") { ++failures; std::cout << "FAIL start preference survives commit\n"; }
    startEnglish.clear();
    startEnglish.toggleLanguage();
    startEnglish.append('g');
    if (startEnglish.text() != U"ㅎ") { ++failures; std::cout << "FAIL idle toggle returns to Korean\n"; }

    expect(model, "dkssudgktpdy", U"안녕하세요");
    expect(model, "gksdudwjsghks", U"한영전환");
    expect(model, "APIfmf", U"API를");
    expect(model, "hello", U"hello");
    expect(model, "world", U"world");
    expect(model, "rkskek", U"가나다");
    expect(model, "ehoTsk", U"됐나");
    expect(model, "github", U"github");
    expect(model, "test123", U"test123");
    expect(model, "tkfkd", U"사랑");
    expect(model, "ehldjdigo", U"되어야해");
    expect(model, "go", U"go");
    expect(model, "a", U"a");
    expect(model, "the", U"the");
    expect(model, "threhrk", U"속도가");
    expect(model, "githubdp", U"github에");
    expect(model, "gkrry", U"학교");
    expect(model, "dhsmf", U"오늘");
    expect(model, "sodlf", U"내일");
    expect(model, "dnflskfk", U"우리나라");
    expect(model, "rkatkgkqslek", U"감사합니다");
    expect(model, "tlrks", U"시간");
    expect(model, "vkdlf", U"파일");
    expect(model, "threh", U"속도");
    expect(model, "dkssud", U"안녕");
    expect(model, "program", U"program");
    expect(model, "computer", U"computer");
    expect(model, "windows", U"windows");
    expect(model, "email", U"email");
    expect(model, "Google", U"Google");
    expect(model, "API", U"API");
    expect(model, "hello123", U"hello123");
    {
        Engine phrase(model);
        bool input = type(phrase, "dlqfur") == U"입력";
        bool speed = type(phrase, "threhrk") == U"속도가";
        if (!input || !speed) { ++failures; std::cout << "FAIL phrase 입력 속도가\n"; }
    }

    // Words start in Korean: the first key shows a jamo, not a Latin letter.
    for (std::string first : {"d", "t", "e", "r"}) {
        Engine e(model);
        e.append(first[0]);
        bool korean = !e.text().empty() && e.text()[0] >= 0x3131 && e.text()[0] <= 0x3163;
        if (!korean) ++failures;
        std::cout << (korean ? "ok   " : "FAIL ") << "first key " << first << " -> " << toUtf8(e.text()) << "\n";
    }

    Engine engine(model);
    for (char k : std::string("dkssud")) engine.append(k);
    engine.toggleLanguage();
    bool toggled = engine.commit() == U"dkssud";
    if (!toggled) ++failures;
    std::cout << (toggled ? "ok   " : "FAIL ") << "toggle 안녕 -> dkssud\n";

    std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("all passed")) << "\n";
    return failures ? 1 : 0;
}
