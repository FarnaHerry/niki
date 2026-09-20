// A small ordered JSON value type with a recursive-descent parser and two
// serializers (compact for protocol lines, pretty for .hui.json files).
//
// hui keeps its own JSON rather than taking a dependency: the document format
// and the MCP stdio framing are the only users, and both want insertion-ordered
// objects so generated files stay reviewable in a text editor.

#pragma once

#include "core/std.h"

namespace hui::json {

enum class Type {
  Null,
  Bool,
  Number,
  String,
  Array,
  Object,
};

class Value;
using Member = std::pair<std::string, Value>;

class Value final {
public:
  Value() = default;
  Value(std::nullptr_t) {}
  Value(bool value) : type_(Type::Bool), bool_(value) {}
  Value(int value) : type_(Type::Number), number_(static_cast<double>(value)) {}
  Value(long long value) : type_(Type::Number), number_(static_cast<double>(value)) {}
  Value(std::size_t value) : type_(Type::Number), number_(static_cast<double>(value)) {}
  Value(double value) : type_(Type::Number), number_(value) {}
  Value(const char* value) : type_(Type::String), string_(value) {}
  Value(std::string value) : type_(Type::String), string_(std::move(value)) {}
  Value(std::vector<Value> value) : type_(Type::Array), array_(std::move(value)) {}
  Value(std::vector<Member> value) : type_(Type::Object), object_(std::move(value)) {}

  static Value ArrayWith(std::vector<Value> items) { return Value(std::move(items)); }
  static Value ObjectWith(std::vector<Member> members) { return Value(std::move(members)); }

  [[nodiscard]] Type type() const { return type_; }
  [[nodiscard]] bool IsNull() const { return type_ == Type::Null; }
  [[nodiscard]] bool IsBool() const { return type_ == Type::Bool; }
  [[nodiscard]] bool IsNumber() const { return type_ == Type::Number; }
  [[nodiscard]] bool IsString() const { return type_ == Type::String; }
  [[nodiscard]] bool IsArray() const { return type_ == Type::Array; }
  [[nodiscard]] bool IsObject() const { return type_ == Type::Object; }

  [[nodiscard]] bool AsBool(bool fallback = false) const { return IsBool() ? bool_ : fallback; }
  [[nodiscard]] double AsNumber(double fallback = 0.0) const { return IsNumber() ? number_ : fallback; }
  [[nodiscard]] long long AsInteger(long long fallback = 0) const {
    return IsNumber() ? static_cast<long long>(number_) : fallback;
  }
  [[nodiscard]] const std::string& AsString() const {
    static const std::string empty;
    return IsString() ? string_ : empty;
  }
  [[nodiscard]] std::string AsString(const std::string& fallback) const { return IsString() ? string_ : fallback; }

  [[nodiscard]] const std::vector<Value>& AsArray() const {
    static const std::vector<Value> empty;
    return IsArray() ? array_ : empty;
  }
  [[nodiscard]] const std::vector<Member>& AsObject() const {
    static const std::vector<Member> empty;
    return IsObject() ? object_ : empty;
  }

  /// Object member lookup; nullptr when absent or not an object.
  [[nodiscard]] const Value* Find(std::string_view key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& [name, value] : object_) {
      if (name == key) return &value;
    }
    return nullptr;
  }

  [[nodiscard]] bool Has(std::string_view key) const { return Find(key) != nullptr; }

  /// Object member read with fallback for the common compact accessors.
  [[nodiscard]] std::string GetString(std::string_view key, const std::string& fallback = "") const {
    const Value* value = Find(key);
    return value != nullptr && value->IsString() ? value->AsString() : fallback;
  }
  [[nodiscard]] double GetNumber(std::string_view key, double fallback = 0.0) const {
    const Value* value = Find(key);
    return value != nullptr && value->IsNumber() ? value->AsNumber() : fallback;
  }
  [[nodiscard]] bool GetBool(std::string_view key, bool fallback = false) const {
    const Value* value = Find(key);
    return value != nullptr && value->IsBool() ? value->AsBool() : fallback;
  }

  /// Sets or replaces one object member, preserving the current order.
  void Set(std::string key, Value value) {
    if (type_ != Type::Object) {
      type_ = Type::Object;
      object_.clear();
    }
    for (auto& [name, existing] : object_) {
      if (name == key) {
        existing = std::move(value);
        return;
      }
    }
    object_.emplace_back(std::move(key), std::move(value));
  }

  /// Removes one object member when present.
  void Erase(std::string_view key) {
    if (type_ != Type::Object) return;
    std::erase_if(object_, [key](const Member& member) { return member.first == key; });
  }

  [[nodiscard]] std::string Dump() const { return DumpValue(*this, "", ""); }
  [[nodiscard]] std::string DumpPretty() const { return DumpValue(*this, "  ", "\n"); }

  /// Parses one complete JSON document. Trailing non-whitespace text is an error.
  [[nodiscard]] static std::expected<Value, std::string> Parse(std::string_view text) {
    Parser parser{text};
    Value value{};
    if (const auto failure = parser.ParseValue(value); failure != nullptr) {
      return std::unexpected(std::string(failure));
    }
    parser.SkipWhitespace();
    if (!parser.AtEnd()) {
      return std::unexpected("hui: unexpected trailing content after the JSON document");
    }
    return value;
  }

private:
  Type type_ = Type::Null;
  bool bool_ = false;
  double number_ = 0.0;
  std::string string_;
  std::vector<Value> array_;
  std::vector<Member> object_;

  static void EscapeInto(std::string& out, std::string_view text) {
    out.push_back('"');
    for (const char raw : text) {
      const auto ch = static_cast<unsigned char>(raw);
      switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
          if (ch < 0x20) {
            static constexpr char digits[] = "0123456789abcdef";
            out += "\\u00";
            out.push_back(digits[(ch >> 4) & 0xF]);
            out.push_back(digits[ch & 0xF]);
          } else {
            out.push_back(raw);
          }
      }
    }
    out.push_back('"');
  }

  static void AppendNumber(std::string& out, double value) {
    if (!std::isfinite(value)) {
      out += "0";
      return;
    }
    if (value == std::floor(value) && std::abs(value) < 1e15) {
      out += std::format("{:.0f}", value);
      return;
    }
    out += std::format("{}", value);
  }

  static std::string DumpValue(const Value& value, std::string_view indent, std::string_view newline) {
    std::string out;
    DumpInto(out, value, indent, newline, 0);
    return out;
  }

  static void DumpInto(std::string& out, const Value& value, std::string_view indent, std::string_view newline,
                       std::size_t depth) {
    const bool pretty = !indent.empty();
    std::string prefix;
    for (std::size_t i = 0; i < depth; ++i) prefix += indent;

    switch (value.type_) {
      case Type::Null: out += "null"; break;
      case Type::Bool: out += value.bool_ ? "true" : "false"; break;
      case Type::Number: AppendNumber(out, value.number_); break;
      case Type::String: EscapeInto(out, value.string_); break;
      case Type::Array: {
        out += "[";
        const auto& items = value.array_;
        for (std::size_t i = 0; i < items.size(); ++i) {
          if (pretty) {
            out += newline;
            out += prefix;
            out += indent;
          }
          DumpInto(out, items[i], indent, newline, depth + 1);
          if (i + 1 < items.size()) out += ",";
        }
        if (pretty && !items.empty()) {
          out += newline;
          out += prefix;
        }
        out += "]";
        break;
      }
      case Type::Object: {
        out += "{";
        const auto& members = value.object_;
        for (std::size_t i = 0; i < members.size(); ++i) {
          if (pretty) {
            out += newline;
            out += prefix;
            out += indent;
          }
          EscapeInto(out, members[i].first);
          out += pretty ? ": " : ":";
          DumpInto(out, members[i].second, indent, newline, depth + 1);
          if (i + 1 < members.size()) out += ",";
        }
        if (pretty && !members.empty()) {
          out += newline;
          out += prefix;
        }
        out += "}";
        break;
      }
    }
  }

  class Parser final {
  public:
    explicit Parser(std::string_view text) : text_(text) {}

    [[nodiscard]] bool AtEnd() const { return position_ >= text_.size(); }

    void SkipWhitespace() {
      while (position_ < text_.size()) {
        const char ch = text_[position_];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
          ++position_;
        } else {
          break;
        }
      }
    }

    [[nodiscard]] const char* ParseValue(Value& out) {
      SkipWhitespace();
      if (AtEnd()) return "hui: unexpected end of JSON input";
      const char ch = text_[position_];
      switch (ch) {
        case '{': return ParseObject(out);
        case '[': return ParseArray(out);
        case '"': {
          std::string text;
          if (const char* failure = ParseString(text)) return failure;
          out = Value(std::move(text));
          return nullptr;
        }
        case 't': return ParseLiteral(out, "true", Value(true));
        case 'f': return ParseLiteral(out, "false", Value(false));
        case 'n': return ParseLiteral(out, "null", Value(nullptr));
        default: return ParseNumber(out);
      }
    }

  private:
    std::string_view text_;
    std::size_t position_ = 0;

    [[nodiscard]] const char* ParseLiteral(Value& out, std::string_view literal, Value value) {
      if (text_.substr(position_, literal.size()) != literal) {
        return "hui: invalid JSON literal";
      }
      position_ += literal.size();
      out = std::move(value);
      return nullptr;
    }

    [[nodiscard]] const char* ParseNumber(Value& out) {
      const std::size_t start = position_;
      if (!AtEnd() && text_[position_] == '-') ++position_;
      bool digits = false;
      while (!AtEnd()) {
        const char ch = text_[position_];
        if (ch >= '0' && ch <= '9') {
          digits = true;
          ++position_;
        } else {
          break;
        }
      }
      if (!AtEnd() && text_[position_] == '.') {
        ++position_;
        while (!AtEnd() && text_[position_] >= '0' && text_[position_] <= '9') {
          digits = true;
          ++position_;
        }
      }
      if (!digits) return "hui: invalid JSON number";
      if (!AtEnd() && (text_[position_] == 'e' || text_[position_] == 'E')) {
        ++position_;
        if (!AtEnd() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
        bool exponent_digits = false;
        while (!AtEnd() && text_[position_] >= '0' && text_[position_] <= '9') {
          exponent_digits = true;
          ++position_;
        }
        if (!exponent_digits) return "hui: invalid JSON exponent";
      }
      const std::string number_text(text_.substr(start, position_ - start));
      double number = 0.0;
      const auto [pointer, error] = std::from_chars(number_text.data(), number_text.data() + number_text.size(), number);
      if (error != std::errc{} || number_text == "-") return "hui: invalid JSON number";
      out = Value(number);
      return nullptr;
    }

    [[nodiscard]] const char* ParseString(std::string& out) {
      if (AtEnd() || text_[position_] != '"') return "hui: expected a JSON string";
      ++position_;
      while (!AtEnd()) {
        const char ch = text_[position_];
        if (ch == '"') {
          ++position_;
          return nullptr;
        }
        if (ch == '\\') {
          ++position_;
          if (AtEnd()) return "hui: unterminated JSON escape";
          const char escape = text_[position_++];
          switch (escape) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
              if (position_ + 4 > text_.size()) return "hui: truncated JSON unicode escape";
              const std::string hex(text_.substr(position_, 4));
              position_ += 4;
              std::uint32_t code = 0;
              const auto [pointer, error] = std::from_chars(hex.data(), hex.data() + hex.size(), code, 16);
              if (error != std::errc{}) return "hui: invalid JSON unicode escape";
              if (code < 0x80) {
                out.push_back(static_cast<char>(code));
              } else if (code < 0x800) {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
              } else {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
              }
              break;
            }
            default: return "hui: invalid JSON escape character";
          }
          continue;
        }
        out.push_back(ch);
        ++position_;
      }
      return "hui: unterminated JSON string";
    }

    [[nodiscard]] const char* ParseArray(Value& out) {
      ++position_;  // '['
      std::vector<Value> items;
      SkipWhitespace();
      if (!AtEnd() && text_[position_] == ']') {
        ++position_;
        out = Value(std::move(items));
        return nullptr;
      }
      while (true) {
        Value item{};
        if (const char* failure = ParseValue(item)) return failure;
        items.push_back(std::move(item));
        SkipWhitespace();
        if (AtEnd()) return "hui: unterminated JSON array";
        if (text_[position_] == ',') {
          ++position_;
          continue;
        }
        if (text_[position_] == ']') {
          ++position_;
          out = Value(std::move(items));
          return nullptr;
        }
        return "hui: expected ',' or ']' in a JSON array";
      }
    }

    [[nodiscard]] const char* ParseObject(Value& out) {
      ++position_;  // '{'
      std::vector<Member> members;
      SkipWhitespace();
      if (!AtEnd() && text_[position_] == '}') {
        ++position_;
        out = Value(std::move(members));
        return nullptr;
      }
      while (true) {
        SkipWhitespace();
        std::string name;
        if (const char* failure = ParseString(name)) return failure;
        SkipWhitespace();
        if (AtEnd() || text_[position_] != ':') return "hui: expected ':' in a JSON object";
        ++position_;
        Value value{};
        if (const char* failure = ParseValue(value)) return failure;
        members.emplace_back(std::move(name), std::move(value));
        SkipWhitespace();
        if (AtEnd()) return "hui: unterminated JSON object";
        if (text_[position_] == ',') {
          ++position_;
          continue;
        }
        if (text_[position_] == '}') {
          ++position_;
          out = Value(std::move(members));
          return nullptr;
        }
        return "hui: expected ',' or '}' in a JSON object";
      }
    }
  };
};

}  // namespace hui::json
