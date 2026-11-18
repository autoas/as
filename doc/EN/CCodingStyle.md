---
layout: post
title: C/C++ Coding Style Guide
category: AUTOSAR
comments: true
---

# AS C/C++ Coding Style Guide

## 1. Document Purpose

This document defines the coding style for C/C++ code in the SSAS project (`infras/`, `app/`, `tools/libraries/`, `tools/asone/`). The style is **not invented** — it is derived from the actual codebase and enforced mechanically by the repository's `.clang-format` (repo root), so most formatting happens automatically. This guide explains the conventions, the reasoning, and the rules clang-format cannot enforce (naming, file organization, logging, error handling).

Quick check before a commit:

```bash
clang-format -i <file.c>          # formatting, driven by the repo .clang-format
python tools/format.py -i <file.c>  # normalize the section banners
python tools/cppcheck/cppcheck.py # static analysis, see tools/cppcheck/MISRA_C_2012.txt
```

---

## 2. File Organization

### 2.1 Per-Module File Set

Each AUTOSAR module lives in its own directory (e.g. `infras/communication/CanTp/`) and consists of:

| File | Contents |
|---|---|
| `<Module>.c` | all implementation code |
| `<Module>.h` | public API, the only header other modules include |
| `<Module>_Priv.h` | private macros/types shared inside the module |
| `<Module>_Cfg.h` | generated configuration header (from `tools/generator/<Module>.py`), include it guarded by `#ifndef <MODULE>_CFG_H` |
| `SConscript` | build registration |

Example: `CanTp.c`, `CanTp.h`, `CanTp_Priv.h`, generated `CanTp_Cfg.h`.

### 2.2 License Header

Every `.c/.h/.hpp/.cpp` file starts with the same banner, optionally followed by `ref:` lines citing the specification the file implements:

```c
/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Transport Layer AUTOSAR CP Release 4.4.0
 */
```

Use the year the file was created. Python files use the same banner with `#`.

### 2.3 Section Banners

Every C/C++ file body is divided into fixed sections, each marked with an exact banner line. The order is fixed; empty sections are kept (see `CanTp_Priv.h`). `tools/format.py -i <file>` normalizes these banners because hand-typing them invites drift:

```c
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
```

Section contents:

- **INCLUDES** — headers; own module header first, then other AS headers, then system headers (`<string.h>`), then third-party (e.g. mbedtls). Conditional includes (`#ifdef USE_DOIP ... #endif`) are written inline, not moved to the end. Do not sort includes — `SortIncludes: false` in `.clang-format`; order is meaningful (e.g. `Std_Types.h` before things that depend on it).
- **MACROS** — `AS_LOG_*` definitions come first, then feature macros, then protocol constants.
- **TYPES** — structs/enums used by this file.
- **DECLARES** — extern declarations and forward declarations of `static` functions.
- **DATAS** — file-scope variables: the module's `static` state (`cantpConfig`) and, in the module's own config file, its exported objects (`Dcm_Config`).
- **LOCALS** — `static` function definitions.
- **FUNCTIONS** — public API definitions.

---

## 3. Naming Conventions

### 3.1 Public Functions

`<Module>_<PascalCase>` — the module prefix is mandatory:

```c
Std_ReturnType CanTp_Transmit(PduIdType txPduId, const PduInfoType *pduInfoPtr);  /* Good */
void CanTp_MainFunction(void);                                                    /* Good */
```

### 3.2 Static (Local) Functions

**Also `<Module>_<PascalCase>`. `static` changes the linkage, never the name.** A file-local function is still a private *API of the module*, not a local variable: it shows up in debugger call stacks, in `ASLOG` traces and in the symbol table of every build, and several modules end up in one translation unit (`LinTp.c` textually includes `CanTp.c`), so a backtrace frame called `nmInit3` tells the reader neither which module it belongs to nor what it does.

```c
static void CanTp_ResetToIdle(CanTp_ChannelContextType *context);  /* Good */
static void CanTp_SendFC(PduIdType rxPduId, uint8_t flowStatus);   /* Good */
static void OsekNm_Init3(NetworkHandleType NetId);                 /* Good */

static void nmInit3(NetworkHandleType NetId);        /* Bad - camelCase, no module prefix */
static void CanTp_resetToIdle(...);                  /* Bad - underscore then camelCase */
static void ResetToIdle(...);                        /* Bad - "it is private" is not a reason */
```

The rule also covers callbacks the module registers in a table, e.g. shell commands: `Com_CmdLsSgFunc`, not `cmdComLsSgFunc`. Note the deliberate asymmetry with section 3.5: file-scope static **variables** use a *lowercase* module prefix (`cantpConfig`), static **functions** use the real `Module_` prefix (`CanTp_ResetToIdle`), because a function name reads like the API it is.

Anything that the check below prints is a finding:

```bash
grep -rnE '^static [A-Za-z_][A-Za-z0-9_ *]*[ *]([a-z][A-Za-z0-9_]*)[[:space:]]*\(' infras --include=*.c
```

Ported third-party sources keep their upstream naming (the lwIP netif port in `TcpIp/config/TcpIp_Cfg.c`, `libraries/libc`, `libraries/heap`, `libraries/factory`): rewriting them makes the next upstream diff unreadable. Everything we own starts with the module prefix.

Likewise, the legacy lowercase `static` functions in `infras/system/fs` (the vfs adapter layer) and `infras/system/kernel/os` (the POSIX kernel port layer) are an **acknowledged exception**: both are adapter layers mirroring external syscall semantics with long-standing conventions, and are not required to be renamed in bulk. New code written into these two directories must still follow this section, otherwise the grep above can never be driven to zero.

### 3.3 Types

AUTOSAR-style suffix `Type`, anonymous `struct`/`enum` with the tag on the closing brace:

```c
/* Bad - a camelCase member mixed with PascalCase ones */
typedef struct {
  uint16_t AuthPduOffset;
  uint8_t bufLen;
} SecOC_TxPduProcConfigType;

/* Good - every member PascalCase, as in the AUTOSAR SWS */
typedef struct {
  uint16_t AuthPduOffset;
  uint8_t BufLen;
} SecOC_TxPduProcConfigType;

typedef enum {
  CANTP_EXTENDED,
  CANTP_MIXED,
} CanTp_AddressingFormatType;      /* Good */

typedef uint8_t SecOC_RxPduProcStateType;   /* typedef of a plain type: also ...Type */
```

**Structs that need a forward declaration or self-reference use the tag `..._s` paired with the typedef `...Type`.** This is the BSD/kernel convention (`struct loader_s`); fixing the pairing means `struct Xxx_s` and `XxxType` always denote the same type, and the queue macros (the type argument of `STAILQ_ENTRY` / `STAILQ_REMOVE` is the tag) plus forward declarations stop drifting apart:

```c
typedef struct Loader_s Loader_Type;   /* forward declaration in the header */

struct Loader_s {                      /* full definition in the .c */
  Srec_FileType *AppSRec;
  ...
};

typedef struct IsoTp_s IsoTp_Type;     /* same pairing: IsoTp_s / IsoTp_Type */
```

Simple structs that need neither keep the anonymous definition (`typedef struct { ... } XxxType;`) — never add a tag for its own sake.
**All struct/union members use `PascalCase`, and one struct never mixes styles.** This is the AUTOSAR convention (`PduInfoType.SduLength`, `SduDataPtr`, `MetaDataPtr`, `CanNmMsgCycleTime`), so following it keeps our own types looking like the SWS ones and makes the code readable to anyone who knows the standard. Decide per type, not per member: if the first member is `AuthPduOffset`, the next one is `BufLen`, not `bufLen`. Pointer members keep the `*` on the name: `uint8_t *Buffer;`.

Members that map 1:1 to an ECUC/SWS parameter keep the exact spec spelling (`MsgCycleTime`, `SoConId`, `RequestAddressAssignment`). The generators emit designated initialisers by that name (`.MsgCycleTime = ...`), so renaming one means changing the generator and every JSON key that feeds it — treat that as a separate, planned change, never a drive-by edit.

The timing and flow-control parameters defined by the standard are a deliberate exception: `N_As`, `N_Bs`, `N_Cr`, `BS`, `STmin` and `N_TA` keep the ISO 15765-2 spelling even when they sit next to `BlockSize` and `LinkLayerDataLength` in the same struct (see 3.7). The test is whether someone who has never read the standard can still look the name up: `N_Bs` is recognisably part of the `N_` timing family, `BS` and `STmin` are the standard's own words. `LL_DL` is **not** covered by that exception — it expresses a readable concept (link-layer data length) and must be written `LinkLayerDataLength`. An exception list collects the standard's wording only, never abbreviations of our own making.

Note the split: **members are `PascalCase`, locals and globals are `camelCase`** (section 3.5). Do not carry `PascalCase` into local variables.

Enum values are `ALL_CAPS` with the module prefix (`CANTP_IDLE`, `CANTP_EXTENDED`).

### 3.4 Macros and Constants

Two kinds of macro, two forms. The rule follows what the macro *is* at the call site, not the fact that it is a `#define`.

**Object-like macros (no parameters)** — they are constants, so `ALL_CAPS_WITH_UNDERSCORES`, always carrying the module prefix. **`static` does not remove the prefix**: a file-private macro is still ours, and a reader hitting `N_PCI_SF` in a log, a debugger watch or a grep across a multi-module build has no way to tell which module owns it.

```c
#define CANTP_N_PCI_SF  0x00u   /* Good */
#define CANTP_N_PCI_CTS 0x30u   /* Good */

#define N_PCI_SF  0x00u         /* Bad - owner unclear outside CanTp.c */
```

"Private to the file" is a linkage detail, not a naming scope — and it is not even stable: `LinTp.c` compiles `CanTp.c` by textual include, so a macro that looks file-local ends up in a second translation unit.

**Function-like macros (they take parameters)** — `ModulePascalCase`: the module name in PascalCase, directly followed by the name, **no underscore**. These are called as `CanTpSetAlarm(100u)`, so they must be named like the call they pretend to be, and the module prefix works exactly as it does for a function.

```c
#define CanTpSetAlarm(v)      /* Good - matches how it is called */
#define CanTpCancelAlarm()    /* Good */
#define CanTpIsAlarmTimeout() /* Good */

#define CANTP_SET_ALARM(v)    /* Bad - reads like a constant expression, may hide a do/while */
```

`CANTP_SET_ALARM` is the worse trap of the two: the call site looks like a constant expression, while the body may hold a `do { ... } while (0)`, a `return`, or a side effect. Code that reads as a value should be a value; code that runs statements should look like a call.

A macro that wraps a few statements is a function in disguise, so judge it exactly like a function — including the module prefix. `nmSendMessage()` or `sdEnterCritical()` inside one `.c` file is the same defect as `static void nmSendMessage(...)`: the call site gives no hint about the owner, while the macro can run `CanIf_Transmit` and start a timer.

```c
#define OsekNmSendMessage()   /* Good */
#define ComMEnterCritical()   /* Good */

#define nmSendMessage()       /* Bad - lowercase, no module prefix */
```

**Exception — value-conversion macros.** A function-like macro that only computes a value, with no side effect and no statement body, may keep the `ALL_CAPS` form, because that really is a constant expression with a parameter:

```c
#define CANTP_CONVERT_MS_TO_MAIN_CYCLES(x)   /* accepted */
#define DCM_CONVERT_MS_TO_MAIN_CYCLES(x)     /* accepted */
```

This is tolerated, not preferred: `CanTpConvertMsToMainCycles(x)` is the recommended spelling, and new code should use it. The exception rests on a judgement ("this one has no side effect") that quietly expires the day someone adds a statement to the body — and then the `ALL_CAPS` name is lying about what it does.

**Exception — established registration/logging macros.** `SHELL_REGISTER(...)`, `DET_VALIDATE(...)`, `ASLOG`/`ASPRINT`/`ASHEXDUMP` keep the `ALL_CAPS` form: their prefix (`SHELL_`/`DET_`/`AS`) already shows the owner at the call site, and they are referenced by generator templates, configuration files and several hundred call sites — renaming buys no readability and leaves behind a large meaningless diff plus a mix of old and new spellings. Leave these as they are instead of applying the two rules above.

Feature macros follow `USE_<LIB>_<FEATURE>` or `<MODULE>_<FEATURE>` (`USE_DOIP`, `CANTP_STMIN_ADJUST`). Prefer the `u` suffix on unsigned literals: `0xF0u`, `1u`.

### 3.5 Variables

- Local variables: `camelCase`, or a genuinely short name for a loop index or a value used twice (`i`, `pos`, `bufReq`) — no Hungarian prefixes (`lLL_DLs`) and no cryptic two-letter abbreviations.
- File-scope statics: `static`, `camelCase`, and **always prefixed with the module name in lowercase** — `cantpConfig`, `secocContext`, `dcmContext`, `ecumContext`, `linifSchedule`. The prefix is not decoration: these variables are module-global state, and `LinTp.c` compiles `CanTp.c` by textual include, so two modules can end up in one translation unit where a bare `config` would be ambiguous in the debugger and in log output. The prefix is the module name lower-cased as a whole (`CanTp` → `cantp`, `EcuM` → `ecum`), not `CanTp_` and not `CanTpConfig`.

  ```c
  static const CanTp_ConfigType *cantpConfig = NULL;   /* Good */
  static Dcm_ContextType dcmContext;                   /* Good */

  static Dcm_ContextType Dcm_Context;                  /* Bad - reads like a type */
  static const CanTp_ConfigType *config = NULL;        /* Bad - no module prefix */
  ```

  The `s_` / `g_` prefixes themselves are not forbidden — in general C or C++ code `static Dcm_ContextType s_Context;` is acceptable. They are just **not recommended inside an AUTOSAR module**: `s_Context` says "some static" while `dcmContext` says "Dcm's context", and in a code base where every module looks like the SWS, the module prefix carries the information that matters.

  In C++ prefer an anonymous namespace over `static` for the same purpose, keeping the same naming (`namespace { int semaphoreCount; }`).

- Globals with external linkage: **`Module_PascalCase`** — the module name in PascalCase, an underscore, then the name: `Dcm_Config`, `CanTp_Config`, `PduR_Config`, `Std_TraceArea_APP`. The two tiers are deliberate and must not be confused:

  | Linkage | Form | Example | Why |
  |---|---|---|---|
  | internal (`static`) | `<module>camelCase`, module lower-cased | `cantpConfig`, `dcmContext` | invisible outside the file, reads as local state |
  | external (`extern`) | `<Module>_PascalCase` | `Dcm_Config`, `PduR_Config` | part of the module's public surface, namespaced exactly like its functions and types |

  Configuration objects are the typical case, so they use the AUTOSAR memory-class idiom: `extern CONSTANT(<Type>, <MODULE>_CONST) <Name>;` in the header, one definition in the module.

  ```c
  /* Dcm_Priv.h */
  extern CONSTANT(Dcm_ConfigType, DCM_CONST) Dcm_Config;

  /* Dcm_Cfg.c (or the generated GEN/Dcm_Cfg.c) */
  const Dcm_ConfigType Dcm_Config = { ... };

  extern const Dcm_ConfigType dcm_Config;   /* Bad - lower-case module prefix on a global */
  extern const Dcm_ConfigType DCM_CONFIG;   /* Bad - looks like a macro */
  extern const Dcm_ConfigType Config;       /* Bad - no module prefix at all */
  ```

  A global whose name does not start with its module is the one identifier a reader cannot trace back: `Dcm_Config` is found by `grep Dcm_`, it sorts next to the module's other symbols in a symbol-table dump, whereas `dcm_Config` hides between `dbg_*` and `dummy` and `DCM_CONFIG` would be mistaken for a macro in review.
- Function parameters: `camelCase` too, for every function including the ones implementing an SWS API — parameter names are not part of the ABI, so they follow our style rather than the specification's spelling (`pduInfoPtr`, `txPduId`).
- **The `Ptr` suffix is optional, not mandatory.** A pointer parameter may be named `data`, `canFrame`, `rxNotification` just as well as `dataPtr`, `canFramePtr`. The name has to say *what the parameter is*, not *that it is a pointer* — the `*` plus the surrounding context already tell the reader that. Pick one form and stay consistent within a module / an API; do not mix `dataPtr` with `canFrame` in the same interface.
- Spell abbreviations out when they are ambiguous. A parameter is not a place for protocol shorthand: `LL_DL` becomes `linkLayerDataLength`, not `llDl` and not `LL_DL`. The generated interface follows the same rule: the JSON key is `LinkLayerDataLength` (see 3.7), never `LL_DL` and never the two-letter `LlDl`, and the `N_` timing keys are spelled `N_TargetAddress`, never `N_TA`.
- Struct members: `PascalCase` (AUTOSAR convention, see 3.3), no `m_` prefix in plain C — unlike locals, which stay `camelCase`.

### 3.6 C++ (tools/libraries, tools/asone)

| Element | Rule | Example |
|---|---|---|
| namespace | `as` for reusable library code | `namespace as { ... }` |
| class | `PascalCase` | `class Semaphore {` |
| method | `PascalCase` for **every** class API we write | `IsTransmit()`, `Wait()`, `Post()` |
| member variable | `m_` + `PascalCase`, no exceptions | `m_Sem`, `m_Msg`, `m_LeData` |
| local | `camelCase`, liberal use of `auto` | `auto vbox = new QVBoxLayout();` |

**`m_` is a prefix, not a style change.** The part after it is the member name in PascalCase, so it starts with a capital letter: `m_LeData`, not `m_leData`; `m_AxisX`, not `m_axisX`; `m_RxBuffer`, not `m_rxBuffer`. An acronym inside the name keeps its own casing (`m_Isotp`).

**Boolean members take a predicate, not a type prefix.** After `m_`, write the condition itself: `m_IsProtected`, `m_HasPayload`, `m_CanTransmit` — not `m_bProtect`, `m_bPayload`. The `b` prefix puts the type into the name but leaves out the meaning: `m_bProtect` never says when it is true, whereas `m_IsProtected` is its own condition, and the defensive `true == m_bValid` comparison becomes noise once the member is called `m_IsValid`. The same ban applies to `m_nBits` and `m_byLua`: `n` and `by` are Hungarian prefixes of the same family, see the matching prohibition for locals in 3.5.

**Every method we declare is `PascalCase`, including small utility classes** — `Semaphore::Wait()` / `Post()`, not `wait()` / `post()`. The only methods that keep a lowercase first letter are Qt's own names, which we may not rename:

- virtual overrides: `timerEvent()`, `paintEvent()`, `mousePressEvent()`
- auto-connect slots generated by `uic`: `on_btnStart_clicked()`
- the `QObject` interface itself: `connect()`, `setStyleSheet()`

**Language-binding layers are the second exception, and only for the exported surface, which is made of strings.** Every C++ symbol in a binding follows this guide: `py::class_<Can>(m, "can")`, `.def("is_opened", &Can::IsOpened)`, and on the Lua side `{"open", LuaCan_Open}` — the C++ side is `Can`, `IsOpened`, `LuaCan_Open`, while the string keys `"can"`, `"is_opened"`, `"open"` are the API published to scripts; renaming them would silently break every user script. The same rule covers the name in `py::class_(m, ...)`, the method names in `.def(...)`, the keyword names in `py::arg(...)` (including the legacy spellings kept in `lKwargAliases`), the keys of the `luaL_Reg` tables and the `luaopen_*` entry point names.

**The C entry points of a dynamic library are not strings, but they follow the same symmetry rule.** AsOne's UI plugins export `extern "C" QWidget *Create(void)` and `extern "C" std::string Name(void)` — the entry names are PascalCase, and the host resolves them by the same name via `PAL_DlSym(dll, "Create")`; both sides must match (a historical case mismatch between the two sides left plugins failing to load).

**The module entry name of a language binding is a host-side name too, not a C++ symbol.** `PYBIND11_MODULE(AsPy, m)` emits `PyInit_AsPy`, which must match the artifact file name `AsPy.pyd` character for character; `import AsPy` looks for exactly that name, so renaming it to `asPy` yields `ImportError: dynamic module does not define module export function`. Lua works the same way: `luaopen_*` and the library name in `luaL_requiref(L, "x509", ...)` line up with `require("x509")` in scripts. These names are part of the exported surface and are never case-rewritten by this guide.

Free functions in the UI tool source files may be lower-camelCase prefixed (e.g. `appUpdateComTxMsg`) — this applies only to free functions, never to class APIs.

**Include guards: the file name upper-cased, with no leading and no trailing underscore.** One rule for `.h` and `.hpp` alike — the extension dot becomes `_`, the name is upper-cased, and nothing is added at either end:

| File | Guard | Wrong |
|---|---|---|
| `CanTp_Priv.h` | `CANTP_PRIV_H` | `_CANTP_PRIV_H_`, `CANTP_PRIV_H_` |
| `Mirror.h` | `MIRROR_H` | `_MIRROR_H_` |
| `Semaphore.hpp` | `SEMAPHORE_HPP` | `_SEMAPHORE_HPP_` |
| `BufferPool.hpp` | `BUFFERPOOL_HPP` | `_BUFFER_POOL_HPP_` |

A guard that begins with an underscore followed by an uppercase letter is reserved to the implementation in both C (C11 7.1.3) and C++ (`[lex.name]`), and a name that both begins and ends with an underscore is reserved unconditionally. Such a guard is technically undefined behaviour: it can collide with a compiler or library internal, and `-Wreserved-identifier` flags it. The trailing underscore has no such hazard but is equally pointless — drop it too, so a reader does not have to guess which form a file uses.

Every `.h` / `.hpp` we own now follows the rule, including the ones the generators emit (`SOAD_CFG_H`, `SOMEIPXF_CFG_H`, `STD_TRACE_APP_CFG_H`) — the template strings inside the generators were changed too, so a regeneration cannot bring the old form back. Only two groups still use the old spelling, and neither is ours to fix: the vendor SDKs under `app/platform`, and third-party sources that are synced from upstream (`infras/libraries/{asn1c,dds,crc,heap}`, `infras/system/net/lwip`, …). Check with:

```bash
grep -rlE '^#[[:space:]]*(ifndef|define)[[:space:]]+(_[A-Za-z0-9_]+|[A-Za-z0-9_]+_)[[:space:]]*$' \
  --include=*.h --include=*.hpp infras tools app \
  | grep -vE '/(GEN|GENT)/|^app/platform/|libraries/(asn1c|dds|crc|heap|mempool|plugin|ringbuffer)|lwip/|freertos/'
```

Three deliberate exceptions remain: the `__weak` / `__naked` / `__packed` / `__constructor` definitions in `Std_Compiler.h` (compiler attribute names, not header guards), and the `#define _SYS_SCHED_H_` / `#define _SYS__PTHREADTYPES_H_` lines in `kernel.h` and `pthread.h`, which **intentionally** squat on the host libc guards to suppress the system headers of the same name.

Free functions in UI tool source files may be `camelCase` with a lowercase prefix (`appUpdateComTxMsg`) — that convention applies to free functions only, never to a class API.

### 3.7 Configuration JSON Keys

The `<Module>.json` files that feed `tools/generator` use **`PascalCase` keys**, matching the struct member or ECUC parameter the value initializes: `RxLimit`, `MsgCycleTime`, `NodeMask` — never `rx_limit`, `msgCycleTime` or `node_mask`. The generator reads the key and emits the same name as the C member or the initialiser comment, so key and code stay visually identical; a snake_case key is a legacy leftover and is renamed together with the generator that reads it.

**No snake_case anywhere in the configuration surface.** The rule is not limited to `<Module>.json`: it covers every place a name is written down and read back — Excel column headers and label cells that the generators ingest, CLI flags and their `argparse` destinations, and the `dict` keys of the Python tools. `data_size` becomes `DataSize`, `number_of_write_cycles` becomes `NumberOfWriteCycles`, `--page_size` becomes `--PageSize`, `args.page_size` becomes `args.PageSize`. A mixed style is worse than either consistent one, because a reader can no longer tell a typo from the convention.

**Short keys must be spelled out.** A key is documentation: it is what a reviewer sees in a diff and what the C member is named after. Two-letter shorthand is not acceptable, even when the protocol defines it:

| bad | good | why |
|---|---|---|
| `"sa"` / `"ta"` | `"SourceAddress"` / `"TargetAddress"` | `sourceAddress` in C, `sa` in JSON — the link is lost |
| `"at"` | `"ActivationType"` | `at` reads like a preposition |
| `"ip"` | `"Ip"` | a lone two-letter key in a DoIP block |
| `"delayUs"` | `"DelayUs"` | same word, same casing as every sibling key |
| `"N_TA"` | `"N_TargetAddress"` | keep the AUTOSAR `N_` timing prefix, spell the rest |
| `"LL_DL"` | `"LinkLayerDataLength"` | all-caps keys belong to macros, not to config; squeezing it to `"LlDl"` is still an abbreviation, only re-cased |
| `"Block Size"` | `"BlockSize"` | no spaces in a key |

Protocol parameter names that are already words stay as the standard spells them (`N_As`, `N_Bs`, `STmin`, `BS`, `CAN_ID_BOTH` in the enum that consumes them). The test is whether a reader who has never seen the standard can guess the meaning — `N_As` passes because the value is a timing parameter of an `N_` series, `sa` does not.

**Names written for someone else are not renamed.** Everything above covers names we define and read back ourselves. The moment a name belongs to an external specification — a third-party tool's input or output format, an XML element name, a file-format suffix — its spelling is part of a contract, and changing it silently desynchronises the tool from its consumers:

| location | keep | why |
|---|---|---|
| output of `tools/utils/trace.py` | `name` / `ph` / `pid` / `tid` / `ts` / `args` / `traceEvents` | Google Trace Event Format; `chrome://tracing` and Perfetto parse these keys |
| `tools/generator/odx/parser.py` | `n.find("p")` | ODX is an ASAM XML schema; `<p>` is defined by the standard |
| `tools/libraries/python/can/can_analyzer.py` | `can_id.endswith("x")` | Vector ASC marks extended frames with an `...x` suffix |
| `jsLvl["dll"]` in `tools/asone` | stays lower case | an optional key already present in existing configs, with no producer in this repo; renaming it only breaks old configs |

The test is simple: **will any code other than ours read this string?** If yes, copy the specification; only if no does our own style apply. The `Trace.json` that feeds the generator is on the near side — it is our config, its timing groups are `Area`, `Type`, `Level` and they are PascalCase; the trace file the same tool produces is on the far side and those keys belong to Google. One tool, two naming systems, and the implementation has to say why (see the comment in `tools/utils/trace.py`), otherwise the next "unify the naming" pass reverts it.

### 3.8 Naming at a glance

| Element | C | C++ |
|---|---|---|
| type | `CanTp_ChannelContextType` | `class Semaphore` |
| public function / method | `CanTp_Transmit()` | `Semaphore::Wait()` |
| static / private function | `CanTp_SendFC()` | `Post()` |
| parameter | `pduInfoPtr`, `linkLayerDataLength` | `int timeoutMs` |
| local | `dataLength`, `i` | `auto vbox` |
| file-scope static (internal linkage) | `cantpConfig`, `dcmContext` (module prefix, lowercase) | `semaphoreCount` in an anonymous namespace |
| global, external linkage | `Dcm_Config`, `PduR_Config` | `Dcm_Config` (same) |
| struct / class member | `SduLength`, `BufLen` | `m_Sem`, `m_LeData` |
| constant macro (no parameters) | `CANTP_N_PCI_SF`, `CANTP_STMIN_ADJUST` | same |
| function-like macro | `CanTpSetAlarm(v)` | same |
| enum value | `CANTP_IDLE` | same |
| include guard | `CANTP_PRIV_H` (`.h`) | `SEMAPHORE_HPP` (`.hpp`) |

Read the table as one rule: **types, functions, methods, members and exported globals are `PascalCase` — globals carrying their `Module_` prefix; everything that holds a temporary value — parameters, locals, file-scope statics — is `camelCase`, with statics carrying a lowercase module prefix (`cantpConfig`); constant macros and enum values are `ALL_CAPS`; a macro that takes parameters is called like a function, so it is named like one (`CanTpSetAlarm(v)`).**

---

## 4. Code Layout

Everything in this section is enforced by `.clang-format` — do not hand-format:

| Option | Value |
|---|---|
| IndentWidth | **2 spaces** (C and C++), never tabs (`UseTab: Never`) |
| ColumnLimit | **100** |
| Braces | attached (K&R) — `{` on the same line for functions, `if`, `for`, ... |
| ContinuationIndentWidth | 2 (wrapped parameters align after the open bracket, `AlignAfterOpenBracket: Align`) |
| PointerAlignment | Right: `CanTp_ChannelContextType *context`, `void *m_Sem` |
| Single-statement bodies | still get braces and a new line (`AllowShortIfStatementsOnASingleLine: false`) |
| Line endings | LF (`UseCRLF: false`), file ends with a newline |

```c
/* Good - 2-space indent, attached braces, camelCase parameters and locals */
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

Multi-line macro bodies use `do { ... } while (0)` with `\` continuations — a statement macro, so `ModulePascalCase` per 3.4:

```c
#define CanTpSetAlarm(v)                                                                           \
  do {                                                                                             \
    Std_TimerInit(&context->Timer, ((v) * 1000 * CANTP_MAIN_FUNCTION_PERIOD));                      \
  } while (0)
```

---

## 5. Preprocessor

### 5.1 Feature Macros and Build Integration

- Platform/library selection is done with `USE_*` macros (`USE_DOIP`, `USE_CANNM`, `USE_LWIP_RAW`), set by the build system via `RegisterConfig`/`register_application` — never edit generated `*_Cfg.h` by hand.
- A module with optional post-build config compiles either `#define <MODULE>_CONFIG (&<Module>_Config)` (static) or `#define <MODULE>_CONFIG <module>Config` (post-build pointer):

```c
#ifdef CANTP_USE_PB_CONFIG
#define CANTP_CONFIG cantpConfig
#else
#define CANTP_CONFIG (&CanTp_Config)
#endif
```

### 5.2 Conditional Compilation Style

Use `#ifndef` / `#if defined(...)` with a plain `#else`; every optional block must still compile when disabled (weak definitions via `__weak` are acceptable, see `app/app/src/app.c`). Indent preprocessor directives is off (`IndentPPDirectives: None`) — keep `#if`/`#endif` at column 0.

### 5.3 Header Guards

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

The guard is the file name upper-cased, `_` for the extension dot, and **nothing else** — no leading and no trailing underscore. One form for `.h` and `.hpp` alike. `_SEMAPHORE_HPP_` and `_CANTP_PRIV_H_` are both wrong: an identifier starting with `_` + uppercase is reserved to the implementation (C11 7.1.3, C++ `[lex.name]`), so such a guard is undefined behaviour. See 3.6.

`#endif` at file end carries a comment naming the guard.

---

## 6. Comments and Traceability

- Comments are in English, `/* ... */` style (clang-format keeps them with `ReflowComments`).
- Every module implementation cites its specification in the license header with `ref:` lines.
- Individual requirements are tagged inline so the code can be audited against the spec:

```c
/* @SWS_DoIP_00008, @SWS_DoIP_00009 */
#define DOIP_GENERAL_HEADER_NEGATIVE_ACK 0x0000u

typedef struct { /* @ECUC_SecOC_00012 */
```

- A short `/* what it is */` comment goes above non-obvious macros:

```c
/* Flow Control Status Mask */
#define CANTP_N_PCI_FS 0x0Fu
```

---

## 7. Logging

Logging uses `ASLOG` from `Std_Debug.h`. Each module defines its own channel macros at the top of the MACROS section, one per severity:

```c
#define AS_LOG_CANTP 0   /* state transitions / verbose, level 3 shows it */
#define AS_LOG_CANTPI 0  /* info: periodic behaviour, level 2 */
#define AS_LOG_CANTPE 2  /* error: on by default */
```

- The suffix convention is `<MODULE>` (verbose), `<MODULE>I` (info), `<MODULE>E` (error). The value is the minimum `AS_LOG_LEVEL` at which the line prints; errors default to 2 so they are visible in CI logs.
- Message format: `"[%d]<text>!\n"` — leading PDU/channel/context id in brackets, trailing `\n` mandatory, `!` for error paths. `%s` of NULL or missing width for `%02X` etc. is a known source of crashes — always pass explicit widths and cast when the type differs (`(int)PduInfo.SduLength`).
- Users enable channels at runtime with the `AS_LOG_<MODULE>` environment variable (e.g. `AS_LOG_CANTPE=3`), so add logs freely but keep the default level quiet.

---

## 8. Error Handling

- Public APIs returning a status use `Std_ReturnType` with `E_OK` / `E_NOT_OK`.
- Parameter/state violations are logged with `ASLOG(<MODULE>E, ...)` and rejected with `E_NOT_OK`; the `Det_ReportError` service exists (`infras/diagnostic/Det`) but most infras modules currently rely on ASLOG — do not introduce scattered Det calls without a decision to use them module-wide.
- Interrupt safety: guard shared state with `EnterCritical()` / `ExitCritical()` behind a module-local macro so it can be compiled out on host builds (see `USE_CANTP_CRITICAL` → `tpEnterCritical()`).
- `assert()` is used in host-side code (`tools/`, app code built for `_WIN32`) but not in target-path AUTOSAR modules.

---

## 9. Build Files

- Each module directory carries an `SConscript` that registers sources with the shared build helpers (`RegisterConfig`, `libsForApp`); generated code goes to `GEN/` and is never committed by hand — it is produced by `tools/generator/<Module>.py` from the app's JSON config.
- Keep generator and module in sync: if you add a config knob, update both the generator and the `<Module>_Cfg` handling in the same change.

---

## 10. Tools

### 10.1 Formatting

- `.clang-format` at the repo root is the single source of truth for layout. Run `clang-format -i` on changed files (VS Code / clangd apply it automatically on save when configured).
- `python tools/format.py -i <file>` rewrites the seven section banners to the canonical text — run it when a banner got mangled or a new file needs the skeleton.

### 10.2 Static Analysis

- `tools/cppcheck/cppcheck.py` runs cppcheck with the rules in `tools/cppcheck/MISRA_C_2012.txt`. Run it on modules you touch; new warnings you introduce should be fixed, not suppressed.

---

**Version**: 1.6
**Date**: November 2026
**Based on**: SSAS codebase patterns, repo-root `.clang-format`, `tools/format.py`
