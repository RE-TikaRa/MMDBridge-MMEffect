# MME 集成与增强说明

本文档描述当前仓库与 `MMEffect_x64_v037` 的关系，以及已经在桥接层实现的增强功能。

---

## 基本模式

当前工程不是直接重写 `MMEffect.dll`，而是采用：

- 前置桥接层
- 原版 MME 后端

运行模式：

```text
d3d9.dll        当前桥接前端
  -> d3d9_mme.dll   原版 MME d3d9 后端
    -> MMEffect.dll / MMHack.dll
```

因此，这个仓库更接近：

> 给原版 MME 增强功能的桥接壳层

而不是：

> 完整替代 MME 的重写实现

---

## 当前已确认的 MME 相关界面

通过原版 `MMEffect.dll` 资源可确认存在：

- `Effect Mapping / エフェクトファイル割り当て` 对话框

该窗口关键控件：

- `SysTabControl32`：ID `1002`
- `SysListView32`：ID `1003`

也就是说，object/effect 分配界面本质上是：

- Tab
- ListView
- 一行表示一条 object -> effect 映射

---

## 当前新增功能

### 1. MME 映射窗口右键新增文件夹入口

在 `Effect Mapping` 窗口中，针对整行映射项，当前桥接层会固定新增：

- `Open Object Folder`
- `Open Effect Folder`

设计原则：

- 不依赖右键点中哪一列
- 不使用“点 object 列 / effect 列”推断
- 因为原界面本来就是整行选择

### 2. 启用 / 禁用逻辑

#### Object

当当前映射项能解析到 object 源文件路径时：

- 启用 `Open Object Folder`

object 路径主要来自：

- `ExpGetPmdFilename`
- `ExpGetAcsFilename`

#### Effect

当当前 effect 不是以下特殊值时：

- `(none)`
- `(hide)`
- `default`
- `(default)`

并且能解析到路径时：

- 启用 `Open Effect Folder`

### 3. MME 映射窗口新增模型绘制顺序调整

在 `Effect Mapping` 窗口中，当前桥接层还新增：

- `Move Model Earlier`
- `Move Model Later`

该功能通过当前选中行解析到对应 PMD / PMX 模型，然后交换 MMD 内部模型顺序字段。

当前只支持模型，不支持 `.x` 附件。

详细逆向记录和偏移说明见：

- [MME_MODEL_ORDER.md](MME_MODEL_ORDER.md)

---

## 路径解析策略

### object

优先顺序：

1. 行文本本身就是可解析路径
2. 行文本匹配已加载模型 / 配件文件名
3. 行文本匹配文件 stem

### effect

优先顺序：

1. 行文本本身就是可解析路径
2. 按 object 所在目录拼接
3. 按 MMD 根目录拼接
4. 若无扩展名，补 `.fx`

---

## 为什么 MME 专用包仍带 `python313.dll`

虽然 MME 专用包不附带导出脚本，但当前前置桥接 `d3d9.dll` 仍然链接：

- `python313.dll`

所以：

- “MME 专用” ≠ “去掉 Python 依赖”

如果后续要做真正的“纯 MME、无 Python 依赖包”，需要继续做代码级拆分。

---

## 当前限制

### 1. 仍然依赖原版 MME 二进制

当前功能增强发生在：

- 前置桥接层

不是直接修改：

- `MMEffect.dll` 源码

### 2. 路径解析依赖运行时行文本和当前对象集

极端情况下，如果 object/effect 行文本与实际文件关系过于特殊，可能需要继续补规则。

### 3. 模型顺序调整依赖特定 MMD 版本偏移

当前模型绘制顺序调整功能绑定测试环境中的 `MikuMikudance.exe`。

如果更换 MMD 主程序版本，需要重新定位内部结构偏移。

---

## 适合继续扩展的方向

基于当前桥接层，还适合继续做：

- MME 映射窗口中文化 / 双语化
- 批量打开 / 批量复制路径
- object/effect 路径复制到剪贴板
- effect 缺失检测
- 常用 effect 目录快捷入口
- `.x` 附件绘制顺序调整

---

## 相关文档

- [../README.md](../README.md)
- [BUILD.md](BUILD.md)
- [PACKAGES.md](PACKAGES.md)
