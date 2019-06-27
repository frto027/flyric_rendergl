#ifndef FRENDERGL_H
#define FRENDERGL_H

#include "fparser_public.h"

#include "ft2build.h"
#include FT_FREETYPE_H

extern void frg_startup(const char *default_font_path);
extern void frg_shutdown();

extern void frg_fontsize_set(frp_size fontsize);
extern void frg_screensize_set(frp_size width,frp_size height);
//return 0 if success
#define LOAD_ERROR_NONE 0
#define LOAD_ERROR_FONT -1
extern int frg_loadlyric(FT_Library lib,FRPFile * file);
extern void frg_renderline(FRPLine * line,frp_time time);
extern void frg_unloadlyrc();

#endif