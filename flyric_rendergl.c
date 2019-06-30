#include "flyric_rendergl.h"
#include "glenv.h"

/* debug only*/
    GLuint buffer,varry;


frp_size frg_fontsize = 40;

frp_size frg_scr_width = 800;
frp_size frg_scr_height = 600;

const char * frg_default_font_path;

frp_bool frg_has_texture = 0;
GLuint frg_texture = 0;

frp_bool frg_program_loaded = 0;
GLuint frg_program;

const char * frg_shaders[] = {
/*
struct TexLocales is FRGWordTextureInfo in C language.
struct PropInfo is FRGNodeProperty in c lang.

point pos:
3 2
0 1

*/
/* vertex shader */
"#version 310 es\n\
struct TexLocales{\
    vec2 texcoord;\
    vec2 texsize;\
};\
struct PropInfo{\
    vec2 hxvy;\
    vec4 color;\
    vec2 scale;\
};\
struct WordInfo{\
    vec2 hoff;\
    vec2 voff;\
    int texid;\
    int propid;\
};\
uniform TexLocales tex_coords[4096];\
uniform PropInfo props[4096];\
uniform WordInfo words[300];\
uniform vec2 screenScale;\
uniform vec2 vxhy;\
layout (location = 0) in int pos_mark;\
out vec2 tex_coord;\
void main(void){\
    int tid = words[gl_InstanceID].texid;\
    int pid = words[gl_InstanceID].propid;\
    vec2 hpos = vec2(props[pid].hxvy.x,vxhy.y) + words[gl_InstanceID].hoff;\
    \
    tex_coord = tex_coords[tid].texcoord;\
    if(pos_mark == 1 || pos_mark == 2){\
        hpos.x = hpos.x + tex_coords[tid].texsize.x;\
        tex_coord.x = tex_coord.x + tex_coords[tid].texsize.x;\
    }\
    if(pos_mark == 2 || pos_mark == 3){\
        hpos.y = hpos.y + tex_coords[tid].texsize.y;\
        tex_coord.y = tex_coord.y + tex_coords[tid].texsize.y;\
    }\
    gl_Position = vec4(hpos,0,1);\
}\
",
/* fragment shader */
"#version 310 es\n\
uniform sampler2D tex;\
in highp vec2 tex_coord;\
out highp vec4 color;\
void main(void){\
\
    color = texture(tex,tex_coord).rrrr;\
}\
"
};

FRPFile * frg_frpfile = NULL;
//=================info of uniform in shader=================
struct FRGWordTextureInfo{
    float tex_x,tex_y;
    float tex_width,tex_height;
}*FRGWordTextureLocales = NULL;//for each word(texture)
struct FRGWordInfo{
    float h_x_off,h_y_off;//文字水平布局时候左下距离property_id对应的Node的偏移
    float v_x_off,v_y_off;//文字垂直布局时候左下距离propryty_id对应的Node的偏移
    //float half_w,half_h;//文字半宽半高 你为什么不问问word_id对应的FRGWordTextureInfo呢？
    int property_id;
    int tex_id;
}*FRGWords = NULL;//for each word
struct FRGNodeProperty{//fill it in runtime
    float h_x/*,h_y*/;//y is the baseline locale(uniform) TODO
    float /*v_x,*/v_y;//x is the baseline locale(uniform) TODO
    float colorR,colorG,colorB,colorA;
    float scaleX,scaleY;
    //others
}*FRGNodeProperties = NULL;//for each node


int frg_max_word_count = 0;

// ================texture locale of each word=========================

struct FRGLineNodeArgs{
    float kerning_h_x_off,kerning_v_y_off;
    float h_width,v_height;
};

struct FRGline{
    int start_word;//从哪个字开始渲染
    int word_count;//渲染几个字
    struct FRGLineNodeArgs * node_args;
}*FRGLines = NULL;
frp_size FRGLinesSize = 0;
void frg_freelines(){
#define FREE(x) do{if(x) {frpfree(x);x=NULL;}}while(0)
    FREE(FRGWordTextureLocales);
    FREE(FRGWords);
    FREE(FRGNodeProperties);
    if(frg_has_texture){
        glDeleteTextures(1,&frg_texture);
        frg_has_texture = 0;
    }
    if(FRGLines){
        //TODO:free lines
        for(frp_size i = 0;i<FRGLinesSize;i++)
            frpfree(FRGLines[i].node_args);
        frpfree(FRGLines);
        FRGLinesSize = 0;
        FRGLines = NULL;
    }
#undef FREE
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
/* font id current line used when load lyric */
int frg_current_font_id = 0;

/* linked list relization for font map */
struct FRGFontMapNode{
    FT_UInt word;
    int fontid;
    struct FRGFontMapNode * next;
}*FRGFontMaps = NULL;
int frg_clear_index_of_word(){
    while(FRGFontMaps){
        struct FRGFontMapNode * n = FRGFontMaps->next;
        frpfree(FRGFontMaps);
        FRGFontMaps = n;
    }
}
int frg_index_of_word(FT_UInt word,int * isnew){
    static int last = 0;
    if(isnew)
        *isnew = 1;
    //TODO this is a hash map,defferent font should use defferent index!
    /* linked list relization.O(n^2).Change it later*/
    struct FRGFontMapNode * curr = FRGFontMaps;
    if(curr == NULL){
        if(isnew)
            *isnew = 1;
        curr = FRGFontMaps = frpmalloc(sizeof(struct FRGFontMapNode));
        curr->fontid = frg_current_font_id;
        curr->word = word;
        curr->next = NULL;
        return 0;
    }
    int id = 0;

    while(curr->next && (curr->word != word || curr->fontid != frg_current_font_id)){
        curr = curr->next;
        id++;
    }

    if(curr->word == word && curr->fontid == frg_current_font_id){
        if(isnew)
            *isnew = 0;
        return id;
    }
    curr->next = frpmalloc(sizeof(struct FRGFontMapNode));
    curr = curr->next;
    curr->fontid = frg_current_font_id;
    curr->word = word;
    curr->next = NULL;
    if(isnew)
        *isnew = 1;
    return id+1;
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
    if(*face == NULL){
        FT_New_Face(lib,frg_default_font_path,0,
        face);
        frg_current_font_id = 0;//0 for default font
        }
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

    FT_Error err = FT_Set_Pixel_Sizes(face,0,frg_fontsize);
    if(FT_Set_Pixel_Sizes(face,0,frg_fontsize) != 0){
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
    frp_size line_count = 0;
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
            frp_uint8 * buff = frpmalloc(sizeof(frp_uint8) * s.len);
            frpstr_fill(frg_frpfile->textpool,s,buff,sizeof(frp_uint8)*s.len);
            int buff_index = 0;

            while(buff[buff_index]){
                int skip;
                FT_UInt curr = FT_Get_Char_Index(face,frg_utf8_to_unicode(buff+buff_index,&skip));
                while(skip-- && buff[buff_index])
                    buff_index++;

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

            frpfree(buff);
            node_count++;
        }

        if(max_node_count < node_count)
            max_node_count = node_count;
        line_count++;
    }

    //malloc
    frg_freelines();
    FRGWords = frpmalloc(sizeof(struct FRGWordInfo) * wordcount);
    FRGWordTextureLocales = frpmalloc(sizeof(struct FRGWordTextureInfo) * texture_count);
    FRGNodeProperties = frpmalloc(sizeof(struct FRGNodeProperty) * max_node_count);
    FRGLines = frpmalloc(sizeof(struct FRGline) * line_count);
    FRGLinesSize = line_count;

    frp_size mxsize = 1;
    while(mxsize < texture_next_tail)
        mxsize <<=1;

    if(!frg_has_texture){
        glGenTextures(1,&frg_texture);
    }
    glBindTexture(GL_TEXTURE_2D,frg_texture);
    glTexStorage2D(GL_TEXTURE_2D,1,GL_R8,FRG_TEXTURE_MAX_WIDTH,mxsize);

    glPixelStorei(GL_UNPACK_ALIGNMENT,1);

    //TODO:init texture locale,word id(locale),property id
    //上面和下面的坐标计算必须一致
    frg_clear_index_of_word();wordcount = 0;texture_x=texture_y=texture_next_tail=0,line_count = 0;
    for(FRPLine * line = flycseg->flyc.lines;line;line = line->next){
        /* start word property */
        FRGLines[line_count].start_word = wordcount;

        /* change font for current line */
        frg_change_font_frp_str(lib,&face,frp_play_property_string_value(line->values,pid_font));
        //update use kerning here please
        FT_UInt lastword = 0;
        frp_size nodecount = 0;

        /* malloc node args for the line */
        for(FRPNode * n = line->node;n;n=n->next)
            nodecount++;
        FRGLines[line_count].node_args = frpmalloc(sizeof(struct FRGLineNodeArgs) * nodecount);
        nodecount = 0;

        for(FRPNode * node = line->node;node;node = node->next){
            frp_str s = frp_play_property_string_value(node->values,pid_text);
            frp_uint8 * buff = frpmalloc(sizeof(frp_uint8) * s.len);
            frpstr_fill(frg_frpfile->textpool,s,buff,sizeof(frp_uint8)*s.len);
            int buff_index = 0;
            //recalculate offset used
            int begin_word_index = wordcount;

            int pen_h_x = 0,pen_v_y = 0;

            while(buff[buff_index]){
                int isFirstWord = buff_index == 0;

                int skip;
                FT_UInt curr = FT_Get_Char_Index(face,frg_utf8_to_unicode(buff+buff_index,&skip));
                while(skip-- && buff[buff_index])
                    buff_index++;

                int isnew;
                int tex_id = FRGWords[wordcount].tex_id = frg_index_of_word(curr,&isnew);
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
                    struct FRGWordTextureInfo * texinfo = FRGWordTextureLocales + tex_id;
                    texinfo->tex_x = texture_x;
                    texinfo->tex_y = texture_y;
                    texinfo->tex_width = bitmap->width;
                    texinfo->tex_height = bitmap->rows;

                    texture_x += bitmap->width;
                }
                //calculate begin pos of word
                //TODO:locale calculate of glyph

                struct FRGLineNodeArgs * nodeargs = FRGLines[line_count].node_args + nodecount;
                if(use_kerning && lastword && curr){
                    FT_Vector delta;
                    FT_Get_Kerning(face,lastword,curr,FT_KERNING_DEFAULT,&delta);
                    if(isFirstWord){
                        nodeargs->kerning_h_x_off = delta.x >> 6;
                        /* no impl vertical kerning*/
                        nodeargs->kerning_v_y_off = 0;
                    }else{
                        pen_h_x += delta.x >> 6;
                        /* no impl vertical kerning */
                        //pen_v_y -= 0;
                    }
                }else{
                    if(isFirstWord)
                        nodeargs->kerning_h_x_off = nodeargs->kerning_v_y_off = 0;
                }
                struct FRGWordInfo *word = FRGWords + wordcount;

                /* horizental */
                word->h_x_off = pen_h_x + face->glyph->bitmap_left;
                word->h_y_off = face->glyph->bitmap_top;
                pen_h_x += face->glyph->advance.x >> 6;
                /* vertical */
                FT_Load_Glyph(face,curr,FT_LOAD_VERTICAL_LAYOUT);
                word->v_x_off = face->glyph->bitmap_left;
                word->v_y_off = pen_v_y + face->glyph->bitmap_top; /*Does bitmap_top is positive for upwards y in vertical layout? */
                pen_v_y -= face->glyph->advance.y >> 6; /* advance is width and height.*/

                wordcount++;
                lastword = curr;
            }
            FRGLines[line_count].node_args[nodecount].h_width = pen_h_x;
            FRGLines[line_count].node_args[nodecount].v_height = -pen_v_y;

            frpfree(buff);
            nodecount++;
        }

        /* word count property */
        FRGLines[line_count].word_count = wordcount - FRGLines[line_count].start_word;
        line_count++;
    }

    frg_clear_index_of_word();

    /* load program */
    if(!frg_program_loaded){
        frg_program_loaded = 1;
        frg_program = glCreateProgram();
        /* vertex shader */
        GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vshader,1,frg_shaders,NULL);
        glCompileShader(vshader);
        {
        char buff[1024];
        glGetShaderInfoLog(vshader,1024,NULL,buff);
        if(buff[0])
            printf("VertexShader compile message:\n%s\n",buff);
        }
        glAttachShader(frg_program,vshader);
        /* fragment shader*/
        GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fshader,1,frg_shaders + 1,NULL);
        glCompileShader(fshader);
        {
        char buff[1024];
        glGetShaderInfoLog(fshader,1024,NULL,buff);
        if(buff[0])
            printf("FragmentShader compile message:\n%s\n",buff);
        }
        glAttachShader(frg_program,fshader);
        /* program */
        glLinkProgram(frg_program);
        glDeleteShader(vshader);
        glDeleteShader(fshader);
        {
        char buff[1024];
        GLint para;
        glGetProgramiv(frg_program,GL_LINK_STATUS,&para);
        glGetProgramInfoLog(frg_program,1024,NULL,buff);
        if(para != GL_FALSE)
            printf("Link shader program error message:\n%s\n",buff);
        }
        glUseProgram(frg_program);
    }

    /* debug */
    GLfloat pts[] = {
    0,0,1,0,1,1,0,1
    };
    glGenBuffers(1,&buffer);
    glGenVertexArrays(1,&varry);
    glBindVertexArray(varry);
    glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(pts),pts,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(0);
}
void frg_renderline(FRPLine * line,frp_time time){



    /* debug only,never do this please... */
    glBindVertexArray(varry);
    glDrawArrays(GL_TRIANGLE_FAN,0,4);

    /*
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
    */
}
void frg_unloadlyrc(){
    frg_freelines();
    frg_clear_index_of_word();
}
