#include "game_state.hpp"

#include <d/d_com_inf_game.h>

namespace midnafx::game_state {
twilight::State sample() {
    // Called only by mod_update on the game thread. State 2 is a Twilight spot, not an
    // active Twilight environment; do not infer one from stage names or player form.
    // getStage() always returns the address of embedded storage, even at the title screen.
    // The player pointer is registered by Link's actor and cleared on scene teardown.
    return twilight::classify(dComIfGp_getPlayer(0) != nullptr, dComIfGp_world_dark_get());
}
} // namespace midnafx::game_state
