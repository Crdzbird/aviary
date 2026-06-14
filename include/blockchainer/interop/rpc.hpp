#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include "blockchainer/interop/json.hpp"

/// Generic JSON-RPC 2.0 client over a pluggable transport (ARCHITECTURE §11).
/// The transport is an interface so the logic is testable with an in-memory mock;
/// a real HTTP transport is a thin adapter (deferred — see IMPROVEMENTS).
namespace blockchainer::interop {

/// A JSON-RPC error returned by the server (carries the numeric code).
class RpcError : public std::runtime_error {
 public:
  RpcError(long code, const std::string& message)
      : std::runtime_error("RPC error " + std::to_string(code) + ": " + message),
        code(code) {}
  long code;
};

/// Sends a request body and returns the response body. Implementations: HTTP
/// POST in production, a canned-response mock in tests.
class RpcTransport {
 public:
  virtual ~RpcTransport() = default;
  virtual std::string send(const std::string& request_json) = 0;
};

class JsonRpcClient {
 public:
  explicit JsonRpcClient(RpcTransport* transport) : transport_(transport) {}

  /// Call [method] with [params] (an array Value). Returns the `result` value;
  /// throws RpcError if the response carries an error.
  json::Value call(const std::string& method, json::Value params);

 private:
  RpcTransport* transport_;
  std::uint64_t next_id_ = 1;
};

}  // namespace blockchainer::interop
