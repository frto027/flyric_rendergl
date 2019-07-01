# Windows build

这是一个用于windows的构建方式

# 环境
编译器版本：MinGW 7.3.0 64-bit

请注意，libs目录中的链接库仅适用于**64bit**编译器和操作系统

记得将编译器的bin目录加入PATH环境变量

编译时使用的实机测试环境：Qt 5.12.3(MinGW 7.3.0 64-bit)

tip:安装包含mingw编译器的qt后直接在开始菜单中打开带环境的cmd

编译：
```
compile.bat
```
可以修改gcc为mingw32-gcc等。需要注意编译器需要和libs中使用的版本对应。

运行
```
run.bat
```
这里将libs加入环境变量了，因为需要里面的几个dll
# 环境相关的东西
依旧使用`main_linux.c`的相关程序，修改了`platform`文件夹来导入windows下的`glew`提供一个openGL的环境，同时`freetype`也是直接使用mingw32-make这条指令来编译的，得到的freetype.a手动重命名为libfreetype.a。
include目录下的文件来源于`freetype` `glew` `glfw`。

# 相关引用

NAME|LINK
----|----
FreeType|https://www.freetype.org/
glew|https://github.com/nigels-com/glew
glfw|https://www.glfw.org/
