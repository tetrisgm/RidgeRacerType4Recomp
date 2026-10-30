/* Presentation-only packet interpolation. Guest timing stays at stock rate. */
#include <stdlib.h>
#include <string.h>
#include "mod_plugins.h"
#include "packet_interpolation_stream.h"
#define R4_FR_PACKAGE "r4.enhancement.frame-rate"
static unsigned active_rate;
static uint32_t resolved_fps(void) {
    char value[16];
    if(!psx_mod_option_value(R4_FR_PACKAGE,"frame-rate","rate",value,sizeof value)) return 60;
    if(!strcmp(value,"auto")) return 60;
    if(!strcmp(value,"display")) return UINT32_MAX;
    char *end;unsigned long rate=strtoul(value,&end,10);
    return end!=value && !*end?(uint32_t)rate:60;
}
static void activate(void) {
    unsigned fps=resolved_fps();active_rate=fps==30?0:fps;
    packet_interpolation_set_rate(active_rate);
}
static void reset_history(void) {packet_interpolation_set_rate(0);}
static void restored(void) {packet_interpolation_set_rate(active_rate);}
PSX_MOD_CONSTRUCTOR(r4_frame_rate_register) {
    psx_mod_register_activation_plugin("r4.framerate",activate);
    psx_mod_register_session_reset_plugin("r4.framerate",reset_history);
    psx_mod_register_savestate_plugin("r4.framerate",restored);
}
