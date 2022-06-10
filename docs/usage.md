## 日志存放路径及结构

OpenHarmony中一些可能导致进程退出的信号会默认由faultloggerd的信号处理器处理，这些信息会记录在以下路径：\
```
/data/log/faultlog/temp
```
其文件名类似cppcrash-1205-1501930043627，为cppcrash-进程号-发生时间
其内容类似
```
Pid:1205  <- 进程号
Uid:0  <- 用户ID
Process name:com.ohos.launcher  <- 模块名
Reason:Signal:SIGSEGV(SEGV_MAPERR)@0x00000004    <- 异常信息
Fault thread Info:
Tid:1044, Name:com.ohos.launch <- 异常线程
#00 pc 0004f8ac /system/lib/libark_jsruntime.so  <- 调用栈
#01 pc 0015aa11 /system/lib/libark_jsruntime.so
#02 pc 001602fb /system/lib/libark_jsruntime.so
#03 pc 00130e27 /system/lib/libark_jsruntime.so
#04 pc 00145157 /system/lib/libark_jsruntime.so
...
Registers:   <- 异常现场寄存器
r0:00000000 r1:00000010 r2:91079268 r3:00000000
r4:91079268 r5:90e00000 r6:90e289c0 r7:91040000
r8:0000001d r9:91079268 r10:00000000
fp:914fec00 ip:00000423 sp:becc4960 lr:b5b0ea15 pc:b5a038ac
FaultStack:   <- 异常线程栈帧，目前向低地址展示4*位宽，向高地址展示16*位宽。
Sp0:becc4960 00a98748    <- 第0帧栈帧
    becc4964 00a98758
    becc4968 b6f77000
    becc496c 8a6a98df
    becc4970 0000001d
    becc4974 91079268
    becc4978 00000000
    becc497c b5b0ea15 /system/lib/libark_jsruntime.so
Sp1:becc4980 becc4a04    <- 第1帧栈帧
    becc4984 8a6a98df
    becc4988 b6f77000
    becc498c 00a9eb30
...
Maps:   <- 虚拟内存空间
3eff96000-3eff97000 rw-s 3eff96000 /dev/mali0
3eff97000-3eff98000 rw-s 3eff97000 /dev/mali0
7febc70000-7febc91000 rw-p 00000000 [stack]   <- 栈
...
```

## 日志规格的配置
日志文件展示的字段可由配置文件配置，在源码仓的路径为 services/config/faultlogger.conf \
版本的路径为 system/etc/faultlogger.conf \
```
faultlogLogPersist=false  <- 是否持久化调试日志，包含代码流程日志以及调试日志
displayRigister=true  <- 是否展示故障寄存器信息
displayBacktrace=true
displayMaps=true  <- 是否展示虚拟内存空间映射
displayFaultStack.switch=true <- 是否展示崩溃线程栈内存
displayFaultStack.lowAddressStep=16 <- 崩溃线程堆栈内存向高地址读取的块的数量(逆栈生长方向)
displayFaultStack.highAddressStep=4 <- 崩溃线程堆栈内存向低地址读取的块的数量(顺栈生长方向)
dumpOtherThreads=false <- 是否展示崩溃进程非崩溃线程的信息
```

## 通过日志定位问题

通过崩溃进程名一般能定界到故障的模块，通过信号能大致猜测崩溃的原因。 \
如范例中的SIGSEGV由内核MemoryAborter产生，原因为访问了非法内存地址，范例中的为0x00000004，高概率是结构体成员空指针。\
Fault Thread Info包含崩溃的线程名以及调用栈，大部分场景下栈的最上层就是崩溃的原因，如空指针访问以及程序主动abort。\
少部分场景调用栈无法定位原因，需要查看其他信息，例如踩内存或者栈溢出。 \

首先根据 寄存器的SP和MAPS中的stack查看是否发生了栈溢出。如果SP不在栈范围内或者靠近栈的下界，应当考虑发生了栈溢出。 \
其次使用addr2line工具解析崩溃栈的行号，这里需要使用带调试信息的二进制。一般在版本编译时会生成带调试信息的二进制，其位置在 \
```
\代码根路径\out\产品\lib.unstripped
\代码根路径\out\产品\exe.unstripped
```
例如rk3568开发板产品生成的携带调试信息的二进制在 \
```
OpenHarmony\out\rk3568\exe.unstripped
```
能够使用 addr2line工具进行偏移到行号的解析，需要注意的是需要使用与崩溃二进制匹配的带调试信息的二进制 \
```
addr2line -e [path to libmali-bifrost-g52-g2p0-ohos.so] 94e0bc
```
使用addr2line后，如果得出的行号看起来不是很正确，可以考虑对 地址进行微调(如减1)，或者考虑关闭一些编译优化，已知使用LTO的二进制可能无法正确获得行号。 \

## 常见引发崩溃的原因
目前监控的信号列表可以参考README,这里主要介绍下常见的几种原因:
1.空指针 \
形如SIGSEGV(SEGV_MAPERR)@0x00000000或r0 r1等传参寄存器的值为0时应首先考虑调用时是否传入了空指针 \
2.结构体/类未初始化指针 \
形如SIGSEGV(SEGV_MAPERR)@0x0000000c或 r1等传参寄存器的值为一个很小的值时应考虑调用入参的结构体成员是否包含空指针 \
3.程序主动终止 \
SIGABRT一般为用户/框架/C库主动触发，大部分场景下跳过C库/abort发起的框架库的第一帧即为崩溃原因 \
这里主要检测的是资源使用类的问题，如线程创建，文件描述符使用，接口调用时序等 \
4.多线程操作集合 \
std的集合为非线程安全，如果多线程添加删除，容易出现SIGSEGV, 如果使用addr2line后的代码行与集合相关，可以考虑这个原因 \
5.不匹配的对象生命周期 \
如使用裸指针保存sptr类型以及shared_ptr类型 \
6.返回临时变量、野指针 \
如返回栈变量的引用，释放后未置空继续访问 \
7.栈溢出 \
如递归调用，析构函数相互调用，特殊的栈(信号栈)中使用大块栈内存 \
8.二进制不匹配 \
如自己编译二进制与实际运行的二进制接口存在差异，数据结构定义存在差异 \
9.踩内存 \
使用有效的野指针，并修改了其中的内存为非法值，访问越界，覆盖了正常的数据 \
10.资源泄漏 \
如虚拟内存泄漏，文件句柄泄漏，线程句柄泄漏等 \
11.提前释放了使用dlsym的函数所在的动态库\
这时可能导致pc、lr以及sp均为无效值\

## 常见问题指引
Q1.崩溃问题的分析步骤 \
1)查看信号值，sigabort查看调用abort的代码，sigsegv,sigill以及sigbus查看寄存器 \
2)查看崩溃地址，signal会携带崩溃访问的地址，如load 非法地址会触发 sigsegv，branch 到合法地址但地址指向的不是代码段会触发sigill \
3)查看寄存器以及虚拟空间的栈地址，检查是否为栈溢出问题 \
4)解析崩溃栈，使用addr2line工具解析到行号，如果是简单的指针访问，这一步大概率能看出问题 \
5)反汇编，可以借助一些商用工具如IDA辅助分析，查看寄存器与代码参数的映射关系，排查可能问题的对象 \
6)使用地址越界检查工具如ASAN,TSAN \

Q2.为什么addr2line无法到行 \
一般行与汇编的关系保存在调试信息中，如果使用了一些编译优化，如LTO会导致一些信息没有保存在Unstripped的二进制中 \
行与汇编为一对多的关系，一些优化可能讲多个代码块编译到一个routine而没有保留映射关系。可以尝试微调相对地址。 \

Q3.为什么有时候栈看起来不完整 \
可能有以下几种原因：\
1)回栈(Unwind)原理上是靠递归读取栈上信息查找前一帧的地址，如果栈帧被覆盖修改，则可能回栈失败 \
2)二进制不包含unwind-table或者unwind-table生成有问题 \

Q4.进程退出却没有crash日志 \
进程退出的原因不一定是未处理的崩溃信号，目前在appspawn/init中添加了进程退出的打印 \
AppSpawn打印到Hilog中的日志如下：
```
93 93 W 0a011/APPSPAWN: [appspawn_service.c:141]com.ohos.launcher with pid 1368 exit with signal:11
93 93 W 0a011/APPSPAWN: [appspawn_service.c:141]ohos.samples.flashlight with pid 1260 exit with signal:6
```
如果hilog丢失还可以使用hisysevent命令查看PROCESS_EXIT打点:
```
{"domain_":"STARTUP","name_":"PROCESS_EXIT","type_":4,"time_":10272435,"tz_":"+0000","pid_":94,"tid_":94,"uid_":0,"PROCESS_NAME":"com.ohos.launcher",
"PID":1005,"UID":20010010,"STATUS":9,"level_":"CRITICAL","tag_":"Stability","id_":"178184357230479813","info_":""}
```
status可以直接使用WIFSIGNALED/WIFEXITED处理，获得实际的exit code 或者signal。\

Init中打印日志如下：
```
[pid=1 0][Init][INFO][init_signal_handler.c:47]SigHandler, SIGCHLD received, Service:misc pid:351 uid:0 status:0.
```
上述日志表明mis服务正常退出，exit code为0.

Q5.打开CoreDump的方法 \

由于rlimit中对core的限制，版本默认不会生成coredump文件，可以手工打开指定进程的coredump
```
toybox prlimit -c -H unlimited -P [pid of target process] \\ 设置Hard limit
toybox prlimit -c -S unlimited -P [pid of target process] \\ 设置Soft limit
echo /data/log/coredump.%p.bin > /proc/sys/kernel/core_pattern \\ 设置生成文件模板
```
如果需要在版本中打开，可以在init.cfg添加类似的命令，设置生成文件的名称以及最大的文件大小

Q6.GDB的位置 \
版本中未提供，可以使用[三方编译的版本](https://gitee.com/stesen/ohos_cross_tools)

Q7.进程崩溃退出而没有日志可能原因? \
如果进程使用了signal/sigaction/sigprocmask等函数屏蔽/拦截了信号的分发，也会导致进程由于信号退出时没有日志。 \
这时可以使用sighook机制来定位,libdfxsighook已经预置到版本中，可以通过修改配置文件加载该so。\
配置文件的路径为:system/etc/init/faultloggerd.cfg
```
            "name" : "pre-init",
            "cmds" : [
                "export LD_PRELOAD libdfxsighook.z.so:libdfx_signalhandler.z.so" // sighook需要放在sighandler之前
            ]
```
加载后会输出如下日志:
```
 [dfx_func_hook.c:67]2361 send signal(9) to 1461 // 进程 1261 给 进程 1461 发送 SIGKILL
 [dfx_func_hook.c:130]current signalhandler addr:0x7faf8199ec, original addr:0x7fafa4ee8c // 使用sigaction替换了系统的signalhandler
```
如果自身的代码里没有使用，可以考虑排查是否引入的三方库中拦截、替换了信号处理。一个例子如下：
```
https://gitee.com/openharmony/third_party_pulseaudio/pulls/20
```

