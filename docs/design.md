## 目标
Faultloggerd 设计上需要达成下面几个目标：
1.准确无遗漏地记录未处理退出信号的现场信息
2.提供本地方法的调用堆栈的获取方法
3.控制记录信息的大小、数量以及访问权限
4.轻量，灵活，使用简单
业界商用系统均提供了类似的功能，也有一些专注类似功能的开源项目以及商业公司。
本文档主要用于记录设计上的一些细节问题，简介可以参考[hiviewdfx\_faultloggerd](https://gitcode.com/openharmony/hiviewdfx_faultloggerd/blob/master/README_zh.md)的介绍。使用可以参考[使用说明](https://gitcode.com/openharmony/hiviewdfx_faultloggerd/blob/master/docs/usage.md)。

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
FaultLogger是运行在[hiview](https://gitcode.com/openharmony/hiviewdfx_hiview/blob/master/README_zh.md)部件中的服务。
通过API向三方应用提供获取自身故障日志的功能，通过[hidumper]向IDE提供日志导出功能。
Faultloggerd和FaultLogger在名字上很类似，却不是一个模块，这样拆分有有以下考虑：
1.分类权限
2.更简单的功能能部署到更小的系统上
3.功能简单就更可靠

## 已知优化点
### 类命名不准确，代码风格优化
1.寄存器名建议使用 'ARM_R1' 而不是 'REGISTER_ONE'
2.类的成员变量优化，合理判断使用struct，而不是都使用Get/Set方法
3.多个模块依赖的公共部件可以移动到common路径下，例如log，目前依赖比较乱

### 进程同时响应本地回栈以及远程回栈可能产生冲突导致部分线程无法获取调用栈
可以统一在faultloggerd管理请求，记录抓栈的目标进程以及方式

### InnerKit中提供返回字符串的接口在调用栈很长时效率较低
提供WriteRemoteStacktraceToFile以及WriteLocalStacktraceToFile的接口，通过交换pipe/file descriptor实现

### APP_FREEZE日志流程优化
1.本地回栈，远程回栈接口可能同时调用，存在冲突的可能。原因在于存在两个不同的入口：\
1.hiview发起的远程回栈
2.aafwk->应用发起的本地回栈
2.日志抓取与进程退出冲突，存在日志尚未抓完进程退出的可能
3.APP_FREEZE事件的字段目前比较混乱，需要简化
4.和卡死相关的信息目前还需要添加cpu/memory/io workload

### 日志优化
由于hilog丢失较为严重，建议导入dmesg写入，作为备选

### Hook优化
目前使用硬编码+dlpreload替换的方式实现
硬编码函数名导致重复的模板代码较多，考虑模板实现

### 暂停时间优化
非崩溃场景回栈目标会暂停整个进程，等抓取现场后再释放进程，目前在rk3568上的foundation进程时间为0.6s左右 \
由于目前回栈使用场景主要是用于定位超时卡死类问题，而不是获取当前进程的快照，所以可以考虑只暂停当前抓栈的线程 \
当然也可以直接复制出一个快照，在快照中进行回栈，快照的方式实现可能会比较复杂。对于卡死类问题，被阻塞的线程应该不会变化 \
对于超时类问题，可以在问题检测处打印workload。
