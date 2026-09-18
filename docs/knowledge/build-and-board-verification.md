# 构建与板侧验证

## 构建

构建命令从 OpenHarmony 源码根目录执行，不在本子目录执行。

### 全量构建

```sh
./build.sh --product-name rk3568 --ccache --no-prebuilt-sdk --build-target faultloggerd_targets
```

### 单元测试构建

```sh
./build.sh --product-name rk3568 --ccache --no-prebuilt-sdk --build-target faultloggerd_tests
```

### 构建目标

| 目标 | 路径 | 产物 |
| --- | --- | --- |
| faultloggerd | `services:faultloggerd` | `/system/bin/faultloggerd` |
| processdump | `tools/process_dump:processdump` | `/system/bin/processdump` |
| dumpcatcher | `tools/dump_catcher:dumpcatcher` | `/system/bin/dumpcatcher` |
| crasher_c | `tests/crasher/crasher_c:crasher_c` | 测试崩溃器（C） |
| crasher_cpp | `tests/crasher/crasher_cpp:crasher_cpp` | 测试崩溃器（C++） |
| libdfx_dumpcatcher | `interfaces/innerkits/dump_catcher` | 抓栈库 |
| libbacktrace_local | `interfaces/innerkits/backtrace` | 本地回栈库 |
| libunwinder | `interfaces/innerkits/unwinder` | 回栈库 |
| dfx_signalhandler | `interfaces/innerkits/signal_handler` | 信号处理器库 |
| libfaultloggerd | `interfaces/innerkits/faultloggerd_client` | 客户端库 |
| crash_validator | `tools/crash_validator` | 崩溃校验器（仅 hisysevent 开启时） |

### 构建开关

`faultloggerd.gni` 中的 `declare_args`：

| 开关 | 默认 | 说明 |
| --- | --- | --- |
| `libunwinder_debug` | false | Unwinder 调试日志 |
| `processdump_minidebuginfo_enable` | true | minidebuginfo 支持 |
| `faultloggerd_hisysevent_enable` | 自动 | hisysevent 支持 |
| `faultloggerd_liteperf_enable` | true | LitePerf 支持 |
| `faultloggerd_manual_start_enable` | false | 手动启动模式 |
| `processdump_parse_lock_owner_enable` | false | 锁持有者解析 |
| `faultloggerd_enable_build_targets` | true | 构建目标开关 |

### 轻量系统

`is_ohos_lite` 定义时，部分模块不构建，使用简化路径（见各 BUILD.gn 中的 `if (defined(ohos_lite))` 分支）。

## 测试

### 测试分类

| 类型 | 路径 | 构建目标 | 说明 |
| --- | --- | --- | --- |
| 单元测试 | `test/unittest/` | `faultloggerd_unittest` | 各模块功能测试 |
| 模块测试 | `test/moduletest/` | `faultloggerd_moduletest` | 模块集成测试 |
| 系统测试 | `test/systemtest/` | `faultloggerd_systemtest` | 端到端测试 |
| 基准测试 | `test/benchmarktest/` | `faultloggerd_benchmarktest` | 性能基准 |
| Fuzz 测试 | `test/fuzztest/` | `faultloggerd_fuzzertest` | 模糊测试 |
| FuncHook 测试 | `test/funchook/` | `faultloggerd_funchook` | 信号/exit hook 测试 |

### 单元测试覆盖模块

`test/unittest/` 下每个 innerkits 模块都有对应测试：
`async_stack`、`backtrace`、`coredump`、`crash_exception`、`dump_catcher`、`faultloggerd`、`formatter`、`kernel_snapshot`、`local_handler`、`minidump`、`panic_handler`、`process_dump`、`procinfo`、`rustc_demangle`、`sigdump_handler`、`signal_handler`、`stack_printer`、`stacktrace_rust`、`unwind`。

### 运行测试

```sh
# 推送测试到设备并执行
hdc file send out/rk3568/tests/unittest/faultloggerd/xxx /data/test/
hdc shell /data/test/xxx
```

## 板侧验证

### 产物替换

修改后需要将编译产物推送到设备验证：

| 产物 | 设备路径 |
| --- | --- |
| faultloggerd | `/system/bin/faultloggerd` |
| processdump | `/system/bin/processdump` |
| dumpcatcher | `/system/bin/dumpcatcher` |
| libdfx_dumpcatcher.z.so | `/system/lib/chipset-sdk-sp` 或 `/system/lib64/chipset-sdk-sp` |
| libunwinder.z.so | `/system/lib/chipset-sdk-sp` 或 `/system/lib64/chipset-sdk-sp` |
| libdfx_signalhandler.z.so | `/system/lib/platformsdk` 或 `/system/lib64/platformsdk` |

### 替换流程

1. **停止服务**：`hdc shell service_control stop faultloggerd` 
2. **remount**：`hdc target mount`
3. **推送**：`hdc file send [local] [device]`
4. **设置权限**：`hdc shell chmod 755 [path]`
5. **恢复 SELinux 标签**：`hdc shell restorecon [path]`
6. **重启服务**：`hdc shell service_control start faultloggerd` rk3568上该命令无法生效，需要`hdc shell reboot`重启设备

### 崩溃验证

```sh
# 使用 crasher 构造崩溃
hdc shell /data/test/crasher_cpp SIGSEGV              # 触发 SIGSEGV
hdc shell /data/test/crasher_c MTCrash            # 多线程崩溃

# 查看生成的崩溃日志
hdc shell ls /data/log/faultlog/temp/
hdc file recv /data/log/faultlog/temp/cppcrash-* ./

# 使用 dumpcatcher 主动抓栈
hdc shell dumpcatcher -p [pid]
```

### 临时目录 SELinux 标签

常见问题：手动删除并重建 `/data/log/faultlog/temp` 会导致 SELinux 标签失效。

```sh
# 检查标签
hdc shell ls -Z /data/log/faultlog/temp/
# 预期：u:object_r:faultloggerd_temp_file:s0

# 修复标签
hdc shell restorecon /data/log/faultlog/temp
```

### 常用调试命令

```sh
# 查看 faultloggerd 服务状态
hdc shell pidof faultloggerd
hdc shell ps -ef | grep faultloggerd

# 查看崩溃日志
hdc shell cat /data/log/faultlog/temp/cppcrash-*

# 查看进程退出打点
hdc shell hidumper -s 1909 -a "-l"

# addr2line 解析
addr2line -e out/rk3568/exe.unstripped/[binary] [offset]
```

## 关键约束

1. **构建目录**：不要在本子目录执行 `./build.sh`，需在 OpenHarmony 根目录。
2. **ASAN 构建**：`is_asan` 时 faultloggerd.cfg 不会安装（见 BUILD.gn 中 `if (!is_asan)` 分支）。
3. **Coredump 仅 ARM64**：`tools/process_dump/coredump/` 源码仅在 `target_cpu == "arm64"` 时编译。
4. **CFI 保护**：faultloggerd 和 processdump 启用 `branch_protector_ret = "pac_ret"`。
5. **符号二进制**：带调试信息的二进制在 `out/[product]/exe.unstripped` 和 `lib.unstripped`。
