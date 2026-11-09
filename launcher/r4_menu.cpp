#if defined(RECOMP_LAUNCHER) && defined(PSX_SDL3)
// R4's in-game settings surface. Rendering/model come from maintained recomp-ui;
// pause, persistence and live application remain owned by psxrecomp's menu host.
#include "psx_menu_host.h"
#include "recomp_launcher.h"
#include "recomp_runtime_ui.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {
struct Row {
    std::string key, section, label, help, package, feature, option;
    std::vector<std::string> values, labels;
    std::vector<const char*> choices;
    RecompRuntimeUiItem item{};
    int feature_index = -1, option_index = -1;
};
std::mutex mutex;
const PsxMenuHost* host;
const RecompLauncherCModProvider* mods;
RecompRuntimeUi* ui;
ImGuiContext* context;
bool gl_ready, paused;
const char* last_change = "Applied";
SDL_JoystickID chord_pad = 0;
unsigned held;
uint64_t previous_tick;
SDL_Window* window;
std::vector<SDL_Event> mouse_events;
std::vector<std::unique_ptr<Row>> rows;
std::vector<RecompRuntimeUiItem> items;

Row& add(const char* key, const char* section, const char* label, const char* help,
         RecompRuntimeUiItemType type, int low = 0, int high = 1, int step = 1) {
    auto r = std::make_unique<Row>();
    r->key=key; r->section=section; r->label=label; r->help=help;
    r->item.type=type; r->item.minimum=low; r->item.maximum=high; r->item.step=step;
    rows.push_back(std::move(r)); return *rows.back();
}
Row* find(const RecompRuntimeUiItem* item) {
    for (auto& r: rows) if (r->key==item->key) return r.get();
    return nullptr;
}
bool online() { return host->netplay_active && host->netplay_active(); }
bool option_value(Row& r, RecompLauncherCModOption& out) {
    return mods && mods->feature_option_get && r.option_index >= 0 &&
        mods->feature_option_get(mods->ctx,r.package.c_str(),r.feature.c_str(),r.option_index,&out);
}
int get(void*, const RecompRuntimeUiItem* item, int* value) {
    Row* r=find(item); if (!r) return 0;
    if (r->feature_index >= 0) {
        if (r->option_index < 0) {
            RecompLauncherCModFeature f{};
            if (!mods->feature_get(mods->ctx,r->feature_index,&f)) return 0;
            *value=f.enabled; return 1;
        }
        RecompLauncherCModOption o{}; if (!option_value(*r,o)) return 0;
        if (!r->values.empty()) {
            auto at=std::find(r->values.begin(),r->values.end(),o.value);
            if (at==r->values.end()) return 0;
            *value=int(at-r->values.begin());
        } else *value=o.type==RECOMP_MOD_OPTION_BOOLEAN ? !std::strcmp(o.value,"true") : std::atoi(o.value);
        return 1;
    }
    RecompLauncherCSettings s{}; host->launcher_settings_get(&s);
    if (r->key=="fullscreen") *value=s.fullscreen!=0;
    else if (r->key=="volume") *value=s.volume;
    else return 0;
    return 1;
}
int enabled(void*, const RecompRuntimeUiItem* item) {
    Row* r=find(item); if (!r) return 0;
    if (r->key=="resume" || r->key=="quit" || r->key=="fullscreen" || r->key=="volume") return 1;
    if (online()) return 0;
    // PGXP is a fixed R4 requirement, never a performance fallback.
    if (r->package=="psx.enhancement.pgxp" && r->option_index < 0) return 0;
    RecompLauncherCModOption o{};
    if (r->option_index >= 0) return option_value(*r,o) && !o.disabled;
    return 1;
}
int set(void*, const RecompRuntimeUiItem* item, int value) {
    Row* r=find(item); if (!r || !enabled(nullptr,item)) return 0;
    if (r->feature_index >= 0) {
        std::string v;
        if (!r->values.empty()) {
            if (value<0 || value>=int(r->values.size())) return 0;
            v=r->values[value];
        } else if (item->type==RECOMP_RUNTIME_UI_BOOL) v=value?"true":"false";
        else v=std::to_string(value);
        int live=host->mod_set(r->package.c_str(),r->feature.c_str(),
                              r->option_index<0?nullptr:r->option.c_str(),v.c_str());
        last_change=live?"Applied":"Saved; restart the game to apply";
        return 1;
    }
    RecompLauncherCSettings s{}; host->launcher_settings_get(&s);
    if (r->key=="fullscreen") s.fullscreen=value;
    else if (r->key=="volume") s.volume=value;
    else return 0;
    unsigned restart=host->launcher_settings_apply(&s);
    last_change=restart?"Saved; restart the game to apply":
        r->key=="volume"?"Applied for this session":"Applied";
    return 1;
}
int action(void*, const RecompRuntimeUiItem* item) {
    if (!std::strcmp(item->key,"resume")) recomp_runtime_ui_close(ui);
    else {
        if (!recomp_runtime_ui_confirm(ui,item,"Press again to end this session")) return 0;
        int how=!std::strcmp(item->key,"restart")?PSX_MENU_EXIT_RELAUNCH:
            !std::strcmp(item->key,"launcher")?PSX_MENU_EXIT_TO_FRONTEND:PSX_MENU_EXIT_QUIT;
        recomp_runtime_ui_close(ui); host->request_exit(how);
    }
    return 0;
}
void visibility(void*,int open) {
    if (open) {
        paused=host->set_paused && host->set_paused(1);
        if (!paused) recomp_runtime_ui_set_status(ui,"Online: the game keeps running");
    } else if (paused) { host->set_paused(0); paused=false; }
}
int attach(void*,void* sdl_window,void*,const PsxMenuHost* h) {
    std::lock_guard<std::mutex> guard(mutex);
    if (!h || h->abi_version!=PSX_MENU_ABI_VERSION || !h->launcher_game_info ||
        !h->launcher_settings_get || !h->launcher_settings_apply || !h->mod_set) return 0;
    host=h; window=static_cast<SDL_Window*>(sdl_window); rows.clear(); items.clear();
    RecompLauncherCGameInfo game{}; host->launcher_game_info(&game); mods=game.mods;
    add("resume","Game","Resume","Return to the current game.",RECOMP_RUNTIME_UI_ACTION);
    add("restart","Game","Restart game","Ends this session and applies saved settings. Save your progress first.",RECOMP_RUNTIME_UI_ACTION);
    add("launcher","Game","Full launcher","Ends this session. Disc setup, rebinding, memory cards and online play are in the launcher.",RECOMP_RUNTIME_UI_ACTION);
    add("quit","Game","Quit","Exit the game. Save your progress first.",RECOMP_RUNTIME_UI_ACTION);
    add("fullscreen","Display","Fullscreen","Switch between windowed and fullscreen.",RECOMP_RUNTIME_UI_BOOL);
    add("volume","Audio","Volume","Game volume for this session.",RECOMP_RUNTIME_UI_INT,0,100,5);
    if (mods && mods->feature_count && mods->feature_get && mods->feature_option_get) {
        for (int f=0; f<mods->feature_count(mods->ctx); ++f) {
            RecompLauncherCModFeature feature{};
            if (!mods->feature_get(mods->ctx,f,&feature) || feature.hidden || feature.channel==RECOMP_MOD_CHANNEL_DEVELOPER) continue;
            std::string key=std::string(feature.package_id)+"/"+feature.id;
            auto& toggle=add(key.c_str(),feature.name,"Enabled",feature.description,RECOMP_RUNTIME_UI_BOOL);
            toggle.package=feature.package_id; toggle.feature=feature.id; toggle.feature_index=f;
            for (int n=0; n<feature.option_count; ++n) {
                RecompLauncherCModOption o{};
                if (!mods->feature_option_get(mods->ctx,feature.package_id,feature.id,n,&o) || o.type==RECOMP_MOD_OPTION_TEXT) continue;
                // The shared model uses int; don't narrow arbitrary 64-bit schemas.
                if (o.min_value<INT_MIN || o.max_value>INT_MAX || o.step>INT_MAX) continue;
                auto type=o.type==RECOMP_MOD_OPTION_BOOLEAN?RECOMP_RUNTIME_UI_BOOL:
                    o.type==RECOMP_MOD_OPTION_CHOICE?RECOMP_RUNTIME_UI_CHOICE:RECOMP_RUNTIME_UI_INT;
                auto& r=add((key+"/"+o.id).c_str(),feature.name,o.label,o.description,type,
                            int(o.min_value),int(o.max_value),std::max(1,int(o.step)));
                r.package=feature.package_id; r.feature=feature.id; r.option=o.id; r.feature_index=f; r.option_index=n;
                if (type==RECOMP_RUNTIME_UI_CHOICE && mods->feature_choice_get) {
                    for (int c=0;c<o.choice_count;++c) {
                        RecompLauncherCModChoice choice{};
                        if (mods->feature_choice_get(mods->ctx,feature.package_id,feature.id,o.id,c,&choice)) {
                            r.values.emplace_back(choice.value); r.labels.emplace_back(choice.label);
                        }
                    }
                    r.item.minimum=0; r.item.maximum=int(r.values.size())-1;
                }
            }
        }
    }
    for (auto& r:rows) {
        for (auto& label:r->labels) r->choices.push_back(label.c_str());
        r->item.key=r->key.c_str();r->item.section=r->section.c_str();r->item.label=r->label.c_str();r->item.description=r->help.c_str();
        r->item.choices=r->choices.empty()?nullptr:r->choices.data();r->item.choice_count=r->choices.size();
        items.push_back(r->item);
    }
    RecompRuntimeUiConfig cfg{};
    cfg.title="Ridge Racer Type 4";cfg.subtitle="In-game settings";cfg.theme="psx";
    cfg.accept_label="Enter / A";cfg.back_label="Esc / B";
    cfg.items=items.data();cfg.item_count=items.size();
    cfg.callbacks.get_value=get;cfg.callbacks.set_value=set;cfg.callbacks.run_action=action;
    cfg.callbacks.is_enabled=enabled;cfg.callbacks.visibility_changed=visibility;
    cfg.callbacks.save=[](void*) { recomp_runtime_ui_set_status(ui,last_change); };
    ui=recomp_runtime_ui_create(&cfg);return ui!=nullptr;
}
int event(void*,const void* ptr) {
    std::lock_guard<std::mutex> guard(mutex);
    if (!ui || !ptr) return 0;
    auto& e=*static_cast<const SDL_Event*>(ptr);
    bool open=recomp_runtime_ui_is_open(ui);int input=-1;
    if (e.type==SDL_EVENT_KEY_DOWN) {
        if (e.key.repeat && (e.key.key==SDLK_ESCAPE || e.key.key==SDLK_F10)) return 1;
        switch(e.key.key) {
        case SDLK_F10: if (!e.key.repeat) input=RECOMP_RUNTIME_UI_INPUT_TOGGLE;break;
        case SDLK_ESCAPE: input=open?RECOMP_RUNTIME_UI_INPUT_BACK:RECOMP_RUNTIME_UI_INPUT_TOGGLE;break;
        case SDLK_UP: input=RECOMP_RUNTIME_UI_INPUT_UP;break;
        case SDLK_DOWN: input=RECOMP_RUNTIME_UI_INPUT_DOWN;break;
        case SDLK_LEFT: input=RECOMP_RUNTIME_UI_INPUT_LEFT;break;
        case SDLK_RIGHT: input=RECOMP_RUNTIME_UI_INPUT_RIGHT;break;
        case SDLK_RETURN: input=RECOMP_RUNTIME_UI_INPUT_ACCEPT;break;
        }
        if (!open && input!=RECOMP_RUNTIME_UI_INPUT_TOGGLE) return 0;
    } else if (e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type==SDL_EVENT_GAMEPAD_BUTTON_UP) {
        if (chord_pad!=e.gbutton.which) { held=0;chord_pad=e.gbutton.which; }
        unsigned before=held,bit=1u<<e.gbutton.button;
        bool down=e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        held=down?(held|bit):(held&~bit);
        constexpr unsigned chord=(1u<<SDL_GAMEPAD_BUTTON_BACK)|(1u<<SDL_GAMEPAD_BUTTON_START);
        if (down && (e.gbutton.button==SDL_GAMEPAD_BUTTON_GUIDE ||
            ((held&chord)==chord && (before&chord)!=chord))) input=RECOMP_RUNTIME_UI_INPUT_TOGGLE;
        else if (open && down) switch(e.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:input=RECOMP_RUNTIME_UI_INPUT_UP;break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:input=RECOMP_RUNTIME_UI_INPUT_DOWN;break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:input=RECOMP_RUNTIME_UI_INPUT_LEFT;break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:input=RECOMP_RUNTIME_UI_INPUT_RIGHT;break;
        case SDL_GAMEPAD_BUTTON_SOUTH:input=RECOMP_RUNTIME_UI_INPUT_ACCEPT;break;
        case SDL_GAMEPAD_BUTTON_EAST:input=RECOMP_RUNTIME_UI_INPUT_BACK;break;
        }
    } else if (e.type==SDL_EVENT_GAMEPAD_REMOVED && e.gdevice.which==chord_pad) held=0;
    if (open && (e.type==SDL_EVENT_MOUSE_MOTION || e.type==SDL_EVENT_MOUSE_BUTTON_DOWN ||
                 e.type==SDL_EVENT_MOUSE_BUTTON_UP || e.type==SDL_EVENT_MOUSE_WHEEL)) {
        mouse_events.push_back(e);
        return 1;
    }
    if (input>=0) recomp_runtime_ui_handle_input(ui,static_cast<RecompRuntimeUiInput>(input),1,0);
    if (input==RECOMP_RUNTIME_UI_INPUT_TOGGLE) return 1;
    if (!open) return 0;
    return e.type==SDL_EVENT_KEY_DOWN || e.type==SDL_EVENT_KEY_UP || e.type==SDL_EVENT_TEXT_INPUT ||
        e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type==SDL_EVENT_GAMEPAD_BUTTON_UP || e.type==SDL_EVENT_GAMEPAD_AXIS_MOTION ||
        e.type==SDL_EVENT_MOUSE_BUTTON_DOWN || e.type==SDL_EVENT_MOUSE_BUTTON_UP || e.type==SDL_EVENT_MOUSE_WHEEL;
}
int is_open(void*) {std::lock_guard<std::mutex> guard(mutex);return ui && recomp_runtime_ui_is_open(ui);}
void set_open(void*,int open) {std::lock_guard<std::mutex> guard(mutex);if(ui) {if(open) recomp_runtime_ui_open(ui);else recomp_runtime_ui_close(ui);}}
void draw(void*,int w,int h) {
    std::lock_guard<std::mutex> guard(mutex);
    if (!ui || !recomp_runtime_ui_is_open(ui) || w<=0 || h<=0) return;
    auto* previous=ImGui::GetCurrentContext();
    if(!context) {context=ImGui::CreateContext();ImGui::SetCurrentContext(context);ImGui::GetIO().IniFilename=nullptr;}
    ImGui::SetCurrentContext(context);
    if(!gl_ready) gl_ready=ImGui_ImplOpenGL3_Init(nullptr);
    if(gl_ready) {
        auto& io=ImGui::GetIO();io.DisplaySize=ImVec2(float(w),float(h));io.DisplayFramebufferScale=ImVec2(1,1);
        int window_w=w, window_h=h;
        SDL_GetWindowSize(window,&window_w,&window_h);
        for (const auto& e:mouse_events) {
            if (e.type==SDL_EVENT_MOUSE_MOTION)
                io.AddMousePosEvent(e.motion.x*w/std::max(1,window_w),e.motion.y*h/std::max(1,window_h));
            else if (e.type==SDL_EVENT_MOUSE_WHEEL)
                io.AddMouseWheelEvent(e.wheel.x,e.wheel.y);
            else {
                int button=e.button.button==SDL_BUTTON_LEFT?0:e.button.button==SDL_BUTTON_RIGHT?1:e.button.button==SDL_BUTTON_MIDDLE?2:-1;
                if (button>=0) io.AddMouseButtonEvent(button,e.type==SDL_EVENT_MOUSE_BUTTON_DOWN);
            }
        }
        mouse_events.clear();
        auto now=SDL_GetPerformanceCounter();io.DeltaTime=previous_tick?float(double(now-previous_tick)/SDL_GetPerformanceFrequency()):1.f/60.f;previous_tick=now;
        ImGui_ImplOpenGL3_NewFrame();ImGui::NewFrame();recomp_runtime_ui_render_imgui(ui);ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    ImGui::SetCurrentContext(previous);
}
void detach(void*) {
    std::lock_guard<std::mutex> guard(mutex);
    if(ui) {recomp_runtime_ui_close(ui);recomp_runtime_ui_destroy(ui);ui=nullptr;}
    if(context) {auto* previous=ImGui::GetCurrentContext();ImGui::SetCurrentContext(context);if(gl_ready)ImGui_ImplOpenGL3_Shutdown();ImGui::DestroyContext(context);ImGui::SetCurrentContext(previous==context?nullptr:previous);}
    context=nullptr;gl_ready=false;host=nullptr;mods=nullptr;previous_tick=0;held=0;window=nullptr;mouse_events.clear();rows.clear();items.clear();
}
const PsxMenuProvider provider={PSX_MENU_ABI_VERSION,"R4 shared UI",nullptr,attach,event,is_open,set_open,draw,detach};
const int registered=psx_menu_set_provider(&provider);
}

#endif
