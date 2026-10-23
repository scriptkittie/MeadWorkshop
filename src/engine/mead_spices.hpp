// Mead Workshop — spices, herbs, flowers and oak (C++17, no Qt dependency).
//
// For each one: how much for a Subtle, Noticeable or Strong character in 5 gallons, when it goes in, how,
// and where each figure comes from. Book figures (Piatz ch.3, ch.10; Schramm ch.6, ch.10, ch.12) come first;
// where the books give none, the figure is a homebrewers' starting point found on the web, named as such.
// Every amount is a starting point: taste and work up to your own liking (Piatz: "you can always add more,
// but it is hard to remove excessive spices").
#pragma once

#include <string>
#include <vector>

namespace mead {

struct SpiceLevel {
  double per5 = 0;       // amount in `unit` for 5 gallons
  std::string src;       // "Schramm ch.10", "Piatz ch.10 recipe", "web: gotmead.com forum", "app: between the books' figures"
};
struct SpiceInfo {
  std::string id, name;          // name as it appears in the recipe ("Ginger root, thin slices")
  std::string unit;              // "oz", "each", "tsp", "g", "cup"
  std::string what;              // for "each": "stick", "bean", "whole clove"…; plural adds "s"
  SpiceLevel subtle, noticeable, strong;
  std::string when;              // "primary" (brew day) | "secondary" (around day 21) | "aging" (after fermentation)
  std::string how;               // how to add it and how long to leave it
  std::string note;              // what the books say about it
  std::string src;               // book chapters for the how/note
  std::vector<std::string> web;  // web pages the non-book figures came from
  bool fromBooks() const;        // all three levels from the books
};

const std::vector<SpiceInfo>& spices();
const SpiceInfo* spiceById(const std::string& id);
const SpiceInfo* spiceForName(const std::string& adjunctName);   // "Ginger root" → ginger (by key word)

// "Subtle" | "Noticeable" | "Strong" (keys "subtle" | "noticeable" | "strong")
const SpiceLevel& spiceLevel(const SpiceInfo& s, const std::string& level);
double spiceAmount(const SpiceInfo& s, const std::string& level, double batchGal);   // in s.unit
std::string spiceMeasure(const SpiceInfo& s, double amount);                          // "7½ oz", "2 sticks", "2½ tsp"

// The Designer's line under a spice: the three levels for this batch, when, and the sources.
std::string spiceGuide(const SpiceInfo& s, double batchGal);

}  // namespace mead
