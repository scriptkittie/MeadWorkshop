// Mead Workshop — a small JSON reader (C++17, no dependencies).
// Only what the app needs: parse the data files and saved designs into a tree of values.
#pragma once

#include <charconv>
#include <cmath>
#include <cstdio>
#include <system_error>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace mead::json {

class Value {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  Value() = default;
  static Value number(double d) { Value v; v.t_ = Type::Number; v.n_ = d; return v; }
  static Value boolean(bool b) { Value v; v.t_ = Type::Bool; v.b_ = b; return v; }
  static Value string(std::string s) { Value v; v.t_ = Type::String; v.s_ = std::move(s); return v; }
  static Value array() { Value v; v.t_ = Type::Array; return v; }
  static Value object() { Value v; v.t_ = Type::Object; return v; }

  Type type() const { return t_; }
  bool isNull() const { return t_ == Type::Null; }
  bool isNumber() const { return t_ == Type::Number; }
  bool isString() const { return t_ == Type::String; }
  bool isBool() const { return t_ == Type::Bool; }
  bool isArray() const { return t_ == Type::Array; }
  bool isObject() const { return t_ == Type::Object; }

  double num(double fallback = 0) const { return t_ == Type::Number ? n_ : fallback; }
  bool flag(bool fallback = false) const { return t_ == Type::Bool ? b_ : fallback; }
  const std::string& str() const { static const std::string empty; return t_ == Type::String ? s_ : empty; }
  const std::vector<Value>& items() const { static const std::vector<Value> none; return t_ == Type::Array ? a_ : none; }
  const std::map<std::string, Value>& members() const { static const std::map<std::string, Value> none; return t_ == Type::Object ? o_ : none; }

  // Object member, or a shared null value when missing.
  const Value& operator[](const std::string& key) const {
    static const Value nullValue;
    if (t_ != Type::Object) return nullValue;
    auto it = o_.find(key);
    return it == o_.end() ? nullValue : it->second;
  }
  bool has(const std::string& key) const { return t_ == Type::Object && o_.count(key) > 0; }

  std::vector<Value>& arr() { return a_; }
  std::map<std::string, Value>& obj() { return o_; }

 private:
  Type t_ = Type::Null;
  double n_ = 0;
  bool b_ = false;
  std::string s_;
  std::vector<Value> a_;
  std::map<std::string, Value> o_;
};

namespace detail {
class Parser {
 public:
  explicit Parser(const std::string& s) : s_(s) {}
  Value parseDocument() {
    Value v = parseValue();
    ws();
    if (i_ != s_.size()) fail("trailing characters");
    return v;
  }

 private:
  const std::string& s_;
  size_t i_ = 0;
  int depth_ = 0;   // nesting depth: a corrupt file with thousands of [ [ [ must fail, not overflow the stack
  static constexpr int kMaxDepth = 200;
  struct Nest {
    Parser& p;
    explicit Nest(Parser& pp) : p(pp) { if (++p.depth_ > kMaxDepth) p.fail("nested too deeply"); }
    ~Nest() { --p.depth_; }
  };

  [[noreturn]] void fail(const char* what) { throw std::runtime_error(std::string("JSON: ") + what + " at " + std::to_string(i_)); }
  void ws() { while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\n' || s_[i_] == '\r' || s_[i_] == '\t')) ++i_; }
  bool lit(const char* w) {
    size_t n = 0;
    while (w[n]) ++n;
    if (s_.compare(i_, n, w) == 0) { i_ += n; return true; }
    return false;
  }
  Value parseValue() {
    ws();
    if (i_ >= s_.size()) fail("unexpected end");
    char c = s_[i_];
    if (c == '{') { Nest n(*this); return parseObject(); }
    if (c == '[') { Nest n(*this); return parseArray(); }
    if (c == '"') return Value::string(parseString());
    if (lit("true")) return Value::boolean(true);
    if (lit("false")) return Value::boolean(false);
    if (lit("null")) return Value();
    return parseNumber();
  }
  Value parseNumber() {
    // from_chars ignores the system locale (strtod would misread "1.5" on a German system).
    const char* start = s_.data() + i_;
    const char* stop = s_.data() + s_.size();
    double d = 0;
    auto r = std::from_chars(start, stop, d);
    if (r.ec != std::errc() || r.ptr == start) fail("bad value");
    i_ += static_cast<size_t>(r.ptr - start);
    return Value::number(d);
  }
  static void appendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
  }
  unsigned hex4() {
    if (i_ + 4 > s_.size()) fail("bad escape");
    unsigned v = 0;
    for (int k = 0; k < 4; ++k) {
      char h = s_[i_++];
      v <<= 4;
      if (h >= '0' && h <= '9') v |= static_cast<unsigned>(h - '0');
      else if (h >= 'a' && h <= 'f') v |= static_cast<unsigned>(h - 'a' + 10);
      else if (h >= 'A' && h <= 'F') v |= static_cast<unsigned>(h - 'A' + 10);
      else fail("bad hex");
    }
    return v;
  }
  std::string parseString() {
    ++i_;  // opening quote
    std::string out;
    while (true) {
      if (i_ >= s_.size()) fail("unterminated string");
      char c = s_[i_++];
      if (c == '"') break;
      if (c != '\\') { out += c; continue; }
      if (i_ >= s_.size()) fail("bad escape");
      char e = s_[i_++];
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          unsigned cp = hex4();
          if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 1 < s_.size() && s_[i_] == '\\' && s_[i_ + 1] == 'u') {
            i_ += 2;
            unsigned lo = hex4();
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          }
          appendUtf8(out, cp);
          break;
        }
        default: fail("bad escape");
      }
    }
    return out;
  }
  Value parseArray() {
    ++i_;
    Value v = Value::array();
    ws();
    if (i_ < s_.size() && s_[i_] == ']') { ++i_; return v; }
    while (true) {
      v.arr().push_back(parseValue());
      ws();
      if (i_ >= s_.size()) fail("unterminated array");
      if (s_[i_] == ',') { ++i_; continue; }
      if (s_[i_] == ']') { ++i_; return v; }
      fail("expected , or ]");
    }
  }
  Value parseObject() {
    ++i_;
    Value v = Value::object();
    ws();
    if (i_ < s_.size() && s_[i_] == '}') { ++i_; return v; }
    while (true) {
      ws();
      if (i_ >= s_.size() || s_[i_] != '"') fail("expected key");
      std::string key = parseString();
      ws();
      if (i_ >= s_.size() || s_[i_] != ':') fail("expected :");
      ++i_;
      v.obj()[key] = parseValue();
      ws();
      if (i_ >= s_.size()) fail("unterminated object");
      if (s_[i_] == ',') { ++i_; continue; }
      if (s_[i_] == '}') { ++i_; return v; }
      fail("expected , or }");
    }
  }
};
}  // namespace detail

inline Value parse(const std::string& text) { return detail::Parser(text).parseDocument(); }

// Write a value as JSON text (UTF-8 passes through; numbers in shortest round-trip form).
inline void write(const Value& v, std::string& out) {
  switch (v.type()) {
    case Value::Type::Null: out += "null"; break;
    case Value::Type::Bool: out += v.flag() ? "true" : "false"; break;
    case Value::Type::Number: {
      if (!std::isfinite(v.num())) { out += "null"; break; }
      char buf[64];
      auto r = std::to_chars(buf, buf + sizeof buf, v.num());
      out.append(buf, r.ptr);
      break;
    }
    case Value::Type::String: {
      out += '"';
      for (unsigned char c : v.str()) {
        switch (c) {
          case '"': out += "\\\""; break;
          case '\\': out += "\\\\"; break;
          case '\n': out += "\\n"; break;
          case '\r': out += "\\r"; break;
          case '\t': out += "\\t"; break;
          default:
            if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
            else out += static_cast<char>(c);
        }
      }
      out += '"';
      break;
    }
    case Value::Type::Array: {
      out += '[';
      bool first = true;
      for (const auto& x : v.items()) { if (!first) out += ','; first = false; write(x, out); }
      out += ']';
      break;
    }
    case Value::Type::Object: {
      out += '{';
      bool first = true;
      for (const auto& [k, x] : v.members()) {
        if (!first) out += ',';
        first = false;
        write(Value::string(k), out);
        out += ':';
        write(x, out);
      }
      out += '}';
      break;
    }
  }
}
inline std::string stringify(const Value& v) { std::string s; write(v, s); return s; }

}  // namespace mead::json
