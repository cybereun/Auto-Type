// JavaScript port of src/core/Engine.cpp, so the website demo decides exactly like the Windows IME.
// Logic and model from Tavi (https://github.com/3x-haust/tavi, MIT License, (c) 2026 3x-haust).
(function (root) {
  "use strict";

  const INITIALS = [..."ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"];
  const VOWELS = [..."ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ"];
  const FINALS = [..." ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ"];
  const KEYS = "rRseEfaqQtTdwWczxvgkoiOjpuPhynbml";
  const JAMO = [..."ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎㅏㅐㅑㅒㅓㅔㅕㅖㅗㅛㅜㅠㅡㅣ"];
  const VOWEL_PAIRS = { "ㅗㅏ": "ㅘ", "ㅗㅐ": "ㅙ", "ㅗㅣ": "ㅚ", "ㅜㅓ": "ㅝ", "ㅜㅔ": "ㅞ", "ㅜㅣ": "ㅟ", "ㅡㅣ": "ㅢ" };
  const FINAL_PAIRS = { "ㄱㅅ": "ㄳ", "ㄴㅈ": "ㄵ", "ㄴㅎ": "ㄶ", "ㄹㄱ": "ㄺ", "ㄹㅁ": "ㄻ", "ㄹㅂ": "ㄼ",
    "ㄹㅅ": "ㄽ", "ㄹㅌ": "ㄾ", "ㄹㅍ": "ㄿ", "ㄹㅎ": "ㅀ", "ㅂㅅ": "ㅄ" };
  const SUFFIXES = ["은", "는", "이", "가", "을", "를", "에", "의", "와", "과", "도", "만", "로", "으로",
    "에서", "에게", "한테", "부터", "까지", "처럼", "보다", "하고", "이랑", "랑", "라고", "이라고", "라는",
    "이라는", "이나", "나", "요", "야", "입니다", "를요", "는요"];

  const isSyllable = (c) => c >= "가" && c <= "힣";
  const isJamo = (c) => c >= "ㄱ" && c <= "ㅣ";
  const isUpper = (c) => c >= "A" && c <= "Z";
  const isLower = (c) => c >= "a" && c <= "z";
  const isLetter = (c) => isUpper(c) || isLower(c);
  const isInitial = (c) => INITIALS.includes(c);
  const shiftKey = (c) => "QWERTOP".includes(c);

  function jamoForKey(key) {
    let i = KEYS.indexOf(key);
    if (i < 0) i = KEYS.indexOf(key.toLowerCase());
    return i < 0 ? null : JAMO[i];
  }

  function compose(keys) {
    let out = "", initial = null, vowel = null, fin = null;
    const rendered = () => {
      const l = initial ? INITIALS.indexOf(initial) : -1;
      const v = vowel ? VOWELS.indexOf(vowel) : -1;
      if (l < 0 || v < 0) return [initial, vowel, fin].filter(Boolean).join("");
      const t = fin ? FINALS.indexOf(fin) : 0;
      return String.fromCharCode(0xac00 + (l * 21 + v) * 28 + t);
    };
    const flush = () => { out += rendered(); initial = vowel = fin = null; };
    for (const key of keys) {
      const jamo = jamoForKey(key);
      if (!jamo) { flush(); out += key; continue; }
      if (VOWELS.includes(jamo)) {
        if (fin) {
          const last = fin;
          const pair = Object.keys(FINAL_PAIRS).find((k) => FINAL_PAIRS[k] === last);
          if (pair) { fin = pair[0]; out += rendered(); initial = pair[1]; }
          else { fin = null; out += rendered(); initial = last; }
          vowel = jamo; fin = null;
        } else if (vowel) {
          const merged = VOWEL_PAIRS[vowel + jamo];
          if (merged) vowel = merged; else { flush(); vowel = jamo; }
        } else vowel = jamo;
      } else if (!initial || !vowel) { flush(); initial = jamo; }
      else if (fin) {
        const merged = FINAL_PAIRS[fin + jamo];
        if (merged) fin = merged; else { flush(); initial = jamo; }
      } else if (FINALS.includes(jamo)) fin = jamo;
      else { flush(); initial = jamo; }
    }
    flush();
    return out;
  }

  function keysForJamo(j) {
    const i = JAMO.indexOf(j);
    if (i >= 0) return KEYS[i];
    for (const table of [VOWEL_PAIRS, FINAL_PAIRS])
      for (const k in table) if (table[k] === j) return keysForJamo(k[0]) + keysForJamo(k[1]);
    return "";
  }
  function keysFor(text) {
    let out = "";
    for (const c of text) {
      if (!isSyllable(c)) { out += keysForJamo(c); continue; }
      const idx = c.charCodeAt(0) - 0xac00;
      out += keysForJamo(INITIALS[Math.floor(idx / 588)]) + keysForJamo(VOWELS[Math.floor(idx / 28) % 21]);
      if (idx % 28) out += keysForJamo(FINALS[idx % 28]);
    }
    return out;
  }
  const SUFFIX_KEYS = new Set(SUFFIXES.map(keysFor));

  const syllableCount = (t) => [...t].filter(isSyllable).length;
  const jamoCount = (t) => [...t].filter(isJamo).length;
  const hasSyllable = (t) => [...t].some(isSyllable);
  const hasLatin = (t) => [...t].some(isLetter);
  const isKoreanOnly = (t) => t.length > 0 && !hasLatin(t) && [...t].some((c) => isSyllable(c) || isJamo(c));
  function isPlausibleKorean(t) {
    if (!isKoreanOnly(t)) return false;
    const stem = isInitial(t[t.length - 1]) ? t.slice(0, -1) : t;
    return stem.length > 0 && jamoCount(stem) === 0;
  }
  const isAcronym = (t) => ![...t].some(isLower) && [...t].filter(isUpper).length >= 2;
  function isStructuredLatin(t) {
    if (["://", "@", "/", "\\", "_", "`", ".", ":"].some((m) => t.includes(m))) return true;
    if (t.startsWith("www.") || /[0-9]/.test(t)) return true;
    let sawLower = false;
    for (const c of t) { if (isLower(c)) sawLower = true; else if (sawLower && isUpper(c)) return true; }
    return false;
  }

  // ------------------------------------------------------------- model
  const SYMBOLS = 27, TRIGRAMS = SYMBOLS ** 3, SYLLABLES = 11172, SMOOTH = 0.25;
  const utf8 = new TextEncoder();

  class Bloom {
    constructor(bits, count) { this.bits = bits; this.count = count; }
    has(value) {
      const bytes = utf8.encode(value);
      const fnv = (seed) => { let h = seed >>> 0; for (const b of bytes) h = Math.imul(h ^ b, 16777619) >>> 0; return h; };
      const first = fnv(2166136261), step = (fnv(16777619) | 1) >>> 0;
      for (let r = 0; r < 4; r++) {
        const pos = ((first + Math.imul(r, step)) >>> 0) % this.count;
        if ((this.bits[pos >>> 3] & (1 << (pos & 7))) === 0) return false;
      }
      return true;
    }
  }

  class Model {
    constructor(buffer) {
      const view = new DataView(buffer);
      let i = 4;
      const magic = String.fromCharCode(...new Uint8Array(buffer, 0, 4));
      if (magic !== "TVM2") throw new Error("bad model");
      const u32 = () => { const v = view.getUint32(i, true); i += 4; return v; };
      const u16s = (n) => { const a = new Uint16Array(n); for (let k = 0; k < n; k++) a[k] = view.getUint16(i + k * 2, true); i += n * 2; return a; };
      const bloom = () => { const bits = u32(); const b = new Bloom(new Uint8Array(buffer, i, bits / 8), bits); i += bits / 8; return b; };
      if (u32() !== TRIGRAMS) throw new Error("bad model");
      this.trigrams = u16s(TRIGRAMS);
      if (u32() !== SYLLABLES) throw new Error("bad model");
      this.syllables = u16s(SYLLABLES);
      this.common = bloom(); this.known = bloom(); this.korean = bloom();
      this.syllableTotal = this.syllables.reduce((a, b) => a + b, 0);
      this.contexts = new Uint32Array(SYMBOLS * SYMBOLS);
      for (let c = 0; c < SYMBOLS * SYMBOLS; c++)
        for (let k = 0; k < SYMBOLS; k++) this.contexts[c] += this.trigrams[c * SYMBOLS + k];
    }
    static norm(token) {
      let out = "";
      for (const c of token) {
        if (c === "'") continue;
        const l = c.toLowerCase();
        if (!isLower(l) || out.length >= 64) return null;
        out += l;
      }
      return out || null;
    }
    english(token) {
      const n = Model.norm(token);
      if (!n) return { score: -Infinity, tier: 0 };
      const tier = this.common.has(n) ? 2 : this.known.has(n) ? 1 : 0;
      const codes = [0, 0, ...[...n].map((c) => c.charCodeAt(0) - 96), 0];
      let total = 0;
      for (let k = 2; k < codes.length; k++) {
        const ctx = codes[k - 2] * SYMBOLS + codes[k - 1];
        total += Math.log((this.trigrams[ctx * SYMBOLS + codes[k]] + SMOOTH) / (this.contexts[ctx] + SMOOTH * SYMBOLS));
      }
      const stat = 7 + (total / (codes.length - 2)) * 1.15;
      return { score: tier === 2 ? Math.max(8.2, stat) : tier === 1 ? Math.max(6.8, stat) : stat, tier };
    }
    koreanScore(text) {
      if (!text) return -Infinity;
      let total = 0, units = 0, syl = 0;
      const denom = this.syllableTotal + SMOOTH * SYLLABLES;
      for (const c of text) {
        if (isSyllable(c)) { total += Math.log((this.syllables[c.charCodeAt(0) - 0xac00] + SMOOTH) / denom); units++; syl++; }
        else if (isJamo(c)) { total -= 11.5; units++; }
      }
      if (!units) return -Infinity;
      let score = 11.2 + total / units;
      if (syl === units && this.korean.has(text)) score = Math.max(score, 8);
      if (syl === 0 && text.length >= 2 && [...text].every((c) => "ㅋㅎㅠㅜㄷㅇㄱㅅㅂㄴ".includes(c))) score = 7;
      return score;
    }
    isEnglishWord(t) { const n = Model.norm(t); return !!n && (this.common.has(n) || this.known.has(n)); }
    isKnownKorean(t) { return this.korean.has(t); }
  }

  // ------------------------------------------------------------- scoring
  function score(model, raw, previous) {
    const scored = [];
    const add = (text, s, why) => { if (text && Number.isFinite(s)) scored.push({ text, score: s, why }); };
    const hangul = compose(raw);
    const english = model.english(raw);
    const wholeKorean = jamoCount(hangul) === 0 && model.isKnownKorean(hangul);
    let en = english.score, ko = model.koreanScore(hangul);
    const firstUpper = [...raw].find(isUpper);
    const koreanShift = [...raw].some(isLower) && firstUpper && shiftKey(firstUpper) &&
      [...raw].every(isLetter) && jamoCount(hangul) === 0;
    let enWhy = "영어 철자 통계";
    if (english.tier === 2) enWhy = "흔한 영어 단어"; else if (english.tier === 1) enWhy = "알려진 영어 단어";
    if (isStructuredLatin(raw) && !koreanShift) { en = Math.max(en, 14); enWhy = "URL·숫자·camelCase"; }
    else if (isAcronym(raw)) { en = Math.max(en, 10); enWhy = "대문자 약어"; }
    if (previous) { if (hasSyllable(previous)) ko += 0.45; else if (hasLatin(previous)) en += 0.35; }
    add(raw, en, enWhy);
    if (hangul !== raw) add(hangul, ko, model.isKnownKorean(hangul) ? "알려진 한국어 단어" : "한글 음절 빈도");

    const pending = hangul.length > 1 && isInitial(hangul[hangul.length - 1]);
    const stem = pending ? hangul.slice(0, -1) : hangul;
    const koreanShifted = [...raw].some(isUpper) && [...raw].every((c) => !isUpper(c) || shiftKey(c));
    const complete = jamoCount(stem) === 0 && (koreanShifted || ![...raw].some(isUpper));
    const stemScore = pending ? model.koreanScore(stem) : ko;
    const knownWhole = model.isKnownKorean(stem);
    const addSplit = (text, s, latin, koreanFirst, why) => {
      const coincidental = complete && (koreanFirst || knownWhole || koreanShifted || (stemScore >= 5 && latin.length <= 3));
      add(text, coincidental ? Math.min(s, ko - 1) : s, why);
    };
    if (raw.length >= 3 && english.tier === 0) {
      for (let split = 2; split < raw.length; split++) {
        const head = raw.slice(0, split), tail = raw.slice(split);
        const allows = !wholeKorean || (firstUpper && !shiftKey(firstUpper));
        const trusted = model.isEnglishWord(head) || isAcronym(head);
        const newName = !trusted && SUFFIX_KEYS.has(tail) && head.length >= 3 &&
          !model.isKnownKorean(hangul) && model.english(head).score >= 2.8;
        if ((trusted || newName) && allows) {
          const tailH = compose(tail);
          const len = Math.min(head.length, 8) / 10;
          if (SUFFIXES.includes(tailH)) { addSplit(head + tailH, 12.5 + len, head, false, "영어 + 조사"); break; }
          if (syllableCount(tailH) >= 2 && (model.isKnownKorean(tailH) || model.koreanScore(tailH) >= 4.7))
            addSplit(head + tailH, 11.8 + len, head, false, "영어 + 한국어");
        }
        if (tail.length >= 2 && !wholeKorean && model.isEnglishWord(tail)) {
          const headH = compose(head);
          if (syllableCount(headH) >= 1 && model.isKnownKorean(headH)) addSplit(headH + tail, 10.5, tail, true, "한국어 + 영어");
        }
      }
    }
    const best = new Map();
    for (const item of scored) if (!best.has(item.text) || item.score > best.get(item.text).score) best.set(item.text, item);
    const ordered = [...best.values()].sort((a, b) => (a.score === b.score ? (a.text < b.text ? -1 : 1) : b.score - a.score));
    if (!ordered.length) return [];
    const max = ordered[0].score;
    const weights = ordered.map((o) => Math.exp(Math.min(0, o.score - max)));
    const total = weights.reduce((a, b) => a + b, 0);
    return ordered.map((o, k) => ({ text: o.text, why: o.why, confidence: weights[k] / total }));
  }

  class Engine {
    constructor(model) { this.model = model; this.previous = ""; this.clear(); }
    clear() { this.raw = ""; this.text = ""; this.candidates = []; this.lock = null; }
    refresh() {
      if (!this.raw) { this.text = ""; this.candidates = []; return; }
      if (this.lock === "ko") { this.text = compose(this.raw); return; }
      if (this.lock === "en") { this.text = this.raw; return; }
      const shownKorean = isPlausibleKorean(this.text);
      this.candidates = score(this.model, this.raw, this.previous);
      let chosen = this.candidates[0];
      if (shownKorean && chosen && !isKoreanOnly(chosen.text) && chosen.confidence < 0.99) {
        const ko = this.candidates.find((c) => isKoreanOnly(c.text) && isPlausibleKorean(c.text));
        if (ko) chosen = ko;
      }
      // Korean first: keys that are not a known English word read as Korean while still plausible.
      if (chosen && !hasSyllable(chosen.text) && !isKoreanOnly(chosen.text) && !isStructuredLatin(this.raw) &&
          !isAcronym(this.raw) &&
          (this.model.english(this.raw).tier === 0 || (this.raw.length === 1 && !"aAI".includes(this.raw)))) {
        const ko = compose(this.raw);
        const starting = jamoCount(ko) === ko.length && ko.length <= 2;
        if (isKoreanOnly(ko) && (isPlausibleKorean(ko) || starting)) { this.text = ko; return; }
      }
      this.text = chosen ? chosen.text : this.raw;
    }
    append(key) { if (this.raw.length < 64) { this.raw += key; this.refresh(); } }
    backspace() { this.raw = this.raw.slice(0, -1); if (!this.raw) this.clear(); else this.refresh(); }
    toggle() { if (!this.raw) return; this.lock = hasSyllable(this.text) || isKoreanOnly(this.text) ? "en" : "ko"; this.refresh(); }
    commit() { const t = this.text; if (t) this.previous = t; this.clear(); return t; }
  }

  const api = { Model, Engine, compose };
  if (typeof module !== "undefined") module.exports = api; else root.AutoType = api;
})(this);
