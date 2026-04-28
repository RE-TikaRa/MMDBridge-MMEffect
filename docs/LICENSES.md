# 许可证说明

当前仓库采用新的 MIT 许可证：

- [../LICENSE.txt](../LICENSE.txt)

这份许可证用于：

- 当前仓库整理后的源码
- 当前仓库新增的桥接层改动
- 当前仓库新增的文档与打包整理

---

## 原始与第三方许可保留策略

本仓库没有移除原始许可，现按来源分别保留：

### 1. 原始 MMDBridge 许可

- [../licenses/MMDBridge-original-LICENSE.txt](../licenses/MMDBridge-original-LICENSE.txt)

这份文件保留了原始 MMDBridge 源码附带的 MIT 许可文本。

### 2. 原始 MMEffect 附带说明

仓库内保留原始运行包参考目录：

- `ori/MMEffect_x64_v037_ori/MMEffect_x64_v037/MMEffect.txt`

### 3. Alembic 相关许可

- `ori/MMDBridge_071_Alembic_64bit/MMDBridge/Alembic-LICENSE.txt`

### 4. Python 相关许可

- `ori/MMDBridge_071_Alembic_64bit/MMDBridge/Python-License.txt`

---

## 当前理解

可以简单理解为：

- **当前仓库本身**：MIT
- **原始 MMDBridge 许可文本**：继续保留
- **原始 MMEffect 说明与第三方依赖许可**：继续随仓库保留

---

## 发行包

当前 dist 里的运行包会继续带上必要说明文件，例如：

- `LICENSE.txt`
- `MMEffect.txt`
- `REFERENCE.txt`
- `Alembic-LICENSE.txt`
- `Python-License.txt`

如果后续发行包结构调整，建议同步检查这些许可文件是否仍被包含。
