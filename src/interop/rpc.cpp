#include "blockchainer/interop/rpc.hpp"

namespace blockchainer::interop {

json::Value JsonRpcClient::call(const std::string& method, json::Value params) {
  json::Value request = json::Value::object();
  request.set("jsonrpc", json::Value::string("2.0"));
  request.set("method", json::Value::string(method));
  request.set("params", std::move(params));
  request.set("id", json::Value::number(static_cast<double>(next_id_++)));

  const std::string response_text = transport_->send(request.dump());
  json::Value response = json::Value::parse(response_text);

  if (response.contains("error") && !response["error"].is_null()) {
    const json::Value& err = response["error"];
    const long code = err.contains("code") ? static_cast<long>(err["code"].as_number()) : 0;
    const std::string msg = err.contains("message") ? err["message"].as_string() : "unknown";
    throw RpcError(code, msg);
  }
  if (!response.contains("result")) {
    throw RpcError(0, "response has neither result nor error");
  }
  return response["result"];
}

}  // namespace blockchainer::interop
