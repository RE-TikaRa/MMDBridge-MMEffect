<a id="readme-top"></a>

<div align="center">
# MMDBridge-MMEffect Fix

把 **MMDBridge 0.7.1 64bit** 整理成可维护的 VS2022/x64 工程，  
并作为前置桥接层与 **MMEffect x64 v0.37** 共存、扩展。

[构建说明](docs/BUILD.md) · [发行包说明](docs/PACKAGES.md) · [MME 集成说明](docs/MME_INTEGRATION.md)

</div>

---

## 目录

- [项目简介](#项目简介)
  - [使用技术](#使用技术)
- [开始使用](#开始使用)
  - [环境要求](#环境要求)
  - [安装与构建](#安装与构建)
- [使用说明](#使用说明)
- [参与维护](#参与维护)
- [许可证](#许可证)
- [致谢](#致谢)

---

## 项目简介

这个仓库的目标很简单：

- 保留 MMDBridge 的桥接与导出能力
- 和 MMEffect 原版运行链路共存
- 在不重写 MME 的前提下，给 MME 加功能

当前运行关系：

```text
d3d9.dll        当前桥接前端
  -> d3d9_mme.dll   原版 MME d3d9 后端
    -> MMEffect.dll / MMHack.dll
```

已经完成的整理与增强：

- VS2022 / x64 工程整理
- Python 3.13.3 路线切换
- MMDBridge 设置窗口中文 / 原文（日语）切换
- 中文系统下脚本下拉框空白修复
- MME `Effect Mapping` 窗口右键新增：
  - `Open Object Folder`
  - `Open Effect Folder`

当前提供三种包：

- 完整桥接包  
  `dist/MMDBridge_MMEffect_x64_v037_python313_fixed.zip`
- MMDBridge 专用包  
  `dist/MMDBridge_x64_python313_only.zip`
- MME 专用精简包  
  `dist/MMEffect_x64_v037_bridge_only.zip`

<p align="right">(<a href="#readme-top">回到顶部</a>)</p>

### 使用技术

- Visual Studio 2022
- C++
- Direct3D 9 / D3DX9
- Python 3.13.3
- pybind11
- MMEffect x64 v0.37 runtime

<p align="right">(<a href="#readme-top">回到顶部</a>)</p>

---

## 开始使用

### 环境要求

运行或构建前，至少需要：

- Windows x64
- MikuMikuDance 64 位版
- Visual Studio 2022（编译时）
- Python 3.13.3
- MMD 自带：
  - `Data/MMDExport.h`
  - `Data/MMDExport.lib`

当前仓库已经整理了本地构建依赖，包括：

- DXSDK 头库 / lib
- pybind11
- MMDExport 本地副本

细节见：

- [docs/BUILD.md](docs/BUILD.md)

### 安装与构建

#### 1. 编译

直接用：

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

核心产物：

```text
Release/x64/d3d9.dll
Release/x64/d3dx9_43.dll
```

#### 2. 安装到 MMD 根目录

##### 完整桥接包

适合：

- 需要 MMDBridge 导出脚本
- 需要脚本扫描
- 同时保留 MMEffect

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
  Data\
  *.py
```

##### MME 专用精简包

适合：

- 只想保留 MME 使用链路
- 不需要 MMDBridge 导出脚本

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

- 当前桥接前端仍然链接 `python313.dll`
- 所以即使是 MME 专用包，也不能删这个 DLL

更多包差异见：

- [docs/PACKAGES.md](docs/PACKAGES.md)

<p align="right">(<a href="#readme-top">回到顶部</a>)</p>

---

## 使用说明

### 脚本扫描规则

MMDBridge 当前只扫描：

> 与 `MikuMikudance.exe` 同级目录下的 `*.py`

也就是说：

- 能识别：`C:\MMD\mmdbridge_obj_general.py`
- 不能识别：`C:\MMD\scripts\mmdbridge_obj_general.py`

如果“使用脚本”下拉框为空，优先检查：

1. 根目录有没有 `.py`
2. 脚本是不是被放进了子目录
3. 当前是不是用的 MME 专用精简包

### 启动后建议检查

1. `MikuMikudance.exe` 能否正常启动
2. 菜单里是否能看到 `MMDBridge`
3. 菜单里是否还能看到 `MMEffect`
4. 完整桥接包下，MMDBridge 设置窗口是否能扫描到脚本
5. MME `Effect Mapping` 窗口右键是否出现：
   - `Open Object Folder`
   - `Open Effect Folder`

### 常见问题

#### 启动时报 0xc000007b

通常是 32/64 位 DLL 混用。优先检查：

- `MikuMikudance.exe`
- `d3d9.dll`
- `d3d9_mme.dll`
- `MMEffect.dll`
- `python313.dll`

#### 提示找不到 `python34.dll`

说明目录里残留了旧版 MMDBridge 文件。  
当前整理版使用的是：

- `python313.dll`

#### MME 菜单不见了

请确认：

- `d3d9_mme.dll` 存在
- `MMEffect.dll` 存在
- `MMHack.dll` 存在

#### MME 右键菜单没有新增项

请确认当前使用的是本仓库重新编译后的：

- `d3d9.dll`

不是旧版桥接 DLL。

<p align="right">(<a href="#readme-top">回到顶部</a>)</p>

---

## 参与维护

这个仓库更偏向整理、修复和功能增强。  
如果继续扩展，建议优先保持：

- 不破坏原版 MME 二进制兼容性
- 不随意改动桥接链路
- 改安装方式或依赖时同步更新文档

目前推荐同时维护这些文档：

- [docs/BUILD.md](docs/BUILD.md)
- [docs/PACKAGES.md](docs/PACKAGES.md)
- [docs/MME_INTEGRATION.md](docs/MME_INTEGRATION.md)

<p align="right">(<a href="#readme-top">回到顶部</a>)</p>

---

## 许可证

当前仓库的主许可证为 MIT：

- [LICENSE.txt](LICENSE.txt)

原始与第三方许可继续保留，见：

- [docs/LICENSES.md](docs/LICENSES.md)
- [licenses/MMDBridge-original-LICENSE.txt](licenses/MMDBridge-original-LICENSE.txt)
- `ori/MMEffect_x64_v037_ori/MMEffect_x64_v037/MMEffect.txt`
- `ori/MMDBridge_071_Alembic_64bit/MMDBridge/Alembic-LICENSE.txt`
- `ori/MMDBridge_071_Alembic_64bit/MMDBridge/Python-License.txt`

<p align="right">(<a href="#readme-top">回到顶部</a>)</p>

---

## 致谢

- [othneildrew/Best-README-Template](https://github.com/othneildrew/Best-README-Template)
- MMDBridge 0.7.1 64bit 原作者与历史维护者
- MMEffect x64 v0.37 原作者与相关资料
- Microsoft DirectX SDK (June 2010)
- pybind11

<p align="right">(<a href="#readme-top">回到顶部</a>)</p>
