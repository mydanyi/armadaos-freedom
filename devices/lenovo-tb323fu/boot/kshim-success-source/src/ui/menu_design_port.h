#ifndef KSHIM_MENU_DESIGN_PORT_H
#define KSHIM_MENU_DESIGN_PORT_H
/* Include in lvgl_port.c after the shared animation helpers and globals.
 * This file never confirms, changes entry order, or owns an input device. */
#include "menu_design_layout.h"
#include "menu_symbols.h"
#include "menu_design_font.h"
#include "menu_design_background.h"
static kshim_design_geometry_t mDesignGeometry;

static inline uint32_t KshimDesignScaleOverride(void) {
#ifdef CONFIG_KSHIM_MENU_SCALE_Q8
    return CONFIG_KSHIM_MENU_SCALE_Q8;
#else
    return 0U;
#endif
}
static inline int32_t KshimDesignPx(uint32_t units) {
    return kshim_design_px(units,mDesignGeometry.scale_q8?mDesignGeometry.scale_q8:256U);
}
static inline int32_t KshimDesignStroke(uint32_t units) {
    return kshim_design_stroke(units,mDesignGeometry.scale_q8?mDesignGeometry.scale_q8:256U);
}
static inline bool KshimDesignReduced(void) {
#if defined(CONFIG_KSHIM_MENU_REDUCED_EFFECTS) && CONFIG_KSHIM_MENU_REDUCED_EFFECTS
    return true;
#else
    return false;
#endif
}
static inline bool KshimDesignBarePanel(void) {
    unsigned k=mMenuDesign.kind;
    return k==KSHIM_DESIGN_METRO || k==KSHIM_DESIGN_ONE_UI || k==KSHIM_DESIGN_MINIMAL || k==KSHIM_DESIGN_CARDS || k==KSHIM_DESIGN_CUPERTINO ||
        k==KSHIM_DESIGN_IOS_HIG || k==KSHIM_DESIGN_HARMONYOS || k==KSHIM_DESIGN_CLOVER || k==KSHIM_DESIGN_BENTO ||
        k==KSHIM_DESIGN_STAR_CHART;
}
static inline uint32_t KshimDesignPressColor(void) {
    return mMenuStyle.Modern?mMenuStyle.Press:mMenuStyle.Focus;
}
static inline uint32_t KshimDesignPressText(void) {
    return mMenuStyle.Modern?mMenuStyle.PressText:mMenuStyle.FocusText;
}
static inline void KshimDesignPlace(lv_obj_t *object,kshim_design_rect_t r) {
    if(!object) return;
    if(r.w<1 || r.h<1) { lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN); return; }
    lv_obj_remove_flag(object,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(object,r.x,r.y); lv_obj_set_size(object,r.w,r.h);
}
static inline void KshimDesignSolid(lv_obj_t *object,uint32_t color,lv_opa_t opacity,lv_style_selector_t state) {
    lv_obj_set_style_bg_color(object,lv_color_hex(color),state);
    lv_obj_set_style_bg_grad_dir(object,LV_GRAD_DIR_NONE,state);
    lv_obj_set_style_bg_opa(object,opacity,state);
}
#include "menu_star_chart.h"
#include "menu_new_themes_render.h"
static void KshimDesignPanelDraw(lv_event_t *event) {
    if(!mMenuDesign.enabled || mDesignGeometry.compact) return;
    KshimStarPanelDraw(event);
    lv_obj_t *panel=lv_event_get_target_obj(event);
    lv_area_t a; lv_obj_get_coords(panel,&a);
    lv_opa_t opa=lv_obj_get_style_opa_recursive(panel,LV_PART_MAIN);
    int32_t p=KshimDesignPx(mMenuDesign.padding), width=lv_obj_get_width(panel);
    KshimNewThemePanelDraw(event);
    if(mMenuDesign.header==1 || mMenuDesign.header==2) {
        uint32_t fill=mMenuDesign.header==1?mMenuStyle.Focus:mMenuStyle.Boot;
        if(mMenuDesign.header==2 && mMenuStyle.Screen==0x121212U) fill=mMenuStyle.Item;
        int32_t height=mDesignGeometry.list.y-KshimDesignPx(8);
        if(height>0 && mDesignGeometry.title.h)
            KshimSymbolRect(event,fill,opa,a.x1,a.y1,width,height,0,0);
    } else if(mMenuDesign.header==3 && mDesignGeometry.title.h) {
        int32_t y=a.y1+mDesignGeometry.list.y-KshimDesignPx(8);
        KshimSymbolLine(event,mMenuStyle.Border,opa,a.x1+p,y,a.x2-p,y,KshimDesignStroke(1),false);
    }
    if(mMenuDesign.kind==KSHIM_DESIGN_GLASS) {
        int32_t inset=KshimDesignPx(mMenuDesign.panel_radius/2);
        KshimSymbolLine(event,mMenuStyle.Muted,(lv_opa_t)((unsigned)opa/2U),
            a.x1+inset,a.y1+1,a.x2-inset,a.y1+1,KshimDesignStroke(1),true);
    }
    if(mMenuDesign.kind==KSHIM_DESIGN_NEON)
        KshimSymbolCorners(event,mMenuStyle.Border,opa,a.x1+1,a.y1+1,width-2,
            lv_obj_get_height(panel)-2,KshimDesignPx(16),KshimDesignStroke(1));
}
static void KshimDesignRowDraw(lv_event_t *event) {
    lv_obj_t *button=lv_event_get_target_obj(event);
    size_t index=(size_t)(uintptr_t)lv_event_get_user_data(event)-1U;
    if(index>=mDesignGeometry.count) return;
    if(mMenuDesign.kind==KSHIM_DESIGN_STAR_CHART) { KshimStarRowDraw(event,index); return; }
    const lv_font_t *font=lv_obj_get_style_text_font(button,LV_PART_MAIN);
    kshim_design_item_geometry_t metrics;
    kshim_design_item_layout(&mMenuDesign,&mDesignGeometry,(unsigned)index,lv_font_get_line_height(font),&metrics);
    lv_area_t a; lv_obj_get_coords(button,&a);
    lv_opa_t opa=lv_obj_get_style_opa_recursive(button,LV_PART_MAIN);
    bool selected=lv_obj_has_state(button,LV_STATE_FOCUSED);
    bool pressed=lv_obj_has_state(button,LV_STATE_PRESSED);
    uint32_t ink=pressed?KshimDesignPressText():selected?mMenuStyle.FocusText:mMenuStyle.Text;
    uint32_t mark=mMenuStyle.FocusBorder;
    if(mMenuDesign.kind==KSHIM_DESIGN_HIGH_CONTRAST || mMenuDesign.kind==KSHIM_DESIGN_TERMINAL)
        mark=ink;
    if(metrics.icon.w) {
        kshim_design_rect_t r=metrics.icon;
        uint32_t surface=mMenuStyle.Modern?mMenuStyle.Navigation:mMenuStyle.Item;
        if(mMenuDesign.kind==KSHIM_DESIGN_CUPERTINO) surface=0xe3e5e8U;
        KshimSymbolMedia(event,mMenuDesign.kind,ink,surface,mMenuStyle.FocusBorder,opa,
            a.x1+r.x,a.y1+r.y,r.w);
    }
    if(mMenuDesign.kind==KSHIM_DESIGN_SOFT_UI) {
        bool inset=selected || pressed;
        uint32_t light=0xffffffU, dark=mMenuStyle.Shadow;
        int32_t pad=KshimDesignPx(8), stroke=KshimDesignStroke(1);
        KshimSymbolLine(event,inset?dark:light,opa,a.x1+pad,a.y1+1,a.x2-pad,a.y1+1,stroke,true);
        KshimSymbolLine(event,inset?dark:light,opa,a.x1+1,a.y1+pad,a.x1+1,a.y2-pad,stroke,true);
        KshimSymbolLine(event,inset?light:dark,opa,a.x1+pad,a.y2-1,a.x2-pad,a.y2-1,stroke,true);
        KshimSymbolLine(event,inset?light:dark,opa,a.x2-1,a.y1+pad,a.x2-1,a.y2-pad,stroke,true);
    }
    int32_t bw=mMenuDesign.kind==KSHIM_DESIGN_HIGH_CONTRAST?KshimDesignPx(2):
        mMenuDesign.kind==KSHIM_DESIGN_CARDS || mMenuDesign.kind==KSHIM_DESIGN_NEON?KshimDesignStroke(1):0;
    if(bw) KshimSymbolRect(event,mMenuDesign.kind==KSHIM_DESIGN_HIGH_CONTRAST?ink:mMenuStyle.Border,
        opa,a.x1,a.y1,lv_obj_get_width(button),lv_obj_get_height(button),mDesignGeometry.compact?0:KshimDesignPx(mMenuDesign.row_radius),bw);
    if(mMenuDesign.kind==KSHIM_DESIGN_IOS_HIG && index+1U<mDesignGeometry.count)
        KshimSymbolLine(event,mMenuStyle.Border,opa,a.x1+KshimDesignPx(12),a.y2,a.x2,a.y2,KshimDesignStroke(1),false);
    KshimNewThemeRowDraw(event,index);
    if(mDesignGeometry.compact) return;
    kshim_design_rect_t r=metrics.mark;
    if(!r.w || !r.h) return;
    int32_t x=a.x1+r.x,y=a.y1+r.y, stroke=kshim_design_max(1,KshimDesignPx(2));
    if(mMenuDesign.mark==KSHIM_MARK_RADIO) {
        KshimSymbolRect(event,selected?mark:mMenuStyle.Muted,opa,x,y,r.w,r.h,r.h,stroke);
        if(selected) KshimSymbolRect(event,mark,opa,x+r.w/4,y+r.h/4,r.w/2,r.h/2,r.h,0);
    } else if(selected) {
        switch(mMenuDesign.mark) {
        case KSHIM_MARK_CHECK: KshimSymbolCheck(event,mark,opa,x,y,r.w,stroke); break;
        case KSHIM_MARK_DOT: {
            int32_t s=kshim_design_min(r.w,KshimDesignPx(6));
            KshimSymbolRect(event,ink,opa,x+(r.w-s)/2,y+(r.h-s)/2,s,s,s,0); break;
        }
        case KSHIM_MARK_CURSOR:
            KshimSymbolLine(event,ink,opa,x,y+r.h/4,x+r.w,y+r.h/2,stroke,false);
            KshimSymbolLine(event,ink,opa,x+r.w,y+r.h/2,x,y+r.h*3/4,stroke,false); break;
        case KSHIM_MARK_BAR:
            KshimSymbolRect(event,mark,opa,x,a.y1+KshimDesignPx(2),KshimDesignPx(3),
                lv_obj_get_height(button)-KshimDesignPx(4),0,0); break;
        case KSHIM_MARK_PILL:
            KshimSymbolRect(event,mark,opa,x,y,KshimDesignPx(3),r.h,KshimDesignPx(2),0); break;
        case KSHIM_MARK_CORNERS:
            if(mMenuDesign.kind==KSHIM_DESIGN_CLOVER) {
                KshimSymbolRect(event,mark,opa,x,y,r.w,r.h,KshimDesignPx(10),stroke);
            } else {
                KshimSymbolCorners(event,mark,opa,a.x1+1,a.y1+1,lv_obj_get_width(button)-2,
                    lv_obj_get_height(button)-2,KshimDesignPx(8),stroke);
            }
            break;
        default: break;
        }
    }
}
static const lv_font_t *KshimDesignLabelFont(const char *text, int32_t width) {
    const lv_font_t *fonts[]={mRowFont,mStatusFont,mInfoFont};
    for (unsigned i=0;i<sizeof(fonts)/sizeof(fonts[0]);++i) {
        lv_point_t size;
        lv_text_get_size(&size,text,fonts[i],0,0,INT32_MAX,LV_TEXT_FLAG_EXPAND);
        if(size.x<=width) return fonts[i];
    }
    return mInfoFont;
}
static int KshimDesignEntry(lv_obj_t *button,size_t index) {
    if(!button || index>=mDesignGeometry.count) return -1;
    lv_obj_t *label=lv_obj_get_child(button,0);
    if(!label) return -1;
    kshim_design_rect_t r=mDesignGeometry.items[index];
    lv_obj_remove_style_all(button);
    lv_obj_set_layout(button,0);
    KshimDesignPlace(button,r);
    lv_obj_set_style_radius(button,mDesignGeometry.compact?0:KshimDesignPx(mMenuDesign.row_radius),0);
    lv_opa_t resting=LV_OPA_COVER, focused=LV_OPA_COVER;
    if(mMenuDesign.kind==KSHIM_DESIGN_MINIMAL || mMenuDesign.kind==KSHIM_DESIGN_CLOVER)
        resting=focused=LV_OPA_TRANSP;
    if(mMenuDesign.kind==KSHIM_DESIGN_CUPERTINO || mMenuDesign.kind==KSHIM_DESIGN_ONE_UI) resting=LV_OPA_TRANSP;
    /* STAR_CHART rows rest clear; only the selected row shows the normal blue. */
    if(mMenuDesign.kind==KSHIM_DESIGN_STAR_CHART) resting=LV_OPA_TRANSP;
    if(mMenuDesign.kind==KSHIM_DESIGN_GLASS && !KshimDesignReduced()) resting=220;
    KshimDesignSolid(button,mMenuStyle.Item,resting,0);
    KshimDesignSolid(button,mMenuStyle.Focus,focused,LV_STATE_FOCUSED);
    KshimDesignSolid(button,KshimDesignPressColor(),LV_OPA_COVER,LV_STATE_PRESSED);
    lv_obj_set_style_text_font(button,mRowFont,0);
    lv_obj_set_style_text_color(button,lv_color_hex(mMenuStyle.Text),0);
    lv_obj_set_style_text_color(button,lv_color_hex(mMenuStyle.FocusText),LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(button,lv_color_hex(KshimDesignPressText()),LV_STATE_PRESSED);
    /* Borders are drawn without changing the content origin. The same label
     * reservations therefore hold for all states, including high contrast. */
    lv_obj_set_style_border_width(button,0,0);
    lv_obj_set_style_pad_all(button,0,0);
    lv_obj_set_style_outline_width(button,0,0);
    lv_obj_set_style_outline_color(button,lv_color_hex(mMenuStyle.FocusBorder),0);
    lv_obj_set_style_outline_opa(button,LV_OPA_COVER,0);
    lv_obj_set_style_outline_pad(button,0,0);
    lv_obj_set_style_outline_width(button,mDesignGeometry.compact?0:KshimDesignPx(mMenuDesign.focus_outline),LV_STATE_FOCUSED);
    /* Surface identifies keyboard selection with its existing left bar, too.
     * STAR_CHART draws its own selection treatment, so no generic outline. */
    lv_obj_set_style_outline_width(button,
        (mDesignGeometry.compact || mMenuDesign.kind==KSHIM_DESIGN_STAR_CHART)?0:
        KshimDesignPx(mMenuDesign.kind==KSHIM_DESIGN_SURFACE?mMenuDesign.focus_outline:2),
        LV_STATE_FOCUSED|LV_STATE_FOCUS_KEY);
    if(!KshimDesignReduced() && !mDesignGeometry.compact) {
        lv_obj_set_style_shadow_width(button,KshimDesignPx(mMenuDesign.row_shadow),0);
        lv_obj_set_style_shadow_color(button,lv_color_hex(mMenuStyle.Modern?mMenuStyle.Shadow:0),0);
        lv_obj_set_style_shadow_opa(button,40,0);
        lv_obj_set_style_shadow_offset_y(button,KshimDesignPx(mMenuDesign.row_shadow/3),0);
        if(mMenuDesign.row_shadow) lv_obj_set_style_shadow_width(button,KshimDesignPx(2),LV_STATE_PRESSED);
        if(mMenuDesign.kind==KSHIM_DESIGN_SOFT_UI) lv_obj_set_style_shadow_width(button,0,LV_STATE_FOCUSED);
        if(mMenuDesign.focus_glow) {
            lv_obj_set_style_shadow_width(button,KshimDesignPx(mMenuDesign.focus_glow),LV_STATE_FOCUSED);
            lv_obj_set_style_shadow_color(button,lv_color_hex(mMenuStyle.FocusBorder),LV_STATE_FOCUSED);
            lv_obj_set_style_shadow_opa(button,64,LV_STATE_FOCUSED);
            lv_obj_set_style_shadow_offset_y(button,0,LV_STATE_FOCUSED);
        }
    }
    kshim_design_item_geometry_t metrics;
    kshim_design_item_layout(&mMenuDesign,&mDesignGeometry,(unsigned)index,lv_font_get_line_height(mRowFont),&metrics);
    if(mMenuDesign.kind==KSHIM_DESIGN_STAR_CHART) {
        const lv_font_t *font=KshimDesignLabelFont(mContext->Entries[index],metrics.label.w);
        kshim_design_item_layout(&mMenuDesign,&mDesignGeometry,(unsigned)index,
                                lv_font_get_line_height(font),&metrics);
        lv_obj_set_style_text_font(label,font,0);
        lv_label_set_text(label,mContext->Entries[index]);
    }
    lv_obj_add_flag(label,LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_remove_flag(label,LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    KshimDesignPlace(label,metrics.label);
    lv_obj_set_style_text_align(label,
        (mMenuDesign.kind==KSHIM_DESIGN_CUPERTINO || mMenuDesign.kind==KSHIM_DESIGN_CLOVER) &&
        (mDesignGeometry.columns>1 || mDesignGeometry.horizontal)?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
    if(metrics.prefix.w) {
        lv_obj_t *prefix=lv_label_create(button);
        if(!prefix) return -1;
        char text[12]; (void)snprintf(text,sizeof text,"%02u",(unsigned)index+1U);
        lv_label_set_text(prefix,text); lv_label_set_long_mode(prefix,LV_LABEL_LONG_CLIP);
        lv_obj_remove_flag(prefix,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(prefix,LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_style_text_font(prefix,mRowFont,0); KshimDesignPlace(prefix,metrics.prefix);
    }
    lv_obj_add_event_cb(button,KshimDesignRowDraw,LV_EVENT_DRAW_POST,(void *)(uintptr_t)(index+1U));
    return 0;
}
static int KshimDesignRelayout(void) {
    if(!mContext || !mFramebuffer || !mPanel || !mTitle || !mStatus || !mList || !mBootButton) return -1;
    if(kshim_design_layout(&mMenuDesign,mContext->LogicalWidth,mContext->LogicalHeight,
        (uint32_t)mContext->EntryCount,KshimDesignScaleOverride(),&mDesignGeometry)!=0) return -2;
    /* Device information, slot help, and settings use the full right panel. */
    if(mMenuDesign.kind==KSHIM_DESIGN_STAR_CHART && !mDesignGeometry.compact &&
       (mInfoState || mBootSlotState || mSettingsState)) {
        mDesignGeometry.illustration=(kshim_design_rect_t){0};
        mDesignGeometry.notice=(kshim_design_rect_t){0};
        mDesignGeometry.status.y=mDesignGeometry.list.y;
        mDesignGeometry.detail.y=mDesignGeometry.status.y+mDesignGeometry.status.h+KshimDesignPx(14);
        int32_t bottom=mDesignGeometry.boot.y-KshimDesignPx(12);
        if(mNotice && lv_label_get_text(mNotice)[0]) {
            lv_point_t size;
            lv_text_get_size(&size,lv_label_get_text(mNotice),mStatusFont,
                lv_obj_get_style_text_letter_space(mNotice,0),
                lv_obj_get_style_text_line_space(mNotice,0),
                mDesignGeometry.detail.w,LV_TEXT_FLAG_NONE);
            mDesignGeometry.notice=(kshim_design_rect_t){
                mDesignGeometry.detail.x,bottom-size.y,mDesignGeometry.detail.w,size.y};
            bottom=mDesignGeometry.notice.y-KshimDesignPx(12);
        }
        mDesignGeometry.detail.h=kshim_design_max(0,bottom-mDesignGeometry.detail.y);
    }
    KshimDesignPlace(mPanel,mDesignGeometry.panel);
    lv_obj_set_style_radius(mPanel,mDesignGeometry.compact?0:KshimDesignPx(mMenuDesign.panel_radius),0);
    lv_obj_set_style_clip_corner(mPanel,true,0);
    lv_obj_set_style_shadow_width(mPanel,KshimDesignReduced() || mDesignGeometry.compact?0:KshimDesignPx(mMenuDesign.panel_shadow),0);
    lv_obj_set_style_shadow_offset_y(mPanel,KshimDesignReduced()?0:KshimDesignPx(mMenuDesign.panel_shadow/4),0);
    int32_t panel_outline=mMenuDesign.kind==KSHIM_DESIGN_HIGH_CONTRAST?2:
        mMenuDesign.kind==KSHIM_DESIGN_CLASSIC || mMenuDesign.kind==KSHIM_DESIGN_TERMINAL || mMenuDesign.kind==KSHIM_DESIGN_GLASS?1:0;
    lv_obj_set_style_outline_width(mPanel,KshimDesignStroke((uint32_t)panel_outline),0);
    KshimDesignPlace(mTitle,mDesignGeometry.title);
    KshimDesignPlace(mStatus,mDesignGeometry.status);
    KshimDesignPlace(mList,mDesignGeometry.list);
    KshimDesignPlace(mBootButton,mDesignGeometry.boot);
    lv_obj_set_style_text_font(mStatus,mDesignGeometry.split?(mHelp?mRowFont:mTitleFont):mStatusFont,0);
    if(mHelp) lv_obj_set_style_text_color(mStatus,KshimTextColor(),0);
    KshimDesignPlace(mNotice,mDesignGeometry.notice);
    KshimDesignPlace(mHelp,mDesignGeometry.footer);
    KshimDesignPlace(mCredit,mDesignGeometry.credit);
    KshimDesignPlace(mLanguage,mDesignGeometry.language);
    lv_obj_set_layout(mList,0);
    lv_obj_set_style_pad_all(mList,0,0);
    lv_obj_set_style_pad_bottom(mList,mDesignGeometry.gutter,0);
    lv_obj_set_style_pad_right(mList,mDesignGeometry.gutter,0);
    lv_obj_set_style_border_width(mList,0,0);
    lv_obj_set_style_radius(mList,0,0);
    lv_obj_set_style_clip_corner(mList,false,0);
    lv_obj_set_scroll_dir(mList,mDesignGeometry.horizontal?LV_DIR_HOR:LV_DIR_VER);
    KshimDesignSolid(mList,mMenuStyle.Item,LV_OPA_TRANSP,0);
    if(mDesignGeometry.split && mMenuDesign.kind!=KSHIM_DESIGN_STAR_CHART)
        KshimDesignSolid(mList,mMenuStyle.Navigation,LV_OPA_COVER,0);
    if(mMenuDesign.kind==KSHIM_DESIGN_IOS_HIG && !mDesignGeometry.compact) {
        KshimDesignSolid(mList,mMenuStyle.Item,LV_OPA_COVER,0);
        lv_obj_set_style_radius(mList,KshimDesignPx(12),0);
        lv_obj_set_style_clip_corner(mList,true,0);
    }
    int32_t radius=mMenuDesign.boot_radius==255?mDesignGeometry.boot.h/2:KshimDesignPx(mMenuDesign.boot_radius);
    radius=kshim_design_min(radius,mDesignGeometry.boot.h/2);
    lv_obj_set_style_radius(mBootButton,mDesignGeometry.compact?0:radius,0);
    if(mDetail) KshimDesignPlace(mDetail,mDesignGeometry.split?mDesignGeometry.detail:(kshim_design_rect_t){0});
    if(mMenuDesign.kind==KSHIM_DESIGN_STAR_CHART) {
        lv_obj_set_style_text_font(mStatus,mRowFont,0);
        lv_obj_set_style_text_color(mStatus,lv_color_hex(mMenuStyle.FocusBorder),0);
        if(mDetail) {
            lv_obj_set_style_text_font(mDetail,(mInfoState || mBootSlotState || mSettingsState)?mInfoFont:mStatusFont,0);
            lv_obj_set_style_text_color(mDetail,lv_color_hex(mMenuStyle.Text),0);
        }
        if(mCredit) lv_obj_set_style_text_font(mCredit,mStatusFont,0);
        if(mLanguage) lv_obj_set_style_text_font(mLanguage,mStatusFont,0);
        lv_obj_set_style_text_align(mCredit,LV_TEXT_ALIGN_RIGHT,0);
        if(mHelp) {
            lv_obj_set_style_text_font(mHelp,mStatusFont,0);
            lv_obj_set_style_text_color(mHelp,lv_color_hex(mMenuStyle.Muted),0);
        }
    }
    KshimNewThemeRelayout();
    lv_obj_update_layout(lv_screen_active());
    return 0;
}
static void KshimDesignDecorate(lv_obj_t *screen) {
    if(!mMenuDesign.enabled) return;
    lv_label_set_text(mTitle,KSHIM_MENU_TEXT_TITLE(mMenuDesign.title));
    lv_label_set_long_mode(mTitle,LV_LABEL_LONG_DOT);
    bool centered=mMenuDesign.kind==KSHIM_DESIGN_CUPERTINO || mMenuDesign.kind==KSHIM_DESIGN_CLOVER;
    lv_obj_set_style_text_align(mTitle,centered?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
    lv_obj_set_style_text_align(mStatus,centered?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
    uint32_t title=mMenuStyle.Text;
    if(mMenuDesign.header==1) title=mMenuStyle.FocusText;
    if(mMenuDesign.header==2) title=mMenuStyle.Screen==0x121212U?mMenuStyle.Text:mMenuStyle.BootText;
    lv_obj_set_style_text_color(mTitle,lv_color_hex(title),0);
    if(mMenuDesign.header==1 || mMenuDesign.header==2) lv_obj_set_style_text_color(mStatus,lv_color_hex(title),0);
    KshimDesignSolid(screen,mMenuStyle.Screen,LV_OPA_COVER,0);
    if(mMenuStyle.Modern && mMenuStyle.ScreenEnd!=mMenuStyle.Screen) {
        lv_obj_set_style_bg_grad_color(screen,lv_color_hex(mMenuStyle.ScreenEnd),0);
        lv_obj_set_style_bg_grad_dir(screen,LV_GRAD_DIR_VER,0);
    }
    bool background=KshimDesignBackground(mBackground);
    lv_opa_t panel_opa=KshimDesignBarePanel()?LV_OPA_TRANSP:LV_OPA_COVER;
    if(mMenuDesign.kind==KSHIM_DESIGN_GLASS && background) panel_opa=220;
    KshimDesignSolid(mPanel,mMenuStyle.Panel,panel_opa,0);
    lv_obj_set_style_border_width(mPanel,0,0);
    int32_t outline=mMenuDesign.kind==KSHIM_DESIGN_HIGH_CONTRAST?2:
        mMenuDesign.kind==KSHIM_DESIGN_CLASSIC || mMenuDesign.kind==KSHIM_DESIGN_TERMINAL || mMenuDesign.kind==KSHIM_DESIGN_GLASS?1:0;
    lv_obj_set_style_outline_width(mPanel,outline,0);
    lv_obj_set_style_outline_color(mPanel,lv_color_hex(mMenuStyle.Border),0);
    lv_obj_set_style_shadow_width(mPanel,KshimDesignReduced()?0:(int32_t)mMenuDesign.panel_shadow,0);
    lv_obj_set_style_shadow_color(mPanel,lv_color_hex(0),0);
    lv_obj_set_style_shadow_opa(mPanel,32,0);
    lv_obj_set_style_shadow_offset_y(mPanel,mMenuDesign.panel_shadow/4,0);
    lv_obj_add_event_cb(mPanel,KshimDesignPanelDraw,LV_EVENT_DRAW_MAIN,NULL);
    lv_obj_set_style_shadow_width(mBootButton,0,0);
    lv_obj_set_style_shadow_width(mBootButton,0,LV_STATE_PRESSED);
    lv_obj_t *label=lv_obj_get_child(mBootButton,0);
    if(label) lv_label_set_text(label,KSHIM_MENU_TEXT_ACTION(mMenuDesign.action));
    lv_obj_set_style_outline_color(mBootButton,lv_color_hex(mMenuStyle.FocusBorder),0);
    lv_obj_set_style_outline_width(mBootButton,
        mMenuDesign.kind==KSHIM_DESIGN_STAR_CHART?0:2,LV_STATE_FOCUSED|LV_STATE_FOCUS_KEY);
    if(mMenuDesign.kind==KSHIM_DESIGN_STAR_CHART)
        lv_obj_add_event_cb(mBootButton,KshimStarBootDraw,LV_EVENT_DRAW_POST,NULL);
}
#endif
