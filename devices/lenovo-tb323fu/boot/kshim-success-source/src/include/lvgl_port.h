#pragma once

#include <stddef.h>
#include <stdint.h>

#include <framebuffer.h>
#include <qcom_keys.h>
#include <touch.h>
#include <menu_locale.h>

/* 49 manifest payloads plus the advanced menu, the selectable language
 * entry, and two system actions when CONFIG_KSHIM_MENU_LANGUAGE_SWITCH is
 * enabled (total 53); the compatibility total is 51 when the switch is off. */
#if defined(CONFIG_KSHIM_MENU_LANGUAGE_SWITCH) && CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
#define KSHIM_MENU_MAX_ENTRIES 53U
#else
#define KSHIM_MENU_MAX_ENTRIES 51U
#endif
#define KSHIM_UI_ENTRY_DURATION_MS 360U
#define KSHIM_UI_FOCUS_DURATION_MS 260U
#define KSHIM_UI_PRESS_DURATION_MS 120U
#define KSHIM_UI_CONFIRM_DURATION_MS 180U

typedef struct kshim_lvgl kshim_lvgl_t;

typedef enum {
  KSHIM_MENU_NONE = 0,
  KSHIM_MENU_BOOT,
  KSHIM_MENU_RECOVERY,
  KSHIM_MENU_POWER_OFF,
} kshim_menu_action_t;

/* RAM outside the runtime slot for the inflated Noto Sans CJK font and the
 * background textures; call before kshim_lvgl_init. Without it (or if it is
 * too small) the menu uses a Latin-only font on a plain background. */
void kshim_lvgl_set_scratch(void *Base, size_t Size);

/* Non-zero when the menu draws with Noto Sans CJK. */
int kshim_lvgl_has_cjk_font(const kshim_lvgl_t *Context);

/* Real mapped key or valid touch press since the last successful set_entries. */
int kshim_lvgl_has_interacted(const kshim_lvgl_t *Context);

/* Initialize the LVGL menu and Qualcomm keypad. */
int kshim_lvgl_init(kshim_lvgl_t *Context, kshim_framebuffer_t *Framebuffer,
                    QcomKeys *Keys);

/* Advance LVGL's cooperative timer loop. elapsed_ms may be zero. */
void kshim_lvgl_frame(kshim_lvgl_t *Context, uint32_t ElapsedMs);

/* Return non-zero after kshim_lvgl_init has installed a display. */
int kshim_lvgl_ready(const kshim_lvgl_t *Context);

/* Consume one Power-confirmed menu choice; NONE means no new confirmation. */
kshim_menu_action_t kshim_lvgl_take_selection(kshim_lvgl_t *Context);
void kshim_lvgl_set_page_title(kshim_lvgl_t *Context, const char *Text);
void kshim_lvgl_set_status(kshim_lvgl_t *Context, const char *Text);
void kshim_lvgl_set_countdown(kshim_lvgl_t *Context, uint32_t timeout_ms);

/* Replace the compatibility menu with 1..KSHIM_MENU_MAX_ENTRIES payload/action
 * labels (53 with language switching, otherwise 51). */
int kshim_lvgl_set_entries(kshim_lvgl_t *Context,
                           const char *const *Names, size_t Count,
                           size_t Initial);
/* Optional explanations and confirmation labels follow the same order as
 * Names. Strings remain caller-owned until replacement or deinit. */
int kshim_lvgl_set_entries_described(kshim_lvgl_t *Context,
                                    const char *const *Names,
                                    const char *const *Descriptions,
                                    const char *const *ActionLabels,
                                    size_t Count, size_t Initial);

/* Assign the semantic role of every current entry for icon drawing. Count must
 * equal the live EntryCount. Passing NULL clears every role to
 * KSHIM_MENU_ROLE_UNKNOWN. No memory is retained; the list is redrawn. */
void kshim_lvgl_set_entry_roles(kshim_lvgl_t *Context,
                                const kshim_menu_role_t *Roles, size_t Count);

/* Consume one confirmed dynamic-menu index; -1 means no new selection. */
int kshim_lvgl_take_index(kshim_lvgl_t *Context);

/* Touch callback for kshim_touch_config_t.emit, in native framebuffer
 * coordinates. Call on the UI/boot CPU, outside rendering. The UI rotation
 * is inverted here; each event is synchronously delivered to LVGL. */
void kshim_lvgl_touch(void *Context, const kshim_touch_event_t *Event);

/* Release the single early-boot UI instance, including partially built menus. */
void kshim_lvgl_deinit(kshim_lvgl_t *Context);

/* Board hook called by the main loop. Return zero if the action was accepted.
 * The weak default reports unavailable until a board supplies a boot/power
 * handler; menu confirmation never pretends to perform a missing action. */
int kshim_menu_activate(kshim_menu_action_t Action);

struct kshim_lvgl {
  kshim_framebuffer_t *Framebuffer;
  /* UI geometry is independent of native scanout geometry and row stride. */
  uint32_t              LogicalWidth;
  uint32_t              LogicalHeight;
  kshim_fb_rotation_t   Rotation;
  QcomKeys            *Keys;
  void                *Display;
  void                *Input;
  void                *PointerInput;
  void                *Group;
  void                *List;
  void                *BootButton;
  void                *Background;
  const char           *Entries[KSHIM_MENU_MAX_ENTRIES];
  const char           *Descriptions[KSHIM_MENU_MAX_ENTRIES];
  const char           *ActionLabels[KSHIM_MENU_MAX_ENTRIES];
  kshim_menu_role_t     Roles[KSHIM_MENU_MAX_ENTRIES];
  size_t                EntryCount;
  size_t                InitialIndex;
  kshim_menu_action_t   PendingAction;
  int                   PendingIndex;
  uint32_t              PointerX;
  uint32_t              PointerY;
  uint32_t              ElapsedMs;
  uint16_t              TextureWidth;
  uint16_t              TextureHeight;
  uint8_t              Ready;
  uint8_t              UserInteracted;
  uint8_t              PointerPressed;
  uint8_t              DefaultEntries;
  uint8_t              ReducedQuality;
  uint8_t              CountdownEnabled;
  uint32_t             CountdownMs;
  uint32_t             CountdownShown;
};
