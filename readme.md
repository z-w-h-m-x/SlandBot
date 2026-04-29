# SlandBot

暂时以Onebot11标准作为接口的API请求框架，未来有更多

注意：项目名缩写为SLB

项目处于早期开发阶段，很多都不完整，但是能用

## 免责声明

- 软件并不提供任何可以使用的实现API接口，并不具备实际性应答能力，请勿用于违法犯罪

- 代码仅供交流学习，后果自负，作者概不负责

- 项目名的缩写**不**为```S B```，并没有任何辱骂人的意思，只是巧合

- 作者指所有贡献者（SlandBotDevelopments）

## TODO

- [] 配置读写
- [] 终端交互（目前存在编码问题）
- [] 完善对onebot11标准的适配
- [] 

## GPLv3作用域

include/Plugin/Plugin文件夹中的内容不使用GPLv3协议，使用MIT（对应目录下有LICENSE文件）

未在此标明的，使用GPLv3协议

## 使用

注意！这只是框架，你所想要的功能可能需要安装或自行编写插件

### 预构建(也许有)

前提：需安装SDL2运行时文件（系统路径可以找到）

给Windows用户：预构建包没有含有sdl2运行时，可以自行去下载后放置在软件目录下

前往Releases下载对应的版本，运行slackbot即可

### 构建

参阅开发

## 开发

目前，此项目只在Windows平台上使用MinGW生成过，暂时未测试其他平台和其他编译器，欢迎补充

- [[WIP]插件开发看这](./doc/Plugin.md)   
- [更多插件示例](https://github.com/z-w-h-m-x/SlackBBot-Plugin-Examples)

### 需求

- cmake
- SDL2
- [json](https://github.com/nlohmann/json)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)

在项目根目录创建名为```thirdparty```的文件夹，其中放入json和httplib的头文件

like this
```
thirdparty
|-- httplib.h
|-- json.hpp
```

### 快速开始

需满足需求

```shell
mkdir build & cd build & cmake .. & make
```

注意：可能需要指定cmake配置生成器

e.g.
```shell
cmake .. -G "MinGW Makefiles"
```

### 贡献

欢迎参与改进、优化项目

欢迎提交issue反馈或者贡献代码

issue可使用中英任何一语（在文档翻译前估计都是中文（（（（）

贡献代码规范

- 文件编码使用**UTF-8**
- 注释中英都可（建议简中以及搭配双语翻译（机翻也可）），请勿使用其他语言
- **建议**命名规则：变量等小驼峰命名法，函数等大驼峰命名法，宏定义下划线。允许加缩写和标签但是需要写注释解释(如果上下文可以在几行内就可以找到缩写对应的全称可以不用加注释解释，或者在文件开头统一解释)

