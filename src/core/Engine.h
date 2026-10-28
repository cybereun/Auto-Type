// Auto-Type core: decides, per word, whether the typed keys are Korean (2-beolsik) or English.
// Ported from Tavi (https://github.com/3x-haust/tavi, MIT License, (c) 2026 3x-haust).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace autotype {

// Hangul composition for the standard 2-beolsik layout.
namespace hangul {
std::u32string compose(const std::string& keys);
int syllableCount(const std::u32string& text);
int jamoCount(const std::u32string& text);
bool isInitial(char32_t c);
}  // namespace hangul

std::string toUtf8(const std::u32string& text);
std::u16string toUtf16(const std::u32string& text);

class Model {
public:
    enum class Tier { Unknown, Known, Common };
    struct Evidence {
        double score;
        Tier tier;
    };

    // Loads the "TVM2" model blob. Returns false (and stays empty) when the data is invalid.
    bool load(const uint8_t* data, size_t size);
    bool loaded() const { return loaded_; }

    Evidence englishEvidence(const std::string& token) const;
    double koreanScore(const std::u32string& text) const;
    bool isEnglishWord(const std::string& token) const;
    bool isKnownKorean(const std::u32string& text) const;

private:
    struct Bloom {
        std::vector<uint8_t> bits;
        uint32_t bitCount = 0;
        bool contains(const std::string& value) const;
    };
    double averageEnglishLogProbability(const std::string& word) const;

    bool loaded_ = false;
    std::vector<uint16_t> trigrams_;
    std::vector<uint32_t> contexts_;
    std::vector<uint16_t> syllables_;
    double syllableTotal_ = 0;
    Bloom commonEnglish_, knownEnglish_, knownKorean_;
};

struct Candidate {
    std::u32string text;
    double confidence;
};

// One word being typed. Feed it ASCII keys; read `text()` for what should be shown.
class Engine {
public:
    explicit Engine(const Model& model) : model_(model) {}

    void append(char key);
    void backspace();
    // Flips the current word between the Korean and the English reading (the 한/영 key).
    void toggleLanguage();
    bool startsInKorean() const { return startKorean_; }
    // Returns the final text and resets the word.
    std::u32string commit();
    void clear();

    bool empty() const { return raw_.empty(); }
    const std::string& raw() const { return raw_; }
    const std::u32string& text() const { return text_; }
    const std::vector<Candidate>& candidates() const { return candidates_; }

private:
    void refresh();
    std::vector<Candidate> score(const std::string& raw) const;

    const Model& model_;
    std::string raw_;
    std::u32string text_;
    std::u32string previous_;
    std::vector<Candidate> candidates_;
    enum class Lock { None, Korean, Latin } lock_ = Lock::None;
    bool startKorean_ = true;
};

}  // namespace autotype
