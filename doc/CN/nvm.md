---
layout: post
title: AUTOSAR 非易失性数据存储（NvM / FEE）
category: AUTOSAR
comments: true
---

# AUTOSAR 非易失性数据存储（NvM / FEE）

绝大多数嵌入式系统都需要一片空间来保存关键非易失性数据，可选器件很多：TF/SD 卡、NAND/NOR Flash 等，汽车电子中最常用的则是 EEPROM 和 Flash。

本文先介绍上层 NvM 的配置与 API，再介绍其 Flash 后端 Fee（Flash EEPROM Emulation）的实现原理。

## 一、EEPROM 与 Flash 的区别

EEPROM 是很常用的方案。一般来说 Flash 和 EEPROM 在写数据前都要先擦除（部分新型 EEPROM 可按字节直接写），EEPROM 的最小擦除单元通常为 8~32 字节，而 Flash 往往在 512 字节以上，且整片擦除，因此 EEPROM 的使用软件更简单。

例如汽车常见的里程数据（总里程 Odometer 4 字节 + 小计里程 Tripmeter 2 字节 + 2 字节校验，共 8 字节），在 EEPROM 上可固定分配从地址 0 开始的 8 个最小擦除单元循环使用：每次冷启动取总里程最大值作为当前值，并据此知道下一次写入地址。也就是说，EEPROM 的典型用法是"一个数据一个或多个固定地址的槽"。

Flash 则不然：最小擦除单元太大，有些 MCU 内部只有寥寥几个 Flash 块，且擦除必须整块进行，EEPROM 式的用法不再现实。于是需要用 Flash 模拟 EEPROM，这就是 AUTOSAR 中的 **Fee（Flash EEPROM Emulation）** 模块。（有些 MCU 标称片内带 EEPROM，同时注明是 Flash 模拟的，通常是芯片厂实现了一套类似算法。）

## 二、NvM 服务层

NvM（NVRAM Manager）位于应用与 Fee/Ea 之间，以"块（Block）"为单位管理非易失性数据：把逻辑块映射到底层 Fee（Flash 仿真）或 Ea（真实 EEPROM 抽象），负责默认值、CRC、读写作业调度等。

### 2.1 JSON 配置

NvM 配置示例（DTC 存储场景，完整文件见 [NvM.json](../../app/app/config/NvM/NvM.json)）：

```json
{
  "class": "NvM",
  "target": "Fee",
  "blocks": [
    {
      "name": "Dem_NvmEventStatusRecord{}",
      "repeat": 8,
      "NumberOfWriteCycles": 100000,
      "data": [
        { "name": "status", "type": "uint8", "default": "0x50" },
        { "name": "testFailedCounter", "type": "uint8", "default": 0 }
      ]
    }
  ]
}
```

顶层属性：

| 属性 | 说明 |
| --- | --- |
| `class` | 固定为 `"NvM"` |
| `target` | 底层存储目标：`"Fee"`（Flash 仿真）或 `"Ea"`（EEPROM 抽象） |
| `blocks` | 逻辑块列表 |

块（block）属性：

| 属性 | 说明 |
| --- | --- |
| `name` | 块名；与 `repeat` 配合时必须以 `{}` 结尾（生成 `Record0` ~ `Record7`） |
| `repeat` | 可选，块实例个数 |
| `NumberOfWriteCycles` | 可选，该块生命周期内的最大写入次数，**默认 10,000,000**，供寿命核算使用 |
| `data` | 块内数据元素列表 |

数据元素（data）属性：

| 属性 | 说明 |
| --- | --- |
| `name` | 元素名；数组形式以 `{}` 结尾 |
| `repeat` | 可选，元素重复份数 |
| `type` | 标量：`int8/int16/int32/uint8/uint16/uint32/uint64`；数组：`<type>_n`（配合 `size`），如 `uint8_n` |
| `size` | 数组元素个数 |
| `default` | 默认值，按 Python 表达式求值（支持 `"0x50"`、`"[0xFF]*13"` 等），用于 ROM 默认值初始化 |

生成器 [NvM.py](../../tools/generator/NvM.py) 读取 JSON 后生成块配置 C 代码（含每块大小、写入次数上限、ROM 默认值），并自动生成按 `repeat` 展开的实例名。

### 2.2 主要 API

NvM 作业是异步的：API 发起请求后由 `NvM_MainFunction` 轮询驱动，结果通过 `NvM_GetErrorStatus` 查询（见 [NvM.h](../../infras/include/NvM.h)）：

```c
void NvM_Init(const NvM_ConfigType *ConfigPtr);

Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvM_DstPtr);   /* 读单个块（无效则给默认值） */
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvM_SrcPtr);
Std_ReturnType NvM_RestoreBlockDefaults(NvM_BlockIdType BlockId, void *NvM_DstPtr);
Std_ReturnType NvM_EraseNvBlock(NvM_BlockIdType BlockId);
Std_ReturnType NvM_InvalidateNvBlock(NvM_BlockIdType BlockId);
Std_ReturnType NvM_SetRamBlockStatus(NvM_BlockIdType BlockId, boolean BlockChanged);
Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, NvM_RequestResultType *RequestResultPtr);

void NvM_ReadAll(void);    /* 开机批量读取所有块 */
void NvM_WriteAll(void);   /* 下电前批量写回所有块 */
void NvM_FirstInitAll(void);
void NvM_MainFunction(void);   /* 必须周期性调用 */
```

## 三、FEE 基本原理

具体实现位于 [infras/memory/Fee](../../infras/memory/Fee)，基本原理如下图：

![autosar-fee-mapping.png](../images/autosar-fee-mapping.png)
<center> 图 1 AUTOSAR FEE 原理图 </center>

上图展示了用 2 个 Flash 块模拟 EEPROM 的过程：使用中总有一个 Flash 块空闲（3 个及以上块的原理类似）。系统就绪时两个块都是空的，软件先使用 BANK0。因为块为空，软件很容易确定下一个数据块 ID 域的底部地址和 DATA 域的上部地址。通常：

* **ID 域**：至少包含数据块编号与数据地址等信息，大小和结构固定；
* **DATA 域**：大小和结构不固定，可在其中附加 CRC/checksum 保证完整性。

图 1 Group1 是依次写入数据块 0/1/2、再写一次数据块 0 之后的状态：存储空间按写入先后动态分配，冷启动时根据 ID 域即可找到每个块最新的有效数据。ID 域与 DATA 域从两头向中间生长，虽然各块数据域大小不同，动态分配也不会浪费空间。

Group2：当 ID 域和 DATA 域合拢、BANK0 剩余空间不足时，软件把所有块的最新数据备份（compact）到 BANK1；之后如 Group3 所示，BANK0 被擦除，留待 BANK1 写满时轮换使用。

本 FEE 实现用自研的 [factory 库](../../infras/libraries/factory)把复杂流程拆成显式状态机，各状态下的工序在 [factory.json](../../infras/memory/Fee/factory.json) 中定义。FEE 共 4 个工作状态（4 台状态机）：

* 初始化（Init）
* 读数据（Read）
* 写数据（Write）
* 数据备份（Backup）

### 3.1 初始化（Fee_Init）

初始化要遍历所有 FEE Bank 的管理区（Admin），确定活动 bank（当前可读写的 bank）；考虑突然掉电，还要判断活动 bank 剩余空间是否足够写入新数据，不足则进入备份流程。

![fee-init.png](../images/fee-init.png)
<center> 图 2 FEE 初始化流程 </center>

[Fee_Priv.h](../../infras/memory/Fee/Fee_Priv.h) 中 `Fee_BankAdminType` 的布局如下，由三部分组成：

```c
 High: | Full Magic | ~ Full Magic | <- Status -\
       | Number     | ~ Number     | <- Info     + <- Bank Admin
 Low:  | FEE Magic  | ~ FEE Magic  | <- Header -/
```

* **Header - FEE Magic**：标识这是被 FEE 正确管理的 Flash Bank（代码中为 ASCII `"FEEF"`）；
* **Info - Number**：记录本 Flash Bank 已被擦写的次数，达到阈值（`FEE_MAX_ERASED_NUMBER`，默认 1,000,000）意味着该 bank 寿命终止；
* **Status - Full Magic**：默认为空白（`0xFFFFFFFF`），bank 无可用空间、开始备份时写入满标记（ASCII `"DEAD"`，见 `FEE_BANK_FULL_MAGIC`）。

三部分位于三个不同的 page，保证可分别独立写入（掉电时不会互相破坏）。

初始化工序（对应 [factory.json](../../infras/memory/Fee/factory.json) 的 Init 状态机）：

1. 初始化，BankID = 0；
2. **ReadBankAdmin**：读取 BankID 所指 bank 的 Admin 以及紧邻 Admin 的第一个 Block（page）；
3. 校验 Admin 中 Header 的 FEE Magic 是否正确，正确则转到步骤 5；
4. **EraseInvalidBank**：擦除该 bank，转到步骤 7；
5. **BlankCheckInfo**：对 Admin 的 Info 做 `Fls_BlankCheck` 确认状态，为空则填充为 `FLS_ERASED_VALUE`；
6. **BlankCheckBlock**：对第一个 Block（page）做 `Fls_BlankCheck`，确认该 bank 是否为空，为空则填充为 `FLS_ERASED_VALUE`。

   > `Fls_BlankCheck` 是可选操作，用于兼容某些擦除后存储值不恒为 `FLS_ERASED_VALUE`（0xFF）的 Flash（如 TC387），以区分读到的是有效数据还是刚擦除的空白状态。

7. BankID++，所有 bank 的 Admin 都读完则进入下一步，否则回到步骤 2；
8. **CheckBankInfo**：检查各 Admin 中 Info 的 Number 是否有效，无效则写入已知的最大 Number（应对擦写 Admin 过程中掉电）；
9. **CheckBankMagic**：检查是否存在非法 Magic（刚被擦过的 bank），有则补写合法 Magic；
10. **GetWorkingBank**：遍历所有 bank 的 Admin 找到活动 bank，被标记为满的 bank 优先；
11. **SearchFreeSpace**：从活动 bank 中顺序遍历已有数据，定位最新有效数据与空闲边界；剩余空间不足以容纳最大块（`FEE_MIN_FREE_SPACE`）则启动备份，否则初始化完成。

### 3.2 读数据（Fee_Read）

Read 状态机只有 **ReadData** 与 **SearchNext** 两个工序：

1. **ReadData**：读取块上下文中记录的最新副本地址，经 Fls 读出数据（`FLS_DIRECT_ACCESS` 时可直接内存访问）；数据尾部带 CRC16 与其按位取反两份校验值，先比对反码再重算 CRC16，均通过才把数据拷给调用方；
2. **SearchNext**：若该副本 CRC 无效（掉电损坏），把当前地址作废，继续向 bank 前部搜索同一 BlockNumber 的下一个（更旧的）有效副本，重复 ReadData；
3. 若整个 bank 都找不到有效副本，则返回该块的 **ROM 默认值**（配置中的 `default`），作业正常结束。

### 3.3 写数据（Fee_Write）

Write 状态机有 **WriteCheckDataChanged / WriteAdmin / WriteData** 三个工序：

1. **WriteCheckDataChanged**：若该块已有历史副本，先读出旧数据并做 CRC 校验；与待写数据逐字节比较，**内容未变化则直接结束**（不占用 Flash，写入磨损最小化）；旧数据 CRC 已损坏则视为必须重写；
2. **WriteAdmin**：检查 ID 域与 DATA 域之间的剩余空间是否还放得下"块管理信息 + 对齐后数据"（`FEE_BLOCK_ADMIN_AND_DATA_SIZE`）。放得下则从 ID 域一侧写入块管理信息（BlockNumber 等）；放不下则先触发 Backup 状态机做压缩换 bank，再继续；
3. **WriteData**：从 DATA 域一侧写入数据本体（自动 2 字节对齐并附加 CRC16 与反码），完成后更新块上下文的最新地址。

### 3.4 数据备份（Backup）

![fee-backup.png](../images/fee-backup.png)
<center> 图 3 FEE 数据备份流程 </center>

Backup 状态机工序（factory.json 中共 14 个节点）：

1. **ReadAdmin**：读取当前 bank 的 Admin 与第一个 Block；
2. **CheckBankStatus**：对 Admin 的 Status 做 BlankCheck，为空则填充 `FLS_ERASED_VALUE`；
3. **EnsureFull**：Status 为空白时写入 `FULL_MAGIC`（"DEAD"），把当前 bank 标记为满；
4. **ReadNextBankAdmin**：读取下一个 bank 的 Admin 与第一个 Block；
5. **BlankCheckNextBankEmpty**：BlankCheck 下一个 bank 的第一个 Block，判断其是否为空；
6. **EnsureNextBankEmpty**：确认下一个 bank 处于可接收备份的空白状态；
7. **EraseNextBank**：擦除下一个 bank；
8. **SetNextBankAdmin**：为下一个 bank 写入合法 Admin。

   > 步骤 7、8 充分考虑备份过程中掉电：掉电后无法知道备份进行到何处，而掉电是小概率事件，为简化实现，干脆擦除重来。

9. **CopyAdmin**：循环，为每个仍有有效数据的块写入新的块管理信息；
10. **CopyReadData**：读取当前 bank 中合法地址指向的数据，CRC 有效则转到步骤 12；
11. **SearchNextData**：在当前 bank 中继续查找下一个有效数据；
12. **CopyData**：把有效数据写入新 bank；未备份完则回到步骤 9；
13. **EraseBank**：全部拷贝完成后擦除当前（已满的）bank；
14. **SetBankAdmin**：为擦除后的 bank 写入合法 Admin，成为新的空闲备用 bank。

## 四、Flash 寿命核算

车载应用通常要求 10 年以上的数据保持期，需要验证 FEE 配置下 bank 的擦除次数不会超过 Flash 的擦写寿命（常见 10 万~100 万次）。

[FeeLifeCycle.py](../../tools/utils/memory/FeeLifeCycle.py) 根据 NvM.json 中各块大小与 `NumberOfWriteCycles` 计算最坏情况下的 bank 备份（擦除）轮数：

```bash
# 基本用法
python tools/utils/memory/FeeLifeCycle.py app/app/config/NvM/NvM.json

# 详细输出
python tools/utils/memory/FeeLifeCycle.py app/app/config/NvM/NvM.json -v

# 自定义 bank 参数
python tools/utils/memory/FeeLifeCycle.py app/app/config/NvM/NvM.json --block_size "32*1024" --num_of_banks 4 -v
```

参数：

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `config` | NvM.json 路径 | 必填 |
| `-v/--verbose` | 输出详细计算过程 | 关闭 |
| `--block_size` | bank 大小（支持表达式） | `32*1024`（32KB） |
| `--page_size` | Flash page 大小（字节） | 8 |
| `--num_of_banks` | bank 数量 | 2 |

工具从两种角度估算最坏备份轮数并取最大值：

* **场景 A（逐块求和）**：每个块独立写入，分别计算空 bank 首次可容纳的份数、每次备份后剩余空间可追加写入的份数，求和后除以 bank 数；
* **场景 B（总量累计）**：所有块的写入同时消耗 bank 空间，按累计写入量计算备份轮数。

实践建议：

* `NumberOfWriteCycles` 按真实需求估算（每日写入次数 × 365 × 寿命年限 × 安全系数 2~10），不要盲目使用默认的一千万；
* bank 越大，备份/擦除频率越低、寿命越长，但单次备份耗时也越长，需按擦除时间权衡；
* 每次修改 NvM 配置后重新运行核算，建议纳入持续集成检查。
