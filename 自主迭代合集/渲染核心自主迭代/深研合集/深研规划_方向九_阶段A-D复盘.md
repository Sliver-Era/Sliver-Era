# 方向九 阶段 A–D 复盘（2026-11-17 第 729 轮）

> 范围：710–728；阶段 E（HDR / 色调映射）与阶段 F 剩余项排入下一窗口。
> 目的：把已完成能力、开关口径、验收数据与限制集中一处，作为后续开发与测试层扩展的依据。

## 一、阶段与交付

| 阶段 | 轮次 | 交付 | 默认关闭证据 |
| --- | --- | --- | --- |
| A 审计与可测性 | 710–712 | PBR 基线四例；参数口径两例；性能基线（实例 / 材质 / 特性组合） | `PBR关闭默认帧稳定`、`环境关闭默认帧稳定` |
| B 环境光照 | 713–715、719–720 | 环境颜色 / 强度字段与门面；常量环境着色；天空盒立方图采样；split-sum BRDF 近似；漫反射方向平均 | 环境强度 0 时整段跳过 |
| C BRDF 完善 | 716–718、721 | 多重散射补偿；镜面抗锯齿；Fresnel/F90 口径统一；光泽度 → 粗糙度修正映射 | `lighting_extra` / `pbr_extra` 全 0 |
| D 材质扩展 | 722–724 | 各向异性 GGX；皮肤 wrap；布料 sheen | `pbr_aniso` / `pbr_sheen` 全 0 |

## 二、参数与开关总表

| 能力 | 门面 | UBO 分量 | 合法域 |
| --- | --- | --- | --- |
| 环境光照 | `environment_set/get` | `environment` 980..983 | 颜色 0..1、强度 ≥0 |
| 多重散射 | `pbr_multiscatter_set/get` | `lighting_extra.z` | 布尔 |
| 镜面抗锯齿 | `pbr_specular_aa_set/get` | `lighting_extra.w` | 布尔 |
| Fresnel / F90 口径 | `pbr_fresnel_roughness_set/get` | `pbr_extra.x` | 布尔 |
| 环境 BRDF 近似 | `pbr_env_brdf_set/get` | `pbr_extra.y` | 布尔 |
| 环境漫反射平均 | `pbr_env_irradiance_set/get` | `pbr_extra.z` | 布尔 |
| 光泽度映射 | `pbr_gloss_mapping_set/get` | `pbr_extra.w` | 布尔（绝对口径优先） |
| 各向异性 | `pbr_aniso_set/get`、`pbr_aniso_strength_set/get` | `pbr_aniso.x/.y` | 强度 0..1 |
| 皮肤 wrap | `pbr_skin_sss_set/get`、`pbr_skin_wrap_set/get` | `pbr_aniso.z/.w` | wrap 0..1 |
| 布料 sheen | `pbr_sheen_set/get`、`pbr_sheen_strength_set/get` | `pbr_sheen.x/.y` | 强度 0..1 |

场景 UBO：996 浮点 / 3984 字节；新增分量全部追加在 976 之后，旧偏移不变。

## 三、验收数据

- 测试：PBR **26 例**、环境 **8 例**；渲染子集 **843**；全量 **1582 / 96 / 0**（361.8 s）。
- 性能（Debug，1000 实例，640×360）：PBR 基线 0.298 ms；全特性 0.281 ms（噪声内）。
- 强光照压力：32 盏点光 ＋ 1000 实例 ＋ 六项特性可出帧（数值见用例 RecordProperty）。
- 双轨：右半切换卡通材质时左半逐像素不变。

## 四、已知限制

1. 各向异性 v1 使用世界 X 投影切线，尚未接入逐物体切线 / UV 方向。
2. wrap 与 sheen 仅作用于主方向光，点光 / 聚光仍为兰伯特。
3. 环境镜面未乘 AO；环境漫反射方向平均仅 5 次采样，非真正预卷积辐照度。
4. 立方图纹理仍为单 mip，无法做「粗糙度 → mip」的镜面预过滤。
5. 中间目标仍为 8 位，曝光后的高光被截断（阶段 E 待做）。

## 五、下一阶段入口

- 729：AgX 色调映射预设（LDR 内，路线 B）。
- 730：场景色附件 RGBA16F（路线 A 第一阶段）。
- 731：后处理链全 16F ＋ 末端统一色调映射。
- 732：阶段 F 剩余项（逐物体材质预算 / 强光照性能基线 Release 复测）。
- 733：方向九复盘收口。
