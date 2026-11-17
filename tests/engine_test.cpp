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
    expect(model, "sork", U"내가");
    expect(model, "sksms", U"나는");
    expect(model, "sjsms", U"너는");
    expect(model, "dlfma", U"이름");
    expect(model, "gksrmf", U"한글");
    expect(model, "dkssud", U"안녕");
    expect(model, "input", U"input");
    expect(model, "speed", U"speed");
    expect(model, "git", U"git");
    expect(model, "Git", U"Git");
    expect(model, "rlt", U"깃");
    expect(model, "push", U"push");
    expect(model, "githubsms", U"github는");

    {
        Engine sentence(model);
        const auto first = type(sentence, "githubsms");
        const auto second = type(sentence, "git");
        const auto third = type(sentence, "push");
        const auto fourth = type(sentence, "di");
        const bool ok = first == U"github는" && second == U"git" && third == U"push" && fourth == U"야";
        if (!ok) ++failures;
        std::cout << (ok ? "ok   " : "FAIL ") << "mixed sentence -> " << toUtf8(first) << " "
                  << toUtf8(second) << " " << toUtf8(third) << " " << toUtf8(fourth) << "\n";
    }

    // A committed English word must not prevent the next Korean word from converting.
    for (const std::string& prior : {"hello", "the", "go", "input", "speed"}) {
        Engine sequence(model);
        type(sequence, prior);
        const auto got = type(sequence, "sork");
        const bool ok = got == U"내가";
        if (!ok) ++failures;
        std::cout << (ok ? "ok   " : "FAIL ") << prior << " + sork -> " << toUtf8(got) << "\n";
    }

    // Exercise the two-set composition rules for compound vowels and final consonants.
    for (const auto& item : std::vector<std::pair<std::string, std::u32string>>{
             {"rhk", U"과"}, {"rho", U"괘"}, {"rhl", U"괴"}, {"rnj", U"궈"},
             {"rnp", U"궤"}, {"rnl", U"귀"}, {"rml", U"긔"}, {"rkrtk", U"각사"},
             {"rksej", U"간더"}, {"rkfrk", U"갈가"}, {"rkqtk", U"갑사"},
             {"Rk", U"까"}, {"Ek", U"따"}, {"Qk", U"빠"}, {"Tk", U"싸"}, {"Wk", U"짜"}}) {
        const auto got = hangul::compose(item.first);
        const bool ok = got == item.second;
        if (!ok) ++failures;
        std::cout << (ok ? "ok   " : "FAIL ") << "compose " << item.first << " -> " << toUtf8(got) << "\n";
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
