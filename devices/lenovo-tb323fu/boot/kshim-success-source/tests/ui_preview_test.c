/* Render reviewable full-resolution menu keyframes as binary PPM. */
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lvgl.h>
#include <lvgl_port.h>
#include <menu_text.h>
#include <menu_locale.h>
#include "../src/ui/menu_style.h"

#ifndef CONFIG_KSHIM_MENU_ENTRY_DURATION_MS
#define CONFIG_KSHIM_MENU_ENTRY_DURATION_MS KSHIM_UI_ENTRY_DURATION_MS
#endif

static uint32_t Inputs[3];

/* Reused by every rendered size; the setup below stays sequential, never
 * concurrent, so one scratch allocation is enough for all stages. */
static uint8_t Scratch[4U << 20] __attribute__((aligned(64)));

static uint32_t ReadInput(void *Context, uint64_t Address)
{
    (void)Context;
    assert(Address >= 1U && Address <= 3U);
    return Inputs[Address - 1U];
}

static void Advance(kshim_lvgl_t *Ui, uint32_t Milliseconds)
{
    while (Milliseconds != 0U) {
        uint32_t Step = Milliseconds > 16U ? 16U : Milliseconds;
        kshim_lvgl_frame(Ui, Step);
        Milliseconds -= Step;
    }
}

/* 按当前 tick 导出完整关键帧，不将显示刷新定时器的相位当成视觉差异。 */
static uint64_t WritePpm(
    const char *Directory,
    const char *Stage,
    const kshim_framebuffer_t *Framebuffer
)
{
    lv_refr_now(NULL);
    char Path[1024];
    int Length = snprintf(Path, sizeof(Path), "%s/menu-%ux%u-%s.ppm",
                          Directory, Framebuffer->width,
                          Framebuffer->height, Stage);
    assert(Length > 0 && (size_t)Length < sizeof(Path));
    FILE *File = fopen(Path, "wb");
    assert(File != NULL);
    assert(fprintf(File, "P6\n%u %u\n255\n", Framebuffer->width,
                   Framebuffer->height) > 0);

    uint64_t Hash = UINT64_C(14695981039346656037);
    unsigned NonBlack = 0U;
    unsigned Different = 0U;
    uint32_t FirstPixel = 0U;
    for (uint32_t Y = 0; Y < Framebuffer->height; Y++) {
        const uint32_t *Row = (const uint32_t *)(const void *)(
            Framebuffer->render_address + (size_t)Y * Framebuffer->stride);
        for (uint32_t X = 0; X < Framebuffer->width; X++) {
            uint32_t Pixel = Row[X];
            uint8_t Rgb[3] = {
                (uint8_t)(Pixel >> 16),
                (uint8_t)(Pixel >> 8),
                (uint8_t)Pixel,
            };
            assert(fwrite(Rgb, sizeof(Rgb), 1U, File) == 1U);
            if (Y == 0U && X == 0U)
                FirstPixel = Pixel & 0xffffffU;
            NonBlack += (Pixel & 0xffffffU) != 0U;
            Different += (Pixel & 0xffffffU) != FirstPixel;
            for (size_t Byte = 0; Byte < sizeof(Rgb); Byte++)
                Hash = (Hash ^ Rgb[Byte]) * UINT64_C(1099511628211);
        }
    }
    assert(NonBlack > Framebuffer->height);
    assert(Different > Framebuffer->height);
    assert(fclose(File) == 0);
    printf("%s  %016" PRIx64 "\n", Path, Hash);
    fflush(stdout);
    return Hash;
}

/* 记录确认入口和动画状态，保留像素变化断言。 */
static void TraceConfirmation(const char *Stage, const kshim_lvgl_t *Ui)
{
    printf("CONFIRM %s: index=%d ready=%u elapsed=%u\n", Stage,
           Ui->PendingIndex, (unsigned)Ui->Ready, (unsigned)Ui->ElapsedMs);
    printf("CONFIRM %s: animation=%u scale_x=%ld scale_y=%ld running=%u\n", Stage,
           (unsigned)(lv_anim_get(Ui->BootButton, NULL) != NULL),
           (long)lv_obj_get_style_transform_scale_x(Ui->BootButton, LV_PART_MAIN),
           (long)lv_obj_get_style_transform_scale_y(Ui->BootButton, LV_PART_MAIN),
           (unsigned)lv_anim_count_running());
    fflush(stdout);
}

static unsigned CountLabels(lv_obj_t *Object, const char *Text)
{
    unsigned Count = 0U;
    if (lv_obj_check_type(Object, &lv_label_class) &&
        strcmp(lv_label_get_text(Object), Text) == 0)
        Count++;
    for (uint32_t Index = 0; Index < lv_obj_get_child_count(Object); Index++)
        Count += CountLabels(lv_obj_get_child(Object, (int32_t)Index), Text);
    return Count;
}

static void AssertDescription(const kshim_lvgl_t *Ui, const char *Name,
                              const char *Description, const char *ActionLabel)
{
    /* The selected name appears both in navigation and in the right heading. */
    if(CountLabels(lv_screen_active(), Name)<2U) fprintf(stderr,"Missing selected name: %s (count %u)\n",Name,CountLabels(lv_screen_active(),Name));
    assert(CountLabels(lv_screen_active(), Name) >= 2U);
    assert(CountLabels(lv_screen_active(), Description) == 1U);
    assert(CountLabels(Ui->BootButton, ActionLabel) == 1U);
}

static void RenderSelections(const char *Directory,
                             const kshim_framebuffer_t *Framebuffer,
                             kshim_lvgl_t *Ui,
                             const char *const *Names,
                             const char *const *Descriptions,
                             const char *const *ActionLabels,
                             const char *const *Stages, size_t Count)
{
    for (size_t Index = 0; Index < Count; Index++) {
        lv_obj_t *Row = lv_group_get_obj_by_index(Ui->Group, (uint32_t)Index);
        assert(Row != NULL);
        lv_group_focus_obj(Row);
        Advance(Ui, 400U);
        AssertDescription(Ui, Names[Index], Descriptions[Index], ActionLabels[Index]);
        (void)WritePpm(Directory, Stages[Index], Framebuffer);
    }
}

/* Reorient the CCW270 native portrait scanout into a logical landscape PPM.
 * For logical (x,y): nativeX = physicalWidth-1-y, nativeY = x. The output
 * filename reports the logical dimensions, not the portrait source size. */
static uint64_t WriteLogicalPpm(const char *Directory, const char *Stage,
                                const kshim_framebuffer_t *Framebuffer,
                                uint32_t LogicalWidth, uint32_t LogicalHeight)
{
    lv_refr_now(NULL);
    char Path[1024];
    int Length = snprintf(Path, sizeof(Path), "%s/menu-%ux%u-%s.ppm",
                          Directory, LogicalWidth, LogicalHeight, Stage);
    assert(Length > 0 && (size_t)Length < sizeof(Path));
    FILE *File = fopen(Path, "wb");
    assert(File != NULL);
    assert(fprintf(File, "P6\n%u %u\n255\n", LogicalWidth, LogicalHeight) > 0);

    uint64_t Hash = UINT64_C(14695981039346656037);
    unsigned NonBlack = 0U;
    unsigned Different = 0U;
    uint32_t FirstPixel = 0U;
    for (uint32_t Y = 0; Y < LogicalHeight; Y++) {
        uint32_t NativeX = Framebuffer->width - 1U - Y;
        for (uint32_t X = 0; X < LogicalWidth; X++) {
            const uint32_t *Row = (const uint32_t *)(const void *)(
                Framebuffer->render_address + (size_t)X * Framebuffer->stride);
            uint32_t Pixel = Row[NativeX];
            uint8_t Rgb[3] = {
                (uint8_t)(Pixel >> 16),
                (uint8_t)(Pixel >> 8),
                (uint8_t)Pixel,
            };
            assert(fwrite(Rgb, sizeof(Rgb), 1U, File) == 1U);
            if (Y == 0U && X == 0U)
                FirstPixel = Pixel & 0xffffffU;
            NonBlack += (Pixel & 0xffffffU) != 0U;
            Different += (Pixel & 0xffffffU) != FirstPixel;
            for (size_t Byte = 0; Byte < sizeof(Rgb); Byte++)
                Hash = (Hash ^ Rgb[Byte]) * UINT64_C(1099511628211);
        }
    }
    assert(NonBlack > LogicalHeight);
    assert(Different > LogicalHeight);
    assert(fclose(File) == 0);
    printf("%s  %016" PRIx64 "\n", Path, Hash);
    fflush(stdout);
    return Hash;
}

/* Runtime role_name/role_description strings are the single source of truth;
 * the legacy literals are gone so bilingual output cannot drift. */
static void StarFillRoles(const kshim_menu_role_t *Roles, size_t Count,
                          const char **Names, const char **Descriptions)
{
    for (size_t Index = 0; Index < Count; Index++) {
        Names[Index] = kshim_menu_role_name(Roles[Index], "");
        Descriptions[Index] = kshim_menu_role_description(Roles[Index], "");
    }
}

static void RenderStarMain(const char *Directory, kshim_lvgl_t *Ui,
                           const kshim_framebuffer_t *Framebuffer,
                           uint32_t LogicalWidth, uint32_t LogicalHeight,
                           const char *Tag)
{
    static const kshim_menu_role_t Roles[8] = {
        KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_TF,
        KSHIM_MENU_ROLE_ADVANCED, KSHIM_MENU_ROLE_SETTINGS,
        KSHIM_MENU_ROLE_DEVICE_INFO,
        KSHIM_MENU_ROLE_RESTART, KSHIM_MENU_ROLE_OFF,
    };
    const char *Names[8];
    const char *Descriptions[8];
    const char *Actions[8];
    StarFillRoles(Roles, 8U, Names, Descriptions);
    Actions[0] = KSHIM_MENU_ACTION_BOOT;
    Actions[1] = KSHIM_MENU_ACTION_BOOT;
    Actions[2] = KSHIM_MENU_ACTION_BOOT;
    Actions[3] = KSHIM_MENU_ACTION_ENTER;
    Actions[4] = KSHIM_MENU_ACTION_ENTER;
    Actions[5] = KSHIM_MENU_ACTION_ENTER;
    Actions[6] = KSHIM_MENU_ACTION_RESTART;
    Actions[7] = KSHIM_MENU_ACTION_OFF;
    assert(kshim_lvgl_set_entries_described(Ui, (const char *const *)Names,
        (const char *const *)Descriptions, (const char *const *)Actions, 8U, 1U) == 0);
    kshim_lvgl_set_entry_roles(Ui, Roles, 8U);
    kshim_lvgl_set_countdown(Ui, Ui->ElapsedMs + 8000U);
    Advance(Ui, 16U);
    AssertDescription(Ui, Names[1], Descriptions[1], Actions[1]);
    char Countdown[128];
    assert(kshim_menu_format_countdown(Countdown, sizeof(Countdown), 8U, Names[1]) > 0);
    assert(CountLabels(lv_screen_active(), Countdown) == 1U);
    char Stage[64];
    int Length = snprintf(Stage, sizeof(Stage), "star-%s-main", Tag);
    assert(Length > 0 && (size_t)Length < sizeof(Stage));
    (void)WriteLogicalPpm(Directory, Stage, Framebuffer, LogicalWidth, LogicalHeight);
}

static void RenderStarAdvanced(const char *Directory, kshim_lvgl_t *Ui,
                               const kshim_framebuffer_t *Framebuffer,
                               uint32_t LogicalWidth, uint32_t LogicalHeight,
                               const char *Tag)
{
    static const kshim_menu_role_t Roles[6] = {
        KSHIM_MENU_ROLE_DIAGNOSTIC, KSHIM_MENU_ROLE_RECOVERY,
        KSHIM_MENU_ROLE_FASTBOOT, KSHIM_MENU_ROLE_EDL, KSHIM_MENU_ROLE_BOOT_SLOT, KSHIM_MENU_ROLE_BACK,
    };
    const char *Names[6];
    const char *Descriptions[6];
    const char *Actions[6];
    StarFillRoles(Roles, 6U, Names, Descriptions);
    Actions[0] = KSHIM_MENU_ACTION_BOOT;
    Actions[1] = KSHIM_MENU_ACTION_ENTER;
    Actions[2] = KSHIM_MENU_ACTION_ENTER;
    Actions[3] = KSHIM_MENU_ACTION_ENTER;
    Names[4] = kshim_menu_text(KSHIM_TEXT_AB);
    Descriptions[4] = kshim_menu_text(KSHIM_TEXT_AB_DESC);
    Actions[4] = KSHIM_MENU_ACTION_ENTER;
    Actions[5] = KSHIM_MENU_ACTION_BACK;
    assert(kshim_lvgl_set_entries_described(Ui, (const char *const *)Names,
        (const char *const *)Descriptions, (const char *const *)Actions, 6U, 0U) == 0);
    kshim_lvgl_set_entry_roles(Ui, Roles, 6U);
    kshim_lvgl_set_countdown(Ui, 0U);
    kshim_lvgl_set_status(Ui, "");
    Advance(Ui, 400U);
    AssertDescription(Ui, Names[0], Descriptions[0], Actions[0]);
    char Stage[64];
    int Length = snprintf(Stage, sizeof(Stage), "star-%s-advanced", Tag);
    assert(Length > 0 && (size_t)Length < sizeof(Stage));
    (void)WriteLogicalPpm(Directory, Stage, Framebuffer, LogicalWidth, LogicalHeight);
}

static void RenderStarLanguagePage(const char *Directory, kshim_lvgl_t *Ui,
                                   const kshim_framebuffer_t *Framebuffer,
                                   uint32_t LogicalWidth, uint32_t LogicalHeight,
                                   kshim_menu_language_t Language, const char *Tag)
{
    static const kshim_menu_role_t Roles[3] = {
        KSHIM_MENU_ROLE_LANG_ZH, KSHIM_MENU_ROLE_LANG_EN, KSHIM_MENU_ROLE_BACK,
    };
    const char *Names[3];
    const char *Descriptions[3];
    const char *Actions[3];
    StarFillRoles(Roles, 3U, Names, Descriptions);
    Actions[0] = kshim_menu_text(KSHIM_TEXT_LANGUAGE_SELECT);
    Actions[1] = kshim_menu_text(KSHIM_TEXT_LANGUAGE_SELECT);
    Actions[2] = KSHIM_MENU_ACTION_BACK;
    size_t Initial = Language == KSHIM_MENU_LANG_EN ? 1U : 0U;
    assert(kshim_lvgl_set_entries_described(Ui, (const char *const *)Names,
        (const char *const *)Descriptions, (const char *const *)Actions, 3U, Initial) == 0);
    kshim_lvgl_set_entry_roles(Ui, Roles, 3U);
    kshim_lvgl_set_countdown(Ui, 0U);
    kshim_lvgl_set_status(Ui, "");
    Advance(Ui, 400U);
    AssertDescription(Ui, Names[Initial], Descriptions[Initial], Actions[Initial]);
    char Stage[64];
    int Length = snprintf(Stage, sizeof(Stage), "star-%s-language", Tag);
    assert(Length > 0 && (size_t)Length < sizeof(Stage));
    (void)WriteLogicalPpm(Directory, Stage, Framebuffer, LogicalWidth, LogicalHeight);
}

static void RenderStarDeviceInfo(const char *Directory, kshim_lvgl_t *Ui,
                                 const kshim_framebuffer_t *Framebuffer,
                                 uint32_t Width, uint32_t Height, const char *Tag)
{
    const kshim_menu_role_t roles[6]={KSHIM_MENU_ROLE_ANDROID,KSHIM_MENU_ROLE_INTERNAL,
        KSHIM_MENU_ROLE_TF,KSHIM_MENU_ROLE_ADVANCED,KSHIM_MENU_ROLE_SETTINGS,KSHIM_MENU_ROLE_DEVICE_INFO};
    const char *names[6],*descriptions[6],*actions[6];
    StarFillRoles(roles,6,names,descriptions);
    for(unsigned i=0;i<6;++i)actions[i]=KSHIM_MENU_ACTION_ENTER;
    actions[5]=kshim_menu_text(KSHIM_TEXT_BACK);
    char summary[1024];
    snprintf(summary,sizeof(summary),"%s: TB323FU\n%s: SM8850\n%s: 12288 MiB\n%s: %s\n%s: preview",
        kshim_menu_text(KSHIM_TEXT_INFO_BOARD),kshim_menu_text(KSHIM_TEXT_INFO_SOC),
        kshim_menu_text(KSHIM_TEXT_INFO_RAM),kshim_menu_text(KSHIM_TEXT_INFO_SLOT),
        kshim_menu_text(KSHIM_TEXT_SLOT_B),kshim_menu_text(KSHIM_TEXT_INFO_VERSION));
    descriptions[5]=summary;
    assert(kshim_lvgl_set_entries_described(Ui,names,descriptions,actions,6,5)==0);
    kshim_lvgl_set_entry_roles(Ui,roles,6);
    kshim_lvgl_set_page_title(Ui,kshim_menu_text(KSHIM_TEXT_INFO_LABEL));
    kshim_lvgl_set_countdown(Ui,0);kshim_lvgl_set_status(Ui,"");Advance(Ui,400);
    AssertDescription(Ui,names[5],summary,actions[5]);
    char stage[80];snprintf(stage,sizeof(stage),"star-%s-device-info",Tag);
    (void)WriteLogicalPpm(Directory,stage,Framebuffer,Width,Height);
}

static void AssertBootSlotHelpFits(lv_obj_t *Object, const char *Text,
                                  const kshim_lvgl_t *Ui)
{
    if (lv_obj_check_type(Object, &lv_label_class) &&
        strcmp(lv_label_get_text(Object), Text) == 0) {
        lv_point_t Size;
        lv_text_get_size(&Size, Text, lv_obj_get_style_text_font(Object, 0),
            lv_obj_get_style_text_letter_space(Object, 0),
            lv_obj_get_style_text_line_space(Object, 0),
            lv_obj_get_content_width(Object), LV_TEXT_FLAG_NONE);
        printf("BOOT SLOT HELP: text=%ldx%ld area=%ldx%ld\n",
            (long)Size.x, (long)Size.y, (long)lv_obj_get_content_width(Object),
            (long)lv_obj_get_content_height(Object));
        assert(Size.x <= lv_obj_get_content_width(Object));
        assert(Size.y <= lv_obj_get_content_height(Object));
        lv_area_t Body, Button;
        lv_obj_get_coords(Object, &Body);
        lv_obj_get_coords(Ui->BootButton, &Button);
        assert(Body.y2 < Button.y1);
    }
    for (uint32_t Index = 0; Index < lv_obj_get_child_count(Object); Index++)
        AssertBootSlotHelpFits(lv_obj_get_child(Object, (int32_t)Index), Text, Ui);
}

static void RenderStarAb(const char *Directory,kshim_lvgl_t *Ui,
                         const kshim_framebuffer_t *Framebuffer,
                         uint32_t Width,uint32_t Height,const char *Tag)
{
    for(unsigned current=0;current<2;++current)for(unsigned page=0;page<5;++page){
        unsigned target=1-current;
        char stage[80],state[96],next[96],body[1152],detail[1536],back[1536];
        snprintf(state,sizeof(state),kshim_menu_text(KSHIM_TEXT_SLOT_STATE_FMT),kshim_menu_text(current?KSHIM_TEXT_SLOT_B:KSHIM_TEXT_SLOT_A));
        snprintf(next,sizeof(next),kshim_menu_text(KSHIM_TEXT_SLOT_TARGET_FMT),kshim_menu_text(target?KSHIM_TEXT_SLOT_B:KSHIM_TEXT_SLOT_A));
        const char *names[3],*descriptions[3],*actions[3];
        kshim_menu_role_t roles[3]={KSHIM_MENU_ROLE_BOOT_SLOT,KSHIM_MENU_ROLE_BOOT_SLOT,KSHIM_MENU_ROLE_BOOT_SLOT};
        size_t count=2,initial=0;
        const char *title=kshim_menu_text(KSHIM_TEXT_AB);
        if(page==0){
            title=kshim_menu_text(KSHIM_TEXT_ADVANCED);
            names[0]=kshim_menu_text(KSHIM_TEXT_AB);names[1]=kshim_menu_text(KSHIM_TEXT_BACK);
            snprintf(detail,sizeof(detail),"%s\n\n%s",state,kshim_menu_text(KSHIM_TEXT_AB_DESC));
            descriptions[0]=detail;descriptions[1]=KSHIM_MENU_DESC_BACK;
            actions[0]=KSHIM_MENU_ACTION_ENTER;actions[1]=KSHIM_MENU_ACTION_BACK;
        }else if(page==1||page==4){
            count=3;initial=target;
            names[current]=kshim_menu_text(current?KSHIM_TEXT_AB_CURRENT_B:KSHIM_TEXT_AB_CURRENT_A);
            names[target]=kshim_menu_text(page==4?(target?KSHIM_TEXT_AB_UNAVAILABLE_B:KSHIM_TEXT_AB_UNAVAILABLE_A):(target?KSHIM_TEXT_AB_B:KSHIM_TEXT_AB_A));
            names[2]=kshim_menu_text(KSHIM_TEXT_AB_BACK);
            if(page==4)snprintf(body,sizeof(body),"%s",kshim_menu_text(KSHIM_TEXT_AB_UNAVAILABLE));
            else snprintf(body,sizeof(body),kshim_menu_text(KSHIM_TEXT_AB_TARGET_DESC),
                kshim_menu_text(current?KSHIM_TEXT_SLOT_B:KSHIM_TEXT_SLOT_A),kshim_menu_text(target?KSHIM_TEXT_SLOT_B:KSHIM_TEXT_SLOT_A),
                          kshim_menu_text(target?KSHIM_TEXT_SLOT_B:KSHIM_TEXT_SLOT_A));
            snprintf(detail,sizeof(detail),"%s\n\n%s",state,body);
            descriptions[target]=detail;descriptions[current]=state;descriptions[2]=kshim_menu_text(KSHIM_TEXT_AB_BACK_DESC);
            actions[current]=kshim_menu_text(KSHIM_TEXT_ACTION_UNAVAILABLE);actions[target]=page==4?actions[current]:KSHIM_MENU_ACTION_ENTER;actions[2]=KSHIM_MENU_ACTION_BACK;
        }else{
            title=kshim_menu_text(target?KSHIM_TEXT_AB_CONFIRM_B:KSHIM_TEXT_AB_CONFIRM_A);
            initial=page==2?1:0;
            names[0]=kshim_menu_text(KSHIM_TEXT_ACTION_SWITCH_RESTART);names[1]=KSHIM_MENU_ACTION_BACK;
            snprintf(body,sizeof(body),kshim_menu_text(KSHIM_TEXT_AB_CONFIRM),kshim_menu_text(target?KSHIM_TEXT_SLOT_B:KSHIM_TEXT_SLOT_A));
            snprintf(detail,sizeof(detail),"%s\n%s\n\n%s",state,next,body);
            snprintf(back,sizeof(back),"%s\n%s\n\n%s",state,next,kshim_menu_text(KSHIM_TEXT_AB_CONFIRM_BACK_DESC));
            descriptions[0]=detail;descriptions[1]=back;
            actions[0]=names[0];actions[1]=KSHIM_MENU_ACTION_BACK;
        }
        assert(kshim_lvgl_set_entries_described(Ui,names,descriptions,actions,count,initial)==0);
        kshim_lvgl_set_entry_roles(Ui,roles,count);
        kshim_lvgl_set_page_title(Ui,title);
        kshim_lvgl_set_countdown(Ui,0);kshim_lvgl_set_status(Ui,"");Advance(Ui,400);
        AssertDescription(Ui,names[initial],descriptions[initial],actions[initial]);
        assert(CountLabels(lv_screen_active(),title)>=1);
        AssertBootSlotHelpFits(lv_screen_active(),descriptions[initial],Ui);
        snprintf(stage,sizeof(stage),"star-%s-ab-%u-%u",Tag,current,page);
        (void)WriteLogicalPpm(Directory,stage,Framebuffer,Width,Height);
    }
}
static void RenderStarSettings(const char *Directory,kshim_lvgl_t *Ui,
                               const kshim_framebuffer_t *Framebuffer,
                               uint32_t Width,uint32_t Height,const char *Tag)
{
    for(unsigned page=0;page<2;++page){
        char device[192],system[192],language[192],automatic[192],stage[80];
        snprintf(device,sizeof(device),kshim_menu_text(KSHIM_TEXT_SETTINGS_DEVICE_AUTO_FMT),kshim_menu_text(KSHIM_TEXT_BOARD_TB323FU_NAME));
        snprintf(system,sizeof(system),kshim_menu_text(KSHIM_TEXT_SETTINGS_DEFAULT_FMT),kshim_menu_get_language()==KSHIM_MENU_LANG_ZH_CN?"机身 ArmadaOS":"Internal ArmadaOS");
        snprintf(language,sizeof(language),kshim_menu_text(KSHIM_TEXT_SETTINGS_LANGUAGE_FMT),kshim_menu_text(KSHIM_TEXT_LANGUAGE_STATE));
        snprintf(automatic,sizeof(automatic),kshim_menu_text(KSHIM_TEXT_DEVICE_AUTO_FMT),kshim_menu_text(KSHIM_TEXT_BOARD_TB323FU_NAME));
        const char *names[4]={device,system,language,kshim_menu_text(KSHIM_TEXT_BACK)};
        const char *descriptions[4]={kshim_menu_text(KSHIM_TEXT_DEVICE_DESC),kshim_menu_text(KSHIM_TEXT_DEFAULT_DESC),kshim_menu_text(KSHIM_TEXT_LANGUAGE_DESC),KSHIM_MENU_DESC_BACK};
        const char *actions[4]={KSHIM_MENU_ACTION_ENTER,KSHIM_MENU_ACTION_ENTER,KSHIM_MENU_ACTION_ENTER,KSHIM_MENU_ACTION_BACK};
        kshim_menu_role_t roles[4]={KSHIM_MENU_ROLE_DEVICE,KSHIM_MENU_ROLE_DEFAULT,KSHIM_MENU_ROLE_LANGUAGE,KSHIM_MENU_ROLE_BACK};
        const char *title=kshim_menu_text(page?KSHIM_TEXT_DEVICE_LABEL:KSHIM_TEXT_SETTINGS_LABEL);
        if(page){
            names[0]=automatic;names[1]=kshim_menu_text(KSHIM_TEXT_DEVICE_TB322FC_NAME);names[2]=kshim_menu_text(KSHIM_TEXT_DEVICE_TB323FU_NAME);names[3]=kshim_menu_text(KSHIM_TEXT_BACK_SETTINGS);
            descriptions[0]=kshim_menu_text(KSHIM_TEXT_DEVICE_AUTO_DESC);descriptions[1]=kshim_menu_text(KSHIM_TEXT_DEVICE_TB322FC_DESC);descriptions[2]=kshim_menu_text(KSHIM_TEXT_DEVICE_TB323FU_DESC);descriptions[3]=kshim_menu_text(KSHIM_TEXT_BACK_SETTINGS_DESC);
            roles[0]=KSHIM_MENU_ROLE_DEVICE_AUTO;roles[1]=KSHIM_MENU_ROLE_DEVICE_TB322FC;roles[2]=KSHIM_MENU_ROLE_DEVICE_TB323FU;
            actions[0]=actions[1]=actions[2]=kshim_menu_text(KSHIM_TEXT_LANGUAGE_SELECT);
        }
        char saved[576];
        if(!page){
            snprintf(saved,sizeof(saved),"%s\n\n%s",descriptions[0],kshim_menu_text(KSHIM_TEXT_SETTINGS_INTERNAL));
            descriptions[0]=saved;
        }
        size_t initial=page?2:0;
        assert(kshim_lvgl_set_entries_described(Ui,names,descriptions,actions,4,initial)==0);
        kshim_lvgl_set_entry_roles(Ui,roles,4);kshim_lvgl_set_page_title(Ui,title);
        kshim_lvgl_set_countdown(Ui,0);
        Advance(Ui,400);
        AssertDescription(Ui,names[initial],descriptions[initial],actions[initial]);
        AssertBootSlotHelpFits(lv_screen_active(),descriptions[initial],Ui);
        snprintf(stage,sizeof(stage),"star-%s-%s",Tag,page?"device-select":"settings");
        (void)WriteLogicalPpm(Directory,stage,Framebuffer,Width,Height);
    }
}
static void RenderStarLanguageSet(const char *Directory, kshim_lvgl_t *Ui,
                                  const kshim_framebuffer_t *Framebuffer,
                                  uint32_t LogicalWidth, uint32_t LogicalHeight,
                                  kshim_menu_language_t Language, const char *Tag)
{
    assert(kshim_menu_set_language(Language) == 0);
    RenderStarMain(Directory, Ui, Framebuffer, LogicalWidth, LogicalHeight, Tag);
    RenderStarAdvanced(Directory, Ui, Framebuffer, LogicalWidth, LogicalHeight, Tag);
    RenderStarLanguagePage(Directory, Ui, Framebuffer, LogicalWidth, LogicalHeight,
                           Language, Tag);
    RenderStarDeviceInfo(Directory, Ui, Framebuffer, LogicalWidth, LogicalHeight, Tag);
    RenderStarAb(Directory, Ui, Framebuffer, LogicalWidth, LogicalHeight, Tag);
    RenderStarSettings(Directory, Ui, Framebuffer, LogicalWidth, LogicalHeight, Tag);
}

static void RenderStarSize(const char *Directory, uint32_t PhysicalWidth,
                           uint32_t PhysicalHeight)
{
    size_t Bytes = (size_t)PhysicalWidth * PhysicalHeight * sizeof(uint32_t);
    uint32_t *Pixels = malloc(Bytes);
    assert(Pixels != NULL);
    memset(Pixels, 0, Bytes);

    kshim_framebuffer_config_t Config = {
        .render_address = (uintptr_t)Pixels,
        .width = PhysicalWidth,
        .height = PhysicalHeight,
        .stride = PhysicalWidth * sizeof(uint32_t),
        .bpp = 32U,
        .format = KSHIM_FB_FORMAT_XRGB8888,
        .buffer_size = Bytes,
    };
    kshim_framebuffer_t Framebuffer;
    assert(kshim_fb_init(&Framebuffer, &Config) == KSHIM_FB_OK);

    QcomKeyDescriptor Descriptors[3];
    const uint32_t Codes[] = {
        QCOM_KEYCODE_VOLUMEUP, QCOM_KEYCODE_VOLUMEDOWN,
        QCOM_KEYCODE_POWER,
    };
    for (size_t Index = 0; Index < 3U; Index++)
        assert(QcomKeyMakeGpioAt(&Descriptors[Index], Index + 1U, 1U,
                                 Codes[Index], 0U) == 0);
    QcomKeys Keys;
    memset(Inputs, 0, sizeof(Inputs));
    assert(QcomKeysInit(&Keys, Descriptors, 3U, ReadInput, NULL) == 0);

    kshim_lvgl_set_scratch(Scratch, sizeof(Scratch));
    kshim_lvgl_t Ui;
    assert(kshim_lvgl_init(&Ui, &Framebuffer, &Keys) == 0);
    assert(kshim_lvgl_has_cjk_font(&Ui));
    assert(Ui.Rotation == KSHIM_FB_ROTATION_CCW_270);
    uint32_t LogicalWidth = PhysicalHeight;
    uint32_t LogicalHeight = PhysicalWidth;
    assert(Ui.LogicalWidth == LogicalWidth && Ui.LogicalHeight == LogicalHeight);

    RenderStarLanguageSet(Directory, &Ui, &Framebuffer, LogicalWidth, LogicalHeight,
                          KSHIM_MENU_LANG_ZH_CN, "zh");
    RenderStarLanguageSet(Directory, &Ui, &Framebuffer, LogicalWidth, LogicalHeight,
                          KSHIM_MENU_LANG_EN, "en");

    kshim_lvgl_deinit(&Ui);
    free(Pixels);
}

static void RenderSize(const char *Directory, uint32_t Width, uint32_t Height, int ProductMenu)
{
    size_t Bytes = (size_t)Width * Height * sizeof(uint32_t);
    uint32_t *Pixels = malloc(Bytes);
    assert(Pixels != NULL);
    memset(Pixels, 0, Bytes);

    kshim_framebuffer_config_t Config = {
        .render_address = (uintptr_t)Pixels,
        .width = Width,
        .height = Height,
        .stride = Width * sizeof(uint32_t),
        .bpp = 32U,
        .format = KSHIM_FB_FORMAT_XRGB8888,
        .buffer_size = Bytes,
    };
    kshim_framebuffer_t Framebuffer;
    assert(kshim_fb_init(&Framebuffer, &Config) == KSHIM_FB_OK);

    QcomKeyDescriptor Descriptors[3];
    const uint32_t Codes[] = {
        QCOM_KEYCODE_VOLUMEUP, QCOM_KEYCODE_VOLUMEDOWN,
        QCOM_KEYCODE_POWER,
    };
    for (size_t Index = 0; Index < 3U; Index++)
        assert(QcomKeyMakeGpioAt(&Descriptors[Index], Index + 1U, 1U,
                                 Codes[Index], 0U) == 0);
    QcomKeys Keys;
    memset(Inputs, 0, sizeof(Inputs));
    assert(QcomKeysInit(&Keys, Descriptors, 3U, ReadInput, NULL) == 0);

    kshim_lvgl_set_scratch(Scratch, sizeof(Scratch));
    kshim_lvgl_t Ui;
    assert(kshim_lvgl_init(&Ui, &Framebuffer, &Keys) == 0);
    assert(kshim_lvgl_has_cjk_font(&Ui));
    static const char *const Entries[] = {
        "Android", "UEFI 诊断", "Recovery 恢复模式", "Factory image",
        "Copied test payload", "Network rescue", "关机",
    };
    static const kshim_menu_role_t ProductRoles[] = {
        KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_TF,
        KSHIM_MENU_ROLE_ADVANCED, KSHIM_MENU_ROLE_RESTART, KSHIM_MENU_ROLE_OFF,
    };
    /* DESC/ACTION macros are runtime expressions now, so the product arrays
     * cannot stay as static initializers; names come from the role API. */
    const char *ProductEntries[6];
    const char *ProductDescriptions[6];
    const char *ProductActions[6];
    for (size_t Index = 0; Index < 6U; Index++) {
        ProductEntries[Index] = kshim_menu_role_name(ProductRoles[Index], "");
        ProductDescriptions[Index] =
            kshim_menu_role_description(ProductRoles[Index], "");
    }
    ProductActions[0] = KSHIM_MENU_ACTION_BOOT;
    ProductActions[1] = KSHIM_MENU_ACTION_BOOT;
    ProductActions[2] = KSHIM_MENU_ACTION_BOOT;
    ProductActions[3] = KSHIM_MENU_ACTION_ENTER;
    ProductActions[4] = KSHIM_MENU_ACTION_RESTART;
    ProductActions[5] = KSHIM_MENU_ACTION_OFF;
    size_t Count = ProductMenu ? 6U : sizeof(Entries) / sizeof(Entries[0]);
    if (ProductMenu)
        assert(kshim_lvgl_set_entries_described(&Ui,
            (const char *const *)ProductEntries,
            (const char *const *)ProductDescriptions,
            (const char *const *)ProductActions, Count, 1U) == 0);
    else
        assert(kshim_lvgl_set_entries(&Ui, (const char *const *)Entries,
                                      Count, 0U) == 0);

    Advance(&Ui, 120U);
    if (ProductMenu)
        AssertDescription(&Ui, ProductEntries[1], ProductDescriptions[1], ProductActions[1]);
    uint64_t EntryHash = WritePpm(Directory, "entry-120ms", &Framebuffer);
    Advance(&Ui, 280U);
    if (ProductMenu)
        AssertDescription(&Ui, ProductEntries[1], ProductDescriptions[1], ProductActions[1]);
    uint64_t ReadyHash = WritePpm(Directory, "ready-400ms", &Framebuffer);
    /* Native LVGL has no custom entry fade: these two frames should be stable. */
    assert((KSHIM_MENU_IS_NATIVE || (ProductMenu && CONFIG_KSHIM_MENU_ENTRY_DURATION_MS == 0))
        ? EntryHash == ReadyHash : EntryHash != ReadyHash);

    if (ProductMenu) {
        kshim_lvgl_set_countdown(&Ui, 8000U);
        Advance(&Ui, 16U);
        AssertDescription(&Ui, ProductEntries[1], ProductDescriptions[1], ProductActions[1]);
        char Countdown[128];
        assert(kshim_menu_format_countdown(Countdown, sizeof(Countdown), 8U,
                                           ProductEntries[1]) > 0);
        assert(CountLabels(lv_screen_active(), Countdown) == 1U);
        (void)WritePpm(Directory, "product-countdown", &Framebuffer);
        kshim_lvgl_set_countdown(&Ui, 0U);
        kshim_lvgl_set_status(&Ui, "");
    }

    lv_obj_t *Third = lv_group_get_obj_by_index(Ui.Group, 2U);
    assert(Third != NULL);
    lv_group_focus_obj(Third);
    Advance(&Ui, KSHIM_UI_FOCUS_DURATION_MS / 2U);
    if (ProductMenu)
        AssertDescription(&Ui, ProductEntries[2], ProductDescriptions[2], ProductActions[2]);
    uint64_t FocusHash = WritePpm(Directory, "focus-130ms", &Framebuffer);
    assert(FocusHash != ReadyHash);

    if (KSHIM_MENU_IS_NATIVE) {
        /* Sample actual native pressed feedback, not a synthetic kShim pulse. */
        lv_area_t area; lv_obj_get_coords(Ui.BootButton, &area);
        kshim_touch_event_t touch = {.type=KSHIM_TOUCH_EVENT_PRESS,
            .x=(uint32_t)((area.x1+area.x2)/2), .y=(uint32_t)((area.y1+area.y2)/2)};
        kshim_lvgl_touch(&Ui, &touch);
        assert(kshim_lvgl_take_index(&Ui) == -1);
        Advance(&Ui, KSHIM_UI_CONFIRM_DURATION_MS / 2U);
        if (ProductMenu)
            AssertDescription(&Ui, ProductEntries[2], ProductDescriptions[2], ProductActions[2]);
        uint64_t ConfirmHash = WritePpm(Directory, "confirm-90ms", &Framebuffer);
        assert(ConfirmHash != FocusHash && lv_obj_has_state(Ui.BootButton,LV_STATE_PRESSED));
        touch.type=KSHIM_TOUCH_EVENT_RELEASE;
        kshim_lvgl_touch(&Ui, &touch);
    } else {
        assert(lv_obj_send_event(Ui.BootButton, LV_EVENT_CLICKED, NULL) == LV_RESULT_OK);
        TraceConfirmation("requested", &Ui);
        Advance(&Ui, KSHIM_UI_CONFIRM_DURATION_MS / 2U);
        if (ProductMenu)
            AssertDescription(&Ui, ProductEntries[2], ProductDescriptions[2], ProductActions[2]);
        uint64_t ConfirmHash = WritePpm(Directory, "confirm-90ms", &Framebuffer);
        TraceConfirmation("sampled", &Ui);
        assert(ConfirmHash != FocusHash);
    }
    assert(kshim_lvgl_take_index(&Ui) == 2);

    if (ProductMenu) {
        static const char *const MainStages[] = {
            "main-android", "main-internal", "main-tf", "main-advanced",
            "main-restart", "main-off",
        };
        RenderSelections(Directory, &Framebuffer, &Ui,
                         (const char *const *)ProductEntries,
                         (const char *const *)ProductDescriptions,
                         (const char *const *)ProductActions, MainStages, Count);
        kshim_lvgl_set_status(&Ui, "电源操作失败");
        Advance(&Ui, 16U);
        AssertDescription(&Ui, ProductEntries[5], ProductDescriptions[5], ProductActions[5]);
        assert(CountLabels(lv_screen_active(), "电源操作失败") == 1U);
        (void)WritePpm(Directory, "product-status", &Framebuffer);

        static const kshim_menu_role_t AdvancedRoles[] = {
            KSHIM_MENU_ROLE_DIAGNOSTIC, KSHIM_MENU_ROLE_RECOVERY,
            KSHIM_MENU_ROLE_FASTBOOT, KSHIM_MENU_ROLE_EDL, KSHIM_MENU_ROLE_BACK,
        };
        const char *Advanced[5];
        const char *AdvancedDescriptions[5];
        const char *AdvancedActions[5];
        for (size_t Index = 0; Index < 5U; Index++) {
            Advanced[Index] = kshim_menu_role_name(AdvancedRoles[Index], "");
            AdvancedDescriptions[Index] =
                kshim_menu_role_description(AdvancedRoles[Index], "");
        }
        AdvancedActions[0] = KSHIM_MENU_ACTION_BOOT;
        AdvancedActions[1] = KSHIM_MENU_ACTION_ENTER;
        AdvancedActions[2] = KSHIM_MENU_ACTION_ENTER;
        AdvancedActions[3] = KSHIM_MENU_ACTION_ENTER;
        AdvancedActions[4] = KSHIM_MENU_ACTION_BACK;
        static const char *const AdvancedStages[] = {
            "advanced-diagnostic", "advanced-recovery", "advanced-fastboot",
            "advanced-edl", "advanced-back",
        };
        assert(kshim_lvgl_set_entries_described(&Ui, (const char *const *)Advanced,
            (const char *const *)AdvancedDescriptions,
            (const char *const *)AdvancedActions, 5U, 0U) == 0);
        Advance(&Ui, 400U);
        AssertDescription(&Ui, Advanced[0], AdvancedDescriptions[0], AdvancedActions[0]);
        (void)WritePpm(Directory, "advanced-ready", &Framebuffer);
        RenderSelections(Directory, &Framebuffer, &Ui, (const char *const *)Advanced,
                         (const char *const *)AdvancedDescriptions,
                         (const char *const *)AdvancedActions, AdvancedStages, 5U);
    }

    kshim_lvgl_deinit(&Ui);
    free(Pixels);
}

int main(int ArgumentCount, char **Arguments)
{
    assert(ArgumentCount == 2 ||
           (ArgumentCount == 3 &&
            (strcmp(Arguments[2], "--tb323fu") == 0 ||
             strcmp(Arguments[2], "--star-chart") == 0)));
    /* Keep the default locale aligned with the board config; never force
     * English on the legacy preview paths. */
#if defined(CONFIG_KSHIM_MENU_CHINESE) && CONFIG_KSHIM_MENU_CHINESE
    assert(kshim_menu_set_language(KSHIM_MENU_LANG_ZH_CN) == 0);
#else
    assert(kshim_menu_set_language(KSHIM_MENU_LANG_EN) == 0);
#endif
    if (ArgumentCount == 3 && strcmp(Arguments[2], "--star-chart") == 0) {
#if defined(CONFIG_KSHIM_UI_ROTATION_CCW) && CONFIG_KSHIM_UI_ROTATION_CCW == 270
        RenderStarSize(Arguments[1], 1904U, 3040U);
        RenderStarSize(Arguments[1], 1080U, 1920U);
#else
        fputs("--star-chart requires CONFIG_KSHIM_UI_ROTATION_CCW == 270\n",
              stderr);
        return 1;
#endif
    } else if (ArgumentCount == 3) {
        RenderSize(Arguments[1], 1904U, 3040U, 1);
#if !defined(CONFIG_KSHIM_UI_ROTATION_CCW) || CONFIG_KSHIM_UI_ROTATION_CCW == 0
        RenderSize(Arguments[1], 3040U, 1904U, 1);
#endif
    } else {
        RenderSize(Arguments[1], 1080U, 2340U, 0);
        RenderSize(Arguments[1], 1920U, 1080U, 0);
    }
    puts("full-resolution menu keyframes rendered");
    return 0;
}
