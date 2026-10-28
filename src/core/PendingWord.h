#pragma once
#include "Engine.h"

namespace autotype {
// A word stays private to the input controller until it is explicitly committed.
class PendingWord {
public:
    explicit PendingWord(const Model& model) : engine_(model) {}
    bool append(char key) { const auto before=engine_.raw().size(); engine_.append(key); return engine_.raw().size()!=before; }
    void backspace() { engine_.backspace(); }
    void toggle() { engine_.toggleLanguage(); }
    void clear() { engine_.clear(); }
    bool empty() const { return engine_.empty(); }
    const std::u32string& preview() const { return engine_.text(); }
    std::u32string finish() { return engine_.commit(); }
private:
    Engine engine_;
};
}
