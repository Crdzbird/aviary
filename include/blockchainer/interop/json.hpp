#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/// A small, dependency-free JSON value + parser + serializer, used by the
/// JSON-RPC client (ARCHITECTURE §11). Objects preserve insertion order so
/// serialization is deterministic (important for testable requests).
namespace blockchainer::json {

class JsonError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class Value {
 public:
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

  Value() : type_(Type::kNull) {}

  static Value null() { return Value(); }
  static Value boolean(bool b) { Value v; v.type_ = Type::kBool; v.bool_ = b; return v; }
  static Value number(double n) { Value v; v.type_ = Type::kNumber; v.num_ = n; return v; }
  static Value string(std::string s) { Value v; v.type_ = Type::kString; v.str_ = std::move(s); return v; }
  static Value array(std::vector<Value> a = {}) { Value v; v.type_ = Type::kArray; v.arr_ = std::move(a); return v; }
  static Value object() { Value v; v.type_ = Type::kObject; return v; }

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::kNull; }
  bool is_object() const { return type_ == Type::kObject; }
  bool is_array() const { return type_ == Type::kArray; }

  bool as_bool() const;
  double as_number() const;
  const std::string& as_string() const;
  const std::vector<Value>& items() const;

  /// Object accessors.
  void set(const std::string& key, Value v);
  bool contains(const std::string& key) const;
  const Value& operator[](const std::string& key) const;

  /// Array push.
  void push_back(Value v);

  std::string dump() const;
  static Value parse(const std::string& text);

 private:
  Type type_;
  bool bool_ = false;
  double num_ = 0.0;
  std::string str_;
  std::vector<Value> arr_;
  std::vector<std::pair<std::string, Value>> obj_;

  void dump_into(std::string& out) const;
};

}  // namespace blockchainer::json
