# Pipeline 测试项依赖关系分析

## 一、当前 Pipeline 节点顺序（ISP 标准流程）

| 序号  | 节点名                  | 功能                | 默认状态 | 输入数据来源                             |
| --- | -------------------- | ----------------- | ---- | ---------------------------------- |
| N0  | OB                   | 光学黑电平扣除           | ✅ 启用 | SCM 测试                             |
| N1  | DPC                  | 坏点校正              | ✅ 启用 | SCM 测试                             |
| N2  | LSC                  | 镜头阴影校正            | ❌ 禁用 | 需要用户提供 Gain Map（依赖 SCM + N0 + N1）  |
| N3  | Denoise              | 降噪                | ✅ 启用 | SCM 测试（间接）                         |
| N4  | Demosaicing          | 去马赛克（Bayer → RGB） | ✅ 启用 | 无直接输入，依赖 N0/N2 的线性空间               |
| N5  | WhiteBalance         | 白平衡               | ❌ 禁用 | Color Char / Quick Calib 测试        |
| N6  | CCM                  | 色彩校正矩阵            | ❌ 禁用 | Color Char / Quick Calib 测试（依赖 N5） |
| N7  | Tone Mapping & Gamma | 动态范围映射 + Gamma 校正 | ✅ 启用 | SCM / Quick Calib 测试               |

## 二、测试模块间的依赖关系图

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│    SCM测试      │    │  Color Char测试  │    │  Quick Calib    │
│ (系统标定)      │    │  (色彩标定)      │    │  (快速标定)      │
└────────┬────────┘    └─────────┬────────┘    └────────┬────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│ SCMResults      │    │ ColorCharResults │    │ QuickCalibResults│
│ - dark_signal   │    │ - wb_gains       │    │ - wb_gains      │
│ - dark_current  │    │ - ccm_matrix     │    │ - ccm_matrix    │
│ - dark_noise    │    │ - color_temp     │    │ - gamma         │
│ - system_gain   │    └─────────┬────────┘    └────────┬────────┘
│ - full_well     │              │                       │
└────────┬────────┘              │                       │
         │                       │                       │
         └───────────┬───────────┴───────────┬───────────┘
                     ▼                       ▼
            ┌─────────────────────────────────────────────┐
            │           Pipeline 节点参数自动配置          │
            │  ┌───────────────────────────────────────┐  │
            │  │ N0(OB):     dark_signal, dark_current │  │
            │  │ N1(DPC):    dark_noise                │  │
            │  │ N5(WB):     wb_gains (R/G/B)          │  │
            │  │ N6(CCM):    ccm_matrix (3x3)          │  │
            │  │ N7(TMGamma): full_well, dynamic_range │  │
            │  └───────────────────────────────────────┘  │
            └─────────────────────────────────────────────┘
```

## 三、当前已实现的依赖

### ✅ SCM → Pipeline（已实现）

在 `PipelinePanel::InitializePipeline()` 中，读取 SCMResults 自动配置：

| SCM 结果字段                                          | Pipeline 节点               | 配置方法                           |
| ------------------------------------------------- | ------------------------- | ------------------------------ |
| `dark_signal_dn`                                  | N0 (OB)                   | `SetBlackLevel()`              |
| `dark_current_e_per_sec` + `system_gain_dn_per_e` | N0 (OB)                   | `SetDarkCurrentCompensation()` |
| `dark_noise_dn`                                   | N1 (DPC)                  | `SetDarkNoise()`               |
| `full_well_capacity_dn`                           | N7 (Tone Mapping & Gamma) | `SetInputRange()`              |
| `dynamic_range_bits`                              | N7 (Tone Mapping & Gamma) | 用于调试参考                         |

**代码位置**: [src/wxui/pipeline_panel.cpp#L73-L105](file:///e:/Projects/Study/CameraToolKit/src/wxui/pipeline_panel.cpp#L73-L105)

## 四、待实现的依赖

### ❌ Color Char → Pipeline（待实现）

需要读取 Color Char 测试结果，自动配置：

| Color Char 结果字段                       | Pipeline 节点       | 配置方法                   |
| ------------------------------------- | ----------------- | ---------------------- |
| `wb_r_gain`, `wb_g_gain`, `wb_b_gain` | N5 (WhiteBalance) | `SetGains(cv::Vec3f)`  |
| `ccm_matrix` (3x3)                    | N6 (CCM)          | `SetMatrix(cv::Mat)`   |
| `ccm_offset`                          | N6 (CCM)          | `SetOffset(cv::Vec3f)` |

### ❌ Quick Calib → Pipeline（待实现）

逻辑与 Color Char 类似，但包含额外的 Gamma 参数：

| Quick Calib 结果字段                      | Pipeline 节点               | 配置方法                   |
| ------------------------------------- | ------------------------- | ---------------------- |
| `wb_r_gain`, `wb_g_gain`, `wb_b_gain` | N5 (WhiteBalance)         | `SetGains(cv::Vec3f)`  |
| `ccm_matrix` (3x3)                    | N6 (CCM)                  | `SetMatrix(cv::Mat)`   |
| `ccm_offset`                          | N6 (CCM)                  | `SetOffset(cv::Vec3f)` |
| `gamma`                               | N7 (Tone Mapping & Gamma) | `SetGamma(float)`      |

## 五、数据流转流程

```
1. 用户执行 SCM 测试
   └─> 结果保存到 session_config (SCMResults)

2. 用户执行 Color Char / Quick Calib 测试
   └─> 结果保存到 session_config (ColorCharResults / QuickCalibResults)

3. 用户打开 Pipeline 页面
   └─> InitializePipeline() 读取配置
       ├─> 加载 SCMResults → 配置 N0, N1, N7
       ├─> 加载 ColorCharResults → 配置 N5, N6 (待实现)
       ├─> 加载 QuickCalibResults → 配置 N5, N6, N7 (待实现)
       └─> 更新节点启用状态与参数来源信息

4. 用户点击"重新加载标定结果"按钮（新增功能）
   └─> ReloadCalibResults() 重新读取配置
       ├─> 清除旧配置
       ├─> 重新加载所有标定结果
       ├─> 更新参数来源信息
       └─> 更新节点列表显示

5. 用户运行 Pipeline
   └─> 按顺序执行已启用节点
       ├─> N0: OB Subtraction（暗电平扣除，依赖曝光时间）
       ├─> N1: DPC（坏点检测与修复，依赖 N0 输出）
       ├─> N2: LSC（镜头阴影校正，依赖 N0+N1 预处理后的亮场图像）
       ├─> N3: Denoise（降噪，依赖线性空间数据）
       ├─> N4: Demosaicing（Bayer → RGB，依赖 N0/N2 线性空间）
       ├─> N5: WhiteBalance（白平衡增益，必须在 CCM 之前执行）
       ├─> N6: CCM（色彩校正矩阵，依赖 N5 白平衡后的 RGB 数据）
       └─> N7: Tone Mapping & Gamma（动态范围映射 → Gamma 校正）

6. 执行完成
   └─> 结果保存到 session_config
       └─> 输出统计信息（尺寸、通道、均值、标准差等）
```

### 5.1 LSC 标定的隐藏前置依赖

LSC（镜头阴影校正）的 Gain Map 生成过程不是独立的，其输入图像必须经过以下预处理：

```
LSC Gain Map 生成流程：
1. 用户提供的亮场图像
   └─> N0: OB Subtraction（扣除暗电流底噪）
       └─> N1: DPC（修复坏点）
           └─> LSC 标定算法（生成高精度 Gain Map）
               └─> 保存到配置文件
                   └─> Pipeline 运行时加载到 N2 节点
```

**关键约束**：

- LSC 的标定依赖于 SCM 数据的先期加载（用于配置 N0 和 N1）
- 亮场图像必须是无暗电流底噪、无坏点的干净图像
- 如果 N0/N1 未正确配置，生成的 Gain Map 将包含底噪和坏点的伪影

### 5.2 N7 (Tone Mapping & Gamma) 的双阶段处理

N7 节点实际上承担了两个独立的处理步骤，操作顺序至关重要：

```
N7 处理流程：
1. Tone Mapping（动态范围映射）
   ├─> 输入：高位深线性数据（如 12-bit，范围 0 ~ full_well_capacity_dn）
   ├─> 操作：将物理传感器满阱的动态范围映射到显示器的 8-bit 范围
   ├─> 公式：output_linear = input / full_well_capacity_dn * 255.0
   └─> 目的：防止高光溢出，保证线性动态范围

2. Gamma Correction（Gamma 曲线校正）
   ├─> 输入：8-bit 线性数据（0 ~ 255）
   ├─> 操作：应用非线性 Gamma 曲线补偿显示器的非线性响应
   ├─> 公式：output_gamma = pow(output_linear / 255.0, 1.0 / gamma) * 255.0
   └─> 目的：使最终图像在显示器上呈现正确的亮度感知

总公式：output = pow((input / full_well) * 255.0 / 255.0, 1.0/gamma) * 255.0
```

### 5.3 WB 与 CCM 的顺序依赖

CCM（色彩校正矩阵）的输入必须是已经应用了 WB（白平衡）增益的 RGB 数据：

```
色彩校正流程（严格顺序）：
1. N5: WhiteBalance
   ├─> 对 RGB 通道分别应用增益：R' = R * r_gain, G' = G * g_gain, B' = B * b_gain
   └─> 目的：将中性色还原为纯白，消除光源色温影响

2. N6: CCM
   ├─> 应用 3x3 色彩校正矩阵：[R_out; G_out; B_out] = CCM * [R'; G'; B'] + offset
   └─> 目的：将相机色彩空间转换为标准色彩空间（如 sRGB）

为什么必须先 WB 后 CCM？
- CCM 的设计假设输入数据已经过白平衡校正
- 不同色温下的 CCM 矩阵参数不同，需要基于白平衡后的中性色计算
- 如果颠倒顺序，CCM 将无法正确补偿光源色彩偏差
```

## 六、实现方案

### 步骤 1：扩展 SessionConfig

在 [include/utils/session_config.h](file:///e:/Projects/Study/CameraToolKit/include/utils/session_config.h) 中添加 `ColorCharResults` 和 `QuickCalibResults` 结构体：

```cpp
struct ColorCharResults {
    double wb_r_gain;          // 白平衡 R 通道增益
    double wb_g_gain;          // 白平衡 G 通道增益
    double wb_b_gain;          // 白平衡 B 通道增益
    cv::Matx33d ccm_matrix;    // 色彩校正矩阵 3x3
    cv::Vec3d ccm_offset;      // CCM 偏移量
    double color_temp;         // 色温
};

struct QuickCalibResults {
    double wb_r_gain;          // 白平衡 R 通道增益
    double wb_g_gain;          // 白平衡 G 通道增益
    double wb_b_gain;          // 白平衡 B 通道增益
    cv::Matx33d ccm_matrix;    // 色彩校正矩阵 3x3
    cv::Vec3d ccm_offset;      // CCM 偏移量
    float gamma;               // Gamma 值
};

// Color Char 结果读写
void SetColorCharResults(const ColorCharResults& results);
std::optional<ColorCharResults> GetColorCharResults() const;

// Quick Calib 结果读写
void SetQuickCalibResults(const QuickCalibResults& results);
std::optional<QuickCalibResults> GetQuickCalibResults() const;
```

### 步骤 2：Color Char 测试完成后保存结果

在 [src/wxui/color_char_calib_panel.cpp](file:///e:/Projects/Study/CameraToolKit/src/wxui/color_char_calib_panel.cpp) 的 Step3 计算完成后，调用 `SetColorCharResults()` 将 WB 和 CCM 结果写入 TOML。

### 步骤 3：Quick Calib 测试完成后保存结果

在 [src/wxui/quick_color_calib_panel.cpp](file:///e:/Projects/Study/CameraToolKit/src/wxui/quick_color_calib_panel.cpp) 的计算完成后，调用 `SetQuickCalibResults()` 将 WB、CCM 和 Gamma 结果写入 TOML。

### 步骤 4：Pipeline 初始化时加载标定结果

在 `InitializePipeline()` 中，读取所有标定结果并自动配置：

```cpp
auto scm = cfg.GetSCMResults();
if (scm) {
    ob_node_->SetBlackLevel(scm->dark_signal_dn > 0 ? scm->dark_signal_dn : 100.0);
    if (scm->dark_current_e_per_sec > 0 && scm->system_gain_dn_per_e > 0) {
        ob_node_->SetDarkCurrentCompensation(scm->dark_current_e_per_sec,
                                              scm->system_gain_dn_per_e, 0.001);
    }
    if (scm->dark_noise_dn > 0) {
        dpc_node_->SetDarkNoise(scm->dark_noise_dn);
    }
    if (scm->full_well_capacity_dn > 0) {
        double effective_max = scm->full_well_capacity_dn - scm->dark_signal_dn;
        gamma_node_->SetInputRange(0.0, effective_max > 0 ? effective_max : 65535.0);
    }
}

auto color_char = cfg.GetColorCharResults();
auto quick_calib = cfg.GetQuickCalibResults();

// 优先级：Quick Calib > Color Char
if (quick_calib) {
    wb_node_->SetGains(cv::Vec3f(quick_calib->wb_r_gain, 
                                  quick_calib->wb_g_gain, 
                                  quick_calib->wb_b_gain));
    cv::Mat ccm_mat(3, 3, CV_32F);
    // 转换 ccm_matrix 到 cv::Mat
    ccm_node_->SetMatrix(ccm_mat);
    ccm_node_->SetOffset(cv::Vec3f(quick_calib->ccm_offset[0],
                                    quick_calib->ccm_offset[1],
                                    quick_calib->ccm_offset[2]));
    gamma_node_->SetGamma(quick_calib->gamma);
    wb_node_->SetEnabled(true);
    ccm_node_->SetEnabled(true);
    UpdateCalibSourceInfo("Quick Calib");
} else if (color_char) {
    wb_node_->SetGains(cv::Vec3f(color_char->wb_r_gain, 
                                  color_char->wb_gain, 
                                  color_char->wb_b_gain));
    cv::Mat ccm_mat(3, 3, CV_32F);
    // 转换 ccm_matrix 到 cv::Mat
    ccm_node_->SetMatrix(ccm_mat);
    ccm_node_->SetOffset(cv::Vec3f(color_char->ccm_offset[0],
                                    color_char->ccm_offset[1],
                                    color_char->ccm_offset[2]));
    wb_node_->SetEnabled(true);
    ccm_node_->SetEnabled(true);
    UpdateCalibSourceInfo("Color Char");
}
```

### 步骤 5：添加参数来源信息显示（新增功能）

在 Pipeline 页面左侧面板的节点列表下方，添加参数来源信息显示区域：

```
┌─────────────────────────────────────────┐
│ Pipeline Nodes                          │
├─────────────────────────────────────────┤
│ [0] OB (ON)                            │
│ [1] DPC (ON)                           │
│ [2] LSC (OFF)                          │
│ [3] Denoise (ON)                       │
│ [4] Demosaicing (ON)                   │
│ [5] WhiteBalance (ON)                  │
│ [6] CCM (ON)                           │
│ [7] ToneMapping&Gamma (ON)             │
├─────────────────────────────────────────┤
│ Calibration Source:                     │
│ • SCM: ✓ Loaded                        │
│ • Color Char: ✗ Not loaded             │
│ • Quick Calib: ✓ Loaded (current)      │
├─────────────────────────────────────────┤
│ [Reload Calibration Results]           │
└─────────────────────────────────────────┘
```

**参数来源信息内容**：

- SCM：显示是否已加载，加载时间
- Color Char：显示是否已加载，加载时间
- Quick Calib：显示是否已加载，加载时间
- 当前使用的标定数据来源（优先级：Quick Calib > Color Char）

### 步骤 6：添加"重新加载标定结果"按钮（新增功能）

在参数来源信息下方添加"重新加载标定结果"按钮，点击后执行以下操作：

```cpp
void PipelinePanel::OnReloadCalibResults(wxCommandEvent& event) {
    auto& cfg = utils::SessionConfig::GetInstance();
    cfg.Load();

    // 重新初始化节点参数
    InitializePipeline();

    // 更新 UI
    UpdateNodeList();
    UpdateCalibSourceInfo();

    UpdateStatus("Calibration results reloaded");
}
```

**按钮功能**：

- 重新读取 session_config 中的所有标定结果
- 重新配置所有 Pipeline 节点参数
- 更新参数来源信息显示
- 更新节点列表状态

### 步骤 7：动态曝光时间支持（新增功能）

当用户在运行时更改相机曝光时间时，需要重算 N0 节点的 Offset：

```cpp
void PipelinePanel::OnExposureTimeChanged(double exposure_time_us) {
    auto& cfg = utils::SessionConfig::GetInstance();
    auto scm = cfg.GetSCMResults();

    if (scm && scm->dark_current_e_per_sec > 0 && scm->system_gain_dn_per_e > 0) {
        // 重算暗电流补偿 Offset
        double dark_current_dn = scm->dark_current_e_per_sec * 
                                 scm->system_gain_dn_per_e * 
                                 (exposure_time_us / 1e6);
        double total_offset = scm->dark_offset_dn + dark_current_dn;

        ob_node_->SetBlackLevel(total_offset);
        ob_node_->SetDarkCurrentCompensation(scm->dark_current_e_per_sec,
                                              scm->system_gain_dn_per_e,
                                              exposure_time_us / 1e6);

        UpdateStatus(wxString::Format("Exposure changed: %.0fus, OB offset recalculated", exposure_time_us));
    }
}
```

**动态重算机制**：

- 暗电流随曝光时间线性增长：`dark_current_dn = dark_current_e_per_sec * gain_dn_per_e * exposure_s`
- 总 Offset = `dark_offset_dn + dark_current_dn`
- `dark_offset_dn` 是在极短曝光下测得的绝对偏置，不随曝光时间变化
- 当曝光时间改变时，需要立即更新 N0 节点的配置
- 如果 Pipeline 正在运行实时预览，需要重新执行后续所有节点

## 七、参数来源优先级规则

当存在多个标定结果时，按照以下优先级选择使用：

| 参数类型                       | 优先级                      | 说明                          |
| -------------------------- | ------------------------ | --------------------------- |
| WB Gains                   | Quick Calib > Color Char | Quick Calib 的结果更全面          |
| CCM Matrix                 | Quick Calib > Color Char | 同上                          |
| Gamma                      | Quick Calib > SCM        | Quick Calib 可能包含自定义 Gamma 值 |
| Black Level / Dark Current | SCM                      | 只有 SCM 提供                   |
| Dark Noise                 | SCM                      | 只有 SCM 提供                   |
| Full Well Capacity         | SCM                      | 只有 SCM 提供                   |

## 八、节点间的关键依赖约束

### 8.1 执行顺序约束

| 约束           | 说明                               | 违反后果               |
| ------------ | -------------------------------- | ------------------ |
| N0 → N1 → N2 | LSC 标定必须使用 N0/N1 预处理后的图像         | Gain Map 包含底噪和坏点伪影 |
| N0/N2 → N4   | Demosaicing 必须在线性空间执行            | 色彩伪影、边缘模糊          |
| N5 → N6      | CCM 必须在 WB 之后执行                  | 色彩校正不准确            |
| N6 → N7      | Tone Mapping & Gamma 必须在所有线性处理之后 | 非线性数据导致错误的动态范围映射   |

### 8.2 数据类型约束

| 节点  | 输入类型                  | 输出类型          | 说明                 |
| --- | --------------------- | ------------- | ------------------ |
| N0  | 12/16-bit Raw         | 12/16-bit Raw | 保持高位深线性数据          |
| N1  | 12/16-bit Raw         | 12/16-bit Raw | 保持高位深线性数据          |
| N2  | 12/16-bit Raw         | 12/16-bit Raw | 保持高位深线性数据          |
| N3  | 12/16-bit Raw         | 12/16-bit Raw | 保持高位深线性数据          |
| N4  | 12/16-bit Raw (Bayer) | 12/16-bit RGB | 通道数从 1 变为 3        |
| N5  | 12/16-bit RGB         | 12/16-bit RGB | 保持高位深线性数据          |
| N6  | 12/16-bit RGB         | 12/16-bit RGB | 保持高位深线性数据（需 Clamp） |
| N7  | 12/16-bit RGB         | 8-bit RGB     | 动态范围映射 + Gamma 校正  |

## 九、关键文件索引

| 文件                                                                                                                   | 说明                                                                  |
| -------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------- |
| [include/utils/session_config.h](file:///e:/Projects/Study/CameraToolKit/include/utils/session_config.h)             | 会话配置管理，包含 SCMResults 结构体定义，需扩展 ColorCharResults 和 QuickCalibResults |
| [include/algo/image_node.h](file:///e:/Projects/Study/CameraToolKit/include/algo/image_node.h)                       | Pipeline 节点接口定义（OBNode, DPCNode, CCMNode 等）                         |
| [src/wxui/pipeline_panel.cpp](file:///e:/Projects/Study/CameraToolKit/src/wxui/pipeline_panel.cpp)                   | Pipeline 页面实现，包含 InitializePipeline() 方法，需添加参数来源信息显示和重新加载按钮         |
| [src/wxui/color_char_calib_panel.cpp](file:///e:/Projects/Study/CameraToolKit/src/wxui/color_char_calib_panel.cpp)   | Color Char 页面实现，Step3 计算 WB 和 CCM，需保存结果到 session_config             |
| [src/wxui/quick_color_calib_panel.cpp](file:///e:/Projects/Study/CameraToolKit/src/wxui/quick_color_calib_panel.cpp) | Quick Calib 页面实现，计算 WB、CCM 和 Gamma，需保存结果到 session_config            |

## 十、配置参数映射表

### SCM 参数 → Pipeline 节点

| TOML Key                     | Pipeline 节点   | 用途       |
| ---------------------------- | ------------- | -------- |
| `scm.dark_signal_dn`         | N0 (OB)       | 黑电平偏置    |
| `scm.dark_current_e_per_sec` | N0 (OB)       | 暗电流补偿    |
| `scm.system_gain_dn_per_e`   | N0 (OB)       | 系统增益     |
| `scm.dark_noise_dn`          | N1 (DPC)      | 坏点检测阈值计算 |
| `scm.full_well_capacity_dn`  | N7 (TM&Gamma) | 输入范围上限   |
| `scm.dynamic_range_bits`     | N7 (TM&Gamma) | 动态范围参考   |

### Color Char 参数 → Pipeline 节点（待实现）

| TOML Key                | Pipeline 节点 | 用途        |
| ----------------------- | ----------- | --------- |
| `color_char.wb_r_gain`  | N5 (WB)     | R 通道白平衡增益 |
| `color_char.wb_g_gain`  | N5 (WB)     | G 通道白平衡增益 |
| `color_char.wb_b_gain`  | N5 (WB)     | B 通道白平衡增益 |
| `color_char.ccm_row_0`  | N6 (CCM)    | CCM 矩阵第一行 |
| `color_char.ccm_row_1`  | N6 (CCM)    | CCM 矩阵第二行 |
| `color_char.ccm_row_2`  | N6 (CCM)    | CCM 矩阵第三行 |
| `color_char.ccm_offset` | N6 (CCM)    | CCM 偏移向量  |
| `color_char.color_temp` | N5 (WB)     | 色温参考      |

### Quick Calib 参数 → Pipeline 节点（待实现）

| TOML Key                 | Pipeline 节点   | 用途        |
| ------------------------ | ------------- | --------- |
| `quick_calib.wb_r_gain`  | N5 (WB)       | R 通道白平衡增益 |
| `quick_calib.wb_g_gain`  | N5 (WB)       | G 通道白平衡增益 |
| `quick_calib.wb_b_gain`  | N5 (WB)       | B 通道白平衡增益 |
| `quick_calib.ccm_row_0`  | N6 (CCM)      | CCM 矩阵第一行 |
| `quick_calib.ccm_row_1`  | N6 (CCM)      | CCM 矩阵第二行 |
| `quick_calib.ccm_row_2`  | N6 (CCM)      | CCM 矩阵第三行 |
| `quick_calib.ccm_offset` | N6 (CCM)      | CCM 偏移向量  |
| `quick_calib.gamma`      | N7 (TM&Gamma) | Gamma 值   |

## 十一、注意事项

1. **节点执行顺序**：Pipeline 节点必须按 ISP 标准顺序执行，不能随意调整。

2. **数据格式兼容性**：Color Char/Quick Calib 输出的 CCM 矩阵可能是 `cv::Matx33d`（double 类型），需要转换为 `cv::Mat`（float 类型）才能传入 CCMNode。

3. **启用状态管理**：当从 Color Char/Quick Calib 加载 WB/CCM 参数后，应自动启用对应的节点；但用户仍可手动禁用。

4. **错误处理**：加载标定结果时需处理文件不存在或格式错误的情况，提供清晰的错误提示。

5. **可追溯性**：在 Pipeline 页面显示参数来源信息，帮助用户理解当前配置状态。

6. **重新加载机制**：添加"重新加载标定结果"按钮后，用户可以在其他测试完成后随时刷新 Pipeline 配置，无需重启应用。

7. **优先级冲突**：当同时存在 Color Char 和 Quick Calib 结果时，Quick Calib 优先级更高；若用户希望使用 Color Char 的结果，需要先删除 Quick Calib 的结果或手动配置参数。

8. **参数一致性**：从不同测试加载参数时，需确保参数值在合理范围内（如 WB 增益应大于 0，Gamma 应大于 0）。

9. **LSC 前置依赖**：LSC 的 Gain Map 生成必须使用经过 N0（OB）和 N1（DPC）预处理的图像，否则会包含底噪和坏点伪影。在 LSC 标定工具中应强制启用 N0 和 N1。

10. **Tone Mapping 与 Gamma 的分离**：N7 节点包含两个独立的处理阶段，必须先执行动态范围映射（基于 full_well_capacity_dn），再应用 Gamma 曲线校正。顺序颠倒会导致高光溢出和色彩失真。

11. **矩阵运算溢出保护**：在 CCM 矩阵乘法运算中，特别是使用 CV_32F 浮点类型和多线程（如 OpenMP）加速时，必须在像素写入目标缓冲区前执行严格的 `cv::saturate_cast` 或手动 Clamp 操作，防止计算结果出现负值或超出当前位深的上限。

12. **动态曝光时间同步**：暗电流随曝光时间线性增长，当用户在运行时更改相机曝光时间时，需要设计机制重算 N0 节点的 Offset（`total_offset = dark_offset_dn + dark_current_e_per_sec * gain_dn_per_e * exposure_s`），而不仅仅是在初始化时加载静态配置。`dark_offset_dn` 必须是在极短曝光下测得的绝对偏置，不含暗电流成分。

13. **WB 与 CCM 的顺序保证**：Pipeline 设计必须确保 N5（WhiteBalance）在 N6（CCM）之前执行，CCM 矩阵的输入必须是已经应用了白平衡增益的 RGB 数据。

## 十二、UI 设计建议

### 参数来源信息区域

在 Pipeline 页面左侧面板中，节点列表下方添加参数来源信息区域：

```
┌──────────────────────────────────────────────────────┐
│  Calibration Source Status                           │
├──────────────────────────────────────────────────────┤
│  ┌──────────┬──────────┬──────────────────────────┐  │
│  │ Module   │ Status   │ Details                  │  │
│  ├──────────┼──────────┼──────────────────────────┤  │
│  │ SCM      │ ✓        │ dark_offset=100, dn=30   │  │
│  │ ColorChar│ ✗        │ Not loaded               │  │
│  │ QuickCal │ ✓ (curr) │ WB: 1.2, 1.0, 0.8       │  │
│  └──────────┴──────────┴──────────────────────────┘  │
├──────────────────────────────────────────────────────┤
│  [Reload Calibration Results]                        │
└──────────────────────────────────────────────────────┘
```

**显示内容**：

- 每个标定模块的状态（已加载/未加载）
- 当前使用的标定数据来源（标记为 "(curr)"）
- 关键参数值预览（如 WB 增益、暗信号等）
- 加载时间戳（可选）

### 重新加载按钮

按钮放置在参数来源信息区域下方，样式与其他按钮保持一致：

- 按钮文字："Reload Calibration Results"
- 功能：重新读取所有标定结果并更新节点配置
- 点击后状态提示："Calibration results reloaded successfully"

### 节点列表增强

在节点列表中，对从标定结果自动配置的节点添加标记：

```
[0] OB (ON) [SCM]
[1] DPC (ON) [SCM]
[2] LSC (OFF) [Manual]
[3] Denoise (ON) [SCM]
[4] Demosaicing (ON) [Default]
[5] WhiteBalance (ON) [Quick Calib]
[6] CCM (ON) [Quick Calib]
[7] ToneMapping&Gamma (ON) [SCM+Quick Calib]
```

**标记规则**：

- `[SCM]`：参数来自 SCM 测试
- `[Color Char]`：参数来自 Color Char 测试
- `[Quick Calib]`：参数来自 Quick Calib 测试
- `[Manual]`：用户手动配置
- `[Default]`：使用默认值
- `[SCM+Quick Calib]`：参数来自多个来源

### 节点依赖关系可视化

在节点列表中，添加依赖关系指示：

```
[0] OB (ON) [SCM]
[1] DPC (ON) [SCM] ← depends on N0
[2] LSC (OFF) [Manual] ← depends on N0 + N1
[3] Denoise (ON) [SCM]
[4] Demosaicing (ON) [Default] ← depends on N0/N2
[5] WhiteBalance (ON) [Quick Calib]
[6] CCM (ON) [Quick Calib] ← depends on N5
[7] ToneMapping&Gamma (ON) [SCM+Quick Calib] ← depends on all previous
```

**依赖标记规则**：

- `← depends on Nx`：明确标识该节点依赖的前置节点
- `← depends on N0 + N1`：标识多个前置依赖
- `← depends on all previous`：标识依赖所有前置节点的输出

### 曝光时间控制

如果支持动态曝光时间调整，在右侧 Control 面板中添加曝光时间输入：

```
┌─────────────────────────────────────────┐
│  Control                               │
├─────────────────────────────────────────┤
│  [Load] [file_path_input]              │
├─────────────────────────────────────────┤
│  [Run All] [Step] [Prev] [Reset] [Save]│
├─────────────────────────────────────────┤
│  Exposure Time: [1000] us              │
│  (Changes require OB offset recalculation)│
├─────────────────────────────────────────┤
│  Input Image Info...                   │
└─────────────────────────────────────────┘
```

**曝光时间控制功能**：

- 输入框允许用户修改曝光时间（微秒）
- 修改后自动触发 N0 节点 Offset 的重新计算
- 如果 Pipeline 正在运行实时预览，自动重新执行后续节点

## 十三、多线程安全增强

在实时预览模式下，Pipeline 需要处理高帧率图像流，必须确保线程安全和内存边界。

### 13.1 参数热更新的线程同步

在"步骤7：动态曝光时间支持"中，UI 线程（wxWidgets 主线程）会修改曝光时间并重新计算 `total_offset`。如果此时 Worker 线程正在使用 OpenMP 的 `#pragma omp parallel for` 遍历执行 N0 (OB) 节点，直接写入新参数会导致数据撕裂。

**改善方案**：

1. **方案A：原子变量（推荐）**
   
   - 在 ImageNode 的接口设计中，针对需要热更新的参数，使用 `std::atomic<double>` 存储
   - 优点：无需锁，直接保证原子性
   - 缺点：仅适用于简单标量类型
   
   ```cpp
   class OBNode {
   private:
       std::atomic<double> black_level_{100.0};  // 原子变量
       std::atomic<double> dark_current_comp_{0.0};
   public:
       double GetBlackLevel() const { return black_level_.load(std::memory_order_acquire); }
       void SetBlackLevel(double value) { black_level_.store(value, std::memory_order_release); }
   };
   ```

2. **方案B：Mutex 锁拷贝**
   
   - 在 `Process(cv::Mat& img)` 执行前进行 Mutex 锁拷贝
   - 确保当前帧处理完毕后再应用新参数
   
   ```cpp
   class OBNode {
   private:
       mutable std::mutex param_mutex_;
       double black_level_{100.0};
   public:
       void Process(cv::Mat& img) {
           double local_offset;
           {
               std::lock_guard<std::mutex> lock(param_mutex_);
               local_offset = black_level_;
           }
           // 使用 local_offset 处理图像...
       }
   };
   ```

### 13.2 内存访问越界防护

在处理 UINT16 的高位深数据时，尤其是在 N2 (LSC) 的边缘填充和 N6 (CCM) 的高并发矩阵乘法中，极易发生 0xC0000005 内存访问违例异常。

**改善方案**：

1. **ROI 边界检查**
   
   - 在进入 `#pragma omp parallel for` 循环块之前，必须完成所有的 ROI 边界检查
   - 禁止在循环内部进行越界访问
   
   ```cpp
   // ❌ 错误：循环内访问越界
   for (int y = 0; y < height; y++) {
       for (int x = 0; x < width; x++) {
           auto val = img.at<ushort>(y, x);  // 可能越界
       }
   }
   
   // ✅ 正确：显式边界检查
   for (int y = 0; y < height; y++) {
       if (y >= img.rows) break;  // 显式检查
       for (int x = 0; x < width; x++) {
           if (x >= img.cols) break;
           auto val = img.at<ushort>(y, x);
       }
   }
   ```

2. **安全的 CCM 矩阵乘法**
   
   - 严禁指针越界读取
   - 统一将 `cv::Matx33d` 转换为 `cv::Mat` (CV_32F) 后，使用 OpenCV 内置的 SIMD 优化函数或严格控制下标的自定义循环
   - 最后再通过 `cv::saturate_cast<ushort>` 安全写回
   
   ```cpp
   cv::Mat Process(const cv::Mat& input) {
       cv::Mat output(input.size(), input.type());
       // 使用 OpenCV 安全的矩阵运算
       cv::transform(input.reshape(1), output.reshape(1), ccm_matrix_);
       output = cv::max(output, 0);  // 下限截断
       output = cv::min(output, 65535.0);  // 上限截断
       return output;
   }
   ```

## 十四、内存分配与数据类型流转规范

ISP 流程中频繁的内存分配是性能杀手。文档中的"8.2 数据类型约束"非常清晰，但可以进一步从工程角度优化。

### 14.1 原地修改与双缓冲

N0 (OB)、N1 (DPC)、N2 (LSC)、N3 (Denoise) 的输入输出通道数和位深完全一致。

**改善方案**：

在实现 `Pipeline::Run()` 时，前 4 个节点应强制采用原地修改（`node->ProcessInPlace(raw_img)`），避免 `cv::Mat::clone()` 带来的内存碎片：

```cpp
// Pipeline::Run()
for (auto& node : nodes_) {
    if (node->IsEnabled()) {
        if (node->SupportsInPlace()) {
            node->ProcessInPlace(current_image_);  // 原地修改
        } else {
            current_image_ = node->Process(current_image_);  // 创建新图像
        }
    }
}
```

**原地修改条件**：

- 输入输出通道数相同
- 输入输出位深相同
- 不需要保留原始图像

### 14.2 格式跃迁点的内存池介入

N4 (Demosaicing) 是通道数跃迁点（1通道变3通道），N7 (TM & Gamma) 是位深跃迁点（16-bit变8-bit）。

**改善方案**：

在这两个节点不要在函数内部临时 `new` 图像，而是复用预先分配好的图像内存池缓冲区：

```cpp
class ImageBufferPool {
private:
    std::vector<cv::Mat> bayer_buffers_;      // 单通道 Bayer 缓冲区
    std::vector<cv::Mat> rgb_buffers_;         // 三通道 RGB 缓冲区
    std::vector<cv::Mat> output_buffers_;      // 输出缓冲区（8-bit）

public:
    cv::Mat GetBayerBuffer(int rows, int cols);
    cv::Mat GetRgbBuffer(int rows, int cols);
    cv::Mat GetOutputBuffer(int rows, int cols);
    void ReturnBuffer(cv::Mat& buffer);  // 归还到池中
};

// Demosaicing 节点使用
class DemosaicNode : public ImageNode {
private:
    ImageBufferPool& pool_;
public:
    cv::Mat Process(const cv::Mat& input) override {
        cv::Mat& output = pool_.GetRgbBuffer(input.rows, input.cols);
        // 执行去马赛克算法...
        return output;  // 复用缓冲区
    }
};
```

**内存池优势**：

- 减少 `new/delete` 调用次数
- 降低内存碎片化
- 处理千万像素级图像流时极大降低 CPU 负载

### 14.3 图像缓冲区管理策略

**问**：当图像流在 N4 节点由 Bayer 单通道展开为 RGB 三通道时，是直接在堆内存中动态分配 `cv::Mat` 还是规划了复用特定作用域的图像缓冲区管理策略？

**答**：推荐使用 **内存池 + 双缓冲** 策略：

```
┌─────────────────────────────────────────────────────────────────┐
│                    Pipeline 内存管理架构                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐    │
│  │ Buffer A     │     │ Buffer B     │     │ Buffer C     │    │
│  │ (Bayer 16T)  │────▶│ (RGB 16T)    │────▶│ (RGB 8T)     │    │
│  └──────────────┘     └──────────────┘     └──────────────┘    │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │                    ImageBufferPool                         │ │
│  │  - 预分配固定大小的 cv::Mat 数组                           │ │
│  │  - 使用 std::vector<cv::Mat> 存储可用缓冲区                │ │
│  │  - GetBuffer() / ReturnBuffer() 接口                      │ │
│  │  - 支持 1-channel 和 3-channel 两种模式                   │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**双缓冲工作流程**：

1. 从内存池获取 Buffer A（单通道）
2. N0-N3 处理后，将 Buffer A 归还到内存池
3. 从内存池获取 Buffer B（三通道）用于 N4 输出
4. N5-N6 处理后，使用 Buffer B
5. 从内存池获取 Buffer C（8-bit）用于 N7 输出
6. Pipeline 结束时，将所有缓冲区归还到内存池

## 十五、算法精度的数学防呆机制

### 15.1 CCM 矩阵的归一化校验

来自 Color Char 或 Quick Calib 的 CCM 矩阵（3x3），其每一行的系数之和理论上应该趋近于 1.0。如果不为 1.0，在应用 N6 时会导致图像整体亮度发生偏移，破坏 N5 (WB) 的成果。

**改善方案**：

在 `ccm_node_->SetMatrix()` 中加入一道校验机制：

```cpp
bool CCMNode::SetMatrix(const cv::Mat& matrix) {
    // 检查矩阵是否为 3x3
    if (matrix.rows != 3 || matrix.cols != 3) {
        return false;
    }

    // 检查每行系数和是否接近 1.0
    for (int i = 0; i < 3; i++) {
        double row_sum = cv::sum(matrix.row(i))[0];
        if (row_sum < 0.9 || row_sum > 1.1) {
            // 输出警告日志
            LOG_WARN("CCM row %d sum (%.4f) deviates from 1.0, calibration may be affected by ambient light",
                     i, row_sum);
        }
    }

    ccm_matrix_ = matrix;
    return true;
}
```

**校验阈值**：

- 正常范围：0.95 ~ 1.05
- 警告范围：0.9 ~ 0.95 或 1.05 ~ 1.1
- 错误范围：< 0.9 或 > 1.1

### 15.2 动态 Offset 的下限截断

> ⚠️ **PHYSICAL DEFINITION CLARIFICATION**
> 
> **暗信号 (Dark Signal) vs 暗电流 (Dark Current)**：
> 
> 在 EMVA 1288 体系中，总暗信号等于"偏置 (Bias / y_dark_offset)"加上"暗电流随时间的累积"。这里的逻辑是否成立，取决于 `scm.dark_offset_dn` 的测试条件。
> 
> **关键定义**：
> - `dark_offset_dn`：**绝对偏置**（Optical Black Bias），必须在极短曝光时间（如 t_exp ≈ 0）下测得的截距值，不随曝光时间变化
> - `dark_current_e_per_sec`：暗电流密度，随曝光时间线性增长
> 
> **双重扣除警告**：如果 `dark_signal_dn` 是在长曝光（例如 100ms）下测得的，它已经包含了暗电流。如果在此基础上再利用曝光时间进行动态累加，就会导致双重扣除 (Double Subtraction)，让暗部细节被彻底切断。

在重新计算动态曝光的 N0 节点 Offset 时，公式为：

```
total_offset = dark_offset_dn + (dark_current_e_per_sec × gain_dn_per_e × exposure_s)
```

**改善方案**：

必须确保 `total_offset` 计算结果始终为正，且在扣除时 `cv::max(pixel - total_offset, 0)` 的执行严格生效：

```cpp
double OBNode::CalculateTotalOffset(double exposure_time_us) {
    auto scm = cfg.GetSCMResults();
    if (!scm) return default_offset_;

    double dark_current_dn = scm->dark_current_e_per_sec * 
                             scm->system_gain_dn_per_e * 
                             (exposure_time_us / 1e6);
    double total_offset = scm->dark_offset_dn + dark_current_dn;

    // 下限截断：确保 Offset 始终为正
    total_offset = std::max(total_offset, 0.0);

    // 上限截断：防止 Offset 超过传感器动态范围的一半
    double max_offset = full_well_capacity_dn_ * 0.5;
    total_offset = std::min(total_offset, max_offset);

    return total_offset;
}

cv::Mat OBNode::Process(const cv::Mat& input) {
    cv::Mat output = input.clone();  // 或原地修改

    double offset = CalculateTotalOffset(current_exposure_us_);

    // 安全执行减法，防止负数下溢
    for (int i = 0; i < output.rows; i++) {
        for (int j = 0; j < output.cols; j++) {
            ushort val = output.at<ushort>(i, j);
            // 使用 saturate_cast 确保安全
            output.at<ushort>(i, j) = cv::saturate_cast<ushort>(val - offset);
        }
    }

    return output;
}
```

**防呆检查点**：

1. `total_offset >= 0`：防止黑电平漂移为负数
2. `total_offset <= full_well / 2`：防止过度扣除导致图像暗部截断
3. `pixel - total_offset`：使用 `cv::saturate_cast` 防止 UINT16 下溢（变成 65535 的白噪点）

**SCM 参数定义表**：

| 参数 | 物理定义 | 单位 | 测试条件 |
|------|----------|------|----------|
| `dark_offset_dn` | 光学黑电平偏置（绝对偏置） | DN | 极短曝光（t_exp ≈ 0） |
| `dark_current_e_per_sec` | 暗电流密度 | e⁻/s | 通过多曝光拟合 |
| `system_gain_dn_per_e` | 系统增益 | DN/e⁻ | SCM 测试结果 |
| `full_well_capacity_dn` | 满阱容量 | DN | SCM 测试结果 |

## 十六、UI/UX 逻辑闭环补充

### 16.1 局部覆盖策略

在"七、参数来源优先级规则"中提到 Quick Calib 优先级高于 SCM 和 Color Char。如果用户只想用 Quick Calib 的 WB，但想用 Color Char 的 CCM 怎么办？

**改善方案**：

在 UI 的右侧控制面板，除了全局的 `[Reload Calibration Results]`，可以在每个节点（如 N5, N6）旁边增加一个小型的下拉菜单，允许用户针对单个节点强制覆盖数据来源：

```
┌─────────────────────────────────────────────────────────┐
│  Node Parameters                                        │
├─────────────────────────────────────────────────────────┤
│  [0] OB                                                 │
│     Black Level: [100.0]  Source: [SCM ▼]               │
│                                                         │
│  [1] DPC                                                │
│     Dark Noise: [30.0]  Source: [SCM ▼]                 │
│                                                         │
│  [5] WhiteBalance                                       │
│     Gains: [1.20, 1.00, 0.80]  Source: [Quick Calib ▼]  │
│     ┌─────────────────────────────────────────────┐    │
│     │ ▼ Auto (Quick Calib)                        │    │
│     │   Color Char                                │    │
│     │   Manual                                    │    │
│     └─────────────────────────────────────────────┘    │
│                                                         │
│  [6] CCM                                                │
│     Matrix: [✓ Loaded]  Source: [Color Char ▼]          │
│     ┌─────────────────────────────────────────────┐    │
│     │   Auto (Color Char)                          │    │
│     │   Quick Calib                                │    │
│     │ ▼ Manual                                     │    │
│     └─────────────────────────────────────────────┘    │
│                                                         │
│  [7] Tone Mapping & Gamma                               │
│     Input Range: [0, 62258]  Source: [SCM ▼]            │
│     Gamma: [2.2]  Source: [Quick Calib ▼]              │
└─────────────────────────────────────────────────────────┘
```

**下拉菜单选项**：

- `Auto`：自动使用优先级最高的来源
- `SCM`：强制使用 SCM 结果
- `Color Char`：强制使用 Color Char 结果
- `Quick Calib`：强制使用 Quick Calib 结果
- `Manual`：使用用户手动输入的值

### 16.2 节点级数据来源选择逻辑

```cpp
enum class CalibSource {
    Auto,       // 自动选择
    SCM,
    ColorChar,
    QuickCalib,
    Manual
};

struct NodeParameterSource {
    CalibSource wb_source = CalibSource::Auto;
    CalibSource ccm_source = CalibSource::Auto;
    CalibSource gamma_source = CalibSource::Auto;
};

class PipelinePanel {
private:
    NodeParameterSource node_sources_;
    std::map<CalibSource, std::optional<WBGains>> cached_wb_results_;
    std::map<CalibSource, std::optional<CCMMatrix>> cached_ccm_results_;

public:
    // 根据节点级选择获取实际数据源
    CalibSource ResolveActualSource(CalibSource auto_source, CalibSource manual_source) {
        if (manual_source != CalibSource::Auto) {
            return manual_source;  // 用户手动选择
        }
        // 否则使用自动优先级
        if (cached_results_[CalibSource::QuickCalib].has_value()) {
            return CalibSource::QuickCalib;
        }
        if (cached_results_[CalibSource::ColorChar].has_value()) {
            return CalibSource::ColorChar;
        }
        if (cached_results_[CalibSource::SCM].has_value()) {
            return CalibSource::SCM;
        }
        return CalibSource::Manual;  // 回退到手动
    }
};
```

### 16.3 参数校验与警告

当用户手动输入参数值时，应进行实时校验并显示警告：

```
[5] WhiteBalance
    Gains: [1.20, 1.00, 0.80]  Source: [Manual ▼]
    ⚠ Warning: WB gains should be positive values (typically 0.5 ~ 2.0)

[6] CCM
    Matrix: [✓ Loaded]  Source: [Manual ▼]
    ⚠ Warning: Row sums deviate from 1.0 by > 5%, check calibration environment
```

**警告触发条件**：

- WB 增益超出合理范围（0.5 ~ 2.0）
- CCM 矩阵行和偏离 1.0 超过 5%
- Gamma 值超出合理范围（1.0 ~ 3.0）
- OB Offset 为负数或超过动态范围上限

## 十七、工程实现检查清单

在代码实现前，应逐项确认以下清单：

### 17.1 线程安全检查

- [ ] OBNode、DPCNode 的热更新参数使用 `std::atomic` 或 Mutex 保护
- [ ] 所有节点参数的 Setter/Getter 在多线程环境下安全
- [ ] Pipeline::Run() 与 UI 线程的参数更新互斥

### 17.2 内存安全检查

- [ ] 进入 OpenMP 并行循环前完成 ROI 边界检查
- [ ] CCM 矩阵乘法使用 `cv::saturate_cast` 写回
- [ ] OB 减法操作防止负数下溢
- [ ] N4 和 N7 使用内存池缓冲区

### 17.3 算法精度检查

- [ ] CCM 矩阵设置时校验行和偏离度
- [ ] OB Offset 计算后校验下限（>= 0）
- [ ] 动态 Offset 重算时校验上限（<= full_well / 2）
- [ ] 手动输入参数时实时校验并显示警告

### 17.4 性能优化检查

- [ ] N0-N3 节点使用原地修改（ProcessInPlace）
- [ ] N4 (Demosaicing) 使用预分配的 RGB 缓冲区
- [ ] N7 (TM & Gamma) 使用预分配的 8-bit 输出缓冲区
- [ ] 内存池正确归还和复用

### 17.5 UI/UX 检查

- [ ] 全局重新加载按钮功能完整
- [ ] 每个节点支持独立的参数来源选择
- [ ] 警告信息清晰、定位准确
- [ ] 节点依赖关系可视化

## 十八、ISP 流水线节点升级建议

从专业工业相机校准的极致要求来看，整体的 ISP 流水线顺序是完全正确的（OB → DPC → LSC → Denoise → Demosaic → WB → CCM → Gamma）。但在流程的深度优化和极端工业场景（如极暗、高倍增益、微距测量）的适应性上，有几个关键点需要进行"节点升级"或"微调删减"。

### 18.1 N0 节点 (OB) 升级：从"标量扣除"到"2D 暗场扣除 (DSNU)"

**现状**：文档中的 N0 节点使用公式 `total_offset = dark_signal_dn + dark_current_dn`，这是一个全局标量 (Scalar) 减法。

**工业级痛点**：

- 对于 12/16-bit 传感器，固定模式噪声（FPN，具体为暗信号非均匀性 DSNU）在每个像素上是不同的
- 在长曝光或高增益下，画面会出现明显的垂直条纹或固定噪点
- 全局标量扣除无法解决逐像素的暗信号差异

**优化建议**：

将 N0 升级为支持 **2D Dark Frame Subtraction**。设计策略如下：

```cpp
class OBNode {
private:
    cv::Mat dark_map_;          // 2D 暗场图像（逐像素暗信号）
    double scalar_offset_;      // 全局标量偏移量（回退策略）
    bool use_dark_map_;        // 是否使用 2D 暗场扣除

public:
    void SetDarkMap(const cv::Mat& dark_map) {
        dark_map_ = dark_map.clone();
        use_dark_map_ = !dark_map.empty();
    }

    cv::Mat Process(const cv::Mat& input) override {
        cv::Mat output(input.size(), input.type());

        if (use_dark_map_) {
            // 逐像素矩阵减法（2D 暗场扣除）
            cv::subtract(input, dark_map_, output, cv::noArray(), CV_16U);
            // 下限截断
            cv::max(output, 0, output);
        } else {
            // 回退到全局标量扣除策略
            for (int i = 0; i < input.rows; i++) {
                for (int j = 0; j < input.cols; j++) {
                    ushort val = input.at<ushort>(i, j);
                    output.at<ushort>(i, j) = cv::saturate_cast<ushort>(val - scalar_offset_);
                }
            }
        }

        return output;
    }
};
```

**暗场图像来源**：

- 在系统初始化时导入一张真实的暗场图像（Dark Map）
- 暗场图像应在相同曝光时间和增益条件下采集
- 如果未提供暗场图像，回退到当前的全局标量扣除策略

**效果对比**：

| 模式      | 优点         | 缺点           | 适用场景         |
| ------- | ---------- | ------------ | ------------ |
| 标量扣除    | 计算简单、速度快   | 无法消除 DSNU    | 低增益、短曝光      |
| 2D 暗场扣除 | 消除逐像素暗信号差异 | 需要额外存储、计算量增加 | 高增益、长曝光、工业测量 |

### 18.2 N2 节点 (LSC) 细分：宏观阴影 (LSC) vs 微观不均匀性 (PRNU)

**现状**：N2 称为镜头阴影校正（消除中心亮边缘暗）。通常这是一个低频、平滑的增益图（甚至可以被压缩为多项式曲面参数）。

**工业级痛点**：

- 对于线扫相机或需要做高精度表面缺陷检测的面阵相机
- 传感器本身的像素响应非均匀性 (PRNU) 是高频的（相邻像素感光能力不同）
- 传统 LSC 的高斯平滑会丢失 PRNU 校正所需的高频信息

**优化建议**：

在平场校正时，确保工具箱支持导出 **像素级 (Pixel-to-Pixel) 的高频 Gain Map**，而不仅仅是经过强力高斯平滑的宏观 LSC Map。

```cpp
class LSCNode {
private:
    cv::Mat lsc_map_;           // 低频镜头阴影校正图（用于标定报告）
    cv::Mat prnu_map_;          // 高频像素响应非均匀性校正图（用于标定报告）
    cv::Mat original_gain_map_; // 原始高精度 Gain Map（用于实际校正）
    bool use_prnu_;             // 是否启用 PRNU 校正

public:
    void SetGainMap(const cv::Mat& gain_map, bool preserve_high_freq = false) {
        original_gain_map_ = gain_map.clone();

        if (preserve_high_freq) {
            // 分离低频和高频分量（用于标定报告导出）
            cv::GaussianBlur(gain_map, lsc_map_, cv::Size(0, 0), 50.0);
            prnu_map_ = gain_map / lsc_map_;
            use_prnu_ = true;
        } else {
            lsc_map_ = gain_map.clone();
            // 对原始 Gain Map 进行高斯平滑，仅保留低频 LSC
            cv::GaussianBlur(gain_map, original_gain_map_, cv::Size(0, 0), 50.0);
            use_prnu_ = false;
        }
    }

    cv::Mat Process(const cv::Mat& input) override {
        cv::Mat output(input.size(), input.type());

        if (use_prnu_) {
            // 如果启用 PRNU（高频+低频），直接使用原始的高精度 Gain Map
            // 数学等价：Input * LSC * (GainMap/LSC) = Input * GainMap
            cv::multiply(input, original_gain_map_, output, 1.0, CV_16U);
        } else {
            // 如果仅启用宏观阴影校正，使用高斯平滑后的 LSC Map
            cv::multiply(input, original_gain_map_, output, 1.0, CV_16U);
        }

        return output;
    }
};
```

> ⚠️ **PERFORMANCE OPTIMIZATION**
> 
> **LSC 与 PRNU 的重复计算问题修复**：
> 
> 根据定义，`prnu_map_ = gain_map / lsc_map_`。那么两次相乘的数学本质是：
> 
> ```
> Output = Input * LSC * (GainMap/LSC) = Input * GainMap
> ```
> 
> 这意味着，只要用户启用了高频校正（`use_prnu_ = true`），完全可以直接用原始的 `gain_map` 乘以输入图像，没必要在 Pipeline 运行时进行两次极其消耗内存带宽的 `cv::multiply`。
> 
> **修复方案**：保留分离 LSC 和 PRNU 的逻辑（用于导出特定的标定报告），但在执行校正时，统一使用 `original_gain_map_`，避免重复计算。

**平场校正流程**：

```
平场图像采集：
1. 均匀光照下拍摄白板
   └─> N0: OB Subtraction（扣除暗电流底噪）
       └─> N1: DPC（修复坏点）
           └─> 计算 Gain Map = 理想均匀值 / 实际测量值
               ├─> 低频分量 (LSC)：高斯平滑后的 Gain Map
               └─> 高频分量 (PRNU)：原始 Gain Map / 低频分量
```

**注意事项**：

- LSC 和 PRNU 通常合并在一个 Flat Field 节点中处理
- 标定算法上需要保留高频信息的选项
- PRNU 校正需要更高精度的平场标定（均匀光源至关重要）

### 18.3 N4 节点 (Demosaicing) 补充：伪彩与拉链效应抑制

**现状**：N3 在 Bayer 空间完成了 Raw Denoise，接着 N4 进行去马赛克。

**工业级痛点**：

- 任何 Demosaicing 算法（如 Malvar-He-Cutler 或 VNG）在物体的高频边缘（如细线条、文字边缘）都会产生伪彩色（色斑）和拉链效应
- 这对于彩色工业测量（如颜色识别、缺陷检测）是不可接受的

**优化建议**：

在 N4 节点内部（或紧跟 N4 之后）集成一个轻量级的 **Chroma Denoise（色度降噪）** 环节：

```cpp
class DemosaicNode {
private:
    bool enable_chroma_denoise_;  // 是否启用色度降噪
    int chroma_blur_size_;        // 色度通道模糊核大小

public:
    cv::Mat Process(const cv::Mat& input) override {
        cv::Mat rgb_output;

        // 执行去马赛克算法
        cv::cvtColor(input, rgb_output, cv::COLOR_BayerBG2BGR);

        if (enable_chroma_denoise_) {
            // 将图像转入 YUV 空间
            cv::Mat yuv_output;
            cv::cvtColor(rgb_output, yuv_output, cv::COLOR_BGR2YUV);

            // 分离通道
            std::vector<cv::Mat> channels(3);
            cv::split(yuv_output, channels);

            // 仅对色度通道 (U, V) 进行平滑
            cv::GaussianBlur(channels[1], channels[1], 
                             cv::Size(chroma_blur_size_, chroma_blur_size_), 0);
            cv::GaussianBlur(channels[2], channels[2], 
                             cv::Size(chroma_blur_size_, chroma_blur_size_), 0);

            // 合并通道并转回 RGB
            cv::merge(channels, yuv_output);
            cv::cvtColor(yuv_output, rgb_output, cv::COLOR_YUV2BGR);
        }

        return rgb_output;
    }
};
```

**色度降噪策略**：

| 策略         | 优点           | 缺点       | 适用场景       |
| ---------- | ------------ | -------- | ---------- |
| YUV 空间色度平滑 | 保持亮度细节，仅消除色偏 | 轻微模糊色彩过渡 | 彩色工业测量     |
| 边缘检测引导滤波   | 精确保护边缘       | 计算复杂度高   | 高精度检测      |
| 无降噪        | 保持原始色彩       | 伪彩色明显    | 不需要色彩精度的场景 |

**伪彩抑制效果对比**：

```
原始去马赛克输出：
┌─────────────────────────────────────────┐
│  高频边缘处出现明显色斑和拉链效应          │
│  ┌──────┐                              │
│  │  ██  │ ← 白色线条边缘出现红绿伪彩      │
│  │  ██  │                              │
│  └──────┘                              │
└─────────────────────────────────────────┘

应用色度降噪后：
┌─────────────────────────────────────────┐
│  边缘色彩纯净，无伪彩色干扰               │
│  ┌──────┐                              │
│  │  ██  │ ← 白色线条边缘干净             │
│  │  ██  │                              │
│  └──────┘                              │
└─────────────────────────────────────────┘
```

## 十九、流程逻辑精简与明确

### 19.1 精简 N7 节点 (Tone Mapping & Gamma) 的线性依赖

**现状**：文档强调公式 `output = pow((input / full_well) * 255.0 / 255.0, 1.0/gamma) * 255.0`。

**优化建议**：

Tone Mapping (动态范围压缩) 和 Gamma 最好解耦为两个明确的阶段：

```
当前设计：
N7 (Tone Mapping & Gamma)
├─> 线性缩放：input / full_well * 255.0
└─> Gamma 校正：pow(x, 1.0/gamma) * 255.0

推荐设计（未来扩展）：
N7 (Tone Mapping)
├─> 阶段1：线性缩放（当前实现）
└─> 阶段2：非线性压缩（未来扩展，如 HDR、局部直方图均衡）

N8 (Gamma Correction)
└─> Gamma 曲线校正

注：当前仅实现阶段1，阶段2为预留扩展点
```

**关键原则**：

| 阶段               | 作用      | 是否线性           | 位置       |
| ---------------- | ------- | -------------- | -------- |
| Tone Mapping     | 动态范围压缩  | 当前：线性；未来：可能非线性 | 在 CCM 之后 |
| Gamma Correction | 显示器响应补偿 | 非线性            | 在所有处理之后  |

**未来扩展注意事项**：

如果未来引入了非线性 Tone Mapping（如针对高反光金属件的局部直方图均衡或 HDR 算法），必须：

1. 将非线性 Tone Mapping 作为一个独立节点放在 Gamma 之前
2. 绝不能在同一个节点中同时做非线性 Tone Mapping 和 Gamma 校正
3. 否则会严重破坏色彩校正的线性度，导致颜色失真

### 19.2 CCM 标定时的 Gamma 前置预处理检查

**检查点**：在"Quick Calib / Color Char"模块生成 CCM 矩阵时，标定算法是否先对参考 24 色卡的 sRGB 目标值进行了一次 **逆 Gamma 解码 (Inverse Gamma)**？

**核心逻辑**：

因为流水线中，N6 (CCM) 发生在 N7 (Gamma) 之前，这意味着：

- N6 的输入和输出都必须是绝对的 **线性空间**
- 如果 CCM 标定工具是直接拿相机的线性 RGB 去拟合包含 Gamma 2.2 的 sRGB 标准值
- 算出来的矩阵不仅误差极大，且在应用到 N6 时会导致暗部色彩严重失真

**校正流程**：

```
CCM 标定流程（正确做法）：
1. 采集色卡图像
   └─> N0-N4 处理（到 RGB 线性空间）
       └─> N5 白平衡（线性空间）
           └─> 提取 24 个色块的线性 RGB 值

2. 准备参考标准值
   └─> 获取 X-Rite 24 色卡的 sRGB 标准值
       └─> 逆 Gamma 解码：linear_RGB = pow(sRGB / 255.0, gamma) * 255.0
           └─> 得到线性空间的参考 RGB 值

3. 求解 CCM 矩阵
   └─> 使用线性空间的相机 RGB 和线性空间的参考 RGB 进行拟合
       └─> 得到正确的 CCM 矩阵（适用于线性空间）

4. 应用 CCM
   └─> 在 N6 节点应用 CCM（输入是线性空间，输出也是线性空间）
       └─> N7 节点应用 Gamma 校正（线性空间 → sRGB 空间）
```

**逆 Gamma 解码公式**：

```
对于 sRGB 标准值（已应用 Gamma 2.2）：
if (sRGB <= 0.04045):
    linear = sRGB / 12.92
else:
    linear = ((sRGB + 0.055) / 1.055) ^ 2.4

对于简单 Gamma 2.2（近似）：
linear = pow(sRGB / 255.0, 2.2) * 255.0
```

**严重警告**：

> ⚠️ **CRITICAL WARNING**
> 
> 输入给 CCM 求解器的参考颜色必须是 **解完 Gamma 的线性值**。
> 
> 如果直接使用包含 Gamma 校正的 sRGB 标准值进行拟合，会导致：
> 
> - 暗部色彩严重失真
> - 亮度响应曲线偏离
> - 色彩精度大幅下降（ΔE 可能超过 10）
> 
> 这是一个**致命错误**，必须在标定流程中严格避免。

## 二十、升级后的完整 ISP 流水线

### 20.1 升级后的节点顺序

| 序号  | 节点名          | 功能            | 默认状态 | 升级内容                           |
| --- | ------------ | ------------- | ---- | ------------------------------ |
| N0  | OB           | 光学黑电平扣除       | ✅ 启用 | 支持 2D 暗场扣除 (DSNU)，回退到标量扣除      |
| N1  | DPC          | 坏点校正          | ✅ 启用 | 无升级                            |
| N2  | LSC          | 镜头阴影校正 + PRNU | ❌ 禁用 | 支持像素级高频 Gain Map，分离 LSC 和 PRNU |
| N3  | Denoise      | 降噪            | ✅ 启用 | 无升级                            |
| N4  | Demosaicing  | 去马赛克 + 色度降噪   | ✅ 启用 | 集成轻量级 Chroma Denoise 环节        |
| N5  | WhiteBalance | 白平衡           | ❌ 禁用 | 无升级                            |
| N6  | CCM          | 色彩校正矩阵        | ❌ 禁用 | 输入必须是线性空间（依赖 N5）               |
| N7  | Tone Mapping | 动态范围映射        | ✅ 启用 | 解耦为独立节点，预留非线性扩展                |
| N8  | Gamma        | Gamma 校正      | ✅ 启用 | 独立节点，在所有处理之后                   |

### 20.2 升级后的数据流

```
完整 ISP 流水线（升级后）：
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  输入：Bayer Raw (12/16-bit)                                                │
│        │                                                                    │
│        ▼                                                                    │
│  N0: OB Subtraction (支持 2D 暗场扣除)                                       │
│        │                                                                    │
│        ▼                                                                    │
│  N1: DPC (坏点检测与修复)                                                    │
│        │                                                                    │
│        ▼                                                                    │
│  N2: LSC + PRNU (低频阴影校正 + 高频像素响应均匀性)                            │
│        │                                                                    │
│        ▼                                                                    │
│  N3: Denoise (Raw 空间降噪)                                                  │
│        │                                                                    │
│        ▼                                                                    │
│  N4: Demosaicing + Chroma Denoise (去马赛克 + 色度降噪)                        │
│        │                                                                    │
│        ▼                                                                    │
│  N5: WhiteBalance (白平衡增益)                                               │
│        │                                                                    │
│        ▼                                                                    │
│  N6: CCM (色彩校正矩阵，输入必须是线性空间)                                    │
│        │                                                                    │
│        ▼                                                                    │
│  N7: Tone Mapping (动态范围映射，预留非线性扩展)                                │
│        │                                                                    │
│        ▼                                                                    │
│  N8: Gamma Correction (Gamma 曲线校正)                                        │
│        │                                                                    │
│        ▼                                                                    │
│  输出：sRGB (8-bit)                                                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 20.3 升级节点的依赖关系

| 节点  | 新增依赖           | 说明                     |
| --- | -------------- | ---------------------- |
| N0  | Dark Map       | 2D 暗场图像（可选，未提供时回退到标量）  |
| N2  | PRNU Map       | 高频像素响应非均匀性校正图（可选）      |
| N4  | Chroma Denoise | 色度降噪参数（可选，默认启用）        |
| N7  | None           | 解耦为独立节点，输入输出均为线性空间     |
| N8  | None           | 独立节点，输入线性空间，输出 sRGB 空间 |

## 二十一、升级建议优先级

根据工业应用场景的优先级，建议按以下顺序实施升级：

| 优先级 | 升级项          | 影响范围  | 实施难度 | 推荐场景    |
| --- | ------------ | ----- | ---- | ------- |
| P0  | CCM Gamma 校验 | 标定精度  | 低    | 所有场景    |
| P1  | N0 2D 暗场扣除   | 图像质量  | 中    | 高增益、长曝光 |
| P2  | N4 色度降噪      | 色彩精度  | 中    | 彩色工业测量  |
| P3  | N2 PRNU 校正   | 测量精度  | 高    | 高精度表面检测 |
| P4  | N7/N8 解耦     | 架构扩展性 | 中    | 未来功能扩展  |

**P0 优先级说明**：CCM 的 Gamma 前置处理是致命错误，必须首先确保正确。其他升级项可以根据具体应用场景选择性实施。

## 二十二、升级后的工程检查清单补充

### 22.1 N0 节点升级检查

- [ ] 支持导入 2D 暗场图像（Dark Map）
- [ ] 未提供暗场图像时自动回退到标量扣除
- [ ] 2D 暗场扣除使用逐像素矩阵减法
- [ ] 下限截断确保非负

### 22.2 N2 节点升级检查

- [ ] 支持导出像素级高频 Gain Map
- [ ] 分离低频 LSC 和高频 PRNU 分量
- [ ] 提供是否保留高频信息的选项
- [ ] 平场标定工具支持高精度均匀光源

### 22.3 N4 节点升级检查

- [ ] 集成轻量级 Chroma Denoise 环节
- [ ] 支持在 YUV/Lab 空间进行色度平滑
- [ ] 提供色度降噪开关和参数调整
- [ ] 不影响亮度通道的细节

### 22.4 N7/N8 节点检查

- [ ] Tone Mapping 和 Gamma 解耦为独立节点
- [ ] 预留非线性 Tone Mapping 的扩展接口
- [ ] Gamma 节点仅做 Gamma 曲线校正
- [ ] 保持线性处理阶段的纯净性

### 22.5 CCM 标定检查

- [ ] 对标定参考值进行逆 Gamma 解码
- [ ] 使用线性空间的值进行 CCM 拟合
- [ ] 在 UI 中添加 Gamma 预处理警告
- [ ] 提供标定数据的线性空间验证工具

## 二十三、N4 节点（去马赛克）深度优化方案

在工业级 ISP 流水线中，N4（去马赛克/Demosaicing）是一个极其危险的"误差放大点"。因为它是整条流水线中唯一一个无中生有（猜测像素）的环节（将单通道的 Bayer 阵列猜测为三通道的 RGB）。如果在这一步产生了边缘锯齿（拉链效应）或高频伪彩，后续的 N6（CCM 色彩校正矩阵）和 N7（Gamma）会将这些微小的瑕疵成倍放大。

### 23.1 核心去马赛克算法选型 (Algorithm Tiers)

建议在软件中不要写死一种算法，而是提供速度与质量的权衡选项。如果是基于 OpenCV 实现，请务必避开默认的性能陷阱。

| 方案 | 算法名称 | OpenCV 实现 | 工业表现 | 适用场景 |
|------|----------|-------------|----------|----------|
| 基础方案 | 双线性插值 (Bilinear) | `cv::COLOR_BayerBG2RGB` | 极差 | 仅限 30fps+ 实时预览，**绝对不能用于高精度测量** |
| 均衡方案 | 可变梯度算法 (VNG) | `cv::COLOR_BayerBG2RGB_VNG` | 良好 | 对色彩还原有要求，但算力有限的常规工业检测 |
| 标杆方案 | 边缘感知插值 (EA) | `cv::COLOR_BayerBG2RGB_EA` | 优秀 | **高精度测量、缺陷检测、高质量色彩标定——推荐默认** |

#### 23.1.1 基础方案：双线性插值 (Bilinear Interpolation)

**实现方式**：OpenCV 默认的 `cv::COLOR_BayerBG2RGB`

**工业表现**：极差。它完全无视图像的边缘走向，在纵横交错的纹理处会产生严重的"拉链效应（Zipper Effect）"。

**适用场景**：仅限 30fps+ 的实时取景预览（Preview），绝对不能用于最终的图像保存或高精度色彩测量。

#### 23.1.2 均衡方案：可变梯度算法 (VNG)

**实现方式**：OpenCV 中的 `cv::COLOR_BayerBG2RGB_VNG`

**工业表现**：良好。它通过计算 8 个方向的梯度来决定插值权重，大幅减少了伪彩色。

**缺陷**：在极其锐利的边缘处仍有轻微模糊，且计算量比双线性大数倍。

**适用场景**：对色彩还原有要求，但算力有限的常规工业检测。

#### 23.1.3 标杆方案：边缘感知插值 (Edge-Aware / EA)

**实现方式**：OpenCV 中的 `cv::COLOR_BayerBG2RGB_EA`，或自行实现 Hamilton-Adams (HA) 算法

**核心原理**：不仅利用同色通道的信息，还利用跨通道的相关性（Cross-channel Correlation）。例如在计算未知的 R 像素时，会引入 G 通道的二阶微分算子（拉普拉斯）作为修正项：

```
R_estimated = R_bilinear + α · ∇²G
```

**工业表现**：优秀。极大地保留了物理边缘的锐度，是目前高端 ISP 的基线标准。

**适用场景**：高精度尺寸测量、缺陷检测、高质量色彩标定。**强烈推荐作为 Pipeline 的默认算法**。

### 23.2 进阶优化：伪彩抑制模块 (FCS - False Color Suppression)

即使使用了 Edge-Aware 算法，在摩尔纹（Moiré pattern）高发区（如拍摄密集的网格、纺织物）依然会产生红蓝交替的伪彩色。需要引入色度降噪模块。

**建议在 N4 节点内部，在插值完成后，紧接着执行以下轻量级防呆处理**：

```
N4 节点完整流程：
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  输入：Bayer Raw (16-bit 单通道)                             │
│        │                                                    │
│        ▼                                                    │
│  Step 1: 去马赛克算法选择                                    │
│        ├─> Bilinear (实时预览)                              │
│        ├─> VNG (均衡方案)                                   │
│        └─> Edge-Aware (默认/高精度)                         │
│        │                                                    │
│        ▼                                                    │
│  Step 2: 色彩空间转换                                        │
│        └─> RGB → YUV (或 YCbCr)                             │
│        │                                                    │
│        ▼                                                    │
│  Step 3: 亮色分离与色度滤波                                  │
│        ├─> Y 通道：保持不变（保留锐度）                        │
│        ├─> U 通道：3x3/5x5 中值滤波或高斯低通                 │
│        └─> V 通道：3x3/5x5 中值滤波或高斯低通                 │
│        │                                                    │
│        ▼                                                    │
│  Step 4: 逆向转换                                           │
│        └─> YUV → RGB                                        │
│        │                                                    │
│        ▼                                                    │
│  输出：RGB (16-bit 三通道)                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**色度降噪策略对比**：

| 策略 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| 3x3 高斯滤波 | 计算最快，平滑效果好 | 轻微模糊色彩细节 | 实时预览、常规检测 |
| 3x3 中值滤波 | 有效消除伪彩斑块 | 计算稍慢 | 摩尔纹高发场景 |
| 5x5 高斯滤波 | 更强的伪彩抑制 | 明显模糊色彩过渡 | 极端摩尔纹场景 |

**工程收益**：这个操作的计算代价极低，但能瞬间抹除高频区域的红蓝伪彩斑块，让画面纯净度产生质的飞跃。

### 23.3 C++ 架构与性能压榨建议

N4 节点是整条 Pipeline 的内存带宽第一大杀手。输入是 16-bit 的单通道（假设 2000 万像素，约 40MB），输出是 16-bit 的三通道（约 120MB）。内存的读写量瞬间膨胀。

#### 23.3.1 复用内存池 (Buffer Pool)

千万不要在 N4 内部 `new` 出那 120MB 的空间。必须向全局的 `MemoryPool` 申请一块预分配的 `CV_16UC3` 连续内存块。

```cpp
class DemosaicNode : public ImageNode {
private:
    ImageBufferPool& pool_;
    DemosaicAlgorithm algo_;  // 算法选择枚举
    bool enable_fcs_;         // 是否启用伪彩抑制

public:
    cv::Mat Process(const cv::Mat& input) override {
        // 从内存池获取预分配的 RGB 缓冲区
        cv::Mat& output = pool_.GetRgbBuffer(input.rows, input.cols, CV_16UC3);

        // 选择去马赛克算法
        int cv_code = CV_COLOR_BayerBG2RGB_EA;  // 默认使用 Edge-Aware
        if (algo_ == DemosaicAlgorithm::BILINEAR) {
            cv_code = CV_COLOR_BayerBG2RGB;
        } else if (algo_ == DemosaicAlgorithm::VNG) {
            cv_code = CV_COLOR_BayerBG2RGB_VNG;
        }

        // 执行去马赛克（output 已预分配，避免隐式分配）
        cv::cvtColor(input, output, cv_code);

        // 伪彩抑制处理
        if (enable_fcs_) {
            ApplyFalseColorSuppression(output);
        }

        return output;  // 返回复用的缓冲区
    }
};
```

#### 23.3.2 避免 OpenCV 的隐式分配

如果在 OpenCV 中直接调用 `cv::cvtColor(src, dst, cv::COLOR_BayerBG2RGB_EA)`，**确保 `dst` 已经预分配好大小和类型**，否则 OpenCV 会在底层偷偷触发重新分配（Reallocation），导致帧率断崖式下跌。

**正确做法**：
```cpp
// ✅ 正确：预分配输出缓冲区
cv::Mat dst(input.size(), CV_16UC3);  // 预分配
cv::cvtColor(src, dst, cv::COLOR_BayerBG2RGB_EA);

// ❌ 错误：让 OpenCV 隐式分配
cv::Mat dst;  // 空矩阵
cv::cvtColor(src, dst, cv::COLOR_BayerBG2RGB_EA);  // 内部会重新分配
```

#### 23.3.3 多线程与 SIMD（针对自研算法）

如果发现 OpenCV 的 EA 算法在千万像素下依然达不到帧率要求（例如耗时 > 50ms），可以考虑以下优化：

**行切块并行处理**：
```cpp
#pragma omp parallel for
for (int row = 0; row < height; row += chunk_size) {
    // 处理 chunk_size 行
    cv::Rect roi(0, row, width, std::min(chunk_size, height - row));
    cv::Mat src_chunk = src(roi);
    cv::Mat dst_chunk = dst(roi);
    cv::cvtColor(src_chunk, dst_chunk, cv_code);
}
```

**AVX2/AVX-512 矢量化**：
- 对于 16-bit 数据，使用 AVX2 或 AVX-512 指令集进行矢量化插值
- 可以使用 Intel IPP 库或自行编写汇编优化
- 预期速度提升：2-3 倍

### 23.4 N4 节点参数配置建议

在 Pipeline 页面的节点参数面板中，为 N4 添加以下可配置选项：

```
[4] Demosaicing (ON) [Default]
    Algorithm: [Edge-Aware ▼]
               ├─ Bilinear (Preview)
               ├─ VNG (Balanced)
               └─ Edge-Aware (High Quality)
    False Color Suppression: [✓ Enabled]
    FCS Kernel Size: [3x3 ▼]
                     ├─ 3x3 (Fast)
                     └─ 5x5 (Strong)
    FCS Filter Type: [Gaussian ▼]
                     ├─ Gaussian (Smooth)
                     └─ Median (Plaque Removal)
```

### 23.5 N4 节点的误差传播分析

去马赛克产生的误差会在后续节点中放大，必须高度重视：

| 误差类型 | 产生原因 | 后续放大效应 | 影响程度 |
|----------|----------|--------------|----------|
| 拉链效应 | 双线性插值无视边缘 | CCM 矩阵乘法进一步扭曲边缘 | 高 |
| 伪彩色 | 高频纹理处的色偏 | Gamma 校正放大暗部色偏 | 高 |
| 边缘模糊 | VNG 算法的平滑特性 | 尺寸测量精度下降 | 中 |
| 摩尔纹 | 采样率不足 | CCM 无法消除周期性伪影 | 高 |

**误差传播链**：
```
N4 (Demosaicing) → N5 (WB) → N6 (CCM) → N7 (TM) → N8 (Gamma)
       │                 │           │           │           │
       ▼                 ▼           ▼           ▼           ▼
   拉链效应            增益放大    矩阵扭曲    范围压缩    暗部放大
   伪彩色              色偏加重    色彩失真    对比度变化  色差加剧
```

**结论**：必须将 N4 节点的质量放在最高优先级，选择 Edge-Aware 算法并启用伪彩抑制是工业级应用的基本要求。

## 二十四、N4 节点优化后的工程检查清单

### 24.1 算法选型检查

- [ ] 支持三种算法选择：Bilinear、VNG、Edge-Aware
- [ ] 默认使用 Edge-Aware 算法
- [ ] 根据场景动态切换算法（预览 vs 保存）

### 24.2 伪彩抑制检查

- [ ] 在去马赛克后自动执行 FCS 模块
- [ ] 支持 YUV 空间的色度通道滤波
- [ ] 提供 FCS 开关和参数调整
- [ ] 不影响亮度通道的锐度

### 24.3 内存管理检查

- [ ] 使用预分配的内存池缓冲区
- [ ] 避免 OpenCV 的隐式内存分配
- [ ] 输出缓冲区类型正确（CV_16UC3）
- [ ] 缓冲区正确归还到内存池

### 24.4 性能优化检查

- [ ] 千万像素图像去马赛克耗时 < 50ms
- [ ] 支持行切块并行处理（OpenMP）
- [ ] 预留 AVX2/SIMD 优化接口
- [ ] 算法切换不触发内存重新分配

## 二十五、位深自适应机制 (Bit-Depth Agnostic Architecture)

CameraToolKit 需要在同一个 Pipeline 架构下兼容 8、10、12、16-bit 的图像流，就不能在算法节点中把数据边界"写死"。为了保证底层 C++ 代码的灵活性和极高的执行效率，需要引入自适应机制。

### 25.1 内存解包与类型收束 (Unpacking & Type Dispatch)

工业相机的 10-bit 和 12-bit 数据流通常不是按标准字节对齐的（例如 12-bit 可能是 2 个像素塞进 3 个字节）。

#### 25.1.1 数据入口层 (Ingress)

在进入 Pipeline 之前，必须进行解包（Unpack）：

| 原始位深 | 存储格式 | 解包后类型 | 说明 |
|----------|----------|-----------|------|
| 8-bit | 标准字节对齐 | `CV_8U` (uint8_t) | 直接使用 |
| 10-bit | 紧凑打包（如 10-bit 塞进 16-bit） | `CV_16U` (uint16_t) | 低位对齐 |
| 12-bit | 2 像素 / 3 字节 或 16-bit 容器 | `CV_16U` (uint16_t) | 低位对齐 |
| 16-bit | 标准字节对齐 | `CV_16U` (uint16_t) | 直接使用 |

**解包规则**：
- 所有非 8-bit 数据统一解包到 `CV_16U` 容器
- 保持低位对齐（数据值不进行缩放）
- 例如：10-bit 的 1023 存储为 16-bit 的 1023（不是 65535）

#### 25.1.2 Pipeline 处理层

内部节点只面对两种底层物理数据结构：
- 8 位无符号整型 (`CV_8U`)
- 16 位无符号整型 (`CV_16U`)

**设计原则**：
- 避免在核心算法中出现处理"1.5 字节"的奇葩逻辑
- 所有节点的 `Process()` 接口必须支持这两种类型
- 通过模板机制实现零成本类型分发

### 25.2 算法参数的"位深自适应" (Bit-Depth Agnostic Math)

> ⚠️ **CRITICAL CORRECTION**
> 
> **位深解包与 DN 值缩放的逻辑一致性**：
> 
> 在"低位对齐"的设计下，传感器输出的 DN (Digital Number) 就是其真实的物理量化结果。假设相机的系统增益为 $2.0 \text{ DN}/e^{-}$，产生了 $10 e^{-}$ 的暗噪声，那么它体现出来的就是 $20 \text{ DN}$。无论你是把它放在 12-bit 的容器里，还是 16-bit 的容器里，只要你没有对其进行左移（<<）操作，它的数值就永远是 20。
> 
> **结论**：Pipeline 内部直接使用 SCM 的 DN 值进行扣除和比较，**无需任何基于位深的乘除缩放**。位深 BitDepth 唯一的用途是用来计算钳位上限（Max_DN）。

#### 25.2.1 满阱与绝对最大值 (Max DN)

系统需要根据传感器配置输入一个 `BitDepth` 变量，动态计算当前图像的最大数字值：

```
Max_DN = 2^BitDepth - 1
```

| 位深 | Max_DN | 说明 |
|------|--------|------|
| 8-bit | 255 | 0 ~ 255 |
| 10-bit | 1023 | 0 ~ 1023 |
| 12-bit | 4095 | 0 ~ 4095 |
| 16-bit | 65535 | 0 ~ 65535 |

**关键警示**：

> ⚠️ **CRITICAL WARNING**
> 
> Tone Mapping 节点映射到 8-bit 时，输入区间必须是 `[0, Max_DN]`，绝不能用 `[0, 65535]` 去做线性拉伸！
> 
> 例如，对于 10-bit 图像：
> - 错误：output = input / 65535 * 255（画面变成一片死黑）
> - 正确：output = input / 1023 * 255（正确的线性映射）

#### 25.2.2 噪声阈值 (DPC & Denoise)

暗噪声和散粒噪声的绝对 DN 值**与位深无关**。SCM 测试必须在相机当前的物理位深下进行，输出的 `dark_noise_dn` 即为当前位深的绝对物理 DN 值。

**DPC 节点的实现**：
```cpp
class DPCNode {
private:
    double dark_noise_dn_;  // SCM 测试结果，直接使用，无需缩放

public:
    void SetDarkNoise(double noise) {
        dark_noise_dn_ = noise;
    }

    double GetDarkNoise() const {
        return dark_noise_dn_;  // 直接返回，无需位深缩放
    }
};
```

#### 25.2.3 OB 偏置扣除 (Black Level)

暗场偏置（Offset）**与位深无关**。SCM 测试输出的 `dark_offset_dn` 即为当前位深的绝对物理 DN 值，直接用于扣除。

**OB 节点的实现**：
```cpp
class OBNode {
private:
    double dark_offset_dn_;    // SCM 测试结果，直接使用，无需缩放
    double dark_current_dn_per_s_;  // 暗电流 DN/s
    double gain_dn_per_e_;     // 系统增益 DN/e-

public:
    void SetDarkOffset(double offset) {
        dark_offset_dn_ = offset;
    }

    void SetDarkCurrent(double dark_current_e_per_s, double gain_dn_per_e) {
        dark_current_dn_per_s_ = dark_current_e_per_s * gain_dn_per_e;
    }

    double CalculateTotalOffset(double exposure_s) const {
        return dark_offset_dn_ + (dark_current_dn_per_s_ * exposure_s);
    }
};
```

**重要定义**：

| 参数 | 定义 | 单位 | 获取方式 |
|------|------|------|----------|
| `dark_offset_dn` | 光学黑电平偏置（绝对偏置） | DN | 在极短曝光下测得（t_exp ≈ 0） |
| `dark_current_dn_per_s` | 暗电流随时间的累积率 | DN/s | 通过 SCM 测试拟合得到 |
| `gain_dn_per_e` | 系统增益 | DN/e⁻ | SCM 测试结果 |

### 25.3 零成本抽象的 C++ 模板分发

由于 8-bit 和 16-bit 的像素遍历需要不同的指针类型（`uchar*` vs `ushort*`），为了避免在 `#pragma omp parallel for` 的每一次像素循环中都使用 `if (img.type() == CV_8U)` 这种极度损耗性能的分支预测，建议使用 C++ 模板机制。

#### 25.3.1 核心处理函数设计（修复内存安全隐患）

> ⚠️ **MEMORY SAFETY FIX**
> 
> 工业相机 SDK（特别是对接 GigE Vision 时）为了内存对齐，经常会输出带有 stride (步长/填充) 的图像缓冲。此时图像数据在内存中并不是 1D 连续的（即 `img.isContinuous()` 为 false）。强制用 `rows * cols * channels` 展开为 1D 循环会导致访问无效的 Padding 内存，触发内存访问违例。

```cpp
template <typename T>
void ProcessChannelData(cv::Mat& img, double offset, double max_dn) {
    int rows = img.rows;
    int cols = img.cols * img.channels();

    // 连续性检查：如果连续，视为单行处理
    if (img.isContinuous()) {
        cols *= rows;
        rows = 1;
    }

    #pragma omp parallel for
    for (int r = 0; r < rows; ++r) {
        T* row_ptr = img.ptr<T>(r);
        for (int c = 0; c < cols; ++c) {
            double val = static_cast<double>(row_ptr[c]) - offset;
            row_ptr[c] = static_cast<T>(std::max(0.0, std::min(val, max_dn)));
        }
    }
}
```

**内存安全设计要点**：

| 要点 | 说明 |
|------|------|
| 连续性判断 | 使用 `img.isContinuous()` 检查内存布局 |
| 行指针遍历 | 使用 `img.ptr<T>(r)` 获取每行的起始指针 |
| 避免越界 | 基于 `cols` 而非 `total_pixels` 进行循环 |
| Padding 安全 | 自动跳过内存填充区域 |

#### 25.3.2 类型分发入口

在节点的 `Process()` 入口进行一次性类型分发：

```cpp
class OBNode {
private:
    double offset_8bit_;
    double offset_16bit_;
    double max_dn_16bit_;

public:
    cv::Mat Process(const cv::Mat& input) override {
        cv::Mat output = input.clone();

        // 一次性类型分发，将分支判断提到循环外部
        if (output.depth() == CV_8U) {
            ProcessChannelData<uint8_t>(output, offset_8bit_, 255.0);
        } else if (output.depth() == CV_16U) {
            ProcessChannelData<uint16_t>(output, offset_16bit_, max_dn_16bit_);
        }

        return output;
    }
};
```

#### 25.3.3 设计优势

| 优势 | 说明 |
|------|------|
| 零成本抽象 | 模板在编译期展开，无运行时开销 |
| 分支预测优化 | 类型判断只做一次，不在循环内重复判断 |
| CPU 缓存友好 | 模板展开后代码更简单，缓存命中率更高 |
| 并行效率高 | OpenMP 可以充分利用多核 |

#### 25.3.4 完整的位深自适应节点示例

```cpp
class BitDepthAgnosticNode : public ImageNode {
private:
    int bit_depth_;
    double param_16bit_;  // 参数存储在 16-bit 基准下

    // 计算当前位深下的有效参数
    double GetEffectiveParam() const {
        return param_16bit_ / std::pow(2, 16 - bit_depth_);
    }

    double GetMaxDN() const {
        return std::pow(2, bit_depth_) - 1;
    }

    template <typename T>
    void ProcessTemplate(cv::Mat& img) {
        double effective_param = GetEffectiveParam();
        double max_dn = GetMaxDN();
        T* data = img.ptr<T>();
        int total = img.rows * img.cols * img.channels();

        #pragma omp parallel for
        for (int i = 0; i < total; ++i) {
            double val = static_cast<double>(data[i]) + effective_param;
            val = std::max(0.0, std::min(val, max_dn));
            data[i] = static_cast<T>(val);
        }
    }

public:
    void SetBitDepth(int bit_depth) {
        bit_depth_ = bit_depth;
    }

    cv::Mat Process(const cv::Mat& input) override {
        cv::Mat output = input.clone();

        if (output.depth() == CV_8U) {
            ProcessTemplate<uint8_t>(output);
        } else if (output.depth() == CV_16U) {
            ProcessTemplate<uint16_t>(output);
        }

        return output;
    }
};
```

### 25.4 位深自适应的全局配置

在 Pipeline 的全局配置中，需要统一管理位深信息：

```cpp
struct PipelineConfig {
    int bit_depth = 12;  // 默认 12-bit
    bool is_raw = true;  // 是否为 Raw 数据

    // 计算当前位深的 Max DN
    int GetMaxDN() const {
        return (1 << bit_depth) - 1;
    }

    // 16-bit 到当前位深的缩放因子
    double GetScaleFactor() const {
        return 1.0 / std::pow(2, 16 - bit_depth);
    }

    // 当前位深到 16-bit 的缩放因子
    double GetUpscaleFactor() const {
        return std::pow(2, 16 - bit_depth);
    }
};
```

### 25.5 位深自适应的数据流图

```
完整的位深自适应数据流：
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│  相机原始数据 (8/10/12/16-bit)                                           │
│        │                                                                │
│        ▼                                                                │
│  数据入口层 (Ingress)                                                    │
│        ├─> 8-bit: 直接使用 CV_8U                                        │
│        └─> 10/12/16-bit: 解包到 CV_16U (低位对齐，不缩放)                 │
│        │                                                                │
│        ▼                                                                │
│  全局配置层                                                              │
│        ├─> BitDepth = 12 (示例)                                         │
│        └─> Max_DN = 4095 (仅用于 Tone Mapping 钳位)                      │
│        │                                                                │
│        ▼                                                                │
│  Pipeline 处理层 (所有节点位深自适应)                                       │
│        ├─> N0(OB): offset = SCM_dark_offset_dn (直接使用，无需缩放)        │
│        ├─> N1(DPC): noise_threshold = SCM_dark_noise_dn (直接使用)        │
│        ├─> N2(LSC): gain_map (直接使用，无需缩放)                         │
│        ├─> N3(Denoise): 噪声参数 (直接使用，无需缩放)                       │
│        ├─> N4(Demosaic): 模板处理 CV_8U/CV_16U                           │
│        ├─> N5(WB): gains 线性参数，不受位深影响                            │
│        ├─> N6(CCM): 矩阵参数，不受位深影响                                │
│        ├─> N7(ToneMapping): input_range = [0, Max_DN]                   │
│        └─> N8(Gamma): 输出 8-bit sRGB                                   │
│        │                                                                │
│        ▼                                                                │
│  输出层                                                                  │
│        └─> sRGB (8-bit)                                                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 二十六、位深自适应的工程检查清单

### 26.1 数据入口层检查

- [ ] 支持 8/10/12/16-bit 原始数据输入
- [ ] 10/12-bit 数据正确解包到 CV_16U
- [ ] 低位对齐，不进行值缩放
- [ ] 提供位深检测机制

### 26.2 全局配置检查

- [ ] 全局 BitDepth 配置统一管理
- [ ] Max_DN 动态计算正确（仅用于 Tone Mapping 钳位）
- [ ] SCM 测试结果直接使用，无需位深缩放
- [ ] `dark_offset_dn` 定义为极短曝光下的绝对偏置
- [ ] `dark_offset_dn` 通过线性回归外推法求得（拍摄至少 3 个不同极短曝光时间的暗场图像，外推至 t=0 的截距），消除残余暗电流影响

> ⚠️ **"极短曝光"定义的歧义性**
> 
> 不同型号相机对"极短曝光"的定义不同，有的可能是 10μs，有的是 100μs。如果用户 SCM 测试时的"极短"比实际采集的"极短"长，会导致黑电平扣除过多。
> 
> **解决方案**：使用外推法求截距，而非依赖单一极短曝光测量值。通过拍摄多个不同曝光时间的暗场图像，建立 `DN vs exposure_time` 的线性回归，取 t=0 时的 y-intercept 作为 `dark_offset_dn`。

### 26.3 节点位深自适应检查

- [ ] OB 节点的黑电平直接使用 SCM 值，无需位深缩放
- [ ] DPC 节点的噪声阈值直接使用 SCM 值，无需位深缩放
- [ ] Denoise 节点的参数直接使用，无需位深缩放
- [ ] Tone Mapping 节点的输入范围使用 Max_DN
- [ ] CCM/WB 节点的线性参数不受位深影响
- [ ] Tone Mapping 节点执行前，强制执行 `cv::threshold(img, img, Max_DN, Max_DN, cv::THRESH_TRUNC)`，确保输入值严格处于 [0, Max_DN] 区间内

> ⚠️ **Tone Mapping 节点的边界保护**
> 
> 如果传感器输出存在过饱和（即在某些像素达到 Max_DN 后仍有杂散光导致溢出到 0 或出现异常值），Tone Mapping 的输入区间可能会越界。
> 
> **解决方案**：在进入 Tone Mapping 节点前，执行阈值截断，确保后续计算不会出现不可预测的色彩扭曲。

### 26.4 C++ 模板分发检查

- [ ] 使用模板机制实现零成本类型分发
- [ ] 类型判断在循环外部进行
- [ ] OpenMP 并行化正确
- [ ] 支持 CV_8U 和 CV_16U 两种类型
- [ ] 内存连续性检查（`img.isContinuous()`）
- [ ] 使用行指针遍历，避免访问 Padding 内存
- [ ] **像素遍历强制采用行指针（Row-pointer）遍历，禁止假设图像为单块连续内存**
- [ ] 若图像非连续，在节点入口处统一执行 `if (!img.isContinuous()) img = img.clone();`

> ⚠️ **OpenCV 连续内存的隐患**
> 
> 工业相机 SDK（特别是 GigE Vision）输出的图像缓冲可能带有 stride (步长/填充)，导致 `img.isContinuous() == false`。
> 
> **硬性约束**：
> - 切勿假定 `rows * cols` 的循环方式在多线程下安全
> - 强制使用行指针遍历：`for (int r = 0; r < rows; ++r) { T* row_ptr = img.ptr<T>(r); ... }`
> - 或在节点入口处强制转换为连续内存

### 26.5 边界处理检查

- [ ] 像素值截断使用 saturate_cast 或手动 clamp
- [ ] 避免溢出和下溢
- [ ] 10/12-bit 数据不会被当作 16-bit 处理
- [ ] Tone Mapping 的线性映射使用正确的 Max_DN

### 26.6 LSC 节点优化检查

- [ ] PRNU 启用时直接使用原始 Gain Map，避免重复乘法
- [ ] LSC 和 PRNU 分离仅用于标定报告导出
- [ ] `original_gain_map_` 统一用于实际校正

### 26.7 暗信号定义检查

- [ ] `dark_offset_dn` 在极短曝光下测得（不含暗电流）
- [ ] `dark_current_e_per_sec` 通过多曝光拟合得到
- [ ] 动态 Offset 计算：`total_offset = dark_offset_dn + dark_current_dn`
- [ ] 防止双重扣除（`dark_offset_dn` 不含暗电流成分）

## 二十七、Pipeline 物理空间流转可视化

为了帮助团队成员更直观地理解 Pipeline 内部的物理空间流转，以下示意图展示了从传感器原始数据到最终 sRGB 输出的完整数据变换过程：

### 27.1 数据位深与色彩空间流转图

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                        Pipeline 物理空间流转示意图                                    │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  传感器原始输出                                                                      │
│  ┌───────────────────────────────────────────┐                                     │
│  │  Bayer Raw (单通道)                        │                                     │
│  │  位深: 8/10/12/16-bit                      │                                     │
│  │  物理空间: 传感器原始 DN 值                 │                                     │
│  │  范围: [0, Max_DN]                         │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  N0: OB Subtraction                        │                                     │
│  │  操作: pixel - total_offset                │                                     │
│  │  物理空间: 去除暗电流底噪                    │                                     │
│  │  输出: 线性 DN 值，范围可能变化              │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  N1: DPC (坏点校正)                         │                                     │
│  │  操作: 坏点检测与修复                       │                                     │
│  │  物理空间: 线性 DN 值                       │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  N2: LSC + PRNU                            │                                     │
│  │  操作: pixel * gain_map                    │                                     │
│  │  物理空间: 线性 DN 值，均匀化后              │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  N3: Denoise                               │                                     │
│  │  操作: 噪声滤波                            │                                     │
│  │  物理空间: 线性 DN 值                       │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  N4: Demosaicing + Chroma Denoise          │                                     │
│  │  操作: Bayer → RGB 插值 + 色度降噪           │                                     │
│  │  物理空间: 线性 RGB 值                      │                                     │
│  │  位深: 仍为原始位深                         │                                     │
│  │  通道: 1 → 3                               │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  N5: WhiteBalance                          │                                     │
│  │  操作: pixel * wb_gains                    │                                     │
│  │  物理空间: 线性 RGB 值                      │                                     │
│  │  色温校正后的白平衡                         │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  N6: CCM (色彩校正矩阵)                      │                                     │
│  │  操作: RGB' = CCM * RGB + offset           │                                     │
│  │  物理空间: 线性 RGB 值                      │                                     │
│  │  颜色空间转换（传感器 RGB → 标准 RGB）       │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  N7: Tone Mapping                          │                                     │
│  │  操作: THRESH_TRUNC + 动态范围压缩           │                                     │
│  │  输入: [0, Max_DN]                         │                                     │
│  │  输出: [0, 255]                            │                                     │
│  │  位深: 16-bit → 8-bit                      │                                     │
│  │  物理空间: 线性 RGB 值                      │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  N8: Gamma Correction                      │                                     │
│  │  操作: output = pow(input/255, 1/gamma)*255 │                                     │
│  │  物理空间: 非线性 sRGB 值                   │                                     │
│  │  位深: 8-bit                               │                                     │
│  └───────────────────────────────────────────┘                                     │
│                      │                                                             │
│                      ▼                                                             │
│  ┌───────────────────────────────────────────┐                                     │
│  │  最终输出                                   │                                     │
│  │  sRGB (8-bit, 3通道)                        │                                     │
│  │  物理空间: 显示器色彩空间                    │                                     │
│  │  范围: [0, 255]                             │                                     │
│  └───────────────────────────────────────────┘                                     │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### 27.2 物理空间与位深变化对照表

| 节点 | 输入空间 | 输出空间 | 位深变化 | 通道变化 | 关键物理操作 |
|------|----------|----------|----------|----------|--------------|
| N0 (OB) | 传感器原始 DN | 线性 DN（去底噪） | 不变 | 不变 | 减法（扣除暗信号） |
| N1 (DPC) | 线性 DN | 线性 DN（去坏点） | 不变 | 不变 | 坏点替换 |
| N2 (LSC) | 线性 DN | 线性 DN（均匀化） | 不变 | 不变 | 乘法（增益校正） |
| N3 (Denoise) | 线性 DN | 线性 DN（去噪） | 不变 | 不变 | 滤波 |
| N4 (Demosaic) | Bayer DN | 线性 RGB | 不变 | 1→3 | 插值 + 度降噪 |
| N5 (WB) | 线性 RGB | 线性 RGB（白平衡） | 不变 | 不变 | 乘法（WB增益） |
| N6 (CCM) | 线性 RGB | 线性 RGB（标准色） | 不变 | 不变 | 矩阵乘法 |
| N7 (TM) | 线性 RGB | 线性 RGB（压缩） | 16→8 | 不变 | 截断 + 线性缩放 |
| N8 (Gamma) | 纯线性 RGB | 非线性 sRGB | 不变 | 不变 | Gamma曲线 |

### 27.3 关键边界点说明

```
关键边界点：

1. N0 → N4：纯线性物理空间
   - 所有操作在传感器原始 DN 值空间进行
   - 数据保持物理意义，无非线性变换
   - 位深不变，范围可能调整

2. N4：通道数跃迁点（1 → 3）
   - Bayer 单通道 → RGB 三通道
   - 需要内存池介入，避免动态分配
   - 伪彩抑制确保色彩准确性

3. N4 → N6：线性 RGB 空间
   - WB 和 CCM 在线性空间执行
   - CCM 输入必须是已应用 WB 的 RGB
   - 参数直接使用，无需位深缩放

4. N7：位深跃迁点（16-bit → 8-bit）
   - 动态范围压缩
   - 必须使用 Max_DN 作为输入上限
   - THRESH_TRUNC 确保输入合法

5. N8：线性 → 非线性跃迁点
   - Gamma 校正引入非线性
   - 输出为显示器空间 sRGB
   - 此后不可再进行线性矩阵运算
```

## 二十八、工程落地细节（规避"隐性 Bug"）

### 28.1 内存访问安全（针对非连续内存）

> ⚠️ **CRITICAL CONSTRAINT**
> 
> **严禁在任何节点中使用 `rows * cols` 这种 1D 数组索引方式遍历图像**
> 
> 工业相机 SDK（特别是 GigE Vision）输出的图像缓冲可能带有 stride (步长/填充)，导致 `img.isContinuous() == false`。如果使用 1D 索引方式，会访问到无效的 Padding 内存。

**正确遍历方式**：

```cpp
// ✅ 正确：使用 cv::Mat::ptr<T>(r) 结合 img.step 或内部的 cols 进行 2D 遍历
for (int r = 0; r < img.rows; ++r) {
    T* row_ptr = img.ptr<T>(r);
    for (int c = 0; c < img.cols * img.channels(); ++c) {
        // 处理 row_ptr[c]
    }
}

// ❌ 错误：严禁使用 1D 数组索引方式
int total = img.rows * img.cols * img.channels();
T* data = img.ptr<T>();  // 可能指向非连续内存
for (int i = 0; i < total; ++i) {
    // data[i] 可能访问到无效的 Padding 内存
}
```

### 28.2 动态 Range 的"软钳位"策略

在 Tone Mapping & Gamma 节点前，添加一个**强制的数据完整性预检查**：

```cpp
// 进入 Tone Mapping 节点前的预检查
void PreToneMappingCheck(cv::Mat& img, int max_dn) {
    // 1. 统计异常像素数量
    int abnormal_count = 0;
    for (int r = 0; r < img.rows; ++r) {
        ushort* row_ptr = img.ptr<ushort>(r);
        for (int c = 0; c < img.cols * img.channels(); ++c) {
            if (row_ptr[c] > max_dn) {
                abnormal_count++;
            }
        }
    }

    // 2. 如果异常像素超过阈值（如 0.1%），输出警告日志
    double abnormal_ratio = abnormal_count / (img.rows * img.cols * img.channels());
    if (abnormal_ratio > 0.001) {
        LOG_WARN("Pre-ToneMapping: %.4f%% pixels exceed Max_DN (%d), ADC overflow detected",
                 abnormal_ratio * 100, max_dn);
    }

    // 3. 强制执行阈值截断
    cv::threshold(img, img, max_dn, max_dn, cv::THRESH_TRUNC);
}
```

**问题根源**：如果输入图像存在明显高于 Max_DN 的噪点（如 ADC 溢出产生），这些异常值会破坏 Gamma 的曲线映射，导致图像出现诡异的噪点色斑。

### 28.3 配置参数的"加载原子化"

> ⚠️ **ATOMIC LOADING CONSTRAINT**
> 
> **SCM, Color Char, Quick Calib 必须作为三个原子化的数据包加载**
> 
> 在 Pipeline 运行中，严禁只加载一半（例如只加载 WB 不加载 CCM），必须保证节点数据的逻辑一致性，否则会造成色彩空间在中间环节崩坏。

**原子化加载策略**：

```cpp
struct CalibrationDataPack {
    SCMResults scm;              // SCM 数据包（完整或空）
    ColorCharResults color_char; // Color Char 数据包（完整或空）
    QuickCalibResults quick_calib; // Quick Calib 数据包（完整或空）
    bool scm_loaded = false;
    bool color_char_loaded = false;
    bool quick_calib_loaded = false;
};

// 加载 Color Char 结果时，必须同时加载 WB 和 CCM
bool LoadColorCharResults(const std::string& path) {
    ColorCharResults results;
    bool wb_loaded = LoadWBResults(path, results);
    bool ccm_loaded = LoadCCMResults(path, results);
    
    // 只有 WB 和 CCM 都成功加载，才标记为已加载
    if (wb_loaded && ccm_loaded) {
        calibration_pack_.color_char = results;
        calibration_pack_.color_char_loaded = true;
        return true;
    } else {
        LOG_ERROR("Color Char data incomplete: WB=%d, CCM=%d, rejecting partial load",
                  wb_loaded, ccm_loaded);
        return false;
    }
}
```

**原子化检查清单**：

| 数据包 | 必须包含的数据 | 部分加载后果 |
|--------|---------------|--------------|
| SCM | dark_offset_dn, dark_current, dark_noise, full_well | OB 扣除错误，噪声参数缺失 |
| Color Char | wb_gains (R/G/B) + ccm_matrix (3x3) | WB 后色彩偏移，CCM 无法应用 |
| Quick Calib | wb_gains + ccm_matrix + gamma | 同 Color Char，Gamma 缺失 |

## 二十九、调试与可观测性（针对"黑盒调试"）

工业相机标定最难的是"定位问题在哪一个节点"。目前的文档主要关注流转，缺乏诊断逻辑。

### 29.1 断点调试模式接口定义

在 `ImagePipeline` 的 `process` 方法中，增加一个 **Debug Mode** 开关：

```cpp
struct DebugConfig {
    bool enabled = false;
    bool save_intermediate_images = true;
    bool save_statistics = true;
    std::string output_dir = "./debug_output/";
    std::vector<int> break_points;  // 在哪些节点后暂停
};

class ImagePipeline {
private:
    DebugConfig debug_config_;

public:
    void SetDebugMode(const DebugConfig& config) {
        debug_config_ = config;
    }

    cv::Mat Process(const cv::Mat& input) {
        cv::Mat current = input;
        
        for (int i = 0; i < nodes_.size(); ++i) {
            current = nodes_[i]->Process(current);
            
            if (debug_config_.enabled) {
                // 导出中间图像
                if (debug_config_.save_intermediate_images) {
                    std::string filename = debug_config_.output_dir + 
                                           "N" + std::to_string(i) + "_" + 
                                           nodes_[i]->GetName() + "_output.tiff";
                    cv::imwrite(filename, current);
                }
                
                // 导出统计信息
                if (debug_config_.save_statistics) {
                    double min_val, max_val, mean_val;
                    cv::minMaxLoc(current, &min_val, &max_val);
                    mean_val = cv::mean(current)[0];
                    LOG_DEBUG("N%d %s: min=%.2f, max=%.2f, mean=%.2f",
                              i, nodes_[i]->GetName().c_str(),
                              min_val, max_val, mean_val);
                }
            }
        }
        
        return current;
    }
};
```

**导出的中间图像命名规则**：

| 文件名 | 对应节点 | 用途 |
|--------|----------|------|
| `N0_OB_output.tiff` | N0 (OB) | 检查暗信号扣除效果 |
| `N1_DPC_output.tiff` | N1 (DPC) | 检查坏点修复效果 |
| `N2_LSC_output.tiff` | N2 (LSC) | 检查平场校正效果 |
| `N3_Denoise_output.tiff` | N3 (Denoise) | 检查降噪效果 |
| `N4_Demosaic_output.tiff` | N4 (Demosaic) | 检查伪彩抑制效果 |
| `N5_WB_output.tiff` | N5 (WB) | 检查白平衡效果 |
| `N6_CCM_output.tiff` | N6 (CCM) | 检查色彩校正效果 |
| `N7_TM_output.tiff` | N7 (TM) | 检查动态范围压缩效果 |
| `N8_Gamma_output.tiff` | N8 (Gamma) | 最终输出 |

### 29.2 可视化数据流检查（线性 vs 非线性）

```
线性空间 vs 非线性空间流转图：

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                                                                                     │
│  【纯线性物理空间】                                                                  │
│                                                                                     │
│    N0(OB) ── N1(DPC) ── N2(LSC) ── N3(Denoise) ── N4(Demosaic)                      │
│       │        │         │           │               │                              │
│       └────────┴─────────┴───────────┴───────────────┘                              │
│                           │                                                         │
│                           ▼                                                         │
│                    【线性 RGB 空间】                                                 │
│                           │                                                         │
│                    N5(WB) ── N6(CCM) ── N7(Tone Mapping)                            │
│                       │         │              │                                     │
│                       └─────────┴──────────────┘                                     │
│                                   │                                                 │
│                                   ▼                                                 │
│                          【非线性 sRGB 空间】                                        │
│                                   │                                                 │
│                              N8(Gamma)                                              │
│                                   │                                                 │
│                                   ▼                                                 │
│                             最终输出                                                 │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

关键规则：
- 纯线性空间（N0-N4）：所有运算保持物理线性关系，矩阵运算安全
- 线性 RGB 空间（N5-N7）：WB 和 CCM 必须在 Gamma 之前执行
- 非线性 sRGB 空间（N8）：此后不可再进行线性矩阵运算

诊断提示：
- 如果 N6_CCM_output.tiff 出现色彩偏移，检查 N5_WB_output.tiff 的白平衡效果
- 如果 N8_Gamma_output.tiff 出现暗部色斑，检查 N7_TM_output.tiff 是否存在溢出
```

## 三十、测试与验收指标（补充量化标准）

文档目前侧重于流程，对"标定是否成功"缺乏明确的量化验收逻辑。以下是各节点的验收指标：

### 30.1 OB（N0）验收指标

**验收标准**：扣除后，暗场区域的平均值应在 $0 \pm 1 \text{ DN}$ 范围内。

```cpp
bool ValidateOBNode(const cv::Mat& ob_output, const cv::Rect& dark_region) {
    cv::Mat dark_area = ob_output(dark_region);
    double mean = cv::mean(dark_area)[0];
    
    // 验收标准：平均值在 0 ± 1 DN 范围内
    bool passed = (mean >= -1.0 && mean <= 1.0);
    
    if (!passed) {
        LOG_WARN("OB validation failed: dark region mean = %.2f DN (expected 0 ± 1)", mean);
    }
    
    return passed;
}
```

### 30.2 DPC（N1）验收指标

**验收标准**：检测后的坏点区域像素值，应与周围 8 邻域像素差值在 $3\sigma$（基于 scm.dark_noise_dn）以内。

```cpp
bool ValidateDPCNode(const cv::Mat& dpc_output, const std::vector<cv::Point>& bad_pixels,
                     double dark_noise_dn) {
    int failed_count = 0;
    
    for (const auto& pt : bad_pixels) {
        // 获取周围 8 邻域像素值
        std::vector<double> neighbors;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dy == 0 && dx == 0) continue;
                int ny = pt.y + dy;
                int nx = pt.x + dx;
                if (ny >= 0 && ny < dpc_output.rows && nx >= 0 && nx < dpc_output.cols) {
                    neighbors.push_back(dpc_output.at<ushort>(ny, nx));
                }
            }
        }
        
        // 计算邻域均值和标准差
        double neighbor_mean = cv::mean(neighbors)[0];
        double pixel_val = dpc_output.at<ushort>(pt.y, pt.x);
        double diff = std::abs(pixel_val - neighbor_mean);
        
        // 验收标准：差值在 3σ 以内
        double threshold = 3.0 * dark_noise_dn;
        if (diff > threshold) {
            failed_count++;
            LOG_WARN("DPC validation failed at (%d,%d): diff=%.2f DN, threshold=%.2f DN",
                     pt.x, pt.y, diff, threshold);
        }
    }
    
    return (failed_count == 0);
}
```

### 30.3 WB（N5）验收指标

**验收标准**：使用 24 色卡中性灰区域，校正后 R/G/B 通道比值应趋近于 $1:1:1$。

```cpp
bool ValidateWBNode(const cv::Mat& wb_output, const cv::Rect& neutral_gray_region) {
    cv::Mat gray_area = wb_output(neutral_gray_region);
    std::vector<cv::Mat> channels(3);
    cv::split(gray_area, channels);
    
    double r_mean = cv::mean(channels[0])[0];
    double g_mean = cv::mean(channels[1])[0];
    double b_mean = cv::mean(channels[2])[0];
    
    // 计算 R/G/B 比值
    double r_g_ratio = r_mean / g_mean;
    double b_g_ratio = b_mean / g_mean;
    
    // 验收标准：比值趋近于 1（容差 ±5%）
    bool passed = (r_g_ratio >= 0.95 && r_g_ratio <= 1.05 &&
                   b_g_ratio >= 0.95 && b_g_ratio <= 1.05);
    
    if (!passed) {
        LOG_WARN("WB validation failed: R/G=%.4f, B/G=%.4f (expected 1.0 ± 0.05)",
                 r_g_ratio, b_g_ratio);
    }
    
    return passed;
}
```

### 30.4 CCM（N6）验收指标

**验收标准**：使用 24 色卡所有色块，校正后与参考值的色差（ΔE）平均值应小于 5。

```cpp
bool ValidateCCMNode(const cv::Mat& ccm_output, const std::vector<cv::Rect>& color_patches,
                     const std::vector<cv::Vec3d>& reference_colors) {
    double total_delta_e = 0.0;
    
    for (size_t i = 0; i < color_patches.size(); ++i) {
        cv::Mat patch = ccm_output(color_patches[i]);
        cv::Vec3d measured = cv::mean(patch);
        cv::Vec3d reference = reference_colors[i];
        
        // 计算 ΔE（简化版，使用 RGB 空间欧氏距离）
        double delta_e = cv::norm(measured - reference);
        total_delta_e += delta_e;
    }
    
    double avg_delta_e = total_delta_e / color_patches.size();
    
    // 验收标准：平均 ΔE < 5
    bool passed = (avg_delta_e < 5.0);
    
    if (!passed) {
        LOG_WARN("CCM validation failed: avg ΔE=%.2f (expected < 5)", avg_delta_e);
    }
    
    return passed;
}
```

### 30.5 Gamma（N8）验收指标

**验收标准**：最终输出图像的动态范围应覆盖 [0, 255]，无明显截断。

```cpp
bool ValidateGammaNode(const cv::Mat& gamma_output) {
    double min_val, max_val;
    cv::minMaxLoc(gamma_output, &min_val, &max_val);
    
    // 统计像素分布
    cv::Mat hist;
    int hist_size = 256;
    float range[] = {0, 256};
    const float* hist_range = {range};
    cv::calcHist(&gamma_output, 1, 0, cv::Mat(), hist, 1, &hist_size, &hist_range);
    
    // 检查低端和高端是否被充分利用
    double low_usage = hist.at<float>(0) + hist.at<float>(1) + hist.at<float>(2);
    double high_usage = hist.at<float>(253) + hist.at<float>(254) + hist.at<float>(255);
    double total_pixels = gamma_output.rows * gamma_output.cols * gamma_output.channels();
    
    // 验收标准：低端和高端均有像素分布（至少 0.1%）
    bool passed = (low_usage / total_pixels > 0.001 && 
                   high_usage / total_pixels > 0.001);
    
    if (!passed) {
        LOG_WARN("Gamma validation failed: min=%.0f, max=%.0f, dynamic range not fully used",
                 min_val, max_val);
    }
    
    return passed;
}
```

### 30.6 验收检查清单汇总

| 节点 | 验收指标 | 容差范围 | 失败后果 |
|------|----------|----------|----------|
| N0 (OB) | 暗场区域平均值 | $0 \pm 1 \text{ DN}$ | 暗部细节丢失或偏亮 |
| N1 (DPC) | 坏点区域与邻域差值 | $< 3\sigma$ | 坏点残留或过度修复 |
| N2 (LSC) | 边缘与中心亮度比值 | $0.95 \sim 1.05$ | 边缘偏暗或偏亮 |
| N3 (Denoise) | 信噪比 (SNR) 提升 | $> 3\text{dB}$ | 噪声残留或过度平滑 |
| N4 (Demosaic) | 伪彩抑制效果 | 目视无伪彩 | 边缘伪彩干扰 |
| N5 (WB) | 中性灰 R/G/B 比值 | $1.0 \pm 0.05$ | 色温偏移 |
| N6 (CCM) | 平均 ΔE | $< 5$ | 色彩失真 |
| N7 (TM) | 动态范围覆盖 | $> 90\%$ | 高光或暗部截断 |
| N8 (Gamma) | 输出范围覆盖 | $[0, 255]$ 全覆盖 | 对比度异常 |

## 三十一、实施优先级与分阶段开发建议

### 31.1 分阶段开发路线图

```
阶段 1：基础路径实现（最高优先级）
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                                                                                     │
│  目标：确保最底层的线性运算（OB 扣除、坏点校正、位深映射）完全跑通                       │
│                                                                                     │
│  实现节点：                                                                          │
│    N0 (OB) ────────> N1 (DPC) ────────> N7 (Tone Mapping) ────────> N8 (Gamma)      │
│       │                 │                      │                    │               │
│       └─────────────────┴──────────────────────┴────────────────────┘               │
│                                                                                     │
│  验收标准：                                                                          │
│    1. OB 扣除后暗场区域平均值 = 0 ± 1 DN                                             │
│    2. DPC 坏点修复后无明显异常像素                                                    │
│    3. Tone Mapping 输出范围覆盖 [0, 255]                                             │
│    4. Gamma 输出无明显截断                                                           │
│                                                                                     │
│  开发周期：1-2 周                                                                    │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

阶段 2：平场与降噪（中优先级）
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                                                                                     │
│  目标：实现均匀化与噪声抑制                                                          │
│                                                                                     │
│  实现节点：                                                                          │
│    N2 (LSC + PRNU) ────────> N3 (Denoise)                                           │
│       │                       │                                                     │
│       └───────────────────────┘                                                     │
│                                                                                     │
│  验收标准：                                                                          │
│    1. LSC 校正后边缘与中心亮度比值 = 1.0 ± 0.05                                      │
│    2. Denoise 后信噪比提升 > 3dB                                                    │
│                                                                                     │
│  开发周期：1 周                                                                      │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

阶段 3：色彩校正（高复杂度）
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                                                                                     │
│  目标：实现去马赛克与色彩校正                                                        │
│                                                                                     │
│  实现节点：                                                                          │
│    N4 (Demosaic) ────────> N5 (WB) ────────> N6 (CCM)                               │
│       │                    │                   │                                    │
│       └────────────────────┴───────────────────┘                                    │
│                                                                                     │
│  验收标准：                                                                          │
│    1. Demosaic 后无伪彩和拉链效应                                                    │
│    2. WB 后中性灰 R/G/B = 1:1:1                                                     │
│    3. CCM 后平均 ΔE < 5                                                             │
│                                                                                     │
│  开发周期：2-3 周                                                                    │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### 31.2 分阶段验收检查清单

**阶段 1 检查清单**：

- [ ] OB 节点实现完成，支持标量扣除和 2D 暗场扣除
- [ ] DPC 节点实现完成，支持动态阈值检测
- [ ] Tone Mapping 节点实现完成，使用 Max_DN 钳位
- [ ] Gamma 节点实现完成，支持标准 Gamma 2.2
- [ ] 基础路径验收通过（OB + DPC + TM + Gamma）
- [ ] Debug Mode 开关实现，可导出中间图像

**阶段 2 检查清单**：

- [ ] LSC 节点实现完成，支持像素级 Gain Map
- [ ] PRNU 校正逻辑实现完成
- [ ] Denoise 节点实现完成，支持参数自适应
- [ ] 平场与降噪验收通过（LSC + Denoise）
- [ ] 内存池机制验证，无动态分配

**阶段 3 检查清单**：

- [ ] Demosaic 节点实现完成，支持 EA 算法
- [ ] Chroma Denoise 逻辑实现完成
- [ ] WB 节点实现完成，支持动态增益
- [ ] CCM 节点实现完成，支持矩阵校验
- [ ] 色彩校正验收通过（Demosaic + WB + CCM）
- [ ] 24 色卡测试 ΔE < 5

### 31.3 完整 Pipeline 集成测试

当三个阶段全部完成后，执行完整 Pipeline 集成测试：

```cpp
bool ValidateFullPipeline(const cv::Mat& input, const cv::Mat& output) {
    // 1. 基础验证
    bool ob_passed = ValidateOBNode(N0_output, dark_region_);
    bool dpc_passed = ValidateDPCNode(N1_output, bad_pixels_, scm_.dark_noise_dn);
    bool gamma_passed = ValidateGammaNode(output);
    
    // 2. 色彩验证（如果启用）
    bool wb_passed = true, ccm_passed = true;
    if (wb_enabled_) {
        wb_passed = ValidateWBNode(N5_output, neutral_gray_region_);
    }
    if (ccm_enabled_) {
        ccm_passed = ValidateCCMNode(N6_output, color_patches_, reference_colors_);
    }
    
    // 3. 综合验收
    bool all_passed = ob_passed && dpc_passed && gamma_passed && wb_passed && ccm_passed;
    
    if (all_passed) {
        LOG_INFO("Full Pipeline validation passed");
    } else {
        LOG_ERROR("Full Pipeline validation failed: OB=%d, DPC=%d, WB=%d, CCM=%d, Gamma=%d",
                  ob_passed, dpc_passed, wb_passed, ccm_passed, gamma_passed);
    }
    
    return all_passed;
}
```

## 三十二、软件测试体系（确保工业现场稳定运行）

引入一套结构化的软件测试体系是确保该 CameraToolKit 能在不同工业现场稳定运行的关键。针对 ISP 这种涉及大量数值运算和硬件依赖的系统，建议从以下四个维度构建测试体系。

### 32.1 单元测试 (Unit Testing)：数值精度验证

ISP 的每个节点（如 N0 OB, N6 CCM）本质上都是一个数学算子，单元测试的核心在于"计算结果的比特级精确性"。

#### 32.1.1 Golden Reference 验证

**建议**：编写一套测试工具，利用 Python (NumPy) 或 MATLAB 生成一组标准的 Raw 数据及预期输出（通过高精度浮点计算），将 C++ 节点的输出与其进行 AbsDiff 比较。

**指标要求**：

| 运算类型 | 允许误差 | 说明 |
|----------|----------|------|
| 12/16-bit 线性运算 | Difference = 0 | 结果必须完全一致 |
| 浮点计算节点（CCM） | Difference < 1 LSB | 允许极小的舍入误差 |
| Gamma 曲线 | Difference < 1 | 非线性运算允许误差 |

**测试工具示例**：

```python
# Python 测试脚本：生成 Golden Reference 并验证
import numpy as np
import cv2

def generate_golden_reference(input_raw, node_type, params):
    """使用 NumPy 高精度计算生成 Golden Reference"""
    if node_type == 'OB':
        # OB 扣除：线性运算，必须完全一致
        output = input_raw - params['total_offset']
        output = np.clip(output, 0, params['max_dn'])
        return output.astype(np.uint16)
    
    elif node_type == 'CCM':
        # CCM 矩阵乘法：允许极小舍入误差
        ccm_matrix = params['ccm_matrix']
        output = np.dot(input_raw.reshape(-1, 3), ccm_matrix.T)
        return output.astype(np.uint16)
    
    # ... 其他节点

def validate_node_output(cpp_output, golden_ref, node_type):
    """验证 C++ 输出与 Golden Reference 的差异"""
    diff = np.abs(cpp_output.astype(np.float64) - golden_ref.astype(np.float64))
    max_diff = np.max(diff)
    
    tolerance = {
        'OB': 0,           # 必须完全一致
        'DPC': 0,          # 必须完全一致
        'LSC': 0,          # 必须完全一致
        'Denoise': 1,      # 允许滤波差异
        'Demosaic': 1,     # 允许插值差异
        'WB': 0,           # 必须完全一致
        'CCM': 1,          # 允许舍入误差（1 LSB）
        'TM': 0,           # 必须完全一致
        'Gamma': 1         # 允许曲线误差
    }
    
    passed = max_diff <= tolerance[node_type]
    print(f"{node_type}: max_diff={max_diff}, tolerance={tolerance[node_type]}, passed={passed}")
    return passed
```

#### 32.1.2 边界值测试 (Boundary Value Testing)

必须强制覆盖输入数据的极端情况：

| 测试场景 | 输入值 | 预期输出 | 验证目标 |
|----------|--------|----------|----------|
| 全黑图像 | 全 0 | 全 0 | OB 扣除不产生负数 |
| 全白图像 | 全 Max_DN | 全 Max_DN | 无溢出截断 |
| 溢出图像 | Max_DN + 1 | Max_DN | THRESH_TRUNC 正确执行 |
| 负数回绕测试 | 0 - offset | 0 | 防止 UINT16 下溢 |
| 坏点边界 | 单像素异常 | 邻域均值 | DPC 正确修复 |

**目标**：验证在任何输入情况下，输出都不会出现"负数回绕"或"非法内存访问"。

### 32.2 集成测试 (Integration Testing)：流水线链路闭环

测试重点在于节点之间的数据流转是否破坏了线性关系。

#### 32.2.1 线性空间的一致性检查

**建议**：在 Pipeline 中插入"探测点"（Probe），通过开启 Debug 模式，导出 N0 到 N6 每一个中间节点的数据。

**校验规则**：

在线性空间节点（N0~N6），检查所有像素是否存在非法截断：

```cpp
bool CheckLinearSpaceConsistency(const cv::Mat& intermediate_output, int expected_bit_depth) {
    double min_val, max_val;
    cv::minMaxLoc(intermediate_output, &min_val, &max_val);
    
    int max_dn = (1 << expected_bit_depth) - 1;
    
    // 检查是否存在非法截断（如提前缩减到 8-bit）
    if (max_val > max_dn) {
        LOG_ERROR("Linear space violation: max_val=%.0f exceeds Max_DN=%d",
                  max_val, max_dn);
        return false;
    }
    
    // 检查是否存在非预期的负数（UINT16 下溢）
    if (min_val < 0) {
        LOG_ERROR("Linear space violation: min_val=%.0f is negative (UINT16 underflow)",
                  min_val);
        return false;
    }
    
    // 检查位深是否被意外缩减
    cv::Mat hist;
    int hist_size = (1 << expected_bit_depth);
    cv::calcHist(&intermediate_output, 1, 0, cv::Mat(), hist, 1, &hist_size, nullptr);
    
    // 如果高位完全没有像素分布，可能被提前截断
    double high_bit_usage = 0;
    for (int i = max_dn - 10; i <= max_dn; ++i) {
        high_bit_usage += hist.at<float>(i);
    }
    
    if (high_bit_usage == 0 && intermediate_output.rows * intermediate_output.cols > 100) {
        LOG_WARN("Potential bit-depth truncation: high bits (Max_DN-10 ~ Max_DN) have no pixels");
    }
    
    return true;
}
```

**关键检查点**：

任何提前发生的非线性失真都会被后续 CCM 矩阵成倍放大。

#### 32.2.2 配置项一致性测试 (Configuration Consistency)

**场景**：切换不同的标定配置文件（如从 Quick Calib 切换回 SCM 默认）。

**校验**：确保 Pipeline 能够即时清除旧的缓存增益矩阵和偏差值，不会出现"新参数与旧残留数据混合"导致图像偏色的情况。

```cpp
bool TestConfigurationConsistency() {
    // 1. 加载 Quick Calib 配置
    LoadCalibrationResults("quick_calib.toml");
    cv::Mat output1 = pipeline.Process(test_image);
    
    // 2. 切换到 SCM 默认配置（清空 WB/CCM）
    LoadCalibrationResults("scm_default.toml");
    cv::Mat output2 = pipeline.Process(test_image);
    
    // 3. 检查输出差异
    double diff = cv::norm(output1, output2, cv::NORM_L2);
    
    // 如果差异太小，可能旧配置未清除
    if (diff < threshold) {
        LOG_ERROR("Configuration consistency test failed: outputs too similar after config switch");
        return false;
    }
    
    // 4. 验证中性灰区域（WB 应该不再生效）
    cv::Rect neutral_gray = GetNeutralGrayRegion();
    double r_mean = cv::mean(output2(neutral_gray))[0];
    double g_mean = cv::mean(output2(neutral_gray))[1];
    double b_mean = cv::mean(output2(neutral_gray))[2];
    
    // 如果 R/G/B 比值仍然接近 1:1:1，说明 WB 参数未清除
    double r_g_ratio = r_mean / g_mean;
    if (std::abs(r_g_ratio - 1.0) < 0.05) {
        LOG_ERROR("WB parameters not cleared after config switch");
        return false;
    }
    
    return true;
}
```

### 32.3 鲁棒性与压力测试 (Robustness & Stress Testing)

针对工业视觉环境的高并发、长时间运行特点进行压力验证。

#### 32.3.1 内存泄漏与碎片测试 (Memory Leak Detection)

**工具建议**：

| 平台 | 工具 | 使用方式 |
|------|------|----------|
| Linux | Valgrind | `valgrind --leak-check=full ./camera_toolkit_test` |
| Windows | Visual Studio Memory Profiler | 调试 → 性能探查器 → 内存使用 |
| Cross-platform | AddressSanitizer | 编译选项 `-fsanitize=address` |

**检查点**：

在 Pipeline 循环处理 10,000 帧图像后，进程的内存占用应处于稳定水平（平稳状态），严禁出现随帧数线性增长。

```cpp
void TestMemoryLeak() {
    size_t initial_memory = GetProcessMemoryUsage();
    
    cv::Mat test_image = cv::imread("test_raw.tiff", cv::IMREAD_UNCHANGED);
    
    for (int i = 0; i < 10000; ++i) {
        cv::Mat output = pipeline.Process(test_image);
        // 不要保存 output，让它自然释放
    }
    
    size_t final_memory = GetProcessMemoryUsage();
    size_t memory_growth = final_memory - initial_memory;
    
    // 允许小量增长（如缓存初始化），但不能超过 10MB
    if (memory_growth > 10 * 1024 * 1024) {
        LOG_ERROR("Memory leak detected: growth=%.2f MB after 10000 frames",
                  memory_growth / (1024.0 * 1024.0));
    } else {
        LOG_INFO("Memory leak test passed: growth=%.2f KB", memory_growth / 1024.0);
    }
}
```

#### 32.3.2 多线程竞态测试 (Thread Sanitizer)

工业相机往往在独立的采集线程中工作，ISP 可能在 Worker 线程中处理。

**测试建议**：使用 C++ 的 ThreadSanitizer (TSan) 进行运行期检查，确保当 UI 线程修改曝光参数时，Worker 线程读取该参数的瞬间不会触发竞态访问。

```cpp
// 编译选项：-fsanitize=thread

void TestThreadRaceCondition() {
    std::atomic<bool> running{true};
    std::atomic<int> frame_count{0};
    
    // UI 线程：动态修改曝光参数
    std::thread ui_thread([&]() {
        for (int i = 0; i < 1000; ++i) {
            double exposure = 1000.0 + i * 10.0;
            pipeline.SetExposureTime(exposure);  // 修改参数
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Worker 线程：处理图像
    std::thread worker_thread([&]() {
        cv::Mat test_image = cv::imread("test_raw.tiff", cv::IMREAD_UNCHANGED);
        while (running) {
            cv::Mat output = pipeline.Process(test_image);
            frame_count++;
        }
    });
    
    // 运行 10 秒后停止
    std::this_thread::sleep_for(std::chrono::seconds(10));
    running = false;
    
    ui_thread.join();
    worker_thread.join();
    
    LOG_INFO("Thread race test completed: %d frames processed", frame_count.load());
}
```

**竞态检测重点**：

| 场景 | 竞态风险 | 解决方案 |
|------|----------|----------|
| UI 修改曝光时间 | Worker 正在读取 | 使用 `std::atomic` 或 Mutex |
| UI 加载新配置 | Worker 正在使用旧配置 | 配置加载时暂停 Worker |
| UI 启用/禁用节点 | Worker 正在执行 Pipeline | 使用节点启用状态的原子标志 |

### 32.4 性能基准测试 (Benchmarking)

ISP 的处理速度决定了相机预览的帧率（FPS）。

#### 32.4.1 分节点耗时统计 (Node Latency Profiling)

**建议**：在 `ImagePipeline::process()` 中使用高精度计时器（如 `std::chrono::high_resolution_clock`）测量每个节点的耗时。

```cpp
struct NodeLatency {
    std::string name;
    double elapsed_ms;
    double percentage;
};

std::vector<NodeLatency> ProfilePipelineLatency(const cv::Mat& input) {
    std::vector<NodeLatency> latencies;
    double total_time = 0.0;
    
    cv::Mat current = input;
    
    for (int i = 0; i < nodes_.size(); ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        current = nodes_[i]->Process(current);
        auto end = std::chrono::high_resolution_clock::now();
        
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        total_time += elapsed_ms;
        
        latencies.push_back({nodes_[i]->GetName(), elapsed_ms, 0.0});
    }
    
    // 计算百分比
    for (auto& latency : latencies) {
        latency.percentage = latency.elapsed_ms / total_time * 100.0;
    }
    
    return latencies;
}
```

**可视化展示**：

在界面显示各节点的"消耗占比"：

```
┌─────────────────────────────────────────────────────────────┐
│  Pipeline Performance Profile                                │
├─────────────────────────────────────────────────────────────┤
│  Node               │ Time (ms) │ Percentage │ Optimization │
│  ──────────────────┼───────────┼────────────┼────────────── │
│  N0 OB              │   2.1     │    3.5%    │   -          │
│  N1 DPC             │   1.5     │    2.5%    │   -          │
│  N2 LSC             │   3.2     │    5.3%    │   -          │
│  N3 Denoise         │   8.4     │   14.0%    │   Medium     │
│  N4 Demosaic ⚠️     │  35.2     │   58.6%    │   HIGH ⚡     │
│  N5 WB              │   1.8     │    3.0%    │   -          │
│  N6 CCM             │   4.5     │    7.5%    │   Low        │
│  N7 TM              │   2.0     │    3.3%    │   -          │
│  N8 Gamma           │   1.3     │    2.2%    │   -          │
│  ──────────────────┼───────────┼────────────┼────────────── │
│  Total              │  60.0     │  100.0%    │   16.7 FPS   │
└─────────────────────────────────────────────────────────────┘

⚠️ Warning: Demosaicing node consumes 58.6% of total time!
⚡ Suggestion: Optimize with AVX2 SIMD or EA algorithm.
```

#### 32.4.2 SIMD 性能回归测试

确保优化（如 AVX/OpenMP）在不同 CPU 环境下都能生效，而不是在某些老旧 CPU 上反而变慢。

```cpp
void TestSIMDPerformanceRegression() {
    std::vector<std::string> cpu_types = {
        "Intel Core i7-10700K",  // 支持 AVX2
        "Intel Core i5-8400",    // 支持 AVX
        "Intel Core i3-6100",    // 仅支持 SSE4.2
        "AMD Ryzen 5 3600"       // 支持 AVX2
    };
    
    // 对于每个 CPU 类型，记录 SIMD 优化前后的性能
    std::map<std::string, double> baseline_fps;
    std::map<std::string, double> optimized_fps;
    
    for (const auto& cpu : cpu_types) {
        // 模拟不同 CPU 环境（实际测试中应使用真实硬件）
        double baseline = MeasureFPSWithoutSIMD();
        double optimized = MeasureFPSWithSIMD();
        
        baseline_fps[cpu] = baseline;
        optimized_fps[cpu] = optimized;
        
        double improvement = (optimized - baseline) / baseline * 100.0;
        
        // 如果优化后性能反而下降，记录警告
        if (improvement < 0) {
            LOG_WARN("SIMD regression on %s: %.2f%% slower", cpu.c_str(), -improvement);
        } else {
            LOG_INFO("SIMD improvement on %s: %.2f%% faster", cpu.c_str(), improvement);
        }
    }
}
```

### 32.5 自动化回归测试架构建议

建议在项目代码库中建立以下自动测试结构：

```
建议的测试目录结构：

tests/
├── unit/                 # 针对单个 Node 的数值计算测试 (使用 GoogleTest)
│   ├── ob_node_test.cpp          # OB 节点测试
│   ├── dpc_node_test.cpp         # DPC 节点测试
│   ├── lsc_node_test.cpp         # LSC 节点测试
│   ├── denoise_node_test.cpp     # Denoise 节点测试
│   ├── demosaic_node_test.cpp    # Demosaic 节点测试
│   ├── wb_node_test.cpp          # WB 节点测试
│   ├── ccm_node_test.cpp         # CCM 节点测试
│   ├── tm_node_test.cpp          # Tone Mapping 节点测试
│   └── gamma_node_test.cpp       # Gamma 节点测试
│   └── golden_reference_test.cpp # Golden Reference 验证
│   └── boundary_test.cpp         # 边界值测试
│
├── integration/          # 模拟全链路数据输入，验证 N0-N7 逻辑 (使用 Python 驱动)
│   ├── pipeline_integration_test.py   # 全链路集成测试
│   ├── linear_space_test.py           # 线性空间一致性测试
│   ├── config_consistency_test.py     # 配置项一致性测试
│   └── debug_mode_test.py             # Debug 模式测试
│
├── robustness/           # 鲁棒性与压力测试
│   ├── memory_leak_test.cpp           # 内存泄漏测试
│   ├── thread_race_test.cpp           # 多线程竞态测试
│   ├── stress_test.cpp                # 10000 帧压力测试
│   └── abnormal_input_test.cpp        # 异常输入测试
│
├── benchmark/            # 性能基准测试
│   ├── latency_profile_test.cpp       # 分节点耗时统计
│   ├── fps_benchmark_test.cpp         # FPS 基准测试
│   ├── simd_regression_test.cpp       # SIMD 性能回归测试
│   └── resolution_scaling_test.cpp    # 不同分辨率性能测试
│
├── data/                 # 预置的测试数据
│   ├── raw_images/                    # Raw 测试图像
│   │   ├── dark_frame.tiff           # 暗场图像
│   │   ├── bright_frame.tiff         # 亮场图像
│   │   ├── color_chart.tiff          # 色卡图像
│   │   ├── edge_test.tiff            # 边缘测试图像
│   │   └── extreme_dark.tiff         # 极暗图像
│   │   └── extreme_bright.tiff       # 极亮图像
│   │   └── overflow_test.tiff        # 溢出测试图像
│   │
│   ├── golden_references/             # Golden Reference 输出
│   │   ├── ob_output_ref.npy
│   │   ├── dpc_output_ref.npy
│   │   ├── lsc_output_ref.npy
│   │   ├── wb_output_ref.npy
│   │   ├── ccm_output_ref.npy
│   │   └── final_output_ref.npy
│   │
│   └── calibration_data/              # 标定数据
│   │   ├── scm_results.toml
│   │   ├── color_char_results.toml
│   │   └── quick_calib_results.toml
│
└── scripts/              # 测试脚本
    ├── run_all_tests.py              # 运行所有测试
    ├── generate_golden_ref.py        # 生成 Golden Reference
    ├── compare_outputs.py            # 对比输出差异
    └── visualize_profile.py          # 可视化性能 Profile
```

### 32.6 ISP 质量评估流程图

为了帮助团队理解如何从测试的角度评估图像处理的质量，以下是 ISP 质量评估流程图：

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           ISP 质量评估流程图                                          │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                        1. 单元测试 (Unit Testing)                             │   │
│  │                                                                               │   │
│  │   输入：标准 Raw 数据 + Golden Reference                                       │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   执行：每个 Node 的数值计算                                                    │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   验证：AbsDiff < tolerance (OB: 0, CCM: 1 LSB)                                │   │
│  │         │                                                                     │   │
│  │         ├─> Pass ──> 进入集成测试                                              │   │
│  │         └─> Fail ──> 定位算子错误，修复代码                                      │   │
│  │                                                                               │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                      │                                              │
│                                      ▼                                              │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                        2. 集成测试 (Integration Testing)                      │   │
│  │                                                                               │   │
│  │   输入：全链路 Pipeline + Debug Mode                                           │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   执行：N0 → N1 → ... → N8                                                     │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   验证：                                                                       │   │
│  │     ├─> 线性空间一致性（N0-N6 无截断）                                           │   │
│  │     ├─> 配置项一致性（切换配置无残留）                                            │   │
│  │     └─> 中间节点数据完整性                                                       │   │
│  │         │                                                                     │   │
│  │         ├─> Pass ──> 进入压力测试                                              │   │
│  │         └─> Fail ──> 定位节点间数据流转问题                                      │   │
│  │                                                                               │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                      │                                              │
│                                      ▼                                              │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                        3. 鲁棒性测试 (Robustness Testing)                     │   │
│  │                                                                               │   │
│  │   输入：10000 帧循环 + 多线程模拟                                               │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   执行：                                                                       │   │
│  │     ├─> 内存泄漏检测（Valgrind/VS Profiler）                                    │   │
│  │     ├─> 线程竞态检测（ThreadSanitizer）                                         │   │
│  │     └─> 异常输入测试（极端值）                                                   │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   验证：                                                                       │   │
│  │     ├─> 内存增长 < 10MB                                                         │   │
│  │     ├─> 无竞态警告                                                              │   │
│  │     └─> 异常输入不崩溃                                                          │   │
│  │         │                                                                     │   │
│  │         ├─> Pass ──> 进入性能测试                                              │   │
│  │         └─> Fail ──> 定位内存/线程问题                                          │   │
│  │                                                                               │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                      │                                              │
│                                      ▼                                              │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                        4. 性能测试 (Benchmarking)                             │   │
│  │                                                                               │   │
│  │   输入：不同分辨率、位深的测试图像                                               │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   执行：                                                                       │   │
│  │     ├─> 分节点耗时 Profile                                                     │   │
│  │     ├─> FPS 基准测试                                                           │   │
│  │     └─> SIMD 性能回归测试                                                      │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   验证：                                                                       │   │
│  │     ├─> 总耗时满足 FPS 要求（如 > 30 FPS）                                      │   │
│  │     ├─> SIMD 优化生效（无性能回归）                                             │   │
│  │     └─> 热点节点优化（如 Demosaic < 50ms）                                      │   │
│  │         │                                                                     │   │
│  │         ├─> Pass ──> 进入验收测试                                              │   │
│  │         └─> Fail ──> 优化热点节点                                               │   │
│  │                                                                               │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                      │                                              │
│                                      ▼                                              │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                        5. 验收测试 (Acceptance Testing)                       │   │
│  │                                                                               │   │
│  │   输入：真实工业场景图像 + 24 色卡                                               │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   执行：                                                                       │   │
│  │     ├─> OB 验收：暗场平均值 0 ± 1 DN                                            │   │
│  │     ├─> DPC 验收：坏点修复 < 3σ                                                │   │
│  │     ├─> WB 验收：中性灰 R/G/B = 1:1:1                                          │   │
│  │     ├─> CCM 验收：ΔE < 5                                                       │   │
│  │     └─> Gamma 验收：动态范围全覆盖                                              │   │
│  │         │                                                                     │   │
│  │         ▼                                                                     │   │
│  │   验证：                                                                       │   │
│  │     ├─> 所有验收指标通过                                                        │   │
│  │     └─> PSNR/SSIM 与 Golden Image 对比                                         │   │
│  │         │                                                                     │   │
│  │         ├─> Pass ──> 发布版本 ✓                                                │   │
│  │         └─> Fail ──> 回退优化，重新测试                                         │   │
│  │                                                                               │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### 32.7 自动化对比工具：Golden Image 验证

工业软件的"测试"往往不仅仅是代码逻辑，还包括对图像真实性的测试。建议保留一个"自动化对比工具"，它能够自动将当前版本 Pipeline 输出的图片与"黄金参考图"（Golden Image）进行比对。

```python
import cv2
import numpy as np

def compare_with_golden_image(current_output, golden_image_path):
    """使用 PSNR 和 SSIM 指标对比当前输出与 Golden Image"""
    
    golden = cv2.imread(golden_image_path, cv2.IMREAD_UNCHANGED)
    
    # 确保 dtype 和 shape 一致
    if current_output.dtype != golden.dtype:
        current_output = current_output.astype(golden.dtype)
    
    # 计算 PSNR (Peak Signal-to-Noise Ratio)
    psnr = cv2.PSNR(current_output, golden)
    
    # 计算 SSIM (Structural Similarity Index)
    # 需要转换为 float 类型
    current_float = current_output.astype(np.float32)
    golden_float = golden.astype(np.float32)
    
    # 对于多通道图像，分别计算每个通道的 SSIM
    if current_output.shape[2] == 3:
        ssim_channels = []
        for i in range(3):
            C1 = current_float[:, :, i]
            C2 = golden_float[:, :, i]
            ssim = calculate_ssim(C1, C2)
            ssim_channels.append(ssim)
        ssim = np.mean(ssim_channels)
    else:
        ssim = calculate_ssim(current_float, golden_float)
    
    # 判断结果
    result = {
        'psnr': psnr,
        'ssim': ssim,
        'passed': True,
        'warnings': []
    }
    
    # PSNR 验收标准：> 30 dB（高质量）
    if psnr < 30:
        result['warnings'].append(f"PSNR too low: {psnr:.2f} dB (expected > 30 dB)")
        result['passed'] = False
    
    # SSIM 验收标准：> 0.95（结构相似度高）
    if ssim < 0.95:
        result['warnings'].append(f"SSIM too low: {ssim:.4f} (expected > 0.95)")
        result['passed'] = False
    
    # 如果有任何警告，触发警报
    if not result['passed']:
        print("⚠️ ALARM: Image quality degradation detected!")
        for warning in result['warnings']:
            print(f"   - {warning}")
        print("   Suggestion: Review recent code changes (especially CCM optimization)")
    
    return result

def calculate_ssim(img1, img2, window_size=11):
    """计算单通道 SSIM"""
    C1 = (0.01 * 255) ** 2
    C2 = (0.03 * 255) ** 2
    
    mu1 = cv2.GaussianBlur(img1, (window_size, window_size), 1.5)
    mu2 = cv2.GaussianBlur(img2, (window_size, window_size), 1.5)
    
    mu1_sq = mu1 ** 2
    mu2_sq = mu2 ** 2
    mu1_mu2 = mu1 * mu2
    
    sigma1_sq = cv2.GaussianBlur(img1 ** 2, (window_size, window_size), 1.5) - mu1_sq
    sigma2_sq = cv2.GaussianBlur(img2 ** 2, (window_size, window_size), 1.5) - mu2_sq
    sigma12 = cv2.GaussianBlur(img1 * img2, (window_size, window_size), 1.5) - mu1_mu2
    
    ssim_map = ((2 * mu1_mu2 + C1) * (2 * sigma12 + C2)) / \
               ((mu1_sq + mu2_sq + C1) * (sigma1_sq + sigma2_sq + C2))
    
    return np.mean(ssim_map)
```

**验收指标表**：

| 指标 | 验收标准 | 说明 |
|------|----------|------|
| PSNR | > 30 dB | 高质量，无明显噪声 |
| SSIM | > 0.95 | 结构相似度高，细节保留 |
| ΔE (色彩) | < 5 | 色彩准确度高 |

**自动化测试 CI 流程**：

```
每次代码提交触发自动化测试：
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                                                                                     │
│  代码提交 ──> CI 触发 ──> 运行测试                                                   │
│                              │                                                      │
│                              ▼                                                      │
│                     ┌────────────────────────┐                                     │
│                     │  单元测试 (GoogleTest)  │                                     │
│                     └────────────────────────┘                                     │
│                              │                                                      │
│                              ▼                                                      │
│                     ┌────────────────────────┐                                     │
│                     │  集成测试 (Python)      │                                     │
│                     └────────────────────────┘                                     │
│                              │                                                      │
│                              ▼                                                      │
│                     ┌────────────────────────┐                                     │
│                     │  性能基准测试           │                                     │
│                     └────────────────────────┘                                     │
│                              │                                                      │
│                              ▼                                                      │
│                     ┌────────────────────────┐                                     │
│                     │  Golden Image 对比      │                                     │
│                     │  (PSNR/SSIM 检查)       │                                     │
│                     └────────────────────────┘                                     │
│                              │                                                      │
│                              ├─> 全部通过 ──> 合并代码 ✓                             │
│                              └─> 任何失败 ──> 阻止合并，发送警报                      │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**小贴士**：一旦某次代码修改（哪怕是微小的 CCM 优化）导致画质意外退化，该测试能立即触发警报，防止低质量代码进入主分支。