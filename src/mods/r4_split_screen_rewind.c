/* r4_split_screen_rewind.c - no local Rewind in 2P VS Battle
 * (r4.modern-controls / split-screen-rewind).
 *
 * Owner rule: no Rewind in multiplayer or split screen. Netplay already
 * refuses Rewind in psxrecomp; this covers local 2P VS Battle. Every guest
 * VBlank, and right after a savestate or Rewind load, the plugin tells the
 * runtime whether a VS session is live (r4_vs_session: VS Battle chosen in
 * the main menu, or the VS race handler running), and psxrecomp's
 * psx_mod_set_rewind_blocked does the rest: Rewind refuses to open, captures
 * no history, and the Modern scheme's Y Rewind button reaches the game as if
 * Rewind were off. The block clears at every mod session start.
 *
 * It is its own hidden, default-on feature, so it applies whatever the
 * Controls feature is set to (Classic, or off) and the launcher never lists
 * it (game.toml [runtime] hide_hidden_mod_features). Reads only. */
#include <stdint.h>

#include "mod_plugins.h"
#include "r4_widescreen_scene.h"

#define R4_SSR_PLUGIN "r4.split-screen-rewind"

static uint32_t r4_ssr_read_word(uint32_t address) {
    return psx_mod_read_word(address);
}

static uint16_t r4_ssr_read_half(uint32_t address) {
    return psx_mod_read_half(address);
}

static void r4_ssr_update(void) {
    psx_mod_set_rewind_blocked(
        r4_vs_session(r4_ssr_read_word, r4_ssr_read_half));
}

PSX_MOD_CONSTRUCTOR(r4_split_screen_rewind_register) {
    (void)psx_mod_register_vblank_plugin(R4_SSR_PLUGIN, r4_ssr_update);
    (void)psx_mod_register_savestate_plugin(R4_SSR_PLUGIN, r4_ssr_update);
}
