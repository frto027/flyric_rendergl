#include "flyric_rendergl.h"
#include "glenv.h"

frp_size frg_fontsize = 40;

frp_size frg_scr_width = 800;
frp_size frg_scr_height = 600;

const char * frg_default_font_path;


FRPFile * frg_frpfile = NULL;
//=================info of uniform in shader=================
struct FRGWordTextureInfo{
    float tex_x,tex_y;
    float tex_width_half,tex_height_half;
}FRGWordTextureLocales[4096];//for each word(texture)
struct FRGWordInfo{
    float center_x_off,center_y_off;//文字中心距离property_id对应的Node的偏移
    //float half_w,half_h;//文字半宽半高 你为什么不问问word_id对应的FRGWordTextureInfo呢？
    int property_id;
    int word_id;
}*FRGWords = NULL;//for each word
struct FRGNodeProperty{
    float center_x,center_y;
    float colorR,colorG,colorB,colorA;
    float scaleX,scaleY;
}*FRGNodeProperties = NULL;//for each node

int frg_max_word_count = 0;

// ================texture locale of each word=========================
struct FRGWordinfo{
    int word_id;
};

struct FRGNode{
    struct FRGWordinfo *word_info;
    int word_count;//texture locale for each word
};

struct FRGline{
    struct FRGNode *nodes;
    frp_size node_count;//for free
}*FRGLines = NULL;

void frg_freelines(){
    if(!FRGLines)
        return;
    if(!frg_frpfile)
        return;
    FRPSeg * seg = frp_getseg(frg_frpfile,"flyc");
    if(!seg)
        return;//can not free
    for(frp_size i = 0;i< seg->flyc.linenumber_all;i++){
        //free line
        for(int j = 0;j<FRGLines[i].node_count;j++){
            //free nodes
            frpfree(FRGLines[i].nodes[j].word_info);
        }
        frpfree(FRGLines[i].nodes);
    }
    frpfree(FRGLines);
    FRGLines = NULL;
}


void frg_startup(const char * default_font_path){
    frg_default_font_path = default_font_path;
    //TODO:init shader program
}
void frg_shutdown(){
    frg_freelines();
}

void frg_fontsize_set(frp_size fontsize){
    frg_fontsize = fontsize;
}

void frg_screensize_set(frp_size width,frp_size height){
    frg_scr_width = width;
    frg_scr_height = height;
}
int frg_loadlyric(FT_Library lib,FRPFile * file){
    if(!file)
        return FRG_LOAD_ERROR_INVALID_FILE;
    FT_Face face;
    if(!FT_New_Face(lib,frg_default_font_path,0,&face))
        return FRG_LOAD_ERROR_FONT;
    
    if(!FT_Set_Pixel_Sizes(face,0,frg_fontsize)){
        FT_Done_Face(face);
        return FRG_LOAD_ERROR_FONT;
    }

    frg_frpfile = file;

    FRPSeg * flycseg = frp_getseg(frg_frpfile,"flyc");
    if(!flycseg){
        FT_Done_Face(face);
        return 0;//no lines
    }

    FRGLines = frpmalloc(sizeof(struct FRGline) * flycseg->flyc.linenumber_all);
    
    for(FRPLine * line = flycseg->flyc.lines;line;line = line->next){
        int node_count = 0;
        for(FRPNode * node = line->node;node;node = node->next)
            node_count++;
        FRGLines[line->linenumber].node_count = node_count;
        struct FRGNode *gnode = FRGLines[line->linenumber].nodes = frpmalloc(sizeof(struct FRGNode) * node_count);
        node_count = 0;
        for(FRPNode * node = line->node;node;node = node->next){
            struct FRGWordinfo *wordinfo = NULL;
            int word_count = 0;
            //do something with word info
            //TODO
            //word_info = ???
            //word_ocunt = ???
            //calculate texture locale
            gnode[node_count].word_info = wordinfo;
            gnode[node_count].word_count = word_count;
            node_count++;
        }
    }
    //TODO:init texture locale

    //TODO:init property


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
void frg_unloadlyrc(){
    frg_freelines();
}