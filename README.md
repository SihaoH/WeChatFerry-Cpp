# WeChatFerry-Cpp

0.1分支引入了Qt6，主要是为了更好的处理中文字符串（其实是工作用习惯了）

整体流程：启动后注入WCF，读取config.json文件进行相关配置，等待用户登录，确认登录后开始监听微信消息，将收取到的消息放到列表里，每秒进行一次处理，若同一人/群聊超过指定时间没有新消息，就合并起来丢给大模型，大模型生成的回答就直接回复（会判断白名单，是否需要回复）

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

### Qt6.8.2
可以直接使用在线安装包按照，不过还是建议自己下载源码来构建。目前只需编译qt-base模块即可。

#### 参考
https://doc.qt.io/qt-6/getting-sources-from-git.html

https://doc.qt.io/qt-6/windows-building.html

https://doc.qt.io/qt-6/configure-options.html

#### 下载源码
`git clone --branch v6.8.2 git://code.qt.io/qt/qt5.git .\src`

#### 生成
```
mkdir build
cd build
..\src\configure.bat -init-submodules -submodules qtbase
..\src\configure.bat -debug-and-release -prefix <path-to-qt>
cmake --build . --parallel
cmake --install .
cmake --install . --config debug
```
*上述步骤有两次install，第一次默认是只安装release*
最后将`<path-to-qt>\bin`添加到系统环境变量PATH中里

### ollama
ChatRobot类接入了ollama，实现使用AI自动回复的功能。
ollama需要自行去[官网下载](https://ollama.com/)并安装运行，推荐使用qwen2.5模型，更符合聊天的场景。

## 设计思路
主要的功能和完整的流程都在Application类中实现了，DataUtil和NngClient是为了更好地管理对象的释放，使用C++类和智能指针将数据收发和数据处理封装了一层。

## 构建
clone源码并进入到源码目录下
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
```

最后打开build/WeChatFerry-Cpp.sln进行编译和调试
