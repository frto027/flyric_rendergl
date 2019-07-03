/* notice:this file is used to debug library only.it create an openGL context with glfw. */

#include <stdio.h>
#include "fparser_public.h"

#include "glenv.h"
#include <GLFW/glfw3.h>

#include "flyric_rendergl.h"

int wind_width = 800;
int wind_height = 600;

void window_size_callback(GLFWwindow* window, int width, int height)
{
    /* change windows size dynamically */
    wind_width = width;
    wind_height = height;
    frg_screensize_set(width,height);
}

char buff[1024 * 1024 * 100];
int main(int argc,char **argv){
    if(argc != 3){
        printf("usage: main_linux [font_path] [lyric_path]\n");
        exit(-1);
    }

    printf("running...\n");

    /* init freetype library */
    FT_Library freetypelib;
    FT_Init_FreeType(&freetypelib);

    /*you MUST init flyric parser before frg_startup*/
    frpstartup();
    /* init flyric rendergl */
    frg_startup(freetypelib,argv[1],0);
    /* here add all system fonts */
    //frg_add_font_file(freetypelib,"bb","/home/frto027/Project/testFreeType/Deng.ttf",0);
    //frg_add_font_file(freetypelib,"aa",argv[1],0);
    //frg_add_font_alias("what called xxx","bb");
    //frg_add_font_alias("xxx","what called xxx");
    /* init openGL context */
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
#ifdef GLFW_TRANSPARENT_FRAMEBUFFER
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER,GLFW_TRUE);
#endif

    GLFWwindow * window = glfwCreateWindow(wind_width,wind_height,"lyric",NULL,NULL);
    glfwSetWindowSizeCallback(window, window_size_callback);

    glfwMakeContextCurrent(window);

#ifdef __GLEW_H__
	glewInit();
#endif

    /* 背景透明 */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    /* prepare a lyric file */
    FRPFile * file;
    {
        FILE * f = fopen(argv[2],"rb");
        if(!f){
            printf("File open failed.\n");
            exit(-1);
        }
        size_t sz = fread(buff,sizeof(frp_uint8),sizeof(buff),f);
        buff[sz]='\0';
        file = frpopen(buff,sz,0);
        if(!file){
            printf("lyric parse failed.");
            exit(-1);
        }
    }
    frg_screensize_set(wind_width,wind_height);//单位是像素，可以频繁调用

    frg_fontsize_set(60);//单位是像素
    frg_loadlyric(file);
    //这以后不能调用frg_fontsize_set，如果要改变字体大小，需要再次调用frg_loadlyric后才能正常渲染。

    double start_time = glfwGetTime();
    int fps = 0;
    int linecount = 0,begline = 0,endline = 0;
    double lastfps = start_time;
    while(!glfwWindowShouldClose(window)){

        /* render here*/
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(0,0,wind_width,wind_height);

        //this is the playing time
        double tim = glfwGetTime() - start_time;
        frp_time ftim = (frp_time)(tim*1000);

        //render line
        begline = -1;
        linecount=0;
        for(FRTNode * tnode = frp_play_getline_by_time(file,ftim);tnode;tnode = tnode->next){
            if(begline < 0)
                begline = tnode->line->linenumber;
            frg_renderline(tnode->line,ftim);
            linecount++;
            endline = tnode->line->linenumber;
        }


        glfwSwapBuffers(window);
        glfwPollEvents();
        fps++;
        if(glfwGetTime() - lastfps > 1){
            printf("fps = %d,last frame :linecount = %d(from %d to %d)\n",fps,linecount,begline,endline);
            fps = 0;
            lastfps = glfwGetTime();
        }

        //wait for 1/60s here lock fps?
    }

    frg_shutdown();
    glfwTerminate();
    frpshutdown();
    FT_Done_FreeType(freetypelib);


}
