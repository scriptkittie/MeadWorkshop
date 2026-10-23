// Mead Workshop — the word list (C++17, no Qt dependency).
//
// Every brewing term the app uses, in everyday words first, with the real name next to it
// (so "Starting sugar reading" is shown with "OG"). The Words tab lists them all, and
// linkTerms() underlines them in any sentence on screen so a tap explains them.
#pragma once

#include <string>
#include <vector>

namespace mead::words {

struct Entry {
  std::string key;      // stable id used in links ("og")
  std::string term;     // the real name ("OG (original gravity)")
  std::string plain;    // everyday words ("Starting sugar reading")
  std::string group;    // "Readings and numbers", "Ingredients", "Steps", "Equipment", "Styles"
  std::string what;     // the explanation, 1–4 sentences
  std::string src;      // where the books cover it ("" if general knowledge)
  std::vector<std::string> match;   // words that link to this entry inside sentences
};

const std::vector<Entry>& glossary();          // in display order, grouped
const std::vector<std::string>& groups();      // the group names, in order
const Entry* find(const std::string& key);

// The sentence as Qt "StyledText": & < > escaped, line breaks as <br>, and the first mention of
// each word-list term wrapped in <a href="term:KEY">…</a>. Matching is whole-word; all-caps
// abbreviations (OG, FG, ABV…) match only in capitals.
std::string linkTerms(const std::string& text);

// Undo linkTerms (for tests): drop the tags, un-escape, <br> back to line breaks.
std::string stripLinks(const std::string& styled);

}  // namespace mead::words
