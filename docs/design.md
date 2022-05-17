## 目标
Faultloggerd 设计上需要达成下面几个目标：
1.准确无遗漏地记录未处理退出信号的现场信息
2.提供本地方法的调用堆栈的获取方法
3.控制记录信息的大小、数量以及访问权限
4.轻量，灵活，使用简单
业界商用系统均提供了类似的功能，也有一些专注类似功能的开源项目以及商业公司。
本文档主要用于记录设计上的一些细节问题，简介可以参考[hiviewdfx\_faultloggerd](https://gitee.com/openharmony/hiviewdfx_faultloggerd/blob/master/README_zh.md)的介绍。使用可以参考[使用说明](https://gitee.com/openharmony/hiviewdfx_faultloggerd/blob/master/docs/usage.md)。

对于目标(1)，需要关注
1)现场信息的范围，目前包括信号，执行上下文，调用栈，异常栈以及虚拟内存空间映射关系。
2)异常时调用限制，例如signal handler里建议只执行async-safe的调用，这样就需要预留一些资源。
3)异常带来的其他副作用对记录的影响，例如某些进程的异常会导致设备复位从而丢失记录。
4)考虑到目标4)，我们应当进尽可能地获取并压缩信息，例如根据栈信息复原调用栈。如果记录缺失就很大概率无法支撑问题定位。

对于目标(2)，需要关注
1)内存占用及释放
2)和其他机制的冲突，例如获取本地调用栈时线程被其他信号中断
3)进程暂停时间

对于目标(3)，需要关注
1)记录老化效率
2)请求效率
3)请求冲突管理

## 和FaultLogger的关系
FaultLogger是运行在[hiview](https://gitee.com/openharmony/hiviewdfx_hiview/blob/master/README_zh.md)部件中的服务。
通过API向三方应用提供获取自身故障日志的功能，通过[hidumper]向IDE提供日志导出功能。
Faultloggerd和FaultLogger在名字上很类似，却不是一个模块，这样拆分有有以下考虑：
1.分类权限
2.更简单的功能能部署到更小的系统上
3.功能简单就更可靠