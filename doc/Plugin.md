# Plugin

this is a doc to guide that how to create a plugin for this program

这是个指导如何创建插件的文档

slb采用动态库作为插件的形式

slb会在启动的时候遍历插件文件夹并加载插件

## language / 语言

[WIP]简体中文(在这)  [(WIP)English](./en/Plugin.md)

## （快速跳转）目录

- [前提条件](#前提条件)
- [插件分类](#插件分类)
- [结构定义](#结构定义)
- [插件接口定义](#插件接口定义)
- [程序接口定义](#程序接口定义)
- [使用SDK(CPP)开发插件](#使用sdkcpp)

## 免责声明

- 软件并不提供任何可以使用的API，并不具备实际性应答能力。

- 代码仅供交流学习，后果自负，作者概不负责。

## 前提条件

任意C/C++开发环境，本程序完整版（指需要有include目录）

当然你也可以选择其他语言，但是比较麻烦。

文件编码使用UTF-8, 在编译选项中最后也设置使用UTF-8

## 安全提醒

请勿使用你不信任的插件，因为插件的特殊性可能有点```问题```。

使用过程须注意信息安全，记得防毒。

## 提示

插件的SDK(CPP)已经提供在程序编译后目录下的 include/${ProjectName}/ 下

当前版本ProjectName为SlackBBot

头文件中已定义导出，还有程序的API（在SDK中）

看不懂写什么？去看示例！

<!-- ## 方法/函数分类

附：这部分保留供将来使用，实际不需要用到。

|名称类别|枚举值|功能|备注|
|-|-|-|-|
|custom|0|功能为自定义插件|暂时保留|
|init|1|初始化函数
|filter|2|过滤器
|action|3|行为|例如戳一戳，好友添加申请
|message|4|聊天消息
|notice|5|系统消息 -->

## 结构定义

参考头文件中的定义（Plugin_FunctionDefinition.h , MessageDefinition.h）

### MessageContent

in MessageDefinition.h

## 插件接口定义

### Base

```CPP
//部分定义
typedef void (*iRF)(int,const char *);//RegisterFunction
typedef void (*iRSF)(int,const char *, SpecialType, int);//RegisterSpecialFunction
typedef void (*iSM)(MessageContent);//SLB_SendMessage
```

|函数名|返回值类型|参数，<参数>...|是否必须|作用（功能）|
|:-|:-:|:-:|-|:-|
PluginName|const char *|-|T|给主程序插件名
iInit|void|int index,iRF,iRSF,iSM|T|插件初始化（index为插件的索引，后面为接口）
Filter_AfterReceiveMessage|MessageContent|F|见[filter](#filter)
Filter_BeforeSendMessage|MessageContent|F|见[filter](#filter)

<!-- #### Start

无返回值

参数

|参数名|类型|作用
|-|-|-|
index|int|插件唯一序号，类似于ID -->

### filter
|函数名|返回值类型|参数，<参数>...|作用（功能）|
|:-|:-:|:-:|:-|
[Filter_AfterReceiveMessage](#filter通用)|Bool|MessageContent|作用于收到消息后
[Filter_BeforeSendMessage](#filter通用)|Bool|MessageContent|作用于在发出消息
<!-- ~~[Fillter_BeforeIntoRoom](#fillter通用)~~|Bool|MessageContent|作用于消息进入“房间”之前 -->

#### Filter通用

|返回值|意义|
|-|-|
true|通过（允许向下传递）
false|过滤

```cpp
//模板
bool Filter_XXXXX(MessageContent message)
{
    bool _returnValue = true;
    /*Code*/
    return _returnValue;
}
```

### 特殊

#### Message的关键词匹配相关

|函数名|返回值类型|参数，<参数>...|作用（功能）|
|:-|:-:|:-:|:-|
任意|void|MessageContent|匹配到关键词触发调用（目前只能消息类型）

#### Message消息处理组相关

关键词无匹配则触发此分类，传递执行

|函数名|返回值类型|参数，<参数>...|作用（功能）|
|:-|:-:|:-:|:-|
任意|bool|[MessageContent](#messagecontent)|没匹配到关键词时触发（返回值为真允许传递执行下一个，逆否）

## 程序接口定义

<!-- ### Room
|函数名|返回值类型|参数，<参数>...|作用（功能）|
|:-|:-:|:-:|:-|
~~[Room_RegisterNewStaus](#Room_RegisterNewStaus)~~|bool|String|注册状态 -->

### Plugin

slb导出的符号

index为插件索引（slb加载时给出）

|函数名|返回值类型|参数，<参数>...|作用（功能）|备注
|:-|:-:|:-:|:-|:-:|
RegisterFunction|-|int index,const char * name|注册函数/功能|暂时只能提交[Fillter](#fillter)
RegisterSpecialFunction|-|int index,const char * name,SpecialType type,int parm|提交自定义名称的函数|暂时只能提交[特殊](#特殊)中的定义，[更多](#registerspecialfunction)

#### RegisterSpecialFunction

type和parm的关系如表

|type|parm|备注|
|:-:|:-:|-|
|Message_Tirgger|IMessageType|
|Message_Normal|-|parm值不会用到，随便填个0

### Message

|函数名|返回值类型|参数，<参数>...|作用（功能）|备注
|:-|:-:|:-:|:-|:-:|
SLB_SendMessage|-|[MessageContent](#messagecontent)|发送消息

## 使用SDK(CPP)

更多示例见[SlackBot-Plugin-Examples](https://github.com/z-w-h-m-x/SlackBot-Plugin-Examples)

封装的接口见[此](#sdkcpp封装)

SDK中定义了插件中需要实现的部分，同时封装了部分接口。

直接引用Plugin_Base.h文件，编译时需加入Plugin_Base.cpp

### SDK插件定义

|函数名|返回值类型|参数，<参数>...|是否必须|作用（功能）|
|:-|:-:|:-:|-|:-|
PluginName|const char *|-|T|给主程序插件名
Init|void|-|T|插件初始化（index为插件的索引，后面为接口）

|变量|类型|作用|
|:-:|:-:|:-:|
pIndex|int|插件索引
RegisterFunction|[iRF](#base)|SLB的RegisterFunction
RegisterSpecialFunction|[iRSF](#base)|SLB的RegisterSpecialFunction
BotSendMessage|[iSM](#base)|SLB的SLB_SendMessage

|宏定义||
|-|-|
Plugin_Extern|C风格的符号导出

### SDK(CPP)封装

Plugin_Base.h

|函数名|返回值类型|参数，<参数>...|作用（功能）|备注
|:-|:-:|:-:|:-|:-:|
SubmitFunction|-|const char * name|调用RegisterFunction|
SubmitSpecialFunction|-|SpecialType type,const char * name,int parm|调用RegisterSpecialFunction|parm的值根据type有不同的定义
SubmitMessageTrigger|-|IMessageType mType,const char * name|调用RegisterSpecialFunction|mType为触发的类型