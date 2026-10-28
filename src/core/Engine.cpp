// Ported from Tavi (https://github.com/3x-haust/tavi, MIT License, (c) 2026 3x-haust).
#include "Engine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace autotype {

namespace {

const std::u32string kInitials = U"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ";
const std::u32string kVowels = U"ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ";
const std::u32string kFinals = U" ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ";
const std::string kKeys = "rRseEfaqQtTdwWczxvgkoiOjpuPhynbml";
const std::u32string kJamo = U"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎㅏㅐㅑㅒㅓㅔㅕㅖㅗㅛㅜㅠㅡㅣ";

const std::map<std::u32string, char32_t> kVowelPairs = {
    {U"ㅗㅏ", U'ㅘ'}, {U"ㅗㅐ", U'ㅙ'}, {U"ㅗㅣ", U'ㅚ'}, {U"ㅜㅓ", U'ㅝ'},
    {U"ㅜㅔ", U'ㅞ'}, {U"ㅜㅣ", U'ㅟ'}, {U"ㅡㅣ", U'ㅢ'}};
const std::map<std::u32string, char32_t> kFinalPairs = {
    {U"ㄱㅅ", U'ㄳ'}, {U"ㄴㅈ", U'ㄵ'}, {U"ㄴㅎ", U'ㄶ'}, {U"ㄹㄱ", U'ㄺ'},
    {U"ㄹㅁ", U'ㄻ'}, {U"ㄹㅂ", U'ㄼ'}, {U"ㄹㅅ", U'ㄽ'}, {U"ㄹㅌ", U'ㄾ'},
    {U"ㄹㅍ", U'ㄿ'}, {U"ㄹㅎ", U'ㅀ'}, {U"ㅂㅅ", U'ㅄ'}};

bool isSyllable(char32_t c) { return c >= 0xAC00 && c <= 0xD7A3; }
bool isJamo(char32_t c) { return c >= 0x3131 && c <= 0x3163; }
bool contains(const std::u32string& s, char32_t c) { return s.find(c) != std::u32string::npos; }

char32_t jamoForKey(char key) {
    size_t i = kKeys.find(key);
    if (i == std::string::npos) i = kKeys.find(static_cast<char>(std::tolower(static_cast<unsigned char>(key))));
    return i == std::string::npos ? 0 : kJamo[i];
}

std::string keysForJamo(char32_t jamo) {
    size_t i = kJamo.find(jamo);
    if (i != std::u32string::npos) return std::string(1, kKeys[i]);
    for (auto* table : {&kVowelPairs, &kFinalPairs})
        for (auto& [pair, merged] : *table)
            if (merged == jamo) return keysForJamo(pair[0]) + keysForJamo(pair[1]);
    return "";
}

// The 2-beolsik keys that type a Korean word (조사 → key sequence).
std::string keysFor(const std::u32string& text) {
    std::string out;
    for (char32_t c : text) {
        if (!isSyllable(c)) { out += keysForJamo(c); continue; }
        int index = static_cast<int>(c - 0xAC00);
        out += keysForJamo(kInitials[index / 588]) + keysForJamo(kVowels[index / 28 % 21]);
        if (index % 28) out += keysForJamo(kFinals[index % 28]);
    }
    return out;
}

bool isUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool isLower(char c) { return c >= 'a' && c <= 'z'; }
bool isLetter(char c) { return isUpper(c) || isLower(c); }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool anyOf(const std::string& s, bool (*f)(char)) { return std::any_of(s.begin(), s.end(), f); }
bool koreanShiftKey(char c) { return std::string("QWERTOP").find(c) != std::string::npos; }

bool isAcronym(const std::string& text) {
    int letters = 0;
    for (char c : text) {
        if (isLower(c)) return false;
        if (isUpper(c)) ++letters;
    }
    return letters >= 2;
}

bool isStructuredLatin(const std::string& text) {
    for (const char* marker : {"://", "@", "/", "\\", "_", "`", ".", ":"})
        if (text.find(marker) != std::string::npos) return true;
    if (text.rfind("www.", 0) == 0 || anyOf(text, isDigit)) return true;
    bool sawLower = false;
    for (char c : text) {
        if (isLower(c)) sawLower = true;
        else if (sawLower && isUpper(c)) return true;
    }
    return false;
}

bool hasSyllable(const std::u32string& t) { return std::any_of(t.begin(), t.end(), isSyllable); }
bool hasLatin(const std::u32string& t) {
    return std::any_of(t.begin(), t.end(), [](char32_t c) { return c < 128 && isLetter(static_cast<char>(c)); });
}
bool isKoreanOnly(const std::u32string& t) {
    return !t.empty() && !hasLatin(t) &&
           std::any_of(t.begin(), t.end(), [](char32_t c) { return isSyllable(c) || isJamo(c); });
}
// Korean whose only stray jamo is a trailing consonant still being typed (안ㄴ, not 채ㅡ).
bool isPlausibleKorean(const std::u32string& t) {
    if (!isKoreanOnly(t)) return false;
    std::u32string stem = hangul::isInitial(t.back()) ? t.substr(0, t.size() - 1) : t;
    return !stem.empty() && hangul::jamoCount(stem) == 0;
}

std::u32string fromAscii(const std::string& s) { return std::u32string(s.begin(), s.end()); }

const std::vector<std::u32string>& suffixes() {
    static const std::vector<std::u32string> list = {
        U"은", U"는", U"이", U"가", U"을", U"를", U"에", U"의", U"와", U"과", U"도", U"만",
        U"로", U"으로", U"에서", U"에게", U"한테", U"부터", U"까지", U"처럼", U"보다", U"하고",
        U"이랑", U"랑", U"라고", U"이라고", U"라는", U"이라는", U"이나", U"나", U"요", U"야",
        U"입니다", U"를요", U"는요"};
    return list;
}
bool isSuffix(const std::u32string& t) {
    return std::find(suffixes().begin(), suffixes().end(), t) != suffixes().end();
}
const std::set<std::string>& suffixKeys() {
    static const std::set<std::string> keys = [] {
        std::set<std::string> out;
        for (auto& s : suffixes()) out.insert(keysFor(s));
        return out;
    }();
    return keys;
}

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr int kSymbols = 27;
constexpr int kTrigrams = kSymbols * kSymbols * kSymbols;
constexpr int kSyllables = 11172;
constexpr double kSmoothing = 0.25;
constexpr size_t kMaxRaw = 64;

bool normalizedAscii(const std::string& token, std::string& out) {
    out.clear();
    for (char c : token) {
        if (c == '\'') continue;
        char lower = isUpper(c) ? static_cast<char>(c + 32) : c;
        if (!isLower(lower) || out.size() >= 64) return false;
        out.push_back(lower);
    }
    return !out.empty();
}

}  // namespace

// ---------------------------------------------------------------- Hangul

bool hangul::isInitial(char32_t c) { return contains(kInitials, c); }

int hangul::syllableCount(const std::u32string& t) {
    return static_cast<int>(std::count_if(t.begin(), t.end(), isSyllable));
}
int hangul::jamoCount(const std::u32string& t) {
    return static_cast<int>(std::count_if(t.begin(), t.end(), isJamo));
}

std::u32string hangul::compose(const std::string& keys) {
    std::u32string out;
    char32_t initial = 0, vowel = 0, final = 0;
    auto rendered = [&]() -> std::u32string {
        size_t l = initial ? kInitials.find(initial) : std::u32string::npos;
        size_t v = vowel ? kVowels.find(vowel) : std::u32string::npos;
        if (l == std::u32string::npos || v == std::u32string::npos) {
            std::u32string s;
            for (char32_t c : {initial, vowel, final}) if (c) s.push_back(c);
            return s;
        }
        size_t t = final ? kFinals.find(final) : 0;
        return std::u32string(1, static_cast<char32_t>(0xAC00 + (l * 21 + v) * 28 + t));
    };
    auto flush = [&] { out += rendered(); initial = vowel = final = 0; };

    for (char key : keys) {
        char32_t jamo = jamoForKey(key);
        if (!jamo) { flush(); out.push_back(static_cast<char32_t>(static_cast<unsigned char>(key))); continue; }
        if (contains(kVowels, jamo)) {
            if (final) {
                char32_t last = final;
                bool split = false;
                for (auto& [pair, merged] : kFinalPairs) {
                    if (merged != last) continue;
                    final = pair[0];
                    out += rendered();
                    initial = pair[1];
                    split = true;
                    break;
                }
                if (!split) { final = 0; out += rendered(); initial = last; }
                vowel = jamo;
                final = 0;
            } else if (vowel) {
                auto it = kVowelPairs.find(std::u32string{vowel, jamo});
                if (it != kVowelPairs.end()) vowel = it->second;
                else { flush(); vowel = jamo; }
            } else {
                vowel = jamo;
            }
        } else if (!initial || !vowel) {
            flush();
            initial = jamo;
        } else if (final) {
            auto it = kFinalPairs.find(std::u32string{final, jamo});
            if (it != kFinalPairs.end()) final = it->second;
            else { flush(); initial = jamo; }
        } else if (contains(kFinals, jamo)) {
            final = jamo;
        } else {
            flush();
            initial = jamo;
        }
    }
    flush();
    return out;
}

std::string toUtf8(const std::u32string& text) {
    std::string out;
    for (char32_t c : text) {
        if (c < 0x80) out += static_cast<char>(c);
        else if (c < 0x800) { out += static_cast<char>(0xC0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

std::u16string toUtf16(const std::u32string& text) {
    std::u16string out;
    for (char32_t c : text) {
        if (c < 0x10000) out += static_cast<char16_t>(c);
        else {
            c -= 0x10000;
            out += static_cast<char16_t>(0xD800 + (c >> 10));
            out += static_cast<char16_t>(0xDC00 + (c & 0x3FF));
        }
    }
    return out;
}

// ---------------------------------------------------------------- Model

bool Model::Bloom::contains(const std::string& value) const {
    if (bitCount == 0) return false;
    auto fnv = [&](uint32_t seed) {
        for (unsigned char b : value) seed = (seed ^ b) * 16777619u;
        return seed;
    };
    uint32_t first = fnv(2166136261u);
    uint32_t step = fnv(16777619u) | 1u;
    for (uint32_t round = 0; round < 4; ++round) {
        uint32_t position = (first + round * step) % bitCount;
        if ((bits[position >> 3] & (1u << (position & 7))) == 0) return false;
    }
    return true;
}

bool Model::load(const uint8_t* data, size_t size) {
    size_t index = 0;
    auto u32 = [&](uint32_t& v) {
        if (index + 4 > size) return false;
        v = data[index] | data[index + 1] << 8 | data[index + 2] << 16 | static_cast<uint32_t>(data[index + 3]) << 24;
        index += 4;
        return true;
    };
    auto u16s = [&](std::vector<uint16_t>& v, uint32_t count) {
        if (index + count * 2ull > size) return false;
        v.resize(count);
        for (uint32_t i = 0; i < count; ++i) v[i] = static_cast<uint16_t>(data[index + i * 2] | data[index + i * 2 + 1] << 8);
        index += count * 2ull;
        return true;
    };
    auto bloom = [&](Bloom& b) {
        uint32_t bits;
        if (!u32(bits) || bits == 0 || bits % 8 || index + bits / 8 > size) return false;
        b.bits.assign(data + index, data + index + bits / 8);
        b.bitCount = bits;
        index += bits / 8;
        return true;
    };
    uint32_t count;
    if (size < 4 || std::string(reinterpret_cast<const char*>(data), 4) != "TVM2") return false;
    index = 4;
    if (!u32(count) || count != kTrigrams || !u16s(trigrams_, count)) return false;
    if (!u32(count) || count != kSyllables || !u16s(syllables_, count)) return false;
    if (!bloom(commonEnglish_) || !bloom(knownEnglish_) || !bloom(knownKorean_) || index != size) return false;

    syllableTotal_ = 0;
    for (uint16_t s : syllables_) syllableTotal_ += s;
    contexts_.assign(kSymbols * kSymbols, 0);
    for (int c = 0; c < kSymbols * kSymbols; ++c)
        for (int k = 0; k < kSymbols; ++k) contexts_[c] += trigrams_[c * kSymbols + k];
    loaded_ = true;
    return true;
}

double Model::averageEnglishLogProbability(const std::string& word) const {
    std::vector<int> codes = {0, 0};
    for (char c : word) codes.push_back(c - 96);
    codes.push_back(0);
    double total = 0;
    for (size_t i = 2; i < codes.size(); ++i) {
        int context = codes[i - 2] * kSymbols + codes[i - 1];
        double count = trigrams_[context * kSymbols + codes[i]];
        double contextCount = contexts_[context];
        total += std::log((count + kSmoothing) / (contextCount + kSmoothing * kSymbols));
    }
    return total / static_cast<double>(codes.size() - 2);
}

Model::Evidence Model::englishEvidence(const std::string& token) const {
    std::string n;
    if (!loaded_ || !normalizedAscii(token, n)) return {-kInf, Tier::Unknown};
    Tier tier = commonEnglish_.contains(n) ? Tier::Common : knownEnglish_.contains(n) ? Tier::Known : Tier::Unknown;
    double statistical = 7.0 + averageEnglishLogProbability(n) * 1.15;
    switch (tier) {
        case Tier::Common: return {std::max(8.2, statistical), tier};
        case Tier::Known: return {std::max(6.8, statistical), tier};
        default: return {statistical, tier};
    }
}

double Model::koreanScore(const std::u32string& text) const {
    if (!loaded_ || text.empty()) return -kInf;
    double total = 0;
    int units = 0, syllableUnits = 0;
    double denominator = syllableTotal_ + kSmoothing * kSyllables;
    for (char32_t c : text) {
        if (isSyllable(c)) {
            total += std::log((syllables_[c - 0xAC00] + kSmoothing) / denominator);
            ++units;
            ++syllableUnits;
        } else if (isJamo(c)) {
            total -= 11.5;
            ++units;
        }
    }
    if (units == 0) return -kInf;
    double score = 11.2 + total / units;
    if (syllableUnits == units && knownKorean_.contains(toUtf8(text))) score = std::max(score, 8.0);
    if (syllableUnits == 0) {
        const std::u32string chat = U"ㅋㅎㅠㅜㄷㅇㄱㅅㅂㄴ";
        bool isChat = text.size() >= 2 && std::all_of(text.begin(), text.end(), [&](char32_t c) { return contains(chat, c); });
        if (isChat) score = 7.0;
    }
    return score;
}

bool Model::isEnglishWord(const std::string& token) const {
    std::string n;
    return loaded_ && normalizedAscii(token, n) && (commonEnglish_.contains(n) || knownEnglish_.contains(n));
}

bool Model::isKnownKorean(const std::u32string& text) const { return loaded_ && knownKorean_.contains(toUtf8(text)); }

// ---------------------------------------------------------------- Engine

std::vector<Candidate> Engine::score(const std::string& raw) const {
    struct Scored { std::u32string text; double score; };
    std::vector<Scored> scored;
    auto add = [&](const std::u32string& text, double s) {
        if (!text.empty() && std::isfinite(s)) scored.push_back({text, s});
    };

    const std::u32string hangulText = hangul::compose(raw);
    const Model::Evidence english = model_.englishEvidence(raw);
    const bool wholeKoreanWord = hangul::jamoCount(hangulText) == 0 && model_.isKnownKorean(hangulText);
    double englishScore = english.score;
    double koreanScore = model_.koreanScore(hangulText);

    auto firstUpper = std::find_if(raw.begin(), raw.end(), isUpper);
    const bool koreanShift = anyOf(raw, isLower) && firstUpper != raw.end() && koreanShiftKey(*firstUpper) &&
                             std::all_of(raw.begin(), raw.end(), isLetter) && hangul::jamoCount(hangulText) == 0;
    if (isStructuredLatin(raw) && !koreanShift) englishScore = std::max(englishScore, 14.0);
    else if (isAcronym(raw)) englishScore = std::max(englishScore, 10.0);
    if (!previous_.empty()) {
        if (hasSyllable(previous_)) koreanScore += 0.45;
        else if (hasLatin(previous_)) englishScore += 0.35;
    }
    add(fromAscii(raw), englishScore);
    if (hangulText != fromAscii(raw)) add(hangulText, koreanScore);

    // Mixed words: English head + Korean particle (APIfmf → API를) or Korean head + English tail.
    const bool pending = hangulText.size() > 1 && hangul::isInitial(hangulText.back());
    const std::u32string stem = pending ? hangulText.substr(0, hangulText.size() - 1) : hangulText;
    const bool koreanShifted = anyOf(raw, isUpper) &&
                               std::all_of(raw.begin(), raw.end(), [](char c) { return !isUpper(c) || koreanShiftKey(c); });
    const bool completeKorean = hangul::jamoCount(stem) == 0 && (koreanShifted || !anyOf(raw, isUpper));
    const double stemScore = pending ? model_.koreanScore(stem) : koreanScore;
    const bool knownWholeKorean = model_.isKnownKorean(stem);
    auto addSplit = [&](const std::u32string& text, double s, const std::string& latin, bool koreanFirst) {
        bool coincidental = completeKorean &&
                            (koreanFirst || knownWholeKorean || koreanShifted || (stemScore >= 5 && latin.size() <= 3));
        add(text, coincidental ? std::min(s, koreanScore - 1) : s);
    };

    const bool ascii = std::all_of(raw.begin(), raw.end(), [](char c) { return static_cast<unsigned char>(c) < 128; });
    if (raw.size() >= 3 && english.tier == Model::Tier::Unknown && ascii) {
        for (size_t split = 2; split < raw.size(); ++split) {
            const std::string head = raw.substr(0, split), tail = raw.substr(split);
            const bool allowsSplit = !wholeKoreanWord || (firstUpper != raw.end() && !koreanShiftKey(*firstUpper));
            const bool trustedHead = model_.isEnglishWord(head) || isAcronym(head);
            const bool plausibleNewName = !trustedHead && suffixKeys().count(tail) && head.size() >= 3 &&
                                          !model_.isKnownKorean(hangulText) && model_.englishEvidence(head).score >= 2.8;
            if ((trustedHead || plausibleNewName) && allowsSplit) {
                const std::u32string tailHangul = hangul::compose(tail);
                const double length = std::min<double>(static_cast<double>(head.size()), 8) / 10;
                if (isSuffix(tailHangul)) {
                    addSplit(fromAscii(head) + tailHangul, 12.5 + length, head, false);
                    break;
                }
                if (hangul::syllableCount(tailHangul) >= 2 &&
                    (model_.isKnownKorean(tailHangul) || model_.koreanScore(tailHangul) >= 4.7))
                    addSplit(fromAscii(head) + tailHangul, 11.8 + length, head, false);
            }
            if (tail.size() >= 2 && !wholeKoreanWord && model_.isEnglishWord(tail)) {
                const std::u32string headHangul = hangul::compose(head);
                if (hangul::syllableCount(headHangul) >= 1 && model_.isKnownKorean(headHangul))
                    addSplit(headHangul + fromAscii(tail), 10.5, tail, true);
            }
        }
    }

    std::map<std::u32string, double> best;
    for (auto& item : scored) {
        auto it = best.find(item.text);
        if (it == best.end() || item.score > it->second) best[item.text] = item.score;
    }
    std::vector<Scored> ordered;
    for (auto& [text, s] : best) ordered.push_back({text, s});
    std::sort(ordered.begin(), ordered.end(), [](const Scored& a, const Scored& b) {
        return a.score == b.score ? a.text < b.text : a.score > b.score;
    });
    std::vector<Candidate> out;
    if (ordered.empty()) return out;
    double maximum = ordered.front().score, total = 0;
    std::vector<double> weights;
    for (auto& item : ordered) { weights.push_back(std::exp(std::min(0.0, item.score - maximum))); total += weights.back(); }
    for (size_t i = 0; i < ordered.size(); ++i) out.push_back({ordered[i].text, total > 0 ? weights[i] / total : 0});
    return out;
}

void Engine::refresh() {
    if (raw_.empty()) { text_.clear(); candidates_.clear(); return; }
    if (lock_ == Lock::Korean) { text_ = hangul::compose(raw_); return; }
    if (lock_ == Lock::Latin) { text_ = fromAscii(raw_); return; }

    const bool shownKorean = isPlausibleKorean(text_);
    candidates_ = score(raw_);
    const Candidate* chosen = candidates_.empty() ? nullptr : &candidates_.front();
    // Once a word reads as Korean, a weak mid-word preference must not flip it back (안 → 아ss → 안녀).
    if (shownKorean && chosen && !isKoreanOnly(chosen->text) && chosen->confidence < 0.99) {
        for (auto& c : candidates_)
            if (isKoreanOnly(c.text) && isPlausibleKorean(c.text)) { chosen = &c; break; }
    }
    // Korean first: a run of keys that is not a known English word reads as Korean while it is still
    // plausible Korean (ㅇ, 아, 되어얗), so words start in Korean instead of flashing English.
    if (chosen && !hasSyllable(chosen->text) && !isKoreanOnly(chosen->text) && !isStructuredLatin(raw_) &&
        !isAcronym(raw_) &&
        (model_.englishEvidence(raw_).tier == Model::Tier::Unknown ||
         (raw_.size() == 1 && std::string("aAI").find(raw_[0]) == std::string::npos))) {
        const std::u32string korean = hangul::compose(raw_);
        const bool startingKorean = hangul::jamoCount(korean) == static_cast<int>(korean.size()) && korean.size() <= 2;
        if (isKoreanOnly(korean) && (isPlausibleKorean(korean) || startingKorean)) {
            text_ = korean;
            return;
        }
    }
    text_ = chosen ? chosen->text : fromAscii(raw_);
}

void Engine::append(char key) {
    if (raw_.size() >= kMaxRaw) return;
    raw_ += key;
    refresh();
}

void Engine::backspace() {
    if (raw_.empty()) return;
    raw_.pop_back();
    if (raw_.empty()) { clear(); return; }
    refresh();
}

void Engine::toggleLanguage() {
    if (raw_.empty()) return;
    lock_ = isKoreanOnly(text_) || hasSyllable(text_) ? Lock::Latin : Lock::Korean;
    refresh();
}

std::u32string Engine::commit() {
    std::u32string result = text_;
    if (!result.empty()) previous_ = result;
    clear();
    return result;
}

void Engine::clear() {
    raw_.clear();
    text_.clear();
    candidates_.clear();
    lock_ = Lock::None;
}

}  // namespace autotype
