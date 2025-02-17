# WeChatFerry-Qt

这是一个Windows PC版微信注入的C++客户端，可以在自己的程序里操作微信进行收发消息等操作；使用了WeChatFerry（后面会简称wcf），因为wcf是用C++写的，所以C++客户端可以更方便调试以及学习wcf的源码。

wcf源码直接使用VS2019进行编译，对想使用其他版本编译器的同学不太友好，所以改用了cmake来构建。经测试VS2022一切正常，其他版本应该也可以。

## 依赖
### 编译工具链
- CMake >= 3.30
- Python >= 3.10
- Visual Studio 2022

### vcpkg
```
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.bat
```
然后将vcpkg.exe的路径添加到系统环境变量PATH里


## 引用
### WeChatFerry v39.3.5
微信注入框架，支持微信3.9.11.25的版本；微信安装包可以从[这里](https://github.com/lich0821/WeChatFerry/releases/download/v39.3.5/WeChatSetup-3.9.11.25.exe)获取

### SilkMp3Converter v0.9.2
语音转mp3，编译WeChatFerry需要用到

## 模块
### smc
即SilkMp3Converter，语音数据转mp3文件

### spy
wcf的dll注入核心，生成的spy.dll会被注入到微信进程

### sdk
wcf的其中一个模块，封装了一些dll注入的操作函数，主要是给其他语言的客户端使用，但咱也可以用

### nnrpc
进程通信相关，原本是直接编译到spy里面的，但由于RPC使用protobuf作为数据格式，所以客户端也能直接使用，就分离出来作为独立的模块（静态链接库）

## 构建
clone源码并进入到源码目录下
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
```

最后打开build/WeChatFerry-Cpp.sln进行编译和调试

## 运行/调试
如果在VS里面直接运行，需要先将WeChatFerry-Cpp设置为启动项。

目前客户端就一个main.cpp，比较简陋，只能获取微信接收的信息，后续完善。
如果重新启动，建议注释掉WxInitSDK和WxDestroySDK的调用，因为spy.dll注入之后，只要不退微信，就能一直hook。

想调试spy的源码，需要附加到WeChat.exe进程。