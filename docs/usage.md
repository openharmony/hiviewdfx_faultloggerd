## 异常定位

OpenHarmony中一些可能导致进程退出的信号会默认由faultloggerd的信号处理器处理，这些信息会记录在
```
/data/log/faultlog/temp
```
生成类似cppcrash-1205-1501930043627的文件
其内容类似
```
Module name:/system/bin/samgr  <- 模块名
Pid:917  <- 进程号
Uid:1005  <- 用户ID
Reason:Signal:SIGBUS(BUS_ADRALN)@0x00000157   <- 异常信息
Tid:955, Name:samgr     <- 异常线程
#00 pc 00008e06 /system/lib/libparaperm_checker.z.so     <- 调用栈
#01 pc 0000a61b /system/lib/libselinux.z.so(avc_audit+338)
#02 pc 0000ab7f /system/lib/libselinux.z.so(avc_has_perm+74)
#03 pc 0000c5a5 /system/lib/libselinux.z.so(selinux_check_access+152)
#04 pc 00007a6f /system/lib/libservice_checker.z.so
#05 pc 00007127 /system/lib/libservice_checker.z.so
#06 pc 00016723 /system/bin/samgr
#07 pc 00016cb3 /system/bin/samgr
#08 pc 000395af /system/lib/libipc_core.z.so(_ZN4OHOS13BinderInvoker13OnTransactionEPKh+374)
#09 pc 000391db /system/lib/libipc_core.z.so(_ZN4OHOS13BinderInvoker14HandleCommandsEj+258)
#10 pc 00039095 /system/lib/libipc_core.z.so(_ZN4OHOS13BinderInvoker13StartWorkLoopEv+52)
#11 pc 00039845 /system/lib/libipc_core.z.so(_ZN4OHOS13BinderInvoker10JoinThreadEb+32)
#12 pc 00035467 /system/lib/libipc_core.z.so(_ZN4OHOS13IPCWorkThread13ThreadHandlerEv+106)
#13 pc 00035723 /system/lib/libipc_core.z.so
#14 pc 000c2270 /system/lib/ld-musl-arm.so.1
#15 pc 0005c444 /system/lib/ld-musl-arm.so.1
```
一般在版本编译时会生成带调试信息的二进制，其位置在
```
\代码根路径\out\产品\lib.unstripped
\代码根路径\out\产品\exe.unstripped
```
例如rk3568开发板产品生成的携带调试信息的二进制在
```
OpenHarmony\out\rk3568\exe.unstripped
```
能够使用 addr2line工具进行偏移到行号的解析
```
addr2line -e [path to libparaperm_checker.z.so] 00008e06
```