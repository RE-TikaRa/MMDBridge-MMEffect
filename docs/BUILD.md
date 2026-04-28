# 构建说明

本文档描述当前仓库在 **Windows + VS2022 + x64** 下的可用构建方式。

---

## 目标产物

核心产物：

- `Release/x64/d3d9.dll`
- `Release/x64/d3dx9_43.dll`

其中：

- `d3d9.dll`：桥接前端
- `d3dx9_43.dll`：D3DX9 代理层

---

## 已知前提

### 1. Visual Studio

- Visual Studio 2022
- 推荐直接使用 Developer PowerShell

### 2. Python

运行时使用：

- Python 3.13.3

工程当前按本机路径引用：

- `C:\Program Files\Python313\Include`
- `C:\Program Files\Python313\libs`

### 3. MMDExport

必须提供：

- `MMDExport.h`
- `MMDExport.lib`

当前仓库约定位置：

- `libs/mmd/include/MMDExport.h`
- `libs/mmd/x64/MMDExport.lib`

如果你手头只有 MMD 安装目录里的版本，可以从：

```text
<MMD>\Data\MMDExport.h
<MMD>\Data\MMDExport.lib
```

复制到上述仓库位置。

### 4. DXSDK

当前仓库已经整理了最小可用的 DXSDK 相关内容：

- `libs/dxsdk/include`
- `libs/dxsdk/x64`

这里只保留当前工程实际需要的头文件与 import lib，不再把整套 DXSDK 文件都放进仓库。

### 5. pybind11

当前工程使用：

- `.deps/vcpkg/installed/x64-windows/include/pybind11`

---

## 推荐构建命令

```powershell
$inst='C:\Program Files\Microsoft Visual Studio\2022\Professional'
& "$inst\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64
msbuild hook.sln /p:Configuration=Release /p:Platform=x64 /nologo /m
```

---

## 当前工程里做过的关键兼容处理

### 1. 本地 DXSDK 头库接入

项目文件已指向：

- `libs/dxsdk/include`
- `libs/dxsdk/x64`

### 2. `d3dx9_43.dll` 构建链打通

`project/d3dx9/d3dx9.vcxproj` 当前会输出：

- `Release/x64/d3dx9_43.dll`

### 3. `d3dx9core.h` 本地补丁

仓库中的：

- `libs/dxsdk/include/d3dx9core.h`

已经针对 `CINTERFACE` 场景做过兼容修改，以便当前代理层源码能通过 VS2022 编译。

---

## 构建成功后

检查这些文件是否生成：

```text
Release/x64/d3d9.dll
Release/x64/d3dx9_43.dll
Release/x64/d3d9_.dll
```

其中：

- `d3d9_.dll` 是工程输出名
- `d3d9.dll` 由 post-build 拷贝得到

---

## 常见构建问题

### 缺少 `d3dx9shader.h`

说明 DXSDK include 没接上。  
优先检查：

- `libs/dxsdk/include`

### 缺少 `pybind11/*.h`

说明 `.deps/vcpkg/installed/x64-windows/include` 不存在或未初始化。

### 缺少 `MMDExport.h` / `MMDExport.lib`

说明没有从 MMD 安装目录补回本地依赖。

### `python313.lib` 找不到

说明本机 Python 安装路径与项目文件不一致，需要调整：

- `project/d3d9/d3d9.vcxproj`

---

## 不推荐的方式

当前不建议优先依赖：

- 老版本 VS2013/VS2015 工程链
- 未补齐依赖的 CMake 链

当前最可靠的是：

- `hook.sln`
- `Release|x64`

---

## 相关文档

- [../README.md](../README.md)
- [PACKAGES.md](PACKAGES.md)
- [MME_INTEGRATION.md](MME_INTEGRATION.md)
