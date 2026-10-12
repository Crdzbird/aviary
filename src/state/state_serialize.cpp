#include <memory>
#include "aviary/account_state.hpp"
#include "aviary/record_state.hpp"
#include "aviary/state.hpp"
#include "aviary/utxo_state.hpp"

namespace aviary {

std::unique_ptr<StateMachine> deserialize_state(StateModel model, codec::ByteView bytes) {
  switch (model) {
    case StateModel::kAccount:
      return AccountStateMachine::deserialize(bytes);
    case StateModel::kUtxo:
      return UtxoStateMachine::deserialize(bytes);
    case StateModel::kRecord:
      return RecordStateMachine::deserialize(bytes);
  }
  return AccountStateMachine::deserialize(bytes);  // unreachable; keeps the compiler happy
}

}  // namespace aviary
