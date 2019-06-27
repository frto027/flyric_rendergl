#include "flyric_rendergl.h"
#include "glenv.h"

frp_size frg_fontsize = 40;

frp_size frg_scr_width = 800;
frp_size frg_scr_height = 600;

const char * frg_default_font_path;

void frg_startup(const char * default_font_path){
    frg_default_font_path = default_font_path;
}
void frg_shutdown(){}

void frg_fontsize_set(frp_size fontsize){
    frg_fontsize = fontsize;
}

void frg_screensize_set(frp_size width,frp_size height){
    frg_scr_width = width;
    frg_scr_height = height;
}
int frg_loadlyric(FT_Library lib,FRPFile * file){
    FT_Face face;
    if(!FT_New_Face(lib,frg_default_font_path,0,&face))
        return LOAD_ERROR_FONT;
    
    if(!FT_Set_Pixel_Sizes(face,0,frg_fontsize)){
        FT_Done_Face(face);
        return LOAD_ERROR_FONT;
    }

    

}
void frg_renderline(FRPLine * line,frp_time time){

    /* debug only,never do this please... */
    float tim = time / 10000.;
    tim -= 1;
    float yoff = -1;
    yoff += line->linenumber / 40.;
    glBegin(GL_TRIANGLE_FAN);
    glColor4b(100,100,0,100);
    glVertex2f(tim,yoff);
    glVertex2f(tim,yoff + 0.2f);
    glVertex2f(tim+0.5f,yoff + 0.2f);
    glVertex2f(tim+0.5f,yoff);
    glEnd();
}
void frg_unloadlyrc(){}