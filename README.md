# MMDBridge-MMEffect

把 **MMDBridge 0.7.1 64bit** 整理成可维护的 **Visual Studio 2022 / x64** 工程，  
并作为前置桥接层与 **MMEffect x64 v0.37** 共存、扩展。

---

## 项目定位

这个仓库不是重写 MME，而是：

- 保留 MMDBridge 的桥接与导出能力
- 与原版 MMEffect 运行链路共存
- 在前置桥接层给 MME 增加功能

当前运行关系大致如下：

```text
d3d9.dll        当前桥接前端
  -> d3d9_mme.dll   原版 MME d3d9 后端
    -> MMEffect.dll / MMHack.dll
```

也就是说，当前工程更接近：

> 用 MMDBridge 做前置壳层，兼容并增强原版 MMEffect。

---

## 当前状态

- 已整理到 **Visual Studio 2022**
- 已确认可编译 **Release / x64**
- 已切换到 **Python 3.13.3**
- 已兼容 `MMEffect_x64_v037`
- 已修复中文系统下 MMDBridge 设置窗口脚本下拉框空白
- 已提供 MMDBridge 设置窗口 **中文 / 原文（日语）** 切换
- 已为 MME `Effect Mapping` 窗口新增右键：
  - `Open Object Folder`
  - `Open Effect Folder`
- 已为 MME 上方分页页签新增：
  - 鼠标滚轮翻页
  - 首尾循环切换

---

## 功能概览

### MMDBridge 侧

- Hook D3D9 / D3DX9 渲染调用
- 获取模型、骨骼、材质、贴图、相机等运行时数据
- 通过 Python 脚本执行导出逻辑
- 支持导出：
  - PMX
  - VMD
  - Alembic

### MMEffect 侧

- 保留原版 `MMEffect.dll` / `MMHack.dll` 运行链路
- 在不改原版 MME 二进制源码的前提下增强界面交互
- 当前重点增强：
  - `Effect Mapping` 窗口右键快捷打开目录
  - 多分页页签滚轮翻页

---

## 仓库结构

```text
src/d3d9             MMDBridge 主桥接逻辑、Python 入口、MMD/MME UI 增强
src/d3dx9_32         D3DX9 代理层，x64 输出名为 d3dx9_43.dll
src/MikuMikuFormats  PMX / VMD 读写
src/umbase           基础工具代码
project/             VS2022 工程
libs/                本地依赖、DXSDK 精简头库、MMDExport 等
ori/                 两套原版运行包参考
Release/x64/         编译产物与导出脚本
dist/                已整理好的发行包
docs/                补充文档
```

`ori/` 当前保留两套原版包，用于对照：

- `MMDBridge_071_Alembic_64bit`
- `MMEffect_x64_v037_ori`

---

## 发行包

当前提供三种包：

### 1. 完整桥接包

- `dist/MMDBridge_MMEffect_x64_v037_python313_fixed.zip`

适合：

- 同时使用 MMDBridge 和 MMEffect
- 需要导出脚本
- 需要扫描 `.py`

### 2. MMDBridge 专用包

- `dist/MMDBridge_x64_python313_only.zip`

适合：

- 只需要 MMDBridge 导出能力
- 不需要 MMEffect 运行时

### 3. MME 专用精简包

- `dist/MMEffect_x64_v037_bridge_only.zip`

适合：

- 只想保留 MME 使用链路
- 不需要 MMDBridge 导出脚本
- 需要当前桥接层对 MME 的增强功能

详细差异见：

- [docs/PACKAGES.md](docs/PACKAGES.md)

---

## 环境要求

### 运行要求

- Windows x64
- MikuMikuDance 64 位版
- Python 3.13.3 运行时

### 构建要求

- Visual Studio 2022
- MMD 自带：
  - `Data/MMDExport.h`
  - `Data/MMDExport.lib`

当前仓库已经整理了本地构建依赖：

- `libs/dxsdk/include`
- `libs/dxsdk/x64/d3dx9.lib`
- `libs/pybind11/include/pybind11`

更多细节见：

- [docs/BUILD.md](docs/BUILD.md)

---

## 构建方法

当前推荐直接使用：

- `hook.sln`

推荐配置：

- `Release`
- `x64`

命令行示例：

```powershell
$inst='C:\Program Files\Microsoft Visual Studio\2022\Professional'
& "$inst\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64
msbuild hook.sln /p:Configuration=Release /p:Platform=x64 /nologo /m
```

主要产物：

```text
Release/x64/d3d9.dll
Release/x64/d3dx9_43.dll
```

---

## 安装说明

### 完整桥接包

解压到 MMD 根目录后，至少应有：

```text
MikuMikuDance 根目录
  d3d9.dll
  d3d9_mme.dll
  d3dx9_43.dll
  python313.dll
  MMEffect.dll
  MMHack.dll
  MikuMikudance.exe
  Data\
  *.py
```

### MMDBridge 专用包

解压后至少应有：

```text
MikuMikuDance 根目录
  d3d9.dll
  d3dx9_43.dll
  python313.dll
  MikuMikudance.exe
  *.py
```

### MME 专用精简包

解压后至少应有：

```text
MikuMikuDance 根目录
  d3d9.dll
  d3d9_mme.dll
  d3dx9_43.dll
  python313.dll
  MMEffect.dll
  MMHack.dll
  MikuMikudance.exe
```

注意：

- 即使是 MME 专用包，当前桥接前端仍然链接 `python313.dll`
- 所以 `python313.dll` 不能删

---

## 使用说明

### 脚本扫描规则

MMDBridge 当前只扫描：

> 与 `MikuMikudance.exe` 同级目录下的 `*.py`

也就是说：

- 能识别：`C:\MMD\mmdbridge_obj_general.py`
- 不能识别：`C:\MMD\scripts\mmdbridge_obj_general.py`

如果“使用脚本”下拉框为空，优先检查：

1. 根目录是否真的放了 `.py`
2. 脚本是否被放进了子目录
3. 当前是否用的是 MME 专用精简包

### 启动后建议检查

1. `MikuMikudance.exe` 是否能正常启动
2. 菜单中是否能看到 `MMDBridge`
3. 菜单中是否还能看到 `MMEffect`
4. 完整桥接包下，MMDBridge 设置窗口是否能扫描到脚本
5. MME `Effect Mapping` 窗口右键是否出现新增项
6. MME 上方分页页签是否能用鼠标滚轮切换

---

## 常见问题

### 1. 启动时报 0xc000007b

通常是 32/64 位 DLL 混用。优先检查：

- `MikuMikudance.exe`
- `d3d9.dll`
- `d3d9_mme.dll`
- `MMEffect.dll`
- `python313.dll`

### 2. 提示找不到 `python34.dll`

说明目录里残留了旧版 MMDBridge 文件。  
当前整理版应使用：

- `python313.dll`

### 3. “使用脚本”里扫描不到内容

优先检查根目录是否存在 `.py` 文件。  
当前不会递归扫描子目录。

### 4. MME 菜单不见了

请确认：

- `d3d9_mme.dll` 存在
- `MMEffect.dll` 存在
- `MMHack.dll` 存在

### 5. MME 右键菜单没有新增项

请确认当前使用的是本仓库重新编译后的：

- `d3d9.dll`

而不是旧版桥接 DLL。

---

## 文档

- [docs/BUILD.md](docs/BUILD.md)
- [docs/PACKAGES.md](docs/PACKAGES.md)
- [docs/MME_INTEGRATION.md](docs/MME_INTEGRATION.md)
- [docs/LICENSES.md](docs/LICENSES.md)

---

## 参与维护

这个仓库更偏向整理、修复和增强。  
如果继续扩展，建议优先保持：

- 不破坏原版 MME 二进制兼容性
- 不随意改动桥接链路
- 调整安装方式、依赖或包结构时同步更新文档

---

## 许可证

当前仓库主许可证为 MIT：

- [LICENSE.txt](LICENSE.txt)

原始与第三方许可继续保留，见：

- [docs/LICENSES.md](docs/LICENSES.md)
- [licenses/MMDBridge-original-LICENSE.txt](licenses/MMDBridge-original-LICENSE.txt)

---

## 致谢

- [othneildrew/Best-README-Template](https://github.com/othneildrew/Best-README-Template)
- MMDBridge 0.7.1 64bit 原作者与历史维护者
- MMEffect x64 v0.37 原作者与相关资料
- Microsoft DirectX SDK (June 2010)
- pybind11
