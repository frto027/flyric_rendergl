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
}*FRGWordTextureLocales = NULL;//for each word(texture)
struct FRGWordInfo{
    float hcenter_x_off,hcenter_y_off;//文字水平布局时候中心距离property_id对应的Node的偏移
    float vcenter_x_off,vcenter_y_off;//文字垂直布局时候中心巨鹿propryty_id对应的Node的偏移
    //float half_w,half_h;//文字半宽半高 你为什么不问问word_id对应的FRGWordTextureInfo呢？
    int property_id;
    int word_id;
}*FRGWords = NULL;//for each word
struct FRGNodeProperty{
    float hcenter_x,hcenter_y;
    float vcenter_x,vcenter_y;
    float colorR,colorG,colorB,colorA;
    float scaleX,scaleY;
    //others
}*FRGNodeProperties = NULL;//for each node

int frg_max_word_count = 0;

// ================texture locale of each word=========================

struct FRGline{
    int start_word;//从哪个字开始渲染
    int word_count;//渲染几个字
}*FRGLines = NULL;

void frg_freelines(){
    if(!FRGLines)
        return;
    if(!frg_frpfile)
        return;
    FRPSeg * seg = frp_getseg(frg_frpfile,"flyc");
    if(!seg)
        return;//can not free
    if(FRGLines){
        frpfree(FRGLines);
        FRGLines = NULL;
    }
}
//===================utf8 to unicode============
FT_ULong frg_utf8_to_unicode(FT_Bytes utfch,int *skip){
    //ascii 0xxxxxxx
    if((utfch[0] & (1<<7)) == 0){
        if(skip)
            *skip = 1;
        return utfch[0];
    }
    //not found 10xxxxxx
    if((utfch[0] & (1<<6)) == 0){
        //not found
        if(skip)
            *skip = 1;
        return 0;
    }

    //110xxxxx
    if((utfch[0] & (1<<5))==0){
        if(skip)
            *skip = 2;
        return ((((FT_ULong)utfch[0])&0x1F)<<6)|
                ((((FT_ULong)utfch[1])&0x3F));
    }
    //1110xxxx
    if((utfch[0] & (1<<4))==0){
        if(skip)
            *skip = 3;
        return ((((FT_ULong)utfch[0])&0x0F)<<(6+6))|
                ((((FT_ULong)utfch[1])&0x3F)<<6)|
                ((((FT_ULong)utfch[2])&0x3F));
    }
    //11110xxx
    if((utfch[0] & (1<<3))==0){
        if(skip)
            *skip = 4;
        return ((((FT_ULong)utfch[0])&0x07)<<(6+6+6))|
                ((((FT_ULong)utfch[1])&0x3F)<<(6+6))|
                ((((FT_ULong)utfch[2])&0x3F)<<6)|
                (((FT_ULong)utfch[3])&0x3F);
    }
    //111110xx
    if((utfch[0] & (1<<2))==0){
        if(skip)
            *skip = 5;
        return ((((FT_ULong)utfch[0])&0x03)<<(6+6+6+6))|
                ((((FT_ULong)utfch[1])&0x3F)<<(6+6+6))|
                ((((FT_ULong)utfch[2])&0x3F)<<(6+6))|
                ((((FT_ULong)utfch[3])&0x3F)<<(6))|
                (((FT_ULong)utfch[4])&0x3F);
    }
    //1111110x
    if((utfch[0] & (1<<1))==0){
        if(skip)
            *skip = 6;
        return ((((FT_ULong)utfch[0])&0x01)<<(6+6+6+6+6))|
                ((((FT_ULong)utfch[1])&0x3F)<<(6+6+6+6))|
                ((((FT_ULong)utfch[2])&0x3F)<<(6+6+6))|
                ((((FT_ULong)utfch[3])&0x3F)<<(6+6))|
                ((((FT_ULong)utfch[4])&0x3F)<<6)|
                (((FT_ULong)utfch[5])&0x3F);
    }
    //not found
    if(skip)
       *skip = 1;
    return 0;
}
int frg_clear_index_of_word(){

}
int frg_index_of_word(FT_UInt word,int * isnew){
    static int last = 0;
    if(isnew)
        *isnew = 1;
    //TODO this is a hash map,defferent font should use defferent index!
    return last++;
}
void frg_change_font(FT_Library lib,FT_Face * face,const char * fontname,int len){
    if(fontname && len < 0){
        len = 0;
        while(fontname[len])
            len++;
    }
    if(len == 0)
        fontname = NULL;
    //TODO change font



    //load default font
    if(fontname == NULL){
        *face = NULL;
    }
    if(*face == NULL)
        *face = FT_New_Face(lib,frg_default_font_path,0,face);
}
void frg_change_font_frp_str(FT_Library lib,FT_Face * face,frp_str font){
    if(!frg_frpfile)
        return;
    if(font.len)
        frg_change_font(lib,face,frg_frpfile->textpool + font.beg,font.len);
}

//==================programs=================
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

    frg_change_font(lib,&face,NULL,0);
    if(!face)
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

    frp_size pid_text = frp_play_get_property_id(frg_frpfile,"Text");
    frp_size pid_font = frp_play_get_property_id(frg_frpfile,"Font");

    FRGLines = frpmalloc(sizeof(struct FRGline) * flycseg->flyc.linenumber_all);
    
    frp_size wordcount = 0;
    frp_size texture_count = 0;
    /* font kerning */
    FT_Bool use_kerning = 0;

    frp_size texture_x = 0,texture_y = 0;
    frp_size texture_next_tail = 0;

    frp_size max_node_count = 0;
    /* get the wordcount,max_node_count and texture size*/
    frg_clear_index_of_word();
    for(FRPLine * line = flycseg->flyc.lines;line;line = line->next){

        /* change font for current line */
        frg_change_font_frp_str(lib,&face,frp_play_property_string_value(line->values,pid_font));
        //update use kerning here please
        frp_size node_count = 0;
        for(FRPNode * node = line->node;node;node = node->next){
            frp_str s = frp_play_property_string_value(node->values,pid_text);
            while(s.len > 0){
                int skip;
                FT_UInt curr = FT_Get_Char_Index(face,frg_utf8_to_unicode(file->textpool,&skip));
                s.beg += skip;
                s.len -= skip;

                int isnew;
                frg_index_of_word(curr,&isnew);
                if(isnew){
                    texture_count++;
                    //add texture locale
                    FT_Load_Glyph(face,curr,FT_LOAD_DEFAULT);
                    FT_Bitmap * bitmap = &(face->glyph->bitmap);
                    //check return or not
                    if(texture_x + bitmap->width >= FRG_TEXTURE_MAX_WIDTH){
                        texture_x = 0;
                        texture_y = texture_next_tail;
                    }
                    //update texture tail
                    if(texture_y + bitmap->rows > texture_next_tail)
                        texture_next_tail = texture_y + bitmap->rows;
                    //now texture_x,texture_y is the locale of word[wordcount]'s texture
                    texture_x += bitmap->width;
                }
                wordcount++;
            }
            node_count++;
        }

        if(max_node_count < node_count)
            max_node_count = node_count;
    }

    //malloc
    if(FRGWords)
        frpfree(FRGWords);
    FRGWords = malloc(sizeof(struct FRGWordInfo) * wordcount);
    if(FRGWordTextureLocales)
        frpfree(FRGWordTextureLocales);
    FRGWordTextureLocales = malloc(sizeof(struct FRGWordTextureInfo) * texture_count);
    if(FRGNodeProperties)
        frpfree(FRGNodeProperties);
    FRGNodeProperties = malloc(sizeof(struct FRGNodeProperty) * max_node_count);

    frp_size mxsize = 1;
    while(mxsize < texture_next_tail)
        mxsize <<=1;
    glTexStorage2D(GL_TEXTURE_2D,0,GL_R8,FRG_TEXTURE_MAX_WIDTH,mxsize);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    //TODO:init texture locale,word id(locale),property id
    //上面和下面的坐标计算必须一致
    frg_clear_index_of_word();wordcount = 0;texture_x=texture_y=texture_next_tail=0;
    for(FRPLine * line = flycseg->flyc.lines;line;line = line->next){

        /* change font for current line */
        frg_change_font_frp_str(lib,&face,frp_play_property_string_value(line->values,pid_font));
        //update use kerning here please
        FT_UInt lastword = 0;
        frp_size nodecount = 0;
        for(FRPNode * node = line->node;node;node = node->next){
            frp_str s = frp_play_property_string_value(node->values,pid_text);
            while(s.len > 0){
                int skip;
                FT_UInt curr = FT_Get_Char_Index(face,frg_utf8_to_unicode(file->textpool,&skip));
                s.beg += skip;
                s.len -= skip;

                int isnew;
                FRGWords[wordcount].word_id = frg_index_of_word(curr,&isnew);
                FRGWords[wordcount].property_id = nodecount;
                if(isnew){
                    //add texture locale
                    FT_Load_Glyph(face,curr,FT_LOAD_RENDER);
                    FT_Bitmap *bitmap = &(face->glyph->bitmap);
                    //check return or not
                    if(texture_x + bitmap->width >= FRG_TEXTURE_MAX_WIDTH){
                        texture_x = 0;
                        texture_y = texture_next_tail;
                    }
                    //update texture tail
                    if(texture_y + bitmap->rows > texture_next_tail)
                        texture_next_tail = texture_y + bitmap->rows;
                    //now texture_x,texture_y is the locale of word[wordcount]'s texture
                    //load to texture
                    glTexSubImage2D(GL_TEXTURE_2D,0,texture_x,texture_y,bitmap->width,bitmap->rows,GL_RED,GL_UNSIGNED_BYTE,bitmap->buffer);
                    texture_x += bitmap->width;
                }
                //calculate begin pos of word
                //TODO:locale calculate of glyph
                wordcount++;
                lastword = curr;
            }
            nodecount++;
        }
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
void frg_unloadlyrc(){
    frg_freelines();
}
