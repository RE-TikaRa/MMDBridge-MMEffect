# MME 模型绘制顺序调整

本文记录当前仓库对 MME `Effect Mapping` 窗口新增的模型绘制顺序调整能力。

---

## 功能入口

在 MME `Effect Mapping` 窗口中，当前桥接层会在菜单中新增：

- `Move Model Earlier`
- `Move Model Later`

选中某个模型映射行后，可通过这两个菜单项调整该模型在 MMD 内部模型队列中的绘制顺序。

---

## 当前实现范围

当前只处理 **PMD / PMX 模型绘制顺序**。

不处理：

- `.x` 附件顺序
- MME `preprocess` / `postprocess` 阶段顺序
- effect technique / pass 顺序
- 不同 MMD 版本的自动偏移适配

---

## 实现位置

核心实现位于：

- [../src/d3d9/d3d9.cpp](../src/d3d9/d3d9.cpp)

相关菜单项 ID：

```cpp
kMMEMenuMoveModelEarlier = 49033
kMMEMenuMoveModelLater = 49034
```

主要函数：

```cpp
resolve_pmd_index_from_text()
get_pmd_internal_order_field()
find_pmd_index_by_internal_order()
can_move_pmd_internal_order()
move_pmd_internal_order()
```

---

## 逆向定位结论

测试环境：

```text
C:\MikuMikuDance V10th - CHS
```

当前 `MikuMikudance.exe`：

- x64
- ImageBase：`0x140000000`
- `ExpGetPmdOrder` RVA：`0xDB600`
- `ExpGetAcsOrder` RVA：`0xDB980`
- `ExpGetCurrentObject` RVA：`0xDBB60`

已定位到的关键结构：

```text
MikuMikudance.exe + 0x1445F8  -> root
root + 0x0BE8                -> PMD/PMX 模型槽数组
root + 0x9E840               -> X 附件槽数组
root + 0xA1B48               -> 模型前附件数量
model + 0x3108               -> 模型内部顺序字段，1 byte
accessory + 0x04AD           -> 附件内部顺序字段，1 byte
```

`ExpGetPmdOrder()` 的核心逻辑等价于：

```text
模型最终顺序 = 模型前附件数量 + model[0x3108]
```

`ExpGetAcsOrder()` 的核心逻辑等价于：

```text
若附件在模型前：
  最终顺序 = -(accessory[0x04AD] + 1)
否则：
  最终顺序 = 模型数量 + accessory[0x04AD] + 1
```

当前功能通过交换两个模型对象的 `model + 0x3108` 字段来改变模型绘制顺序。

---

## 版本绑定说明

该实现依赖当前 `MikuMikudance.exe` 的内部偏移。

如果更换 MMD 主程序版本，需要重新确认：

- `root` 指针地址
- 模型槽数组偏移
- 模型内部顺序字段偏移

否则菜单项可能失效，或者写入错误内存。

---

## 构建与测试记录

构建命令：

```powershell
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
msbuild hook.sln /p:Configuration=Release /p:Platform=x64 /nologo /m
```

已通过构建：

```text
Release/x64/d3d9.dll
```

已部署到测试环境：

```text
C:\MikuMikuDance V10th - CHS\d3d9.dll
```

部署前备份：

```text
C:\MikuMikuDance V10th - CHS\d3d9.dll.bak_order_20260505_204213
```

---

## 后续可扩展方向

- 支持 `.x` 附件顺序调整
- 增加 “Move to Top / Move to Bottom”
- 将菜单文字中文化
- 在 UI 中显示当前模型顺序编号
- 为不同 MMD 版本增加偏移签名扫描
