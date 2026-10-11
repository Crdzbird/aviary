#include "aviary/interop/json.hpp"

#include <cmath>
#include <cstdio>
#include <sstream>

namespace aviary::json {

bool Value::as_bool() const {
  if (type_ != Type::kBool) throw JsonError("json: not a bool");
  return bool_;
}
double Value::as_number() const {
  if (type_ != Type::kNumber) throw JsonError("json: not a number");
  return num_;
}
const std::string& Value::as_string() const {
  if (type_ != Type::kString) throw JsonError("json: not a string");
  return str_;
}
const std::vector<Value>& Value::items() const {
  if (type_ != Type::kArray) throw JsonError("json: not an array");
  return arr_;
}

void Value::set(const std::string& key, Value v) {
  if (type_ != Type::kObject) throw JsonError("json: not an object");
  for (auto& kv : obj_) {
    if (kv.first == key) { kv.second = std::move(v); return; }
  }
  obj_.emplace_back(key, std::move(v));
}
bool Value::contains(const std::string& key) const {
  if (type_ != Type::kObject) return false;
  for (const auto& kv : obj_) if (kv.first == key) return true;
  return false;
}
const Value& Value::operator[](const std::string& key) const {
  if (type_ != Type::kObject) throw JsonError("json: not an object");
  for (const auto& kv : obj_) if (kv.first == key) return kv.second;
  throw JsonError("json: missing key '" + key + "'");
}
void Value::push_back(Value v) {
  if (type_ != Type::kArray) throw JsonError("json: not an array");
  arr_.push_back(std::move(v));
}

// -- Serialization ------------------------------------------------------------

namespace {
void escape_into(const std::string& s, std::string& out) {
  out.push_back('"');
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
}
}  // namespace

void Value::dump_into(std::string& out) const {
  switch (type_) {
    case Type::kNull: out += "null"; break;
    case Type::kBool: out += bool_ ? "true" : "false"; break;
    case Type::kNumber: {
      if (num_ == std::floor(num_) && std::abs(num_) < 1e15) {
        out += std::to_string(static_cast<long long>(num_));
      } else {
        std::ostringstream os; os << num_; out += os.str();
      }
      break;
    }
    case Type::kString: escape_into(str_, out); break;
    case Type::kArray: {
      out.push_back('[');
      for (std::size_t i = 0; i < arr_.size(); ++i) {
        if (i) out.push_back(',');
        arr_[i].dump_into(out);
      }
      out.push_back(']');
      break;
    }
    case Type::kObject: {
      out.push_back('{');
      for (std::size_t i = 0; i < obj_.size(); ++i) {
        if (i) out.push_back(',');
        escape_into(obj_[i].first, out);
        out.push_back(':');
        obj_[i].second.dump_into(out);
      }
      out.push_back('}');
      break;
    }
  }
}

std::string Value::dump() const {
  std::string out;
  dump_into(out);
  return out;
}

// -- Parsing ------------------------------------------------------------------

namespace {
class Parser {
 public:
  explicit Parser(const std::string& s) : s_(s) {}

  Value parse() {
    skip_ws();
    Value v = parse_value();
    skip_ws();
    if (pos_ != s_.size()) throw JsonError("json: trailing data");
    return v;
  }

 private:
  const std::string& s_;
  std::size_t pos_ = 0;
  int depth_ = 0;

  char peek() {
    if (pos_ >= s_.size()) throw JsonError("json: unexpected end");
    return s_[pos_];
  }
  char take() { char c = peek(); ++pos_; return c; }
  void skip_ws() {
    while (pos_ < s_.size()) {
      char c = s_[pos_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_; else break;
    }
  }
  void expect(char c) {
    if (take() != c) throw JsonError(std::string("json: expected '") + c + "'");
  }

  // Bound recursion: adversarial RPC responses (deeply nested arrays/objects)
  // would otherwise overflow the stack - a remote DoS.
  static constexpr int kMaxDepth = 200;
  struct DepthGuard {
    int& d;
    DepthGuard(int& d) : d(d) {
      if (++d > kMaxDepth) throw JsonError("json: nesting too deep");
    }
    ~DepthGuard() { --d; }
  };

  Value parse_value() {
    DepthGuard guard(depth_);
    skip_ws();
    char c = peek();
    switch (c) {
      case '{': return parse_object();
      case '[': return parse_array();
      case '"': return Value::string(parse_string());
      case 't': case 'f': return parse_bool();
      case 'n': return parse_null();
      default: return parse_number();
    }
  }

  Value parse_object() {
    expect('{');
    Value obj = Value::object();
    skip_ws();
    if (peek() == '}') { take(); return obj; }
    while (true) {
      skip_ws();
      std::string key = parse_string();
      skip_ws();
      expect(':');
      obj.set(key, parse_value());
      skip_ws();
      char c = take();
      if (c == '}') break;
      if (c != ',') throw JsonError("json: expected ',' or '}'");
    }
    return obj;
  }

  Value parse_array() {
    expect('[');
    Value arr = Value::array();
    skip_ws();
    if (peek() == ']') { take(); return arr; }
    while (true) {
      arr.push_back(parse_value());
      skip_ws();
      char c = take();
      if (c == ']') break;
      if (c != ',') throw JsonError("json: expected ',' or ']'");
    }
    return arr;
  }

  std::string parse_string() {
    expect('"');
    std::string out;
    while (true) {
      char c = take();
      if (c == '"') break;
      if (c == '\\') {
        char e = take();
        switch (e) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'u': {
            // Parse \uXXXX; emit as UTF-8 (basic BMP, no surrogate pairs).
            int cp = 0;
            for (int i = 0; i < 4; ++i) {
              char h = take();
              cp <<= 4;
              if (h >= '0' && h <= '9') cp |= h - '0';
              else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
              else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
              else throw JsonError("json: bad \\u escape");
            }
            if (cp < 0x80) out.push_back(static_cast<char>(cp));
            else if (cp < 0x800) {
              out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
              out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
              out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
              out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
              out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            break;
          }
          default: throw JsonError("json: bad escape");
        }
      } else {
        out.push_back(c);
      }
    }
    return out;
  }

  Value parse_bool() {
    if (s_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value::boolean(true); }
    if (s_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value::boolean(false); }
    throw JsonError("json: invalid literal");
  }
  Value parse_null() {
    if (s_.compare(pos_, 4, "null") == 0) { pos_ += 4; return Value::null(); }
    throw JsonError("json: invalid literal");
  }
  Value parse_number() {
    std::size_t start = pos_;
    if (peek() == '-') take();
    while (pos_ < s_.size()) {
      char c = s_[pos_];
      if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') ++pos_;
      else break;
    }
    if (pos_ == start) throw JsonError("json: invalid number");
    return Value::number(std::stod(s_.substr(start, pos_ - start)));
  }
};
}  // namespace

Value Value::parse(const std::string& text) { return Parser(text).parse(); }

}  // namespace aviary::json
