# 发行包说明

当前仓库提供三种主要发行包。

---

## 1. 完整桥接包

文件：

- `dist/MMDBridge_MMEffect_x64_v037_python313_fixed.zip`

用途：

- 需要 MMDBridge 导出脚本
- 需要脚本扫描
- 需要与 MME 共存

典型内容：

- `d3d9.dll`
- `d3d9_mme.dll`
- `d3dx9_43.dll`
- `python313.dll`
- `MMEffect.dll`
- `MMHack.dll`
- 全套 `mmdbridge_*.py`
- `alembic_assign_scripts`

适合：

- 导出 PMX / VMD / Alembic
- 研究桥接数据
- 同时使用 MME

---

## 2. MMDBridge 专用包

文件：

- `dist/MMDBridge_x64_python313_only.zip`

用途：

- 只需要 MMDBridge 导出能力
- 需要导出脚本
- 不需要附带 MMEffect 运行时

典型内容：

- `d3d9.dll`
- `d3dx9_43.dll`
- `python313.dll`
- 全套 `mmdbridge_*.py`
- `alembic_assign_scripts`

注意：

- 这是“MMDBridge 专用包”，不附带：
  - `d3d9_mme.dll`
  - `MMEffect.dll`
  - `MMHack.dll`
- 当前桥接层在没有 `d3d9_mme.dll` 时会回退到系统 `D3D9.DLL`

适合：

- 单独使用 MMDBridge 导出脚本
- 研究桥接采集数据

---

## 3. MME 专用精简包

文件：

- `dist/MMEffect_x64_v037_bridge_only.zip`

用途：

- 只想保留 MME 使用体验
- 不需要 MMDBridge 导出脚本
- 需要当前桥接层增加的 MME 菜单增强

典型内容：

- `d3d9.dll`
- `d3d9_mme.dll`
- `d3dx9_43.dll`
- `python313.dll`
- `MMEffect.dll`
- `MMHack.dll`
- `MMEffect.txt`
- `REFERENCE.txt`

注意：

- 这是“MME 专用包”，不是“无 Python 依赖包”
- 当前 `d3d9.dll` 仍链接 `python313.dll`
- 因此 `python313.dll` 仍然必须随包提供

---

## 两者差异

| 项目 | 完整桥接包 | MMDBridge 专用包 | MME 专用精简包 |
|---|---|---|---|
| MMDBridge 导出脚本 | 有 | 有 | 无 |
| `.py` 脚本扫描 | 有意义 | 有意义 | 无意义 |
| MME 运行时 | 有 | 无 | 有 |
| MME 菜单增强 | 有 | 无意义 | 有 |
| 适合导出工作流 | 是 | 是 | 否 |
| 适合只跑 MME | 可以 | 否 | 更适合 |

---

## 当前已加入的 MME 增强

两种包都带有当前桥接层新增的 MME 功能：

- `Effect Mapping` 窗口右键新增：
  - `Open Object Folder`
  - `Open Effect Folder`

详见：

- [MME_INTEGRATION.md](MME_INTEGRATION.md)

---

## 安装提示

统一要求：

- 所有文件与 `MikuMikudance.exe` 同级放置

完整桥接包额外要求：

- `.py` 脚本也必须与 `MikuMikudance.exe` 同级

---

## 相关文档

- [../README.md](../README.md)
- [BUILD.md](BUILD.md)
