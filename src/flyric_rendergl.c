#include "flyric_rendergl.h"
#include "glenv.h"
//cos and sin
#include <math.h>

#define MAX_WORD_PER_LINE 510
#define MAX_PROPERTY_PER_LINE 300

#define TO_STR1(x) #x
#define TO_STR(x) TO_STR1(x)

frp_size frg_fontsize = 40;

frp_size frg_scr_width = 800;
frp_size frg_scr_height = 600;

const char * frg_default_font_path;

frp_bool frg_has_texture = 0;
GLuint frg_texture = 0;

frp_bool frg_program_loaded = 0;
GLuint frg_program;

frp_bool frg_buffer_initailized = 0;
GLuint frg_buffer_wordinfos,frg_buffer_props,frg_buffer_lineprop;//,frg_uniform_lineprops,frg_uniform_propinfo;
GLuint frg_buffer_points,frg_vertex_arr;
GLint frg_uniform_screenScale;

GLint frg_point_ids [] = {0,1,2,3};

frp_size frg_ids_colorR,frg_ids_colorG,frg_ids_colorB,frg_ids_colorA,
    frg_ids_anchor_x,frg_ids_anchor_y,frg_ids_self_anchor_x,frg_ids_self_anchor_y,
    frg_ids_transX,frg_ids_transY,frg_ids_rotX,frg_ids_rotY,frg_ids_rotZ,
    frg_ids_HV;

const char * frg_shaders[] = {
/*
struct TexLocales is FRGWordTextureInfo in C language.
struct PropInfo is FRGNodeProperty in c lang.

point pos:
0 1
3 2

*/
/* vertex shader */
"#version 310 es\n\
struct PropInfo{\
    vec2 hxvy;\
    vec2 scale;\
    vec4 color;\
    mat4 trans;\
};\
struct WordInfo{\
    vec2 hoff;\
    vec2 voff;\
    vec2 texcoord;\
    vec2 texsize;\
    int texid;\
    int propid;\
};\
layout (std140, binding = 2) uniform PropInfoBlock{ PropInfo props["TO_STR(MAX_PROPERTY_PER_LINE)"]; };\
layout (std140, binding = 3) uniform WordInfoBlock{ WordInfo words["TO_STR(MAX_WORD_PER_LINE)"]; };\
layout (std140, binding = 4) uniform LineProps{\
    vec2 vxhy;\
    float hv;\
};\
uniform vec2 screenScale;\
layout (location = 0) in int pos_mark;\
out vec2 tex_coord;\
out float rcc;\
out vec4 word_color;\
void main(void){\
    rcc = 0.;\
    int windex = gl_InstanceID;\
    int pid = words[windex].propid;\
    vec2 hpos = vec2(props[pid].hxvy.x,vxhy.y) + words[windex].hoff * screenScale;\
    vec2 vpos = vec2(vxhy.x,props[pid].hxvy.y) + words[windex].voff * screenScale;\
    vec2 pos = vec2(\
    (hpos.x - vpos.x) * cos(hv*3.1415926/2.) + vpos.x,\
    (vpos.y - hpos.y) * sin(hv*3.1415926/2.) + hpos.y);\
    tex_coord = words[windex].texcoord;\
    if(pos_mark == 1 || pos_mark == 2){\
        pos.x += words[windex].texsize.x * screenScale.x;\
        tex_coord.x += + words[windex].texsize.x;\
    }\
    if(pos_mark == 2 || pos_mark == 3){\
        pos.y += - words[windex].texsize.y * screenScale.y;\
        tex_coord.y += + words[windex].texsize.y;\
    }\
    gl_Position = props[pid].trans * vec4(pos,0,1);\
    word_color = props[pid].color;\
}\
",
/* fragment shader */
"#version 310 es\n\
uniform sampler2D tex;\
uniform highp vec2 texScale;\
in highp vec2 tex_coord;\
in highp float rcc;\
in highp vec4 word_color;\
out highp vec4 color;\
void main(void){\
    color = texture(tex,tex_coord * texScale).rrrr * word_color;\
}\
"
};

FRPFile * frg_frpfile = NULL;
//=================info of uniform in shader=================
/* 这个结构体不再储存于GPU */
struct FRGWordTextureInfo{
    GLfloat tex_x,tex_y;
    GLfloat tex_width,tex_height;
}*FRGWordTextureLocales = NULL;//for each word(texture)
struct FRGWordInfo{
    GLfloat h_x_off,h_y_off;//文字水平布局时候左下距离property_id对应的Node的偏移
    GLfloat v_x_off,v_y_off;//文字垂直布局时候左下距离propryty_id对应的Node的偏移
    GLfloat texcoor_x,texcoor_y,texsize_w,texsize_h;
    //float half_w,half_h;//文字半宽半高 你为什么不问问word_id对应的FRGWordTextureInfo呢？
    GLint tex_id;
    GLint property_id;
    GLint PADDING[2];
}*FRGWords = NULL;//for each word
struct FRGNodeProperty{//fill it in runtime
    GLfloat h_x/*,h_y*/;//y is the baseline locale(uniform) TODO
    GLfloat /*v_x,*/v_y;//x is the baseline locale(uniform) TODO
    GLfloat scaleX,scaleY;
    GLfloat colorR,colorG,colorB,colorA;
    GLfloat trans[4][4];
    //others
}*FRGNodeProperties = NULL;//for each node

struct{
    GLfloat vx,hy;
    GLfloat hv;
}FRGLineProperty;

int frg_max_word_count = 0;

// ================texture locale of each word=========================
/* for cpu use only */
struct FRGLineNodeArgs{
    float kerning_h_x_off,kerning_v_y_off;
    float h_width,v_height;
};

struct FRGline{
    int start_word;//从哪个字开始渲染
    int word_count;//渲染几个字
    float h_width,v_height;
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
//======================fonts=======================
struct FRGFontNode{
    frp_uint8 * fontname;
    FT_Face * face;
    int font_id;
    struct FRGFontNode * next;
}*FRGFontList = NULL;
int frg_next_font_id = 0;

//=====================matrix functions=============
void frg_mat_e(float mat[4][4]){
    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++)
            mat[i][j] = i==j;
}
//a x b -> b
void frg_mat_mul(float a[4][4],float b[4][4]){
    float c[4][4];
    for(int i=0;i<4;i++){
        for(int j=0;j<4;j++){
            c[j][i]=0;
            for(int k=0;k<4;k++){
                c[j][i] += a[k][i]*b[j][k];
            }
        }
    }
    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++)
            b[i][j]=c[i][j];
}
/*
1 0 0 x
0 1 0 y
0 0 1 z
0 0 0 1
*/
void frg_mat_move(float x,float y,float z,float mat[4][4]){
    for(int i=0;i<4;i++){
        for(int j=0;j<4;j++){
            mat[i][j] = i == j;
        }
    }
    mat[3][0]=x;
    mat[3][1]=y;
    mat[3][2]=z;
}
void frg_rot_x(float x,float mat[4][4]){
    for(int i=0;i<4;i++){
        for(int j=0;j<4;j++){
            mat[i][j] = i == j;
        }
    }
    mat[1][1] = mat[2][2] = cosf(x);
    mat[1][2] = sinf(x);
    mat[2][1] = -mat[1][2];
}
void frg_rot_y(float x,float mat[4][4]){
    for(int i=0;i<4;i++){
        for(int j=0;j<4;j++){
            mat[i][j] = i == j;
        }
    }
    mat[0][0] = mat[2][2] = cosf(x);
    mat[2][0] = sinf(x);
    mat[0][2] = -mat[2][0];
}
void frg_rot_z(float x,float mat[4][4]){
    for(int i=0;i<4;i++){
        for(int j=0;j<4;j++){
            mat[i][j] = i == j;
        }
    }
    mat[0][0] = mat[1][1] = cosf(x);
    mat[0][1] = sinf(x);
    mat[1][0] = -mat[0][1];
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
void frg_change_font(FT_Face * face,const char * fontname,int len){
    if(fontname && len < 0){
        len = 0;
        while(fontname[len])
            len++;
    }
    if(len == 0)
        fontname = NULL;
    //ALDO change font
    struct FRGFontNode * got = FRGFontList;
    if(got == NULL){
        //error:no init
    }
    if(fontname == NULL){
        while(got->next)
            got = got->next;//default font
    }else{
        while(got->next){
            int i;
            for(i=0;i<len;i++){
                if(fontname[i] != got->fontname[i])
                    break;//for
            }
            if(i == len)
                break;//while,founded
            //not found
            got = got->next;
        }
    }
    //frg_bool is_default_font = got->next == NULL;
    *face = *(got->face);

    //for frg_index_of_word used
    frg_current_font_id = got->font_id;
}
void frg_change_font_frp_str(FT_Face * face,frp_str font){
    if(!frg_frpfile)
        return;
    if(font.len)
        frg_change_font(face,frg_frpfile->textpool + font.beg,font.len);
}

//==================programs=================
void frg_add_font_file(FT_Library lib,frp_uint8 * fontname,const char * filepath,int index){
    FT_Face * face = frpmalloc(sizeof(FT_Face));
    FT_New_Face(lib,filepath,index,face);
    if(!*face){
        //error:create face failed
        frpfree(face);
        return;
    }

    struct FRGFontNode * n = frpmalloc(sizeof(struct FRGFontNode));
    frp_size len = 0;
    while(fontname[len++])
        continue;
    n->fontname = frpmalloc(sizeof(frp_uint8) * len);
    while(len--)
        n->fontname[len] = fontname[len];
    n->face = face;
    //if(frg_next_font_id < 2)
    /* set pixel size */
    FT_Set_Pixel_Sizes(*(n->face),0,frg_fontsize);

    n->next = FRGFontList;
    n->font_id = frg_next_font_id++;
    FRGFontList = n;
}
void frg_add_font_alias(frp_uint8 * new_name,frp_uint8 * priv_fontname){
    struct FRGFontNode * got = FRGFontList;
    if(priv_fontname){
        while(got && frpstr_rrcmp(got->fontname,priv_fontname) != 0){
            got = got->next;
        }
    }else if(got){
        //priv is default font
        while(got->next)
            got = got->next;
    }

    if(got){
        //add new font here
        struct FRGFontNode * n = frpmalloc(sizeof(struct FRGFontNode));
        frp_size len = 0;
        while(new_name[len++])
            continue;
        n->fontname = frpmalloc(sizeof(frp_uint8) * len);
        while(len--)
            n->fontname[len] = new_name[len];
        n->face = got->face;
        n->next = FRGFontList;
        n->font_id = got->font_id;
        FRGFontList = n;
    }else{
        //not found
    }
}
void frg_free_fontlist(){
    while(FRGFontList){
        frpfree(FRGFontList->fontname);
        if(*(FRGFontList->face)){
            FT_Done_Face(*(FRGFontList->face));
            *(FRGFontList->face) = NULL;
        }
        struct FRGFontNode * n = FRGFontList->next;
        frpfree(FRGFontList);
        FRGFontList = n;
    }
    frg_next_font_id = 0;
}

void frg_startup(FT_Library lib,const char * default_font_path,int default_font_index){
    /* font init */
    frg_free_fontlist();
    FRGFontList = frpmalloc(sizeof(struct FRGFontNode));
    FRGFontList->face = frpmalloc(sizeof(FT_Face));
    FT_New_Face(lib,default_font_path,default_font_index,FRGFontList->face);
    FRGFontList->fontname = NULL;
    FRGFontList->next = NULL;

    FRGFontList->font_id = frg_next_font_id++;
    frp_anim_add_support("ColorR");
    frp_anim_add_support("ColorG");
    frp_anim_add_support("ColorB");
    frp_anim_add_support("ColorA");

    frp_anim_add_support("AnchorX");
    frp_anim_add_support("AnchorY");
    frp_anim_add_support("SelfAnchorX");
    frp_anim_add_support("SelfAnchorY");

    frp_anim_add_support("Rotate");
    frp_anim_add_support("TransX");
    frp_anim_add_support("TransY");
    /* add some property */
    frp_flyc_add_parse_rule("HV",FRP_FLYC_PTYPE_NUM);
    frp_anim_add_support("HV");
    frp_flyc_add_parse_rule("RotateX",FRP_FLYC_PTYPE_NUM);
    frp_anim_add_support("RotateX");
    frp_flyc_add_parse_rule("RotateY",FRP_FLYC_PTYPE_NUM);
    frp_anim_add_support("RotateY");
    frp_flyc_add_parse_rule("RotateZ",FRP_FLYC_PTYPE_NUM);
    frp_anim_add_support("RotateZ");

    //declare that i support animation
}
void frg_shutdown(){
    frg_freelines();
}


void frg_fontsize_set(frp_size fontsize){
    frg_fontsize = fontsize;
    struct FRGFontNode * n = FRGFontList;
    while(n){
        FT_Set_Pixel_Sizes(*(n->face),0,fontsize);
        n = n->next;
    }
}

void frg_screensize_set(frp_size width,frp_size height){
    frg_scr_width = width;
    frg_scr_height = height;
    if(frg_buffer_initailized){
        glUniform2f(frg_uniform_screenScale,2./frg_scr_width,2./frg_scr_height);
    }
}
int frg_loadlyric(FRPFile * file){
    if(!file)
        return FRG_LOAD_ERROR_INVALID_FILE;
    FT_Face face;

    frg_frpfile = file;


    FRPSeg * flycseg = frp_getseg(frg_frpfile,"flyc");
    if(!flycseg){
        return 0;//no lines
    }

    /* init ids */
    frg_ids_colorR = frp_play_get_property_id(file,"ColorR");
    frg_ids_colorG = frp_play_get_property_id(file,"ColorG");
    frg_ids_colorB = frp_play_get_property_id(file,"ColorB");
    frg_ids_colorA = frp_play_get_property_id(file,"ColorA");
    frg_ids_anchor_x = frp_play_get_property_id(file,"AnchorX");
    frg_ids_anchor_y = frp_play_get_property_id(file,"AnchorY");
    frg_ids_self_anchor_x = frp_play_get_property_id(file,"SelfAnchorX");
    frg_ids_self_anchor_y = frp_play_get_property_id(file,"SelfAnchorY");

    frg_ids_transX = frp_play_get_property_id(file,"TransX");
    frg_ids_transY = frp_play_get_property_id(file,"TransY");
    frg_ids_rotX = frp_play_get_property_id(file,"RotateX");
    frg_ids_rotY = frp_play_get_property_id(file,"RotateY");
    frg_ids_rotZ = frp_play_get_property_id(file,"RotateZ");
    frg_ids_HV = frp_play_get_property_id(file,"HV");

    frp_size pid_text = frp_play_get_property_id(frg_frpfile,"Text");
    frp_size pid_font = frp_play_get_property_id(frg_frpfile,"Font");

    FRGLines = frpmalloc(sizeof(struct FRGline) * flycseg->flyc.linenumber_all);

    frp_size wordcount = 0;
    frp_size texture_count = 0;
    frp_size line_count = 0;

    frp_size texture_x = 0,texture_y = 0;
    frp_size texture_next_tail = 0;

    frp_size max_node_count = 0;
    /* get the wordcount,max_node_count and texture size*/
    frg_clear_index_of_word();
    /*start of word info MUST align with the value(used by BindBufferRange) */
    GLint uniform_buffer_align = 1;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,&uniform_buffer_align);

    for(FRPLine * line = flycseg->flyc.lines;line;line = line->next){
        /* align memory here */
        if(wordcount % uniform_buffer_align != 0){
            wordcount = (wordcount/uniform_buffer_align + 1)*uniform_buffer_align;
        }

        /* change font for current line */
        frg_change_font_frp_str(&face,frp_play_property_string_value(line->values,pid_font));
        /* font kerning */
        FT_Bool use_kerning = FT_HAS_KERNING(face);

        //update use kerning here please
        frp_size node_count = 0;
        for(FRPNode * node = line->node;node;node = node->next){
            frp_str s = frp_play_property_string_value(node->values,pid_text);
            frp_uint8 * buff = frpmalloc(sizeof(frp_uint8)*(s.len + 5));
            frpstr_fill(frg_frpfile->textpool,s,buff,sizeof(frp_uint8)*(s.len + 5));
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
        frg_has_texture = 1;
        glGenTextures(1,&frg_texture);
    }
    glBindTexture(GL_TEXTURE_2D,frg_texture);
    glTexStorage2D(GL_TEXTURE_2D,1,GL_R8,FRG_TEXTURE_MAX_WIDTH,mxsize);

    glPixelStorei(GL_UNPACK_ALIGNMENT,1);

    //TODO:init texture locale,word id(locale),property id
    //上面和下面的坐标计算必须一致
    frg_clear_index_of_word();wordcount = 0;texture_x=texture_y=texture_next_tail=0,line_count = 0;
    for(FRPLine * line = flycseg->flyc.lines;line;line = line->next){
        /* align memory here */
        if(wordcount % uniform_buffer_align != 0){
            wordcount = (wordcount/uniform_buffer_align + 1)*uniform_buffer_align;
        }

        /* start word property */
        FRGLines[line_count].start_word = wordcount;

        /* change font for current line */
        frg_change_font_frp_str(&face,frp_play_property_string_value(line->values,pid_font));
        /* font kerning */
        FT_Bool use_kerning = FT_HAS_KERNING(face);

        FT_UInt lastword = 0;
        frp_size nodecount = 0;

        /* malloc node args for the line */
        for(FRPNode * n = line->node;n;n=n->next)
            nodecount++;
        FRGLines[line_count].node_args = frpmalloc(sizeof(struct FRGLineNodeArgs) * nodecount);
        nodecount = 0;

        int line_pen_hx = 0,line_pen_vy = 0;
        for(FRPNode * node = line->node;node;node = node->next){
            frp_str s = frp_play_property_string_value(node->values,pid_text);
            frp_uint8 * buff = frpmalloc(sizeof(frp_uint8)*(s.len + 5));
            frpstr_fill(frg_frpfile->textpool,s,buff,sizeof(frp_uint8)*(s.len + 5));
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
                }else{
                    //do not setup texture
                    FT_Load_Glyph(face,curr,FT_LOAD_DEFAULT);
                }

                /* fill texture info with tex_id */
                FRGWords[wordcount].texcoor_x = FRGWordTextureLocales[tex_id].tex_x;
                FRGWords[wordcount].texcoor_y = FRGWordTextureLocales[tex_id].tex_y;
                FRGWords[wordcount].texsize_w = FRGWordTextureLocales[tex_id].tex_width;
                FRGWords[wordcount].texsize_h = FRGWordTextureLocales[tex_id].tex_height;

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
                        line_pen_hx += delta.x >> 6;
                        /* no impl vertical kerning */
                        //pen_v_y -= 0;
                        //line_pen_vy -= 0;
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
                line_pen_hx += face->glyph->advance.x >> 6;

                /* vertical */
                FT_Load_Glyph(face,curr,FT_LOAD_VERTICAL_LAYOUT);
                word->v_x_off = face->glyph->bitmap_left;
                word->v_y_off = pen_v_y - face->glyph->bitmap_top; /*Does bitmap_top is positive for upwards y in vertical layout? */
                pen_v_y -= face->glyph->advance.y >> 6; /* advance is width and height.*/
                line_pen_vy -= face->glyph->advance.y >> 6;

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
        FRGLines[line_count].h_width = line_pen_hx;
        FRGLines[line_count].v_height = - line_pen_vy;
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
        if(para == GL_FALSE)
            printf("Link shader program error message:\n%s\n",buff);
        }
        glUseProgram(frg_program);
    }

    /* buffers */
    if(!frg_buffer_initailized){
        frg_buffer_initailized = 1;
        glGenBuffers(1,&frg_buffer_props);
        glGenBuffers(1,&frg_buffer_wordinfos);
        glGenBuffers(1,&frg_buffer_lineprop);

        glGenBuffers(1,&frg_buffer_points);
    }

    /* buffer datas */
    glBindBuffer(GL_UNIFORM_BUFFER,frg_buffer_props);
    glBufferData(GL_UNIFORM_BUFFER,sizeof(struct FRGNodeProperty) * max_node_count,NULL,GL_DYNAMIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER,frg_buffer_wordinfos);
    glBufferData(GL_UNIFORM_BUFFER,sizeof(struct FRGWordInfo) * wordcount,FRGWords,GL_STATIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER,frg_buffer_lineprop);
    glBufferData(GL_UNIFORM_BUFFER,sizeof(FRGLineProperty),&FRGLineProperty,GL_DYNAMIC_DRAW);


    glGenVertexArrays(1,&frg_vertex_arr);
    glBindVertexArray(frg_vertex_arr);
    glBindBuffer(GL_ARRAY_BUFFER,frg_buffer_points);
    glBufferData(GL_ARRAY_BUFFER,sizeof(frg_point_ids),frg_point_ids,GL_STATIC_DRAW);

    glVertexAttribIPointer(0,1,GL_INT,0,0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_UNIFORM_BUFFER,0);

    /* bind with uniform */

    glBindBufferBase(GL_UNIFORM_BUFFER,2,frg_buffer_props);
    glBindBufferBase(GL_UNIFORM_BUFFER,3,frg_buffer_wordinfos);
    glBindBufferBase(GL_UNIFORM_BUFFER,4,frg_buffer_lineprop);
    //glUniformBlockBinding(frg_program,glGetUniformBlockIndex(frg_program,"TexCoorBlock"),frg_buffer_tex_coords);
    //glUniformBlockBinding(frg_program,glGetUniformBlockIndex(frg_program,"PropInfoBlock"),frg_buffer_props);
    //glUniformBlockBinding(frg_program,glGetUniformBlockIndex(frg_program,"WordInfoBlock"),frg_buffer_wordinfos);
    //glUniformBlockBinding(frg_program,glGetUniformBlockIndex(frg_program,"LineProps"),frg_buffer_lineprop);

    /* texture Scales */
    glUniform2f(glGetUniformLocation(frg_program,"texScale"),1./FRG_TEXTURE_MAX_WIDTH,1./mxsize);

    /* uniforms filled when rendering */
    //frg_uniform_lineprop = glGetUniformBlockIndex(frg_program,"LineProps");
    frg_uniform_screenScale = glGetUniformLocation(frg_program,"screenScale");
    glUniform2f(frg_uniform_screenScale,2./frg_scr_width,2./frg_scr_height);
    //frg_uniform_lineprops = glGetUniformBlockIndex(frg_program,"LineProps");
    //frg_uniform_propinfo = glGetUniformBlockIndex(frg_program,"PropInfoBlock");
    //frg_buffer_props

    /* debug */
    /*
    glGenBuffers(1,&buffer);
    glGenVertexArrays(1,&varry);
    glBindVertexArray(varry);
    glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(pts),pts,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(0);
    */
}
void frg_renderline(FRPLine * line,frp_time time){
    int i = 0;
    int linenum = line->linenumber;

    /* update line property */
    glBindBufferRange(GL_UNIFORM_BUFFER,3,frg_buffer_wordinfos,(GLintptr)(sizeof(struct FRGWordInfo) * FRGLines[linenum].start_word),sizeof(struct FRGWordInfo) * FRGLines[linenum].word_count);
    FRGLineProperty.hy = 0;
    FRGLineProperty.vx = 0;

    FRGLineProperty.hv = frp_play_property_float_value(time,line->values,frg_ids_HV);

    float pen_hx = 0;
    float pen_vy = 0;

    float anchor_x = frp_play_property_float_value(time,line->values,frg_ids_anchor_x);
    float anchor_y = frp_play_property_float_value(time,line->values,frg_ids_anchor_y);
    float self_anchor_x = frp_play_property_float_value(time,line->values,frg_ids_self_anchor_x);
    float self_anchor_y = frp_play_property_float_value(time,line->values,frg_ids_self_anchor_y);
    /* calculate start position for anchor */

    /* horizental */
    pen_hx = ( anchor_x - 0.5f ) * frg_scr_width - self_anchor_x * FRGLines[linenum].h_width;
    FRGLineProperty.hy = (2 * anchor_y - 1 ) - self_anchor_y * (frg_fontsize) * 2. / frg_scr_height;
    /* vertical */
    FRGLineProperty.vx = (2 * anchor_x - 1) - self_anchor_x * frg_fontsize * 2. / frg_scr_height;
    pen_vy = (anchor_y - 0.5f) * frg_scr_height + (1 - self_anchor_y) * FRGLines[linenum].v_height;
    //pen_vy = frg_scr_height / 2;

    float line_transform[4][4];
    float temp_mat[4][4];
    frg_mat_e(line_transform);

    {
        float cx = anchor_x * 2 - 1;
        float cy = anchor_y * 2 - 1;

        frg_mat_move(-cx,-cy,0,temp_mat);
        frg_mat_mul(temp_mat,line_transform);


        frg_rot_x(frp_play_property_float_value(time,line->values,frg_ids_rotX),temp_mat);
        frg_mat_mul(temp_mat,line_transform);

        frg_rot_y(frp_play_property_float_value(time,line->values,frg_ids_rotY),temp_mat);
        frg_mat_mul(temp_mat,line_transform);

        frg_rot_z(frp_play_property_float_value(time,line->values,frg_ids_rotZ),temp_mat);
        frg_mat_mul(temp_mat,line_transform);

        frg_mat_move(cx,cy,0,temp_mat);
        frg_mat_mul(temp_mat,line_transform);
    }

    //move
    frg_mat_move(
        frp_play_property_float_value(time,line->values,frg_ids_transX) * frg_fontsize * 2. / frg_scr_width,
        frp_play_property_float_value(time,line->values,frg_ids_transY) * frg_fontsize * 2. / frg_scr_height,
        0,
        temp_mat
    );
    frg_mat_mul(temp_mat,line_transform);


    struct FRGNodeProperty * prop = FRGNodeProperties;

    for(FRPNode * n = line->node;n;n = n->next,prop++,i++){
        /* update node property */
        struct FRGLineNodeArgs * nodearg = FRGLines[linenum].node_args + i;
        pen_hx += nodearg->kerning_h_x_off;
        pen_vy += nodearg->kerning_v_y_off;

        prop->h_x = pen_hx * 2. / frg_scr_width;
        prop->v_y = pen_vy * 2. / frg_scr_height;


        float sanchorx = frp_play_property_float_value(time,n->values,frg_ids_self_anchor_x);
        float sanchory = frp_play_property_float_value(time,n->values,frg_ids_self_anchor_y);


        float hcx = (pen_hx + nodearg->h_width * sanchorx) * 2. / frg_scr_width;
        float hcy = FRGLineProperty.hy + frg_fontsize * sanchory;//???
        float vcx = FRGLineProperty.vx + frg_fontsize * sanchorx;
        float vcy = (pen_vy + nodearg->v_height * sanchory) * 2. / frg_scr_width;

        float cx = (hcx - vcx) * cosf(FRGLineProperty.hv * 3.1415926) + vcx;
        float cy = (vcy - hcy) * sinf(FRGLineProperty.hv * 3.1415926) + hcy;

        frg_mat_e(prop->trans);
        frg_mat_move(
            -cx,-cy,0,prop->trans);
        frg_rot_x(frp_play_property_float_value(time,n->values,frg_ids_rotX),temp_mat);
        frg_mat_mul(temp_mat,prop->trans);
        frg_rot_y(frp_play_property_float_value(time,n->values,frg_ids_rotY),temp_mat);
        frg_mat_mul(temp_mat,prop->trans);

        frg_rot_z(frp_play_property_float_value(time,n->values,frg_ids_rotZ),temp_mat);
        frg_mat_mul(temp_mat,prop->trans);
        frg_mat_move(cx,cy,0,temp_mat);
        frg_mat_mul(temp_mat,prop->trans);
        frg_mat_move(
            frp_play_property_float_value(time,n->values,frg_ids_transX) * frg_fontsize * 2. / frg_scr_width,
            frp_play_property_float_value(time,n->values,frg_ids_transY) * frg_fontsize * 2. / frg_scr_height,
            0,
            temp_mat
        );
        frg_mat_mul(temp_mat,prop->trans);

        frg_mat_mul(line_transform,prop->trans);

        /* colors */
        prop->colorR = frp_play_property_float_value(time,n->values,frg_ids_colorR);
        prop->colorG = frp_play_property_float_value(time,n->values,frg_ids_colorG);
        prop->colorB = frp_play_property_float_value(time,n->values,frg_ids_colorB);
        /* if alpha = 1,the word is hide.(for the default value = 0)*/
        prop->colorA =1 - frp_play_property_float_value(time,n->values,frg_ids_colorA);

        /* append for next line */
        pen_hx += nodearg->h_width;
        pen_vy -= nodearg->v_height;

    }

    /* save property to OpenGL buffer */
    glBindBuffer(GL_UNIFORM_BUFFER,frg_buffer_props);
    glBufferSubData(GL_UNIFORM_BUFFER,0,sizeof(struct FRGNodeProperty) * i,FRGNodeProperties);

    glBindBuffer(GL_UNIFORM_BUFFER,frg_buffer_lineprop);
    glBufferSubData(GL_UNIFORM_BUFFER,0,sizeof(FRGLineProperty),&FRGLineProperty);

    glBindBuffer(GL_UNIFORM_BUFFER,0);

    /* draw words */
    glBindVertexArray(frg_vertex_arr);
    glDrawArraysInstanced(GL_TRIANGLE_FAN,0,4,FRGLines[linenum].word_count);

}
void frg_unloadlyrc(){
    frg_freelines();
    frg_clear_index_of_word();
    frg_buffer_initailized = 0;
    glDeleteBuffers(1,&frg_buffer_props);
    glDeleteBuffers(1,&frg_buffer_wordinfos);

}
