#include <stdio.h>
#include "fparser_public.h"

#include "glenv.h"
#include <GLFW/glfw3.h>

#include "flyric_rendergl.h"

int wind_width = 800;
int wind_height = 600;

void window_size_callback(GLFWwindow* window, int width, int height)
{
    wind_width = width;
    wind_height = height;
    frg_screensize_set(width,height);
}

int main(int argc,char **argv){
    if(argc != 3){
        printf("usage: main_linux [font_path] [lyric_path]\n");
        exit(-1);
    }

    printf("running...\n");

    /* init freetype library */
    FT_Library freetypelib;
    FT_Init_FreeType(&freetypelib);

    /* init flyric parser*/
    frpstartup();

    /* init openGL context */
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);


    GLFWwindow * window = glfwCreateWindow(wind_width,wind_height,"lyric",NULL,NULL);
    glfwSetWindowSizeCallback(window, window_size_callback);

    glfwMakeContextCurrent(window);
    /* 背景透明 */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    /* prepare a lyric file */
    char buff[4096];
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

    /* init flyric rendergl */
    frg_startup(argv[1]);
    frg_screensize_set(800,600);
    frg_fontsize_set(60);

    frg_loadlyric(freetypelib,file);

    double start_time = glfwGetTime();
    int fps = 0;
    double lastfps = start_time;
    while(!glfwWindowShouldClose(window)){

        /* render here*/
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(0,0,wind_width,wind_height);

        //this is the playing time
        double tim = glfwGetTime() - start_time;
        frp_time ftim = (frp_time)(tim*1000);


        //render line
        for(FRTNode * tnode = frp_play_getline_by_time(file,ftim);tnode;tnode = tnode->next){

            frg_renderline(tnode->line,ftim);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
        fps++;
        if(glfwGetTime() - lastfps > 1){
            printf("fps = %d\n",fps);
            fps = 0;
            lastfps = glfwGetTime();
        }

        //wait for 1/60s
    }

    frg_shutdown();
    glfwTerminate();
    frpshutdown();
    FT_Done_FreeType(freetypelib);


}
