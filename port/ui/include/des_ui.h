/* The wrapper UI's interface to the rest of the port (port/ui, docs/32). */
#ifndef DES_UI_H
#define DES_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Compatibility entry for the title hooks. The current build uses no_ui.cpp:
 * no wrapper screens, hotkeys, settings.ini loader or renderer callbacks.
 * The old port/ui implementation is kept as reference but is not linked. */
void des_ui_early_init(void);

/* The UI's sounds, by id (ui::Sound in port/ui/src/sound.hpp, in this order). */
enum {
    DES_UI_SOUND_CLICK = 0,       /* a button pressed */
    DES_UI_SOUND_PLAY,            /* the start screen's Continue */
    DES_UI_SOUND_BINDING_CHANGED,
    DES_UI_SOUND_MENU_OPEN,       /* the menu bar opened */
    DES_UI_SOUND_MENU_CLOSE,
    DES_UI_SOUND_WINDOW_OPEN,     /* a window (settings, a question) opened */
    DES_UI_SOUND_WINDOW_CLOSE,
    DES_UI_SOUND_TAB_CHANGED,
    DES_UI_SOUND_ITEM_FOCUS,      /* the cursor moved */
    DES_UI_SOUND_ITEM_CHANGE,     /* a value changed */
    DES_UI_SOUND_ITEM_ENABLE,     /* a value turned on */
    DES_UI_SOUND_ITEM_DISABLE,    /* a value turned off */
    DES_UI_SOUND_WARNING          /* a warning question */
};

/* Plays a UI sound with the game's own menu sounds. Null (the default) is
 * silent. Called on the render thread, from the overlay's frame hook. */
extern void (*g_des_ui_sound)(int id);

#ifdef __cplusplus
}
#endif
#endif
