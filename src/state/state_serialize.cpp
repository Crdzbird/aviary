#include "blockchainer/account_state.hpp"
#include "blockchainer/state.hpp"
#include "blockchainer/utxo_state.hpp"

namespace blockchainer {

std::unique_ptr<StateMachine> deserialize_state(StateModel model, codec::ByteView bytes) {
  switch (model) {
    case StateModel::kAccount:
      return AccountStateMachine::deserialize(bytes);
    case StateModel::kUtxo:
      return UtxoStateMachine::deserialize(bytes);
  }
  return AccountStateMachine::deserialize(bytes);  // unreachable; keeps the compiler happy
}

}  // namespace blockchainer
