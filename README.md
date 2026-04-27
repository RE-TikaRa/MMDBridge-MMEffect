# MMDBridge

MMDBridge 是一个面向 **MikuMikuDance 64 位版** 的 D3D9 桥接插件。  
它会在运行时拦截 MMD 的渲染数据，并把模型、骨骼、材质、贴图、相机等信息交给内置 Python 脚本处理，用于导出 PMX、VMD、Alembic 等数据。

## 当前状态

- 已整理到 **Visual Studio 2022**
- 已确认可编译 **Release / x64**
- 已修复中文系统下插件设置窗口的**下拉框空白**问题
- 插件设置界面已提供**中文 / 原文（日语）**切换
- 已兼容 `MMEffect_x64_v037`
- 已切换到 **Python 3.13.3** 路线

当前推荐安装包：

- `dist/MMDBridge_MMEffect_x64_v037_python313_fixed.zip`

## 功能概览

- 拦截 D3D9 / D3DX9 渲染调用
- 获取当前帧的模型、骨骼、材质、贴图与相机信息
- 通过 Python 脚本执行导出逻辑
- 支持导出：
  - PMX
  - VMD
  - Alembic

插件设置界面支持：

- 选择导出脚本
- 控制脚本是否执行
- 设置导出帧范围
- 设置导出 fps
- 中文 / 日语界面切换

## 环境要求

- Windows 64 位
- Visual Studio 2022（编译时）
- MikuMikuDance 64 位版
- DirectX SDK (June 2010) 或等效 D3DX9 头文件 / 库
- Python 3.13.3
- MMD 自带：
  - `Data/MMDExport.h`
  - `Data/MMDExport.lib`

## 仓库结构

```text
src/d3d9             主桥接逻辑、插件设置窗口、Python 入口
src/d3dx9_32         D3DX9 相关代理层
src/MikuMikuFormats  PMX / VMD 读写
src/umbase           基础工具代码
project/             旧版 Visual Studio 工程
Release/             脚本与构建产物目录
```

## 构建

当前只保留并推荐使用：

- `cmake_vs2022_64.bat`
- `hook.sln`

### 方式一：CMake

执行：

```bat
cmake_vs2022_64.bat
```

会生成：

```text
build_vs2022_64/mmdbridge.sln
```

### 方式二：直接用旧 solution

直接使用：

- `hook.sln`

用 Visual Studio 2022 打开并编译：

- `Release`
- `x64`

## 安装

### 推荐方式

直接使用已经整理好的安装包：

- `dist/MMDBridge_MMEffect_x64_v037_python313_fixed.zip`

### 安装目标目录

例如：

- `C:\MikuMikuDance V10th - CHS`

### 解压后目录中至少应存在

```text
C:\MikuMikuDance V10th - CHS
  d3d9.dll
  d3d9_mme.dll
  d3dx9_32.dll
  D3DX9_43.dll
  python313.dll
  MMEffect.dll
  MMHack.dll
  MikuMikudance.exe
  Data\
```

其中 `Data` 目录内应包含：

```text
Data\
  MMDExport.h
  MMDExport.lib
```

## 与 MMEffect_x64_v037 共存

当前整理后的结构如下：

- `d3d9.dll`：MMDBridge 主代理
- `d3d9_mme.dll`：MMEffect 原始 `d3d9.dll` 改名后的版本
- `MMEffect.dll`
- `MMHack.dll`

运行链路大致为：

- MMDBridge 的 `d3d9.dll`
- 转发到 `d3d9_mme.dll`
- 再由 MMEffect / 系统 D3D9 完成剩余流程

这样可以尽量同时保留：

- MMDBridge 菜单
- MMEffect 菜单

## 启动后如何确认安装成功

启动 MMD 后，重点检查：

1. `MikuMikudance.exe` 是否能正常启动
2. 菜单中是否能看到 **MMDBridge**
3. 菜单中是否还能看到 **MMEffect**
4. MMDBridge 设置窗口中的“使用脚本”下拉框是否能扫描出脚本

如果以上都正常，说明安装基本成功。

## 常见问题

### 1. 启动时报 0xc000007b

通常意味着 **32 位 / 64 位 DLL 混用**，或者运行库不匹配。  
请优先检查：

- `MikuMikudance.exe` 是否为 64 位
- `d3d9.dll` 是否为 64 位
- `d3d9_mme.dll` 是否为 64 位
- `MMEffect.dll` 是否为 64 位
- `python313.dll` 是否为 64 位

### 2. 提示找不到 python34.dll

当前整理版应当使用：

- `python313.dll`

而不是：

- `python34.dll`

如果仍然提示缺少 `python34.dll`，说明目录里残留了旧版本文件，或者没有被新包正确覆盖。

### 3. “使用脚本”里扫描不到内容

请确认：

- `.py` 文件和 `MikuMikudance.exe` 在同一目录层级
- 新版 `d3d9.dll` 已正确覆盖到 MMD 根目录
- 没有误用旧版 DLL

### 4. MME 菜单不见了

请确认：

- `d3d9_mme.dll` 存在
- `MMEffect.dll` 存在
- `MMHack.dll` 存在
- 当前使用的是整理后的共存包，而不是只放了 MMDBridge 单独 DLL

### 5. 缺少 VC 运行库

如果系统报缺少 VC 相关 DLL，请安装：

- Visual C++ 2015–2022 x64 Runtime

## 许可证

本项目使用 MIT License：

- [LICENSE.txt](LICENSE.txt)

此外，运行包中还可能包含：

- Python 许可证
- Alembic 许可证
- MMEffect 附带说明文件

请按各自文件说明使用。

## 贡献者

- [Contributors](https://github.com/RE-TikaRa/MMDBridge/graphs/contributors)
