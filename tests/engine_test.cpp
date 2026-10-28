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
