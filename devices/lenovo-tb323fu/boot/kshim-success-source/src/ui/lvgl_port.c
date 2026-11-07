#include <lvgl_port.h>
#include <menu_text.h>
#include <boot_metrics.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

#include <backdrop.h>
#include <lvgl.h>
#include <ui_font.h>

#include "menu_style.h"
#include "menu_style_render.h"
#include "menu_design.h"

#ifndef CONFIG_KSHIM_MENU_ENTRY1
#define CONFIG_KSHIM_MENU_ENTRY1 "Boot"
#endif
#ifndef CONFIG_KSHIM_MENU_ENTRY2
#define CONFIG_KSHIM_MENU_ENTRY2 "Recovery"
#endif
#ifndef CONFIG_KSHIM_LVGL_BUFFER_BYTES
#define CONFIG_KSHIM_LVGL_BUFFER_BYTES (16U * 1024U)
#endif
#ifndef CONFIG_KSHIM_BACKDROP_TEXTURE_PIXELS
#define CONFIG_KSHIM_BACKDROP_TEXTURE_PIXELS (256U * 256U)
#endif
#ifndef CONFIG_KSHIM_UI_ROTATION_CCW
#define CONFIG_KSHIM_UI_ROTATION_CCW 0
#endif
#ifndef CONFIG_KSHIM_MENU_ENTRY_DURATION_MS
#define CONFIG_KSHIM_MENU_ENTRY_DURATION_MS KSHIM_UI_ENTRY_DURATION_MS
#endif
#if CONFIG_KSHIM_UI_ROTATION_CCW != 0 && CONFIG_KSHIM_UI_ROTATION_CCW != 90 && \
    CONFIG_KSHIM_UI_ROTATION_CCW != 180 && CONFIG_KSHIM_UI_ROTATION_CCW != 270
#error "CONFIG_KSHIM_UI_ROTATION_CCW must be 0, 90, 180 or 270"
#endif

#define KSHIM_BACKDROP_REFRESH_MS 120U

static uint8_t mLvglRenderBuffers[2][CONFIG_KSHIM_LVGL_BUFFER_BYTES]
    __attribute__((aligned(8)));
/* The background textures and the inflated font live in scratch RAM
 * outside the runtime slot (kshim_lvgl_set_scratch). */
static void *mScratch;
static size_t mScratchSize;
static uint32_t *mBackdropTextures[2];
static const void *mFontData;
static lv_font_t *mFonts[4];
static const lv_font_t *mTitleFont;
static const lv_font_t *mStatusFont;
static const lv_font_t *mInfoFont;
static lv_image_dsc_t mBackdropImages[2];
static kshim_backdrop_layout_t mBackdropLayout;
static QcomKeys *mKeys;
static kshim_framebuffer_t *mFramebuffer;
static lv_indev_t *mKeyInput;
static lv_indev_t *mPointerInput;
static lv_display_t *mDisplay;
static lv_obj_t *mBackground;
static lv_obj_t *mPanel;
static lv_obj_t *mList;
static lv_obj_t *mBootButton;
static lv_obj_t *mStatus;
static lv_obj_t *mTitle;
static lv_obj_t *mDetail;
static lv_obj_t *mNotice;
static lv_obj_t *mHelp;
static lv_obj_t *mCredit;
static lv_obj_t *mLanguage;
static kshim_lvgl_t *mContext;
static uint32_t mKey;
static uint32_t mLastTextureUpdate;
static uint8_t mKeyReleasePending;
static uint8_t mActiveTexture;
static uint16_t mRowHeight;
static const lv_font_t *mRowFont;
static uint8_t mColumns;
static uint8_t mSplit;
static uint8_t mRibbon;
static uint16_t mTileHeight;
static uint16_t mTileWidth;

static uint32_t min_u32(uint32_t first, uint32_t second)
{
  return first < second ? first : second;
}

static uint32_t max_u32(uint32_t first, uint32_t second)
{
  return first > second ? first : second;
}

static uint32_t clamp_u32(uint32_t value, uint32_t minimum, uint32_t maximum)
{
  return min_u32(max_u32(value, minimum), maximum);
}

static lv_color_t KshimTextColor(void)
{
  return lv_color_hex(mMenuStyle.Text);
}

static lv_color_t KshimMutedColor(void)
{
  return lv_color_hex(mMenuStyle.Muted);
}

static lv_color_t KshimAccentColor(void)
{
  return lv_color_hex(mMenuStyle.Focus);
}

static void KshimLvglFlush(lv_display_t *Display, const lv_area_t *Area,
                           uint8_t *Pixels)
{
  int32_t Width;
  int32_t Height;
  uint32_t SourceStride;
  if (Display == NULL || Area == NULL || Pixels == NULL) {
    if (Display != NULL)
      lv_display_flush_ready(Display);
    return;
  }
  Width = Area->x2 - Area->x1 + 1;
  Height = Area->y2 - Area->y1 + 1;
  if (Width <= 0 || Height <= 0 || mDisplay == NULL || mFramebuffer == NULL || mContext == NULL) {
    lv_display_flush_ready(Display);
    return;
  }
  SourceStride = lv_draw_buf_width_to_stride((uint32_t)Width, LV_COLOR_FORMAT_ARGB8888);
  (void)kshim_fb_blit_argb8888_rotated(mFramebuffer, Area->x1, Area->y1,
      (uint32_t)Width, (uint32_t)Height, (const uint32_t *)(const void *)Pixels,
      SourceStride, mContext->Rotation);
  kshim_fb_sync();
  if (lv_display_flush_is_last(Display))
    kshim_boot_metrics_record_once(KSHIM_BOOT_STAGE_FIRST_FRAME);
  lv_display_flush_ready(Display);
}

static void KshimLvglReadKeys(lv_indev_t *Input, lv_indev_data_t *Data)
{
  QcomKeyEvent Event;
  int Status;
  (void)Input;
  if (Data == NULL)
    return;
  Data->state = LV_INDEV_STATE_RELEASED;
  Data->key = mKey;
  Data->continue_reading = false;
  if (mKeyReleasePending != 0U) {
    mKeyReleasePending = 0U;
    return;
  }
  if (mKeys == NULL)
    return;
  Status = QcomKeysPoll(mKeys, &Event);
  if (Status != QCOM_KEYS_EVENT || Event.Type != QCOM_KEY_EVENT_PRESS)
    return;
  switch (Event.Action) {
  case QCOM_KEY_ACTION_UP: mKey = LV_KEY_PREV; break;
  case QCOM_KEY_ACTION_DOWN: mKey = LV_KEY_NEXT; break;
  case QCOM_KEY_ACTION_SELECT: mKey = LV_KEY_ENTER; break;
  default: return;
  }
  if (mContext != NULL) mContext->UserInteracted = 1U;
  Data->key = mKey;
  Data->state = LV_INDEV_STATE_PRESSED;
  mKeyReleasePending = 1U;
}

static void KshimLvglReadPointer(lv_indev_t *Input, lv_indev_data_t *Data)
{
  (void)Input;
  if (Data == NULL || mContext == NULL)
    return;
  Data->point.x = (int32_t)mContext->PointerX;
  Data->point.y = (int32_t)mContext->PointerY;
  Data->state = mContext->PointerPressed != 0U ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  Data->continue_reading = false;
}

static void KshimSetTranslateX(void *Object, int32_t Value)
{
  lv_obj_set_style_translate_x(Object, Value, 0);
}

static void KshimSetTranslateY(void *Object, int32_t Value)
{
  lv_obj_set_style_translate_y(Object, Value, 0);
}

static void KshimSetOpacity(void *Object, int32_t Value)
{
  if (Value < LV_OPA_TRANSP) Value = LV_OPA_TRANSP;
  if (Value > LV_OPA_COVER) Value = LV_OPA_COVER;
  lv_obj_set_style_opa(Object, (lv_opa_t)Value, 0);
}

static void KshimSetScale(void *Object, int32_t Value)
{
  lv_obj_set_style_transform_scale(Object, Value, 0);
}

static void KshimSetConfirmPulse(void *Object, int32_t Value)
{
  int32_t Triangle = Value <= 512 ? Value : 1024 - Value;
  int32_t Amplitude = mMenuDesign.enabled ? 6 : 14;
  lv_obj_set_style_transform_scale(Object, 256 + Triangle * Amplitude / 512, 0);
}

/* 执行可替换动画；现代预设使用平滑过渡，旧预设保留原有时间常量。 */
static void KshimAnimate(
    lv_obj_t *Object,
    lv_anim_exec_xcb_t Callback,
    int32_t Start,
    int32_t End,
    uint32_t Duration,
    lv_anim_path_cb_t Path
)
{
  lv_anim_t Animation;
  if (Object == NULL || Callback == NULL)
    return;
  lv_anim_delete(Object, Callback);
  lv_anim_init(&Animation);
  lv_anim_set_var(&Animation, Object);
  lv_anim_set_exec_cb(&Animation, Callback);
  lv_anim_set_values(&Animation, Start, End);
  lv_anim_set_duration(&Animation, Duration);
  lv_anim_set_path_cb(&Animation, Path);
  lv_anim_start(&Animation);
}

/* Expanded details are selected by the focused row's role. */
static bool mInfoState;
static bool mBootSlotState;

#include "menu_design_port.h"

static int KshimButtonIndex(const lv_obj_t *Button)
{
  if (mContext == NULL || mContext->Group == NULL || Button == NULL)
    return -1;
  lv_group_t *Group = (lv_group_t *)mContext->Group;
  uint32_t Count = lv_group_get_obj_count(Group);
  for (uint32_t Index = 0; Index < Count; Index++) {
    if (lv_group_get_obj_by_index(Group, Index) == Button)
      return (int)Index;
  }
  return -1;
}

static void KshimShowEntry(size_t Index)
{
  if (mContext == NULL || Index >= mContext->EntryCount) return;
  /* An ordinary row restores the baseline STAR chart layout. */
  const char *EntryAction = mContext->ActionLabels[Index];
  bool Info = mMenuDesign.kind == KSHIM_DESIGN_STAR_CHART &&
      mContext->Roles[Index] == KSHIM_MENU_ROLE_DEVICE_INFO &&
      EntryAction != NULL && strcmp(EntryAction, KSHIM_MENU_ACTION_BACK) == 0;
  bool BootSlot = mMenuDesign.kind == KSHIM_DESIGN_STAR_CHART &&
      mContext->Roles[Index] == KSHIM_MENU_ROLE_BOOT_SLOT;
  if (Info != mInfoState || BootSlot != mBootSlotState) {
    mInfoState = Info;
    mBootSlotState = BootSlot;
    (void)KshimDesignRelayout();
  }
  if (mStatus != NULL) lv_label_set_text(mStatus, mContext->Entries[Index]);
  if (mDetail != NULL) {
    const char *Description = mContext->Descriptions[Index];
    if (Description == NULL || Description[0] == '\0')
      Description = mHelp != NULL ? KSHIM_MENU_DESC_GENERIC : KSHIM_MENU_TEXT_DETAIL;
    lv_label_set_text(mDetail, Description);
  }
  if (mHelp != NULL && mBootButton != NULL) {
    const char *Action = mContext->ActionLabels[Index];
    if (Action == NULL || Action[0] == '\0') Action = KSHIM_MENU_ACTION_BOOT;
    lv_obj_t *Label = lv_obj_get_child(mBootButton, 0);
    if (Label != NULL) {
      lv_label_set_text(Label, Action);
      lv_obj_center(Label);
    }
  }
}

static void KshimConfirmIndex(size_t Index)
{
  if (mContext == NULL || Index >= mContext->EntryCount)
    return;
  mContext->PendingIndex = (int)Index;
  if (mContext->DefaultEntries != 0U && Index < KSHIM_MENU_POWER_OFF)
    mContext->PendingAction = (kshim_menu_action_t)(Index + 1U);
  KshimShowEntry(Index);
  /* STAR avoids transformed intermediate layer allocation in the 512 KiB heap. */
  if (mBootButton != NULL && !KSHIM_MENU_IS_NATIVE &&
      mMenuDesign.kind != KSHIM_DESIGN_STAR_CHART)
    KshimAnimate(mBootButton, KshimSetConfirmPulse, 0, 1024,
                 KSHIM_UI_CONFIRM_DURATION_MS, lv_anim_path_ease_in_out);
}

static void KshimMenuEvent(lv_event_t *Event)
{
  if (Event == NULL || mContext == NULL)
    return;
  lv_obj_t *Button = lv_event_get_target_obj(Event);
  lv_obj_t *Label = lv_obj_get_child(Button, 0);
  lv_event_code_t Code = lv_event_get_code(Event);
  int Index = (int)(uintptr_t)lv_event_get_user_data(Event) - 1;
  if (Code == LV_EVENT_FOCUSED || Code == LV_EVENT_DEFOCUSED) {
    if (Label != NULL && !KSHIM_MENU_IS_NATIVE) {
      int32_t Offset = lv_obj_get_style_translate_x(Label, LV_PART_MAIN);
      int32_t Slide = mMenuDesign.enabled ? 0 : 12;
      KshimAnimate(Label, KshimSetTranslateX, Offset,
          Code == LV_EVENT_FOCUSED ? Slide : 0, KSHIM_UI_FOCUS_DURATION_MS,
          mMenuDesign.enabled ? lv_anim_path_ease_out : lv_anim_path_overshoot);
    }
    if (KSHIM_MENU_IS_NATIVE) {
      if (Code == LV_EVENT_FOCUSED) lv_obj_add_state(Button, LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
      else lv_obj_remove_state(Button, LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
    }
    if (Code == LV_EVENT_FOCUSED) {
      lv_obj_scroll_to_view(Button, LV_ANIM_ON);
      if (Index >= 0) KshimShowEntry((size_t)Index);
    }
    return;
  }
  if (Code == LV_EVENT_PRESSED) {
    lv_indev_t *Input = lv_event_get_indev(Event);
    if (Input != NULL && lv_indev_get_type(Input) == LV_INDEV_TYPE_POINTER)
      lv_group_focus_obj(Button);
    if (KSHIM_MENU_IS_NATIVE) return;
    KshimAnimate(Button, KshimSetScale,
        lv_obj_get_style_transform_scale_x(Button, LV_PART_MAIN),
        mMenuDesign.enabled ? mMenuDesign.press_scale : 248,
        KSHIM_UI_PRESS_DURATION_MS, lv_anim_path_ease_out);
    return;
  }
  if (Code == LV_EVENT_RELEASED || Code == LV_EVENT_PRESS_LOST) {
    if (KSHIM_MENU_IS_NATIVE) return;
    KshimAnimate(Button, KshimSetScale,
        lv_obj_get_style_transform_scale_x(Button, LV_PART_MAIN),
        256, KSHIM_UI_PRESS_DURATION_MS, lv_anim_path_ease_out);
    return;
  }
  if (Code != LV_EVENT_CLICKED || Index < 0 || (size_t)Index >= mContext->EntryCount)
    return;
  lv_indev_t *Input = lv_event_get_indev(Event);
  /* A row tap changes focus. Touch confirmation remains on the fixed Boot
   * button, so an imprecise scroll cannot launch an image. */
  if (Input != NULL && lv_indev_get_type(Input) == LV_INDEV_TYPE_POINTER)
    return;
  KshimConfirmIndex((size_t)Index);
}

static void KshimBootEvent(lv_event_t *Event)
{
  if (Event == NULL || lv_event_get_code(Event) != LV_EVENT_CLICKED ||
      mContext == NULL || mContext->Group == NULL)
    return;
  lv_obj_t *Focused = lv_group_get_focused((lv_group_t *)mContext->Group);
  int Index = KshimButtonIndex(Focused);
  if (Index >= 0)
    KshimConfirmIndex((size_t)Index);
}

#include "menu_native.h"
#include "menu_layout_dispatch.h"

static int KshimRebuildEntries(size_t Initial)
{
  if (mContext == NULL || mList == NULL || mContext->Group == NULL ||
      mContext->EntryCount == 0U || Initial >= mContext->EntryCount)
    return -1;
  if (KshimRelayoutChrome() != 0)
    return -2;
  lv_group_t *Group = (lv_group_t *)mContext->Group;
  lv_group_remove_all_objs(Group);
  lv_obj_clean(mList);
  for (size_t Index = 0; Index < mContext->EntryCount; Index++) {
    lv_obj_t *Button = lv_list_add_button(mList, NULL, mContext->Entries[Index]);
    if (Button == NULL || KshimStyleEntry(Button, Index) != 0)
      return -3;
    lv_obj_add_event_cb(Button, KshimMenuEvent, LV_EVENT_ALL, (void *)(uintptr_t)(Index + 1U));
    lv_group_add_obj(Group, Button);
  }
  lv_obj_t *InitialButton = lv_group_get_obj_by_index(Group, (uint32_t)Initial);
  if (InitialButton == NULL) return -4;
  lv_group_focus_obj(InitialButton);
  if (KSHIM_MENU_IS_NATIVE) lv_obj_add_state(InitialButton, LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
  lv_obj_scroll_to_view(InitialButton, LV_ANIM_OFF);
  KshimShowEntry(Initial);
  if (mNotice != NULL) lv_label_set_text(mNotice, "");
  return 0;
}

static void KshimPrepareImage(lv_image_dsc_t *Image, uint32_t *Pixels)
{
  *Image = (lv_image_dsc_t){
      .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888,
          .flags = 0, .w = mBackdropLayout.texture_width, .h = mBackdropLayout.texture_height,
          .stride = (uint16_t)(mBackdropLayout.texture_width * sizeof(uint32_t))},
      .data_size = (uint32_t)((size_t)mBackdropLayout.texture_width * mBackdropLayout.texture_height * sizeof(uint32_t)),
      .data = (const uint8_t *)(const void *)Pixels,
  };
}

/* 旧 FLAT 使用两张动态纹理；其他风格将 scratch 留给字体。 */
static void KshimPlaceScratch(void)
{
  size_t Texture = (size_t)CONFIG_KSHIM_BACKDROP_TEXTURE_PIXELS * sizeof(uint32_t);
  uint8_t *Cursor = mScratch;
  size_t Left = mScratchSize;
  mBackdropTextures[0] = mBackdropTextures[1] = NULL;
  mFontData = NULL;
  if (Cursor == NULL || ((uintptr_t)Cursor & 63U) != 0U) return;
  if (mMenuStyle.AnimatedBackdrop != 0U && Left >= 2U * Texture) {
    mBackdropTextures[0] = (uint32_t *)(void *)Cursor;
    mBackdropTextures[1] = (uint32_t *)(void *)(Cursor + Texture);
    Cursor += 2U * Texture;
    Left -= 2U * Texture;
  }
  (void)kshim_ui_font_inflate(Cursor, Left, &mFontData);
}

static int KshimRenderInitialBackground(void)
{
  mActiveTexture = 0U;
  mLastTextureUpdate = 0U;
  if (mBackdropTextures[0] == NULL) return 0;
  if (kshim_backdrop_render(mBackdropTextures[0], CONFIG_KSHIM_BACKDROP_TEXTURE_PIXELS,
      &mBackdropLayout, 0U, true) != 0) return -1;
  KshimPrepareImage(&mBackdropImages[0], mBackdropTextures[0]);
  KshimPrepareImage(&mBackdropImages[1], mBackdropTextures[1]);
  return 0;
}

static void KshimCreateFonts(uint32_t ShortEdge)
{
  if (KSHIM_MENU_IS_NATIVE) { KshimNativeCreateFonts(); return; }
  static const int32_t Sizes[3][3] = {{48, 28, 22}, {28, 20, 16}, {20, 14, 12}};
  const int32_t *Size = Sizes[ShortEdge >= 720U ? 0 : ShortEdge >= 320U ? 1 : 2];
  int32_t DesignSizes[3];
  uint32_t Scale = 0U;
  if (mMenuDesign.enabled) {
    Scale = kshim_design_scale(mContext->LogicalWidth, mContext->LogicalHeight,
                                      KshimDesignScaleOverride());
    DesignSizes[0] = kshim_design_max(12, kshim_design_px(mMenuDesign.title_size, Scale));
    DesignSizes[1] = kshim_design_max(12, kshim_design_px(mMenuDesign.row_size, Scale));
    DesignSizes[2] = kshim_design_max(10, kshim_design_px(mMenuDesign.status_size, Scale));
    Size = DesignSizes;
  }
  for (unsigned Index = 0; Index < 3U; Index++) {
    mFonts[Index] = mFontData == NULL ? NULL :
        lv_tiny_ttf_create_data_ex(mFontData, kshim_ui_font_size(), Size[Index],
            LV_FONT_KERNING_NONE, Index == 0U ? 32U : LV_TINY_TTF_CACHE_GLYPH_CNT);
  }
  /* The STAR summary body on a short-edge <=1080 panel needs a smaller face;
   * slot three is created only there, sharing the body glyph cache, and stays
   * NULL everywhere else so every other layout keeps the shared status font. */
  mFonts[3] = NULL;
  if (mMenuDesign.enabled && mMenuDesign.kind == KSHIM_DESIGN_STAR_CHART &&
      ShortEdge <= 1080U && mFontData != NULL) {
    mFonts[3] = lv_tiny_ttf_create_data_ex(mFontData, kshim_ui_font_size(),
        kshim_design_max(10, kshim_design_px(14U, Scale)),
        LV_FONT_KERNING_NONE, LV_TINY_TTF_CACHE_GLYPH_CNT);
  }
  mTitleFont = mFonts[0] != NULL ? mFonts[0] : &lv_font_montserrat_20;
  mRowFont = mFonts[1] != NULL ? mFonts[1] : &lv_font_montserrat_20;
  mStatusFont = mFonts[2] != NULL ? mFonts[2] : &lv_font_montserrat_20;
  mInfoFont = mFonts[3] != NULL ? mFonts[3] : mStatusFont;
  KshimDesignFonts();
}

/* Fonts are released with LVGL's heap after the labels stop using them. */
static void KshimForgetFonts(void)
{
  KshimDesignForgetFonts();
  for (unsigned Index = 0; Index < 4U; Index++) mFonts[Index] = NULL;
  mTitleFont = mRowFont = mStatusFont = mInfoFont = NULL;
}

/* 构建共享控件树；不同风格不复制输入或启动状态机。 */
static int KshimBuildChrome(void)
{
  lv_obj_t *Screen = lv_screen_active();
  if (Screen == NULL || mContext == NULL) return -1;
  uint32_t ShortEdge = min_u32(mContext->LogicalWidth, mContext->LogicalHeight);
  KshimCreateFonts(ShortEdge);
  if (KSHIM_MENU_IS_NATIVE) return KshimNativeBuildChrome();
  mRowHeight = ShortEdge >= 720U ? 72U : ShortEdge >= 320U ? 52U : 24U;
  if (mMenuStyle.Layout == 5U && ShortEdge >= 160U && mRowHeight < 44U)
    mRowHeight = 44U;
  lv_obj_set_style_bg_color(Screen, lv_color_hex(mMenuStyle.Screen), 0);
  lv_obj_set_style_bg_opa(Screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(Screen, KshimTextColor(), 0);
  lv_obj_set_scrollbar_mode(Screen, LV_SCROLLBAR_MODE_OFF);

  mBackground = lv_image_create(Screen);
  if (mBackground == NULL) return -1;
  lv_obj_set_size(mBackground, LV_PCT(100), LV_PCT(100));
  lv_obj_align(mBackground, LV_ALIGN_CENTER, 0, 0);
  if (mBackdropTextures[0] != NULL) lv_image_set_src(mBackground, &mBackdropImages[0]);
  lv_image_set_inner_align(mBackground, LV_IMAGE_ALIGN_STRETCH);
  lv_image_set_antialias(mBackground, true);
  lv_obj_remove_flag(mBackground, LV_OBJ_FLAG_CLICKABLE);

  mPanel = lv_obj_create(Screen);
  if (mPanel == NULL) return -1;
  lv_obj_set_style_bg_color(mPanel, lv_color_hex(mMenuStyle.Panel), 0);
  lv_obj_set_style_bg_opa(mPanel, mMenuStyle.PanelOpacity, 0);
  lv_obj_set_style_border_width(mPanel, 0, 0);
  lv_obj_set_style_outline_width(mPanel, mMenuStyle.PanelOutlineWidth, 0);
  lv_obj_set_style_outline_pad(mPanel, 0, 0);
  lv_obj_set_style_outline_color(mPanel, lv_color_hex(mMenuStyle.Border), 0);
  lv_obj_set_style_outline_opa(mPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(mPanel, 0, 0);
  lv_obj_set_style_pad_all(mPanel, 0, 0);
  lv_obj_set_scrollbar_mode(mPanel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_remove_flag(mPanel, LV_OBJ_FLAG_SCROLLABLE);
  KshimModernPanel(Screen, mPanel);

  mTitle = lv_label_create(mPanel);
  if (mTitle == NULL) return -1;
  lv_label_set_text(mTitle, KSHIM_MENU_TEXT_TITLE(
      mMenuStyle.Layout == 1U ? "kShimC Boot Manager" :
      mMenuStyle.Layout == 3U ? "Boot library" :
      mMenuStyle.Layout == 4U ? "kShimC Boot Picker" :
      mMenuStyle.Layout == 5U ? "Start up" : "Select OS to Boot"));
  lv_obj_set_style_text_font(mTitle, mTitleFont, 0);
  lv_obj_set_style_text_color(mTitle, KshimTextColor(), 0);
  if (mMenuStyle.Modern != 0U) lv_label_set_long_mode(mTitle, LV_LABEL_LONG_DOT);

  mStatus = lv_label_create(mPanel);
  if (mStatus == NULL) return -1;
  lv_label_set_text(mStatus, KSHIM_MENU_TEXT_STATUS);
  lv_obj_set_style_text_font(mStatus, mStatusFont, 0);
  lv_obj_set_style_text_color(mStatus, KshimMutedColor(), 0);
  lv_label_set_long_mode(mStatus, LV_LABEL_LONG_DOT);
  if (ShortEdge < 160U) lv_obj_add_flag(mStatus, LV_OBJ_FLAG_HIDDEN);

  mList = lv_list_create(mPanel);
  if (mList == NULL) return -1;
  lv_obj_set_style_bg_opa(mList, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(mList, 0, 0);
  lv_obj_set_style_radius(mList, 0, 0);
  lv_obj_set_style_pad_all(mList, 0, 0);
  lv_obj_set_style_pad_row(mList, max_u32(2U, KshimPadding() / mMenuStyle.RowGapDivisor), 0);
  lv_obj_set_style_bg_color(mList, lv_color_hex(mMenuStyle.Scrollbar), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(mList, LV_OPA_50, LV_PART_SCROLLBAR);
  lv_obj_set_style_width(mList, 3, LV_PART_SCROLLBAR);
  lv_obj_set_scrollbar_mode(mList, LV_SCROLLBAR_MODE_AUTO);

  mBootButton = lv_button_create(mPanel);
  if (mBootButton == NULL) return -1;
  lv_obj_set_style_bg_color(mBootButton, lv_color_hex(mMenuStyle.Boot), 0);
  lv_obj_set_style_bg_opa(mBootButton, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(mBootButton, lv_color_hex(mMenuStyle.BootPressed), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(mBootButton, 0, 0);
  lv_obj_set_style_shadow_width(mBootButton, 0, 0);
  lv_obj_set_style_outline_width(mBootButton, 0, LV_STATE_FOCUS_KEY);
  KshimModernBoot(mBootButton);
  lv_obj_add_event_cb(mBootButton, KshimBootEvent, LV_EVENT_CLICKED, NULL);
  lv_obj_t *BootLabel = lv_label_create(mBootButton);
  if (BootLabel == NULL) return -1;
  lv_label_set_text_static(BootLabel, KSHIM_MENU_TEXT_ACTION("Boot"));
  lv_obj_set_style_text_font(BootLabel, mRowFont, 0);
  lv_obj_set_style_text_color(BootLabel, lv_color_hex(mMenuStyle.BootText), 0);
  lv_obj_center(BootLabel);

  if (mMenuStyle.Layout == 1U || mMenuDesign.kind == KSHIM_DESIGN_ONE_UI) {
    mDetail = lv_label_create(mPanel);
    if (mDetail == NULL) return -1;
    lv_label_set_text(mDetail, KSHIM_MENU_TEXT_DETAIL);
    lv_obj_set_style_text_font(mDetail, mStatusFont, 0);
    lv_obj_set_style_text_color(mDetail, KshimMutedColor(), 0);
    lv_label_set_long_mode(mDetail, LV_LABEL_LONG_DOT);
    lv_obj_remove_flag(mDetail, LV_OBJ_FLAG_CLICKABLE);
  }
#if defined(CONFIG_KSHIM_MENU_ENTRY_DETAILS) && CONFIG_KSHIM_MENU_ENTRY_DETAILS
  if (mMenuDesign.kind == KSHIM_DESIGN_SURFACE || mMenuDesign.kind == KSHIM_DESIGN_STAR_CHART) {
    mNotice = lv_label_create(mPanel);
    mHelp = lv_label_create(mPanel);
    if (mNotice == NULL || mHelp == NULL) return -1;
    lv_label_set_text(mNotice, "");
    lv_label_set_text(mHelp, KSHIM_MENU_TEXT_HELP);
    lv_obj_set_style_text_font(mNotice, mStatusFont, 0);
    lv_obj_set_style_text_font(mHelp, mStatusFont, 0);
    lv_obj_set_style_text_color(mNotice, KshimMutedColor(), 0);
    lv_obj_set_style_text_color(mHelp, KshimMutedColor(), 0);
    lv_label_set_long_mode(mNotice, LV_LABEL_LONG_WRAP);
    lv_label_set_long_mode(mHelp, LV_LABEL_LONG_WRAP);
    lv_obj_remove_flag(mNotice, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(mHelp, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(mDetail, LV_LABEL_LONG_WRAP);
  }
#endif
  if (mMenuDesign.kind == KSHIM_DESIGN_STAR_CHART) {
    mCredit = lv_label_create(mPanel);
    mLanguage = lv_label_create(mPanel);
    if (mCredit == NULL || mLanguage == NULL) return -1;
    lv_label_set_text(mCredit, KSHIM_MENU_TEXT_CREDIT);
    lv_label_set_text(mLanguage, KSHIM_MENU_TEXT_LANGUAGE_STATE);
    lv_obj_set_style_text_font(mCredit, mStatusFont, 0);
    lv_obj_set_style_text_font(mLanguage, mStatusFont, 0);
    lv_obj_set_style_text_color(mCredit, KshimMutedColor(), 0);
    lv_obj_set_style_text_color(mLanguage, KshimTextColor(), 0);
    lv_label_set_long_mode(mCredit, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(mLanguage, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_remove_flag(mCredit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(mLanguage, LV_OBJ_FLAG_CLICKABLE);
  }
  if (mMenuDesign.enabled) KshimDesignDecorate(Screen);
  lv_obj_update_layout(Screen);
  if (KshimRelayoutChrome() != 0) return -1;
  if (CONFIG_KSHIM_MENU_ENTRY_DURATION_MS != 0U) {
    lv_obj_set_style_translate_y(mPanel, 32, 0);
    lv_obj_set_style_opa(mPanel, LV_OPA_TRANSP, 0);
    KshimAnimate(mPanel, KshimSetTranslateY, 32, 0, CONFIG_KSHIM_MENU_ENTRY_DURATION_MS, lv_anim_path_ease_out);
    KshimAnimate(mPanel, KshimSetOpacity, LV_OPA_TRANSP, LV_OPA_COVER,
        CONFIG_KSHIM_MENU_ENTRY_DURATION_MS, lv_anim_path_ease_out);
  }
  return 0;
}

int kshim_lvgl_init(kshim_lvgl_t *Context, kshim_framebuffer_t *Framebuffer, QcomKeys *Keys)
{
  const char *const DefaultEntries[] = {
      CONFIG_KSHIM_MENU_ENTRY1, CONFIG_KSHIM_MENU_ENTRY2,
#if !defined(CONFIG_KSHIM_UEFI_TEST_MENU) || !CONFIG_KSHIM_UEFI_TEST_MENU
      KSHIM_MENU_TEXT_POWEROFF,
#endif
  };
  if (mContext != NULL) return -9;
  if (Context != NULL) *Context = (kshim_lvgl_t){0};
  if (Context == NULL || Framebuffer == NULL || Framebuffer->width == 0U ||
      Framebuffer->height == 0U || Framebuffer->render_address == 0U ||
      Framebuffer->width > INT32_MAX || Framebuffer->height > INT32_MAX) return -1;
  const bool SwapAxes = CONFIG_KSHIM_UI_ROTATION_CCW == 90 || CONFIG_KSHIM_UI_ROTATION_CCW == 270;
  const uint32_t LogicalWidth = SwapAxes ? Framebuffer->height : Framebuffer->width;
  const uint32_t LogicalHeight = SwapAxes ? Framebuffer->width : Framebuffer->height;
  if (LogicalWidth < 64U || LogicalHeight < 48U) return -1;
  if (kshim_fb_format_bpp(Framebuffer->format) == 0U) return -2;
  if (LogicalWidth > UINT32_MAX / sizeof(uint32_t)) return -3;
  uint32_t RenderStride = LogicalWidth * sizeof(uint32_t);
  if (sizeof(mLvglRenderBuffers[0]) < RenderStride) return -3;
  if (kshim_backdrop_layout(LogicalWidth, LogicalHeight,
      CONFIG_KSHIM_BACKDROP_TEXTURE_PIXELS, sizeof(DefaultEntries) / sizeof(DefaultEntries[0]), &mBackdropLayout) != 0) return -4;
  *Context = (kshim_lvgl_t){.Framebuffer = Framebuffer, .Keys = Keys,
      .LogicalWidth = LogicalWidth, .LogicalHeight = LogicalHeight,
      .Rotation = (kshim_fb_rotation_t)CONFIG_KSHIM_UI_ROTATION_CCW,
      .PendingIndex = -1, .PointerX = LogicalWidth / 2U,
      .PointerY = LogicalHeight / 2U, .DefaultEntries = 1U};
  Context->EntryCount = sizeof(DefaultEntries) / sizeof(DefaultEntries[0]);
  for (size_t Index = 0; Index < Context->EntryCount; Index++) Context->Entries[Index] = DefaultEntries[Index];
  mContext = Context;
  mKeys = Keys;
  mFramebuffer = Framebuffer;
  mDisplay = NULL;
  mKeyInput = NULL;
  mPointerInput = NULL;
  mBackground = NULL;
  mPanel = NULL;
  mList = NULL;
  mBootButton = NULL;
  mStatus = NULL;
  mTitle = NULL;
  mDetail = NULL;
  mNotice = NULL;
  mHelp = NULL;
  mCredit = NULL;
  mLanguage = NULL;
  mColumns = 1U;
  mSplit = 0U;
  mRibbon = 0U;
  mTileHeight = 0U;
  mTileWidth = 0U;
  mKey = LV_KEY_ENTER;
  mKeyReleasePending = 0U;
  mInfoState = false;
  mBootSlotState = false;
  KshimPlaceScratch();
  kshim_boot_metrics_record_once(KSHIM_BOOT_STAGE_FONT);
  if (KshimRenderInitialBackground() != 0) goto Failed;
  lv_init();
  mDisplay = lv_display_create((int32_t)LogicalWidth, (int32_t)LogicalHeight);
  if (mDisplay == NULL) goto Failed;
  lv_display_set_color_format(mDisplay, LV_COLOR_FORMAT_ARGB8888);
  lv_display_set_antialiasing(mDisplay, true);
  lv_display_set_flush_cb(mDisplay, KshimLvglFlush);
  lv_display_set_buffers(mDisplay, mLvglRenderBuffers[0], mLvglRenderBuffers[1],
      sizeof(mLvglRenderBuffers[0]), LV_DISPLAY_RENDER_MODE_PARTIAL);
  mKeyInput = lv_indev_create();
  if (mKeyInput == NULL) goto Failed;
  lv_indev_set_type(mKeyInput, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(mKeyInput, KshimLvglReadKeys);
  lv_indev_set_display(mKeyInput, mDisplay);
  mPointerInput = lv_indev_create();
  if (mPointerInput == NULL) goto Failed;
  lv_indev_set_type(mPointerInput, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(mPointerInput, KshimLvglReadPointer);
  lv_indev_set_display(mPointerInput, mDisplay);
  lv_indev_set_mode(mPointerInput, LV_INDEV_MODE_EVENT);
  lv_group_t *Group = lv_group_create();
  if (Group == NULL) goto Failed;
  lv_group_set_default(Group);
  lv_indev_set_group(mKeyInput, Group);
  Context->Group = Group;
  if (KshimBuildChrome() != 0 || KshimRebuildEntries(0U) != 0) goto Failed;
  Context->Display = mDisplay;
  Context->Input = mKeyInput;
  Context->PointerInput = mPointerInput;
  Context->List = mList;
  Context->BootButton = mBootButton;
  Context->Background = mBackground;
  Context->TextureWidth = mBackdropLayout.texture_width;
  Context->TextureHeight = mBackdropLayout.texture_height;
  Context->Ready = 1U;
  kshim_boot_metrics_record_once(KSHIM_BOOT_STAGE_UI);
  return 0;
Failed:
  kshim_lvgl_deinit(Context);
  return -5;
}

void kshim_lvgl_frame(kshim_lvgl_t *Context, uint32_t ElapsedMs)
{
  if (Context == NULL || Context != mContext || Context->Ready == 0U) return;
  if (ElapsedMs != 0U) {
    lv_tick_inc(ElapsedMs);
    Context->ElapsedMs += ElapsedMs;
    if (ElapsedMs > 40U) Context->ReducedQuality = 1U;
  }
  lv_timer_handler();
  if (Context->CountdownEnabled != 0U && mStatus != NULL) {
    uint32_t remaining = Context->ElapsedMs < Context->CountdownMs ? Context->CountdownMs - Context->ElapsedMs : 0U;
    uint32_t seconds = (remaining + 999U) / 1000U;
    if (seconds != Context->CountdownShown) {
      char countdown[192];
#if defined(CONFIG_KSHIM_MENU_LANGUAGE_SWITCH) && CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
      const char *name = (mHelp != NULL && Context->InitialIndex < Context->EntryCount)
          ? Context->Entries[Context->InitialIndex] : NULL;
      (void)kshim_menu_format_countdown(countdown, sizeof(countdown), seconds, name);
#else
#if defined(CONFIG_KSHIM_MENU_ENTRY_DETAILS) && CONFIG_KSHIM_MENU_ENTRY_DETAILS
      if (mHelp != NULL && Context->InitialIndex < Context->EntryCount) {
        const char *name = Context->Entries[Context->InitialIndex];
#if defined(CONFIG_KSHIM_MENU_CHINESE) && CONFIG_KSHIM_MENU_CHINESE
        (void)snprintf(countdown, sizeof(countdown), "%u 秒后启动%s", seconds, name);
#else
        (void)snprintf(countdown, sizeof(countdown), "Boot %s in %u s", name, seconds);
#endif
      } else
#endif
      {
#if defined(CONFIG_KSHIM_MENU_CHINESE) && CONFIG_KSHIM_MENU_CHINESE
        (void)snprintf(countdown, sizeof(countdown), "自动启动倒计时 %u 秒", seconds);
#else
        (void)snprintf(countdown, sizeof(countdown), "Auto boot in %u s / 自动启动倒计时 %u 秒", seconds, seconds);
#endif
      }
#endif
      kshim_lvgl_set_status(Context, countdown);
      Context->CountdownShown = seconds;
    }
  }
  if (Context->ElapsedMs - mLastTextureUpdate >= KSHIM_BACKDROP_REFRESH_MS &&
      mBackground != NULL && mBackdropTextures[0] != NULL) {
    uint8_t Next = (uint8_t)(mActiveTexture ^ 1U);
    lv_image_cache_drop(&mBackdropImages[Next]);
    if (kshim_backdrop_render(mBackdropTextures[Next], CONFIG_KSHIM_BACKDROP_TEXTURE_PIXELS,
        &mBackdropLayout, Context->ElapsedMs, Context->ReducedQuality != 0U) == 0) {
      lv_image_set_src(mBackground, &mBackdropImages[Next]);
      mActiveTexture = Next;
      mLastTextureUpdate = Context->ElapsedMs;
    }
  }
}

int kshim_lvgl_ready(const kshim_lvgl_t *Context)
{
  return Context != NULL && Context == mContext && Context->Ready != 0U;
}

kshim_menu_action_t kshim_lvgl_take_selection(kshim_lvgl_t *Context)
{
  if (!kshim_lvgl_ready(Context)) return KSHIM_MENU_NONE;
  kshim_menu_action_t Action = Context->PendingAction;
  Context->PendingAction = KSHIM_MENU_NONE;
  return Action;
}

int kshim_lvgl_take_index(kshim_lvgl_t *Context)
{
  if (!kshim_lvgl_ready(Context)) return -1;
  int Index = Context->PendingIndex;
  Context->PendingIndex = -1;
  return Index;
}

int kshim_lvgl_set_entries(kshim_lvgl_t *Context, const char *const *Names, size_t Count, size_t Initial)
{
  return kshim_lvgl_set_entries_described(Context, Names, NULL, NULL, Count, Initial);
}

int kshim_lvgl_set_entries_described(kshim_lvgl_t *Context, const char *const *Names,
                                    const char *const *Descriptions,
                                    const char *const *ActionLabels, size_t Count, size_t Initial)
{
  if (!kshim_lvgl_ready(Context) || Names == NULL || Count == 0U || Count > KSHIM_MENU_MAX_ENTRIES || Initial >= Count) return -1;
  for (size_t Index = 0; Index < Count; Index++) {
    if (Names[Index] == NULL || Names[Index][0] == '\0') return -2;
  }
  Context->EntryCount = Count;
  Context->InitialIndex = Initial;
  Context->DefaultEntries = 0U;
  Context->PendingAction = KSHIM_MENU_NONE;
  Context->PendingIndex = -1;
  for (size_t Index = 0; Index < Count; Index++) {
    Context->Entries[Index] = Names[Index];
    Context->Descriptions[Index] = Descriptions != NULL ? Descriptions[Index] : NULL;
    Context->ActionLabels[Index] = ActionLabels != NULL ? ActionLabels[Index] : NULL;
    Context->Roles[Index] = KSHIM_MENU_ROLE_UNKNOWN;
  }
  for (size_t Index = Count; Index < KSHIM_MENU_MAX_ENTRIES; Index++) {
    Context->Entries[Index] = NULL;
    Context->Descriptions[Index] = NULL;
    Context->ActionLabels[Index] = NULL;
    Context->Roles[Index] = KSHIM_MENU_ROLE_UNKNOWN;
  }
  int Result = KshimRebuildEntries(Initial);
  if (Result == 0) {
    Context->UserInteracted = 0U;
#if defined(CONFIG_KSHIM_MENU_LANGUAGE_SWITCH) && CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
    if (mTitle != NULL)
      lv_label_set_text(mTitle, KSHIM_MENU_TEXT_TITLE(
          mMenuDesign.enabled ? mMenuDesign.title : "Select OS to Boot"));
    if (mHelp != NULL) lv_label_set_text(mHelp, KSHIM_MENU_TEXT_HELP);
    if (mCredit != NULL) lv_label_set_text(mCredit, KSHIM_MENU_TEXT_CREDIT);
    if (mLanguage != NULL) lv_label_set_text(mLanguage, KSHIM_MENU_TEXT_LANGUAGE_STATE);
#endif
  }
  return Result;
}

void kshim_lvgl_set_entry_roles(kshim_lvgl_t *Context, const kshim_menu_role_t *Roles, size_t Count)
{
  if (!kshim_lvgl_ready(Context) || Count != Context->EntryCount ||
      Count > KSHIM_MENU_MAX_ENTRIES) return;
  for (size_t Index = 0; Index < KSHIM_MENU_MAX_ENTRIES; Index++)
    Context->Roles[Index] = (Roles != NULL && Index < Count) ? Roles[Index] : KSHIM_MENU_ROLE_UNKNOWN;
  /* STAR_CHART re-evaluates the info geometry from the actually focused row:
   * roles may change under a stable focus, so refresh from the group focus and
   * fall back to the initial index. */
  if (mMenuDesign.kind == KSHIM_DESIGN_STAR_CHART) {
    int FocusedIndex = KshimButtonIndex(lv_group_get_focused((lv_group_t *)Context->Group));
    if (FocusedIndex < 0) FocusedIndex = (int)Context->InitialIndex;
    if (FocusedIndex >= 0 && (size_t)FocusedIndex < Context->EntryCount)
      KshimShowEntry((size_t)FocusedIndex);
  }
  if (mList != NULL) lv_obj_invalidate(mList);
}

void kshim_lvgl_set_status(kshim_lvgl_t *Context, const char *Text)
{
  /* In expanded STAR details the status label carries the entry title, so keep
   * writes on the (hidden) notice instead of clearing that title. */
  lv_obj_t *Notice;
  if (mMenuDesign.kind == KSHIM_DESIGN_STAR_CHART && (mInfoState || mBootSlotState)) {
    Notice = mNotice;
  } else {
    Notice = mNotice != NULL && !lv_obj_has_flag(mNotice, LV_OBJ_FLAG_HIDDEN) ? mNotice : mStatus;
  }
  if (kshim_lvgl_ready(Context) && Text != NULL && Notice != NULL) lv_label_set_text(Notice, Text);
}

void kshim_lvgl_set_countdown(kshim_lvgl_t *Context, uint32_t timeout_ms)
{
  if (!kshim_lvgl_ready(Context)) return;
  Context->CountdownMs = timeout_ms;
  Context->CountdownEnabled = timeout_ms != 0U;
  Context->CountdownShown = UINT32_MAX;
}

void kshim_lvgl_touch(void *Context, const kshim_touch_event_t *Event)
{
  kshim_lvgl_t *Ui = Context;
  if (!kshim_lvgl_ready(Ui) || Event == NULL) return;
  if (Event->type == KSHIM_TOUCH_EVENT_PRESS &&
      Event->x < Ui->Framebuffer->width && Event->y < Ui->Framebuffer->height)
    Ui->UserInteracted = 1U;
  const uint32_t NativeX = min_u32(Event->x, Ui->Framebuffer->width - 1U);
  const uint32_t NativeY = min_u32(Event->y, Ui->Framebuffer->height - 1U);
  switch (Ui->Rotation) {
  case KSHIM_FB_ROTATION_CCW_90:
    Ui->PointerX = Ui->Framebuffer->height - 1U - NativeY;
    Ui->PointerY = NativeX;
    break;
  case KSHIM_FB_ROTATION_180:
    Ui->PointerX = Ui->Framebuffer->width - 1U - NativeX;
    Ui->PointerY = Ui->Framebuffer->height - 1U - NativeY;
    break;
  case KSHIM_FB_ROTATION_CCW_270:
    Ui->PointerX = NativeY;
    Ui->PointerY = Ui->Framebuffer->width - 1U - NativeX;
    break;
  default:
    Ui->PointerX = NativeX;
    Ui->PointerY = NativeY;
    break;
  }
  switch (Event->type) {
  case KSHIM_TOUCH_EVENT_PRESS:
  case KSHIM_TOUCH_EVENT_MOVE:
    Ui->PointerPressed = 1U;
    break;
  case KSHIM_TOUCH_EVENT_RELEASE:
    Ui->PointerPressed = 0U;
    break;
  case KSHIM_TOUCH_EVENT_CANCEL:
    Ui->PointerPressed = 0U;
    /* A bus failure or controller reset must not confirm the Boot button. */
    lv_indev_wait_release(mPointerInput);
    break;
  default: return;
  }
  /* Deliver every transition, including a complete tap in one FIFO batch. */
  lv_indev_read(mPointerInput);
}

void kshim_lvgl_set_scratch(void *Base, size_t Size)
{
  if (mContext != NULL) return;
  mScratch = Base;
  mScratchSize = Base != NULL ? Size : 0U;
}

int kshim_lvgl_has_cjk_font(const kshim_lvgl_t *Context)
{
  return kshim_lvgl_ready(Context) && mFonts[1] != NULL;
}

int kshim_lvgl_has_interacted(const kshim_lvgl_t *Context)
{
  return kshim_lvgl_ready(Context) && Context->UserInteracted != 0U;
}

/* 在 LVGL 删除对象前关闭外部入口和焦点回调，避免访问已删除的兄弟控件。 */
void kshim_lvgl_deinit(kshim_lvgl_t *Context)
{
  if (Context == NULL || Context != mContext) return;
  Context->Ready = 0U;
  mContext = NULL;
  lv_deinit();
  KshimForgetFonts();
  mBackdropTextures[0] = mBackdropTextures[1] = NULL;
  mFontData = NULL;
  *Context = (kshim_lvgl_t){0};
  mKeys = NULL;
  mFramebuffer = NULL;
  mDisplay = NULL;
  mKeyInput = NULL;
  mPointerInput = NULL;
  mBackground = NULL;
  mPanel = NULL;
  mList = NULL;
  mBootButton = NULL;
  mStatus = NULL;
  mTitle = NULL;
  mDetail = NULL;
  mNotice = NULL;
  mHelp = NULL;
  mCredit = NULL;
  mLanguage = NULL;
  mColumns = 1U;
  mSplit = 0U;
  mRibbon = 0U;
  mTileHeight = 0U;
  mTileWidth = 0U;
  mInfoState = false;
  mBootSlotState = false;
}
