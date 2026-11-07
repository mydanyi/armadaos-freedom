#!/usr/bin/env python3
"""Compile actual DPU split/rotation functions with a host-side DRM state fixture.

The fixture removes device/MMIO/clock dependencies. Rectangle transformation and
clipping are taken from the kernel under test, not reimplemented here.
Generated fixture files are written below --work.
"""
import argparse
import pathlib
import subprocess
import sys

def function(text, name):
    import re
    match = re.search(r"^(?:static\s+)?(?:inline\s+)?(?:bool|u32|u64|int|void|uint32_t)\s+" +
                      re.escape(name) + r"\s*\(", text, re.M)
    if not match:
        raise ValueError("missing function: " + name)
    start = text.find("{", match.end())
    depth = 1
    i = start + 1
    while depth:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    return text[match.start():i]

PREFIX = r"""
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
typedef uint32_t u32;
typedef uint64_t u64;
#define BIT(n) (1UL << (n))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define DIV_ROUND_UP_ULL(a,b) (((a)+(b)-1)/(b))
#define DIV_ROUND_DOWN_ULL(a,b) ((a)/(b))
#define mul_u32_u32(a,b) ((u64)(u32)(a)*(u32)(b))
#define DRM_MODE_ROTATE_0 BIT(0)
#define DRM_MODE_ROTATE_90 BIT(1)
#define DRM_MODE_ROTATE_180 BIT(2)
#define DRM_MODE_ROTATE_270 BIT(3)
#define DRM_MODE_ROTATE_MASK 15
#define DRM_MODE_REFLECT_X BIT(4)
#define DRM_MODE_REFLECT_Y BIT(5)
#define DPU_SSPP_INLINE_ROTATION 2
#define DPU_SSPP_SMART_DMA_V1 3
#define DPU_SSPP_SMART_DMA_V2 4
#define PIPES_PER_STAGE 2
#define PIPES_PER_PLANE 4
#define DPU_DEBUG_PLANE(pdpu,...) ((void)(pdpu))
#define DPU_ERROR(...) ((void)0)
#define test_bit(n,p) (!!(*(p) & BIT(n)))
#define MSM_FORMAT_IS_YUV(f) ((f)->is_yuv)
#define MSM_FORMAT_IS_UBWC(f) ((f)->ubwc)
struct drm_rect { int x1, y1, x2, y2; };
static int drm_rect_width(const struct drm_rect *r) { return r->x2-r->x1; }
static int drm_rect_height(const struct drm_rect *r) { return r->y2-r->y1; }
static bool drm_rect_visible(const struct drm_rect *r)
{ return drm_rect_width(r)>0 && drm_rect_height(r)>0; }
static bool drm_rotation_90_or_270(unsigned int r)
{ return r & (DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_270); }
static void drm_rect_fp_to_int(struct drm_rect *d, const struct drm_rect *s)
{ *d=(struct drm_rect){s->x1>>16,s->y1>>16,s->x2>>16,s->y2>>16}; }
struct drm_framebuffer { int width, height; };
struct drm_plane { int unused; };
struct drm_plane_state {
 struct drm_rect src, dst;
 struct drm_framebuffer *fb;
 unsigned int rotation;
};
struct drm_display_mode { int hdisplay, vdisplay, vtotal, fps; };
static int drm_mode_vrefresh(const struct drm_display_mode *m) { return m->fps; }
#define do_div(n,d) ((n)/=(d))
struct drm_crtc_state { struct drm_display_mode adjusted_mode; unsigned int num_lm; };
struct dpu_sw_pipe_cfg { struct drm_rect src_rect, dst_rect; unsigned int rotation; int mixer_x; bool qseed3_v11_valid; };
struct dpu_plane_state {
 struct drm_plane_state base;
 struct dpu_sw_pipe_cfg pipe_cfg[PIPES_PER_PLANE];
};
struct dpu_rotation_cfg {
 u32 rot_maxheight, rot_max_downscale_numerator, rot_max_downscale_denominator;
 size_t rot_num_formats;
 const u32 *rot_format_list;
};
struct dpu_sspp_sub_blks { const struct dpu_rotation_cfg *rotation_cfg; };
struct dpu_sspp_cfg { int id; unsigned long features; const struct dpu_sspp_sub_blks *sblk; };
struct dpu_caps { uint32_t max_linewidth; };
struct dpu_mdss_cfg { const struct dpu_caps *caps; int sspp_count; const struct dpu_sspp_cfg *sspp; };
struct dpu_plane { struct drm_plane base; int pipe; const struct dpu_mdss_cfg *catalog; };
struct dpu_kms { struct { u64 max_core_clk_rate; } perf; };
struct dpu_hw_sspp { const struct dpu_sspp_cfg *cap; };
struct dpu_sw_pipe { struct dpu_hw_sspp *sspp; };
struct msm_format { bool is_yuv, ubwc; u32 pixel_format; };
static bool dpu_find_format(u32 fmt, const u32 *formats, size_t count)
{ for(size_t i=0;i<count;i++) if(formats[i]==fmt) return true; return false; }
static struct dpu_kms kms = {{1000000000}};
static bool dpu_use_virtual_planes;
#define to_dpu_plane(p) ((struct dpu_plane *)(p))
#define to_dpu_plane_state(p) ((struct dpu_plane_state *)(p))
static struct dpu_kms *_dpu_plane_get_kms(struct drm_plane *p)
{ (void)p; return &kms; }
static unsigned int dpu_crtc_get_num_lm(const struct drm_crtc_state *s)
{ return s->num_lm; }
"""

TESTS = r"""
static int failures, cases;
#define CHECK(c) do { if (!(c)) { \
 fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#c); failures++; } } while(0)

static void split_case(unsigned int rotation, int height, bool virt,
                       bool support, int wanted_pipes, int wanted_error)
{
 const struct dpu_rotation_cfg rot={.rot_maxheight=1088};
 const struct dpu_sspp_sub_blks sblk={&rot};
 const struct dpu_caps caps={8192};
 struct dpu_sspp_cfg cfg[]={
  {1, support ? BIT(DPU_SSPP_INLINE_ROTATION) : 0, &sblk},
  {2, 0, &sblk},
 };
 struct dpu_mdss_cfg cat={&caps,2,cfg};
 struct dpu_plane plane={{0},1,&cat};
 struct drm_framebuffer fb={3040,height};
 struct dpu_plane_state state={0};
 struct drm_crtc_state crtc={{.hdisplay=height,.vdisplay=3040},2};
 int rotated=drm_rotation_90_or_270(rotation);
 int rc, n, total_area=0;
 cases++;
 if (!rotated) crtc.adjusted_mode=(struct drm_display_mode){.hdisplay=3040,.vdisplay=height};
 state.base.fb=&fb;
 state.base.rotation=rotation;
 state.base.src=(struct drm_rect){0,0,3040<<16,height<<16};
 state.base.dst=(struct drm_rect){0,0,crtc.adjusted_mode.hdisplay,crtc.adjusted_mode.vdisplay};
 /* An old rotated state must not poison a later unrotated allocation. */
 state.pipe_cfg[0].rotation=DRM_MODE_ROTATE_90;
 state.pipe_cfg[1].rotation=DRM_MODE_ROTATE_90;
 dpu_use_virtual_planes=virt;
 rc=dpu_plane_split(&plane.base,&state.base,&crtc);
 if (rc!=wanted_error) {
  fprintf(stderr,"rotation=%u height=%d virt=%d support=%d rc=%d expected=%d\n",
          rotation,height,virt,support,rc,wanted_error);
  failures++; return;
 }
 if (rc) return;
 n=!!drm_rect_width(&state.pipe_cfg[0].src_rect)+
   !!drm_rect_width(&state.pipe_cfg[1].src_rect);
 CHECK(n==wanted_pipes);
 for(int i=0;i<n;i++) {
  struct dpu_sw_pipe_cfg *p=&state.pipe_cfg[i];
  struct drm_rect transformed=p->src_rect;
  if (rotated && support) CHECK(drm_rect_height(&p->src_rect)<=1088);
  CHECK(p->rotation==rotation);
  CHECK(p->src_rect.x1>=0 && p->src_rect.y1>=0);
  CHECK(p->src_rect.x2<=3040 && p->src_rect.y2<=height);
  total_area+=drm_rect_width(&p->src_rect)*drm_rect_height(&p->src_rect);
  drm_rect_rotate(&transformed,3040,height,rotation);
  CHECK(!memcmp(&transformed,&p->dst_rect,sizeof(transformed)));
 }
 CHECK(total_area==3040*height);
 if(n==2) {
  CHECK(state.pipe_cfg[0].dst_rect.x2==state.pipe_cfg[1].dst_rect.x1);
  CHECK(state.pipe_cfg[0].dst_rect.x1==0);
  CHECK(state.pipe_cfg[1].dst_rect.x2==crtc.adjusted_mode.hdisplay);
 }
}

static void multirect_case(unsigned int rotation, bool expected)
{
 struct dpu_sw_pipe_cfg cfg={{0,0,512,512},{0,0,512,512},rotation,0,false};
 struct msm_format fmt={.is_yuv=false};
 cases++;
 CHECK(!!dpu_plane_is_multirect_capable(NULL,&cfg,&fmt)==expected);
}

static void inline_case(int width, int height, int dest_height, bool limited,
                        bool ubwc, u32 format, int expected)
{
 const u32 formats[]={1,2};
 const struct dpu_rotation_cfg rot={1088,limited?11:0,limited?5:0,2,formats};
 const struct dpu_sspp_sub_blks sblk={&rot};
 const struct dpu_sspp_cfg caps={1,BIT(DPU_SSPP_INLINE_ROTATION),&sblk};
 struct dpu_hw_sspp sspp={&caps};
 struct dpu_sw_pipe pipe={&sspp};
 struct dpu_plane plane={0};
 struct drm_rect src={0,0,width,height};
 struct drm_rect dst={0,0,height,dest_height};
 struct msm_format fmt={false,ubwc,format};
 cases++;
#ifdef HAS_ROTATION_DOWNSCALE_LIMIT
 CHECK(dpu_plane_check_inline_rotation(&plane,&pipe,src,dst,&fmt)==expected);
#else
 (void)dst;
 CHECK(dpu_plane_check_inline_rotation(&plane,&pipe,src,&fmt)==expected);
#endif
}

static void clock_case(unsigned int rotation, int src_w, int src_h,
                       int dst_w, int dst_h, u64 expected)
{
 struct drm_display_mode mode={.vtotal=3120,.fps=165};
 struct dpu_sw_pipe_cfg cfg={{0,0,src_w,src_h},{0,0,dst_w,dst_h},rotation,0,false};
 cases++;
 CHECK(_dpu_plane_calc_clk(&mode,&cfg)==expected);
}

int main(void)
{
 unsigned int turns[]={DRM_MODE_ROTATE_90,DRM_MODE_ROTATE_270,
                       DRM_MODE_ROTATE_90|DRM_MODE_REFLECT_X,
                       DRM_MODE_ROTATE_270|DRM_MODE_REFLECT_Y};
 for(unsigned int i=0;i<ARRAY_SIZE(turns);i++) {
  split_case(turns[i],1904,true,true,2,0);
  split_case(turns[i],1088,true,true,1,0);
  split_case(turns[i],1089,true,true,2,0);
  split_case(turns[i],2176,true,true,2,0);
  split_case(turns[i],2177,true,true,0,-E2BIG);
  split_case(turns[i],1904,false,true,2,0);
  /* Unsupported catalog: splitting does not invent rotation capability.
     The existing per-pipe atomic validation remains responsible for rejection. */
  split_case(turns[i],1904,true,false,1,0);
 }
 split_case(DRM_MODE_ROTATE_0,1904,true,true,1,0);
 split_case(DRM_MODE_ROTATE_180,1904,true,true,1,0);
 multirect_case(DRM_MODE_ROTATE_0,true);
 multirect_case(DRM_MODE_ROTATE_180,true);
 multirect_case(DRM_MODE_ROTATE_90,false);
 multirect_case(DRM_MODE_ROTATE_270,false);
 multirect_case(DRM_MODE_ROTATE_270|DRM_MODE_REFLECT_X,false);
 inline_case(2200,952,1000,true,true,1,0);
 inline_case(2201,952,1000,true,true,1,-ERANGE);
 inline_case(4000,952,1000,true,true,1,-ERANGE);
 inline_case(3040,952,3040,true,true,2,0);
 inline_case(1000,952,3040,true,true,2,0);
 inline_case(4000,952,1000,false,true,1,0);
 inline_case(3040,1089,3040,true,true,1,-EINVAL);
 inline_case(3040,952,3040,true,false,1,-EINVAL);
 inline_case(3040,952,3040,true,true,3,-EINVAL);
 clock_case(DRM_MODE_ROTATE_90,3040,952,952,3040,490089600);
 clock_case(DRM_MODE_ROTATE_270,3040,952,952,1520,980179200);
 clock_case(DRM_MODE_ROTATE_0,952,3040,952,1520,980179200);
 clock_case(DRM_MODE_ROTATE_180,952,3040,952,3040,490089600);
 clock_case(DRM_MODE_ROTATE_90,1520,952,952,3040,490089600);
 printf("%d cases, %d failed assertions\n",cases,failures);
 return failures ? 1 : 0;
}
"""

def main():
    p = argparse.ArgumentParser()
    p.add_argument("source", type=pathlib.Path)
    p.add_argument("--work", required=True, type=pathlib.Path)
    a = p.parse_args()
    a.work.mkdir(parents=True, exist_ok=True)
    plane = (a.source/"drivers/gpu/drm/msm/disp/dpu1/dpu_plane.c").read_text()
    rect = (a.source/"drivers/gpu/drm/drm_rect.c").read_text()
    parts = [PREFIX]
    parts.append(function(plane,"_dpu_plane_calc_clk"))
    if "rot_max_downscale_numerator" in plane:
        parts.append("#define HAS_ROTATION_DOWNSCALE_LIMIT 1")
    parts.append(function(plane,"dpu_plane_check_inline_rotation"))
    for n in ("clip_scaled", "drm_rect_clip_scaled",
              "drm_rect_rotate", "drm_rect_rotate_inv"):
        parts.append(function(rect,n))
    if "static u32 dpu_plane_get_max_linewidth(" in plane:
        parts.append(function(plane,"dpu_plane_get_max_linewidth"))
    for n in ("dpu_plane_split", "dpu_plane_is_multirect_capable"):
        parts.append(function(plane,n))
    parts.append(TESTS)
    src=a.work/"rotation_fixture.c"
    exe=a.work/"rotation_fixture"
    src.write_text("\n\n".join(parts))
    subprocess.run(["gcc","-std=gnu11","-Wall","-Wextra","-Werror","-Wno-sign-compare",
                    "-O1","-g","-fsanitize=undefined,address",
                    str(src),"-o",str(exe)],check=True)
    return subprocess.run([str(exe)]).returncode

if __name__=="__main__":
    sys.exit(main())
