# 使用OpenGL来渲染歌词动画
尽量写通用，shader希望直接使用OpenGL ES编写。
主要的环境依赖：`opengl`,`freetype`,`flyric_parser`

计划的具体实现：

平台|会用到的一些库
------|------
Windows| `glew`、`glfw`、`freetype`、`mingw`
Linux|`mesa-libGL`、`glfw`、`freetype`
Android|`ndk`

总之先写linux。

# 编译说明
## Codeblocks IDE
配置了Codeblocks IDE工程来封装Makefile，运行之前请注意启动参数的配置。
## linux
开发环境是`fedora 30`  
安装opengl(大概是这样的)
```
sudo yum install mesa-libGL mesa-libGL-devel
```
然后安装freetype
```
sudo yum install freetype freetype-devel
```
**先执行下面的脚本，编译需要用的库，每次`make clean`之后都要重新生成**
```
./gen_library.sh
```
然后`make`即可生成`main_linux.out`
### run.sh文件
一键运行。。。我拿来debug用的
## Windows

见目录builds/windows_gcc

# 移植说明
会把绘图相关的东西全写到flyric_rendergl.c/h里面，使用`glenv.h`来提供opengl的头文件定义，初始化之类的根据不同的平台来编写，linux的放在`main_linux.c`里面。
