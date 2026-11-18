---
layout: post
title: C/C++ 编码风格指南
category: AUTOSAR
comments: true
---

# AS C/C++ 编码风格指南

## 1. 文档目的

本文档定义 SSAS 项目中 C/C++ 代码（`infras/`、`app/`、`tools/libraries/`、`tools/asone/`）的编码风格。这些规则**不是凭空设计的**——全部提炼自现有代码库，并且由仓库根目录的 `.clang-format` 机械强制执行，因此大部分排版工作会自动完成。本指南解释这些约定的内容、原因，以及 clang-format 无法覆盖的规则（命名、文件组织、日志、错误处理）。

提交前快速检查：

```bash
clang-format -i <file.c>          # 排版，读取仓库根目录的 .clang-format
python tools/format.py -i <file.c>  # 规范化段横幅
python tools/cppcheck/cppcheck.py # 静态检查，规则见 tools/cppcheck/MISRA_C_2012.txt
```

---

## 2. 文件组织

### 2.1 模块的文件组成

每个 AUTOSAR 模块独占一个目录（如 `infras/communication/CanTp/`），包含：

| 文件 | 内容 |
|---|---|
| `<Module>.c` | 全部实现代码 |
| `<Module>.h` | 对外 API，其他模块只允许包含这一个头文件 |
| `<Module>_Priv.h` | 模块内部共享的私有宏/类型 |
| `<Module>_Cfg.h` | 生成的配置头文件（来自 `tools/generator/<Module>.py`），包含时用 `#ifndef <MODULE>_CFG_H` 保护 |
| `SConscript` | 构建注册 |

例如：`CanTp.c`、`CanTp.h`、`CanTp_Priv.h`、生成的 `CanTp_Cfg.h`。

### 2.2 版权头

每个 `.c/.h/.hpp/.cpp` 文件以同一段注释开头，实现具体规范的文件可选加 `ref:` 行注明出处：

```c
/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Transport Layer AUTOSAR CP Release 4.4.0
 */
```

年份使用文件创建年份。Python 文件使用相同的横幅，只是改用 `#`。

### 2.3 段横幅

每个 C/C++ 文件按固定顺序划分段落，每段用一行精确的横幅标记；空段也保留（见 `CanTp_Priv.h`）。手工敲这些横幅很容易敲错，`tools/format.py -i <file>` 可以将其规范化为标准文本：

```c
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
```

各段内容：

- **INCLUDES** —— 头文件；本模块头文件在最前，然后是其他 AS 头文件、系统头文件（`<string.h>`）、第三方库（如 mbedtls）。条件包含（`#ifdef USE_DOIP ... #endif`）就地书写，不要挪到最后。**不要对 include 排序**——`.clang-format` 中 `SortIncludes: false`，顺序是有意义的（例如 `Std_Types.h` 必须在依赖它的头文件之前）。
- **MACROS** —— `AS_LOG_*` 定义在最前，随后是特性宏、协议常量。
- **TYPES** —— 本文件使用的结构体/枚举。
- **DECLARES** —— extern 声明与 `static` 函数的前向声明。
- **DATAS** —— 文件级变量：模块私有的 `static` 状态（`cantpConfig`），以及模块自己的配置文件里导出的对象（`Dcm_Config`）。
- **LOCALS** —— `static` 函数定义。
- **FUNCTIONS** —— 对外 API 定义。

---

## 3. 命名约定

### 3.1 公开函数

`<Module>_<PascalCase>` —— 模块前缀是强制的：

```c
Std_ReturnType CanTp_Transmit(PduIdType txPduId, const PduInfoType *pduInfoPtr);  /* 正确 */
void CanTp_MainFunction(void);                                                    /* 正确 */
```

### 3.2 静态（局部）函数

**同样使用 `<Module>_<PascalCase>`。`static` 改的是链接属性，永远不是名字。** 文件内私有的函数依然是这个模块的私有 *API*，不是局部变量：它会出现在调试器调用栈、`ASLOG` 打印和每次构建的符号表里，而且多个模块会落进同一个编译单元（`LinTp.c` 以文本包含方式编译 `CanTp.c`）——一个叫 `nmInit3` 的栈帧，既看不出属于哪个模块，也看不出在做什么。

```c
static void CanTp_ResetToIdle(CanTp_ChannelContextType *context);  /* 正确 */
static void CanTp_SendFC(PduIdType rxPduId, uint8_t flowStatus);   /* 正确 */
static void OsekNm_Init3(NetworkHandleType NetId);                 /* 正确 */

static void nmInit3(NetworkHandleType NetId);        /* 错误 —— camelCase，无模块前缀 */
static void CanTp_resetToIdle(...);                  /* 错误 —— 下划线后又接 camelCase */
static void ResetToIdle(...);                        /* 错误 —— “它是私有的”不是理由 */
```

这条规则同样管模块注册到表里的回调，例如 shell 命令：写 `Com_CmdLsSgFunc`，不写 `cmdComLsSgFunc`。注意与 3.5 节刻意形成的对照：文件级静态**变量**用小写模块前缀（`cantpConfig`），静态**函数**用真正的 `Module_` 前缀（`CanTp_ResetToIdle`）——函数名读起来应当像它所代表的 API。

下面这条命令打印出来的每一个结果都是待修项：

```bash
grep -rnE '^static [A-Za-z_][A-Za-z0-9_ *]*[ *]([a-z][A-Za-z0-9_]*)[[:space:]]*\(' infras --include=*.c
```

移植过来的第三方源码保持上游命名（`TcpIp/config/TcpIp_Cfg.c` 里的 lwIP netif 移植层、`libraries/libc`、`libraries/heap`、`libraries/factory`）：改这些名字会让下一次同步上游的 diff 无法阅读。我们自己写的代码一律带模块前缀。

同理，`infras/system/fs`（vfs 适配层）与 `infras/system/kernel/os`（POSIX 内核移植层）里的历史 `static` 函数（`vfs_open` 之外尚存的小写名字）也列为**已知例外**：这两处是对接外部 syscall 语义的适配层，存量约定已久，不要求一次性改齐；但**新写入这两个目录的代码仍须遵守本节**，否则上面那条 grep 永远清不到零。

### 3.3 类型

采用 AUTOSAR 风格的 `Type` 后缀，`struct`/`enum` 匿名定义、标签写在右花括号上：

```c
/* 错误 —— 同一个结构体里混入了 camelCase 成员 */
typedef struct {
  uint16_t AuthPduOffset;
  uint8_t bufLen;
} SecOC_TxPduProcConfigType;

/* 正确 —— 成员全部 PascalCase，与 AUTOSAR SWS 一致 */
typedef struct {
  uint16_t AuthPduOffset;
  uint8_t BufLen;
} SecOC_TxPduProcConfigType;

typedef enum {
  CANTP_EXTENDED,
  CANTP_MIXED,
} CanTp_AddressingFormatType;      /* 正确 */

typedef uint8_t SecOC_RxPduProcStateType;   /* 给普通类型起别名，同样以 ...Type 结尾 */
```

**需要前置声明或自引用的结构体，标签用 `..._s`，typedef 名用 `...Type`，二者一一对应。** 这是 BSD/内核代码的惯例（`struct loader_s`），写成一条固定的配对规则后，`struct Xxx_s` 与 `XxxType` 永远指向同一个类型，队列宏（`STAILQ_ENTRY`/`STAILQ_REMOVE` 的 type 参数要的是标签）与前置声明也不会再各写各的：

```c
typedef struct Loader_s Loader_Type;   /* 头文件里前置声明 */

struct Loader_s {                      /* .c 里给出完整定义 */
  Srec_FileType *AppSRec;
  ...
};

typedef struct IsoTp_s IsoTp_Type;     /* 同样的配对：IsoTp_s / IsoTp_Type */
```

不需要自引用的简单结构体仍然用匿名定义（`typedef struct { ... } XxxType;`），不必为了标签而标签。
**结构体/联合体成员一律使用 `PascalCase`，且同一个结构体内不允许混用。** 这是 AUTOSAR 的惯例（`PduInfoType.SduLength`、`SduDataPtr`、`MetaDataPtr`、`CanNmMsgCycleTime`），遵循它能让自研类型与 SWS 中的类型保持一致，熟悉标准的人读起来毫无障碍。按类型统一决策，而不是逐个成员决定：第一个成员写成 `AuthPduOffset`，下一个就是 `BufLen`，而不是 `bufLen`。指针成员的 `*` 跟在变量名上：`uint8_t *Buffer;`。

与 ECUC/SWS 参数一一对应的成员，必须保留规范中的确切拼写（`MsgCycleTime`、`SoConId`、`RequestAddressAssignment`）。生成器正是按该名字产生指定初始化器（`.MsgCycleTime = ...`），改一个名字就要同时改生成器和喂给它的每一个 JSON 键——这属于独立的计划性修改，绝不做顺手改动。

标准规定的时序与流控参数是有意的例外：`N_As`、`N_Bs`、`N_Cr`、`BS`、`STmin`、`N_TA` 与 `BlockSize`、`LinkLayerDataLength` 之外的同族成员排在一起时，仍按 ISO 15765-2 的原文书写（见 3.7）。判断标准是「没读过标准的人能否从名字反查到它」：`N_Bs` 一望即知属于 `N_` 时序参数族，`BS`、`STmin` 是标准里的原词。反过来说，`LL_DL` **不在**例外内——它表达的是一个可读的概念（链路层数据长度），必须写成 `LinkLayerDataLength`；一份例外清单只收标准原文，不收自己发明的缩写。

注意这条分界：**成员用 `PascalCase`，局部变量和全局变量用 `camelCase`**（见 3.5）。不要把 `PascalCase` 带到局部变量上。

枚举值为 `ALL_CAPS` 并带模块前缀（`CANTP_IDLE`、`CANTP_EXTENDED`）。

### 3.4 宏与常量

宏分两类，对应两种写法。判断依据是它在调用点「是什么」，而不是它恰好由 `#define` 定义。

**对象式宏（不带参数）**——它们是常量，用 `全大写下划线分隔`，并且一律带模块前缀。**加了 `static` 也不能去掉前缀**：文件私有的宏仍然属于本模块，而别人在日志、调试器监视窗口或跨模块的 grep 里看到 `N_PCI_SF` 时，根本无从判断它归谁管。

```c
#define CANTP_N_PCI_SF  0x00u   /* 正确 */
#define CANTP_N_PCI_CTS 0x30u   /* 正确 */

#define N_PCI_SF  0x00u         /* 错误 —— 出了 CanTp.c 就看不出归属 */
```

「文件私有」是链接属性，不是命名作用域；而且它连稳定都谈不上——`LinTp.c` 以文本包含方式编译 `CanTp.c`，看起来只属于一个文件的宏，实际上进了两个编译单元。

**函数式宏（带参数）**——用 `ModulePascalCase`：模块名保持 PascalCase，**不加下划线**，直接跟名字。它们是这样被调用的：`CanTpSetAlarm(100u)`，因此命名必须和它伪装的那个调用一致，模块前缀的作用与函数完全相同。

```c
#define CanTpSetAlarm(v)      /* 正确 —— 与调用写法一致 */
#define CanTpCancelAlarm()    /* 正确 */
#define CanTpIsAlarmTimeout() /* 正确 */

#define CANTP_SET_ALARM(v)    /* 错误 —— 像常量表达式，实际可能藏着 do/while */
```

`CANTP_SET_ALARM` 是两者中更危险的陷阱：调用点看起来是个常量表达式，宏体里却可能是 `do { ... } while (0)`、一个 `return`，或者带副作用。读起来像值的东西就该是值；会执行语句的东西就该长成调用的样子。

只包几条语句的宏，本质就是一个伪装成宏的函数，因此要拿函数的标准来判断它——包括模块前缀。只在一个 `.c` 里出现的 `nmSendMessage()`、`sdEnterCritical()`，和 `static void nmSendMessage(...)` 是同一种缺陷：调用点看不出归属，而宏体里可能正在调用 `CanIf_Transmit`、启动定时器。

```c
#define OsekNmSendMessage()   /* 正确 */
#define ComMEnterCritical()   /* 正确 */

#define nmSendMessage()       /* 错误 —— 小写、无模块前缀 */
```

**例外——取值转换类宏。** 如果函数式宏只计算一个值，既无副作用也没有语句体，可以保留全大写形式，因为它确实是「带参数的常量表达式」：

```c
#define CANTP_CONVERT_MS_TO_MAIN_CYCLES(x)   /* 可接受 */
#define DCM_CONVERT_MS_TO_MAIN_CYCLES(x)     /* 可接受 */
```

这是**容忍**而非推荐：`CanTpConvertMsToMainCycles(x)` 才是建议写法，新代码应该用它。这个例外的成立前提是一个判断（「这个宏没有副作用」），而哪天有人往宏体里加了一条语句，前提就悄悄失效了——那时全大写的名字就在撒谎。

**例外——注册/日志类既有约定宏。** `SHELL_REGISTER(...)`、`DET_VALIDATE(...)`、`ASLOG`/`ASPRINT`/`ASHEXDUMP` 保持全大写：它们的前缀（`SHELL_`/`DET_`/`AS`）在调用点已经说明了归属，而且被生成器模板、历史配置和数百处调用点引用，改名不带来任何可读性收益，只留下大片无意义的 diff与新老写法混杂。这类宏按原样保留，不按上两条改名。

特性宏遵循 `USE_<LIB>_<FEATURE>` 或 `<MODULE>_<FEATURE>`（`USE_DOIP`、`CANTP_STMIN_ADJUST`）。无符号字面量加 `u` 后缀：`0xF0u`、`1u`。

### 3.5 变量

- 局部变量：`camelCase`；只有循环下标或只出现两次的值才用真正的短名（`i`、`pos`、`bufReq`）——不用匈牙利前缀（`lLL_DLs`），也不用两个字母的晦涩缩写。
- 文件级静态变量：`static`、`camelCase`，并且**必须带小写的模块名前缀**——`cantpConfig`、`secocContext`、`dcmContext`、`ecumContext`、`linifSchedule`。这个前缀不是装饰：这些变量是模块级全局状态，而 `LinTp.c` 是以文本包含的方式编译 `CanTp.c` 的，两个模块可能落进同一个编译单元，此时一个光秃秃的 `config` 在调试器和日志里都会分不清归属。前缀是模块名整体小写（`CanTp` → `cantp`，`EcuM` → `ecum`），不是 `CanTp_`，也不是 `CanTpConfig`。

  ```c
  static const CanTp_ConfigType *cantpConfig = NULL;   /* 正确 */
  static Dcm_ContextType dcmContext;                   /* 正确 */

  static Dcm_ContextType Dcm_Context;                  /* 错误 —— 看起来像类型名 */
  static const CanTp_ConfigType *config = NULL;        /* 错误 —— 没有模块前缀 */
  ```

  `s_` / `g_` 前缀本身并不是禁用的——在一般的 C/C++ 代码里，`static Dcm_ContextType s_Context;` 可以接受。只是在 AUTOSAR 模块内部**不推荐**：`s_Context` 只说明「这是个静态变量」，而 `dcmContext` 说明「这是 Dcm 的上下文」。在一个每个模块都长得像 SWS 的代码库里，模块前缀才是携带关键信息的部分。

  C++ 中同样的用途优先用匿名命名空间代替 `static`，命名规则不变（`namespace { int semaphoreCount; }`）。

- 外部链接的全局变量：**`Module_PascalCase`**——模块名保持 PascalCase，加下划线，再跟变量名：`Dcm_Config`、`CanTp_Config`、`PduR_Config`、`Std_TraceArea_APP`。这两档是刻意分开的，不能混：

  | 链接属性 | 形式 | 示例 | 原因 |
  |---|---|---|---|
  | 内部链接（`static`） | `<module>camelCase`，模块名小写 | `cantpConfig`、`dcmContext` | 文件外不可见，读起来就是局部状态 |
  | 外部链接（`extern`） | `<Module>_PascalCase` | `Dcm_Config`、`PduR_Config` | 属于模块的公开面，命名与它的函数、类型保持一致 |

  典型场景是配置对象，采用 AUTOSAR 的内存段写法：头文件里 `extern CONSTANT(<类型>, <MODULE>_CONST) <名字>;`，模块内只定义一次。

  ```c
  /* Dcm_Priv.h */
  extern CONSTANT(Dcm_ConfigType, DCM_CONST) Dcm_Config;

  /* Dcm_Cfg.c（或生成的 GEN/Dcm_Cfg.c） */
  const Dcm_ConfigType Dcm_Config = { ... };

  extern const Dcm_ConfigType dcm_Config;   /* 错误 —— 全局变量用了小写模块前缀 */
  extern const Dcm_ConfigType DCM_CONFIG;   /* 错误 —— 看起来像宏 */
  extern const Dcm_ConfigType Config;       /* 错误 —— 完全没有模块前缀 */
  ```

  名字不以模块开头的全局变量，是读者唯一无法反查归属的那类标识符：`Dcm_Config` 用 `grep Dcm_` 就能找到，在符号表里也和本模块的其他符号排在一起；而 `dcm_Config` 会混在 `dbg_*` 和 `dummy` 之间，`DCM_CONFIG` 在评审时会被当成宏。
- 函数参数：同样用 `camelCase`，实现 SWS API 的函数也不例外——参数名不属于 ABI，因此按我们自己的风格写，而不是照抄规范的拼写（`pduInfoPtr`、`txPduId`）。
- **`Ptr` 后缀是可选的，不是必须的。** 指针参数写成 `data`、`canFrame`、`rxNotification` 完全可以；写成 `dataPtr`、`canFramePtr` 同样可以。要表达的是「这个参数是什么意思」，而不是「它是个指针」——读代码的人从 `*` 和前一个词的上下文就能看出是指针。选一种写法后在同一个模块/同一个 API 里保持一致即可，不要出现 `dataPtr` 与 `canFrame` 混排这种前后不一的情况。
- 缩写有歧义时要写全。参数位置不是用协议简写的地方：`LL_DL` 应写作 `linkLayerDataLength`，既不是 `llDl` 也不是 `LL_DL`。生成接口遵循同一规则：JSON 键是 `LinkLayerDataLength`（见 3.7），不是 `LL_DL`，也不是压到两个字母的 `LlDl`；`N_` 系列时序键写作 `N_TargetAddress`，不是 `N_TA`。
- 结构体成员：`PascalCase`（AUTOSAR 惯例，见 3.3），纯 C 代码不用 `m_` 前缀——与局部变量相反，局部变量保持 `camelCase`。

### 3.6 C++（tools/libraries、tools/asone）

| 元素 | 规则 | 示例 |
|---|---|---|
| 命名空间 | 可复用库代码放 `as` | `namespace as { ... }` |
| 类 | `PascalCase` | `class Semaphore {` |
| 方法 | 我们自己写的**所有**类 API 都用 `PascalCase` | `IsTransmit()`、`Wait()`、`Post()` |
| 成员变量 | `m_` + `PascalCase`，无例外 | `m_Sem`、`m_Msg`、`m_LeData` |
| 局部变量 | `camelCase`，多用 `auto` | `auto vbox = new QVBoxLayout();` |

**`m_` 只是前缀，不改变后面的命名风格。** 前缀之后是 PascalCase 的成员名，因此首字母必须大写：写 `m_LeData` 而不是 `m_leData`，写 `m_AxisX` 而不是 `m_axisX`，写 `m_RxBuffer` 而不是 `m_rxBuffer`。名字里的缩写保持自身大小写（`m_Isotp`）。

**布尔成员用谓词前缀，不用类型前缀。** `m_` 之后直接写判断式：`m_IsProtected`、`m_HasPayload`、`m_CanTransmit`，不写 `m_bProtect`、`m_bPayload`。`b` 前缀把类型写进了名字，读出来却读不出含义——`m_bProtect` 看不出「什么时候为真」，`m_IsProtected` 一句话就是它的条件；连 `true == m_bValid` 这种为了消歧义才写的比较，在 `m_IsValid` 之后也成了噪音。同理不写 `m_nBits`、`m_byLua`：`n`、`by` 和 `b` 是同一类匈牙利前缀，见 3.5 对局部变量的同一条禁令。

**我们自己声明的每个方法都用 `PascalCase`，小型工具类也不例外**——`Semaphore::Wait()` / `Post()`，而不是 `wait()` / `post()`。唯一保留小写开头的是 Qt 自己的名字，这些不能改名：

- 虚函数重写：`timerEvent()`、`paintEvent()`、`mousePressEvent()`
- `uic` 生成的自动连接槽：`on_btnStart_clicked()`
- `QObject` 接口本身：`connect()`、`setStyleSheet()`

**头文件保护宏：文件名全大写，首尾都不加下划线。** `.h` 与 `.hpp` 完全同一条规则——扩展名的点换成 `_`、整体大写，两端什么都不加：

| 文件 | 保护宏 | 错误写法 |
|---|---|---|
| `CanTp_Priv.h` | `CANTP_PRIV_H` | `_CANTP_PRIV_H_`、`CANTP_PRIV_H_` |
| `Mirror.h` | `MIRROR_H` | `_MIRROR_H_` |
| `Semaphore.hpp` | `SEMAPHORE_HPP` | `_SEMAPHORE_HPP_` |
| `BufferPool.hpp` | `BUFFERPOOL_HPP` | `_BUFFER_POOL_HPP_` |

以「下划线 + 大写字母」开头的标识符，在 C（C11 7.1.3）和 C++（`[lex.name]`）中都是保留给实现使用的；首尾都带下划线的名字更是无条件保留。这样的保护宏属于未定义行为：可能与编译器或库内部符号冲突，`-Wreserved-identifier` 也会报警。尾部下划线没有这个风险，但同样没有意义——一并去掉，免得读代码的人还要猜这个文件用的是哪种写法。

本仓库自己的 `.h` / `.hpp` 已全部按此规则命名，包括生成器写出的那些（`SOAD_CFG_H`、`SOMEIPXF_CFG_H`、`STD_TRACE_APP_CFG_H`）——生成器里的模板串也一并改过，重新生成的产物不会退回旧写法。仍保留旧写法的只有两类，都不归本规范管：`app/platform` 下的芯片厂商 SDK，以及 `infras/libraries/{asn1c,dds,crc,heap}`、`infras/system/net/lwip` 等跟随上游同步的第三方源码。核对命令：

```bash
grep -rlE '^#[[:space:]]*(ifndef|define)[[:space:]]+(_[A-Za-z0-9_]+|[A-Za-z0-9_]+_)[[:space:]]*$' \
  --include=*.h --include=*.hpp infras tools app \
  | grep -vE '/(GEN|GENT)/|^app/platform/|libraries/(asn1c|dds|crc|heap|mempool|plugin|ringbuffer)|lwip/|freertos/'
```

命中结果只剩三处刻意为之的写法：`Std_Compiler.h` 的 `__weak` / `__naked` / `__packed` / `__constructor`（编译器属性名，不是保护宏），以及 `kernel.h`、`pthread.h` 里 `#define _SYS_SCHED_H_`、`#define _SYS__PTHREADTYPES_H_` ——那几行是**故意**占用宿主 libc 的保护宏，用来压掉系统的同名头。

**语言绑定层是第二个例外，且只限于字符串形式的导出面。** 绑定层里 C++ 符号（类名、方法、函数）一律照本规范写：`py::class_<Can>(m, "can")`、`.def("is_opened", &Can::IsOpened)`、lua 侧 `{"open", LuaCan_Open}`——C++ 侧是 `Can`、`IsOpened`、`LuaCan_Open`，字符串键 `"can"`、`"is_opened"`、`"open"` 才是发布给脚本的 API，改名会让所有用户脚本静默失效。同一条规则覆盖：`py::class_(m, ...)` 的类名、`.def(...)` 的方法名、`py::arg(...)` 的关键字名（包括 `lKwargAliases` 里的 legacy 拼写）、`luaL_Reg` 表的键与 `luaopen_*` 入口名。

**动态库的 C 入口函数不是字符串，但要遵循同一条对称规则。** AsOne 的 UI 插件导出 `extern "C" QWidget *Create(void)` 与 `extern "C" std::string Name(void)`——入口名用 PascalCase，宿主侧 `PAL_DlSym(dll, "Create")` 按同名解析，两侧必须一致（历史上两侧大小写不一致导致插件加载失败）。

**语言绑定的模块入口名同样是宿主侧的名字，不是 C++ 符号。** `PYBIND11_MODULE(AsPy, m)` 生成 `PyInit_AsPy`，必须与产物文件名 `AsPy.pyd` 逐字符一致；`import AsPy` 只认这个名字，改成 `asPy` 就会 `ImportError: dynamic module does not define module export function`。Lua 侧同理：`luaopen_*` 与 `luaL_requiref(L, "x509", ...)` 的库名跟脚本里的 `require("x509")` 对齐。这类名字属于导出面，不受本规范的大小写改写。

UI 工具源文件中的自由函数可以是小写前缀的 `camelCase`（如 `appUpdateComTxMsg`）——这条只适用于自由函数，绝不适用于类 API。

### 3.7 配置 JSON 键名

给 `tools/generator` 供数的 `<Module>.json`，键名同样用 **`PascalCase`**，与它初始化的结构体成员或 ECUC 参数保持一致：`RxLimit`、`MsgCycleTime`、`NodeMask`——不写 `rx_limit`、`msgCycleTime`、`node_mask`。生成器读键名后，输出的正是同名 C 成员或同名初始化注释，键名与代码因此一眼对得上；snake_case 键名属于历史遗留，应当连同读取它的生成器一起改掉。

**配置面上任何位置都不许出现 snake_case。** 这条规则不限于 `<Module>.json`：凡是「写下来再读回去」的名字都算——生成器读取的 Excel 表头与标签单元格、命令行参数及其 `argparse` 目标名、Python 工具里的 `dict` 键。`data_size` 改成 `DataSize`，`number_of_write_cycles` 改成 `NumberOfWriteCycles`，`--page_size` 改成 `--PageSize`，`args.page_size` 改成 `args.PageSize`。风格混用比统一采用任何一种都糟：读者再也分不清某处是小失误还是约定。

**过短的键名必须写全。** 键名本身就是文档：评审时它出现在 diff 里，生成的 C 成员也照它命名。两个字母的简写不可接受，即便协议里就是这么定义的：

| 反例 | 正例 | 原因 |
|---|---|---|
| `"sa"` / `"ta"` | `"SourceAddress"` / `"TargetAddress"` | C 里是 `sourceAddress`、JSON 里是 `sa`，对应关系断了 |
| `"at"` | `"ActivationType"` | `at` 读起来像介词 |
| `"ip"` | `"Ip"` | DoIP 配置块里孤零零一个两字母键 |
| `"delayUs"` | `"DelayUs"` | 同一个词，大小写要与相邻键一致 |
| `"N_TA"` | `"N_TargetAddress"` | 保留 AUTOSAR 的 `N_` 时序前缀，其余写全 |
| `"LL_DL"` | `"LinkLayerDataLength"` | 全大写属于宏，不属于配置；压到 `"LlDl"` 同样是缩写，只是换了大小写 |
| `"Block Size"` | `"BlockSize"` | 键名里不带空格 |

本身已是完整单词的协议参数名，按标准写法保留（`N_As`、`N_Bs`、`STmin`、`BS`，以及消费它们的枚举里的 `CAN_ID_BOTH`）。判断标准是：没读过标准的人能不能猜出含义——`N_As` 可以，因为它是一个 `N_` 系列时序参数；`sa` 不行。

**写给别人读的，不改名。** 上面所有规则只覆盖我们自己定义、自己再读回来的名字。一旦某个名字属于外部规范——第三方工具的输入输出格式、XML 的元素名、文件格式的后缀——它的拼写就是契约的一部分，改名等于改格式，会让工具与它的消费者静默错位：

| 位置 | 必须保留 | 原因 |
|---|---|---|
| `tools/utils/trace.py` 的输出 | `name` / `ph` / `pid` / `tid` / `ts` / `args` / `traceEvents` | Google Trace Event Format，`chrome://tracing` 和 Perfetto 按这些键解析 |
| `tools/generator/odx/parser.py` | `n.find("p")` | ODX 是 ASAM 的 XML 规范，元素名 `<p>` 由标准规定 |
| `tools/libraries/python/can/can_analyzer.py` | `can_id.endswith("x")` | Vector ASC 用 `...x` 后缀标记扩展帧 |
| `tools/asone` 的 `jsLvl["dll"]` | 保持小写 | 历史配置里已有的可选项，仓库内没有生产者，改名只会让老配置失效 |

判断方法很直接：**这个字符串会不会被我们之外的代码读到？** 会，就照抄规范；不会，才轮到本规范。喂给生成器的 `Trace.json` 属于前者——那是我们的配置，键按时序分组写成 `Area`、`Type`、`Level`，同样必须是 PascalCase；而它生成的 trace 结果不同，那些键归 Google 管。同一个工具一边输入、一边输出，两套命名并存是对的，实现里要写明原因（见 `tools/utils/trace.py` 的注释），否则下一轮"统一命名"还会把它改回去。

### 3.8 命名速查

| 元素 | C | C++ |
|---|---|---|
| 类型 | `CanTp_ChannelContextType` | `class Semaphore` |
| 公开函数 / 方法 | `CanTp_Transmit()` | `Semaphore::Wait()` |
| 静态 / 私有函数 | `CanTp_SendFC()` | `Post()` |
| 参数 | `pduInfoPtr`、`linkLayerDataLength` | `int timeoutMs` |
| 局部变量 | `dataLength`、`i` | `auto vbox` |
| 文件级静态变量（内部链接） | `cantpConfig`、`dcmContext`（小写模块前缀） | 匿名命名空间中的 `semaphoreCount` |
| 全局变量（外部链接） | `Dcm_Config`、`PduR_Config` | `Dcm_Config`（同左） |
| 结构体 / 类成员 | `SduLength`、`BufLen` | `m_Sem`、`m_LeData` |
| 常量宏（无参数） | `CANTP_N_PCI_SF`、`CANTP_STMIN_ADJUST` | 同左 |
| 函数式宏（带参数） | `CanTpSetAlarm(v)` | 同左 |
| 枚举值 | `CANTP_IDLE` | 同左 |
| 头文件保护宏 | `CANTP_PRIV_H`（`.h`） | `SEMAPHORE_HPP`（`.hpp`） |

整张表其实只有一条规则：**类型、函数、方法、成员和导出的全局变量用 `PascalCase`，全局变量还要带 `Module_` 前缀；所有承载临时值的名字——参数、局部变量、文件级静态变量——用 `camelCase`，其中静态变量带小写模块前缀（`cantpConfig`）；常量宏和枚举值用 `ALL_CAPS`；带参数的宏是被当函数调用的，所以按函数命名（`CanTpSetAlarm(v)`）。**

---

## 4. 代码排版

本节所有内容都由 `.clang-format` 强制执行——不要手工排版：

| 选项 | 取值 |
|---|---|
| IndentWidth | **2 个空格**（C 与 C++ 相同），禁用 Tab（`UseTab: Never`） |
| ColumnLimit | **100** |
| 花括号 | 附着式（K&R）——函数、`if`、`for` 等的 `{` 与语句同行 |
| ContinuationIndentWidth | 2（换行参数在开括号后对齐，`AlignAfterOpenBracket: Align`） |
| PointerAlignment | 右对齐：`CanTp_ChannelContextType *context`、`void *m_Sem` |
| 单语句分支 | 仍需花括号并换行（`AllowShortIfStatementsOnASingleLine: false`） |
| 换行符 | LF（`UseCRLF: false`），文件以换行结尾 |

```c
/* 正确 —— 2 空格缩进、附着花括号、参数与局部变量均为 camelCase */
static PduLengthType CanTp_GetDL(PduLengthType len, PduLengthType linkLayerDataLength) {
  PduLengthType dataLength = linkLayerDataLength;
  uint32_t i;
  const PduLengthType linkLayerDataLengths[] = {8, 12, 16, 20, 24, 32, 48};

  for (i = 0; i < ARRAY_SIZE(linkLayerDataLengths); i++) {
    if (len <= linkLayerDataLengths[i]) {
      dataLength = linkLayerDataLengths[i];
      break;
    }
  }
  return dataLength;
}
```

多行宏体使用 `do { ... } while (0)` 加 `\` 续行——这是语句型宏，按 3.4 节用 `ModulePascalCase`：

```c
#define CanTpSetAlarm(v)                                                                           \
  do {                                                                                             \
    Std_TimerInit(&context->Timer, ((v) * 1000 * CANTP_MAIN_FUNCTION_PERIOD));                      \
  } while (0)
```

---

## 5. 预处理

### 5.1 特性宏与构建集成

- 平台/库选择通过 `USE_*` 宏完成（`USE_DOIP`、`USE_CANNM`、`USE_LWIP_RAW`），由构建系统的 `RegisterConfig`/`register_application` 设置——**绝不手工编辑生成的 `*_Cfg.h`**。
- 支持 Post-Build 配置的模块要么用静态配置 `#define <MODULE>_CONFIG (&<Module>_Config)`，要么用指针 `#define <MODULE>_CONFIG <module>Config`：

```c
#ifdef CANTP_USE_PB_CONFIG
#define CANTP_CONFIG cantpConfig
#else
#define CANTP_CONFIG (&CanTp_Config)
#endif
```

### 5.2 条件编译风格

使用 `#ifndef` / `#if defined(...)` 加普通 `#else`；每个可选代码块在宏关闭时也必须能编译通过（可用 `__weak` 弱定义，见 `app/app/src/app.c`）。不缩进预处理指令（`IndentPPDirectives: None`）——`#if`/`#endif` 保持在第 0 列。

### 5.3 头文件保护宏

```c
#ifndef CANTP_PRIV_H      /* CanTp_Priv.h  */
#define CANTP_PRIV_H
...
#endif /* CANTP_PRIV_H */
```

```cpp
#ifndef SEMAPHORE_HPP     // Semaphore.hpp
#define SEMAPHORE_HPP
...
#endif  // SEMAPHORE_HPP
```

保护宏就是文件名全大写、扩展名的点换成下划线，**别的不加**——首尾都不要下划线。`.h` 与 `.hpp` 只有这一种写法。`_SEMAPHORE_HPP_` 和 `_CANTP_PRIV_H_` 都是错的：以「下划线 + 大写字母」开头的标识符保留给实现使用（C11 7.1.3、C++ `[lex.name]`），这样的保护宏属于未定义行为。详见 3.6。

文件末尾的 `#endif` 带注释标明对应的保护宏。

---

## 6. 注释与需求追溯

- 注释使用英文，`/* ... */` 风格（clang-format 会用 `ReflowComments` 自动整理）。
- 每个模块实现在版权头的 `ref:` 行注明所实现规范的出处。
- 具体需求在代码内用标签标注，便于对照规范审计：

```c
/* @SWS_DoIP_00008, @SWS_DoIP_00009 */
#define DOIP_GENERAL_HEADER_NEGATIVE_ACK 0x0000u

typedef struct { /* @ECUC_SecOC_00012 */
```

- 不直观的宏在其上方加一行简短说明：

```c
/* Flow Control Status Mask */
#define CANTP_N_PCI_FS 0x0Fu
```

---

## 7. 日志

日志使用 `Std_Debug.h` 中的 `ASLOG`。每个模块在 MACROS 段开头定义自己的日志通道宏，每个级别一个：

```c
#define AS_LOG_CANTP 0   /* 状态迁移/详细日志，级别 3 时输出 */
#define AS_LOG_CANTPI 0  /* 信息：周期性行为，级别 2 */
#define AS_LOG_CANTPE 2  /* 错误：默认输出 */
```

- 后缀约定：`<MODULE>`（详细）、`<MODULE>I`（信息）、`<MODULE>E`（错误）。宏值是该行输出所需的最低 `AS_LOG_LEVEL`；错误默认 2，保证 CI 日志可见。
- 消息格式：`"[%d]<内容>!\n"`——开头的方括号内是 PDU/通道/上下文 id，**结尾必须有 `\n`**，错误路径以 `!` 结尾。`%s` 遇到 NULL、`%02X` 缺宽度等是已知的崩溃来源——务必显式指定宽度并在类型不符时强转（`(int)PduInfo.SduLength`）。
- 用户通过 `AS_LOG_<MODULE>` 环境变量在运行时打开通道（如 `AS_LOG_CANTPE=3`），因此可以放心多加日志，但默认级别必须安静。

---

## 8. 错误处理

- 有状态返回的公开 API 使用 `Std_ReturnType`，取值 `E_OK` / `E_NOT_OK`。
- 参数/状态非法时用 `ASLOG(<MODULE>E, ...)` 记录并返回 `E_NOT_OK`；`Det_ReportError` 服务是存在的（`infras/diagnostic/Det`），但目前大多数 infras 模块依赖 ASLOG——未经全局决策不要在个别模块单独引入 Det 调用。
- 中断安全：共享状态用 `EnterCritical()` / `ExitCritical()` 保护，且封装在模块局部宏后面，以便主机构建时直接编译为空（见 `USE_CANTP_CRITICAL` → `tpEnterCritical()`）。
- `assert()` 用于宿主机侧代码（`tools/`、`_WIN32` 下构建的 app 代码），不用于目标机路径的 AUTOSAR 模块。

---

## 9. 构建文件

- 每个模块目录带有 `SConscript`，通过共享构建助手（`RegisterConfig`、`libsForApp`）注册源文件；生成代码放入 `GEN/`，不手工提交——由 `tools/generator/<Module>.py` 根据 app 的 JSON 配置生成。
- 生成器与模块保持同步：新增配置项时，同一次修改中要同时更新生成器和模块内 `<Module>_Cfg` 的处理代码。

---

## 10. 工具

### 10.1 格式化

- 仓库根目录的 `.clang-format` 是排版规则的唯一权威来源。对改动的文件执行 `clang-format -i`（配置好后 VS Code / clangd 会在保存时自动执行）。
- `python tools/format.py -i <file>` 将 7 个段横幅重写为标准文本——横幅被敲坏或新文件需要骨架时使用。

### 10.2 静态检查

- `tools/cppcheck/cppcheck.py` 按 `tools/cppcheck/MISRA_C_2012.txt` 中的规则运行 cppcheck。改动的模块要跑一遍；新引入的告警应当修复而不是压制。

---

**版本**：1.6
**日期**：2026 年 11 月
**依据**：SSAS 代码库现状、仓库根目录 `.clang-format`、`tools/format.py`
