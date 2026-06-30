#pragma once

#include <opencv2/core/mat.hpp>
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <cmath>

namespace mvtk {
namespace algo {

// 位深自适应的全局配置
struct PipelineConfig {
    int bit_depth = 12;      // 默认 12-bit
    bool is_raw = true;      // 是否为 Raw 数据

    // 计算当前位深的 Max DN
    int GetMaxDN() const { return (1 << bit_depth) - 1; }

    // 16-bit 到当前位深的缩放因子
    double GetScaleFactor() const { return 1.0 / std::pow(2, 16 - bit_depth); }

    // 当前位深到 16-bit 的缩放因子
    double GetUpscaleFactor() const { return std::pow(2, 16 - bit_depth); }
};

// 单节点耗时统计（性能分析用）
struct NodeLatency {
    std::string name;
    double elapsed_ms;
    double percentage;
};

// 调试模式配置
struct DebugConfig {
    bool enabled = false;
    bool save_intermediate_images = true;
    bool save_statistics = true;
    std::string output_dir = "./debug_output/";
};

class ImageNode {
public:
    virtual ~ImageNode() = default;

    virtual cv::Mat Process(const cv::Mat& input) = 0;
    virtual bool ProcessInPlace(cv::Mat& input) = 0;
    virtual std::string GetName() const = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetEnabled(bool enabled) = 0;
};

enum class DenoisePosition {
    PRE_LSC,
    POST_LSC,
    BOTH
};

class CorrectionPipeline {
public:
    CorrectionPipeline() : denoise_position_(DenoisePosition::POST_LSC) {}

    void AddNode(std::shared_ptr<ImageNode> node);
    void RemoveNode(const std::string& name);
    void Clear();

    cv::Mat Process(const cv::Mat& input);
    bool ProcessInPlace(cv::Mat& input);

    size_t GetNodeCount() const;
    std::shared_ptr<ImageNode> GetNode(const std::string& name);
    std::vector<std::string> GetNodeNames() const;

    void EnableNode(const std::string& name, bool enabled);
    bool IsNodeEnabled(const std::string& name) const;

    void SetPipelineOrder(const std::vector<std::string>& order);

    void SetDenoisePosition(DenoisePosition position);
    DenoisePosition GetDenoisePosition() const;

    void UpdatePipelineOrder();

    // ====== Debug Mode 与性能分析 ======
    // 开启/关闭调试模式（导出中间图像与统计信息）
    void SetDebugMode(const DebugConfig& config);
    // 分节点耗时统计，返回每个节点的耗时与占比
    std::vector<NodeLatency> ProfileLatency(const cv::Mat& input);

private:
    std::vector<std::shared_ptr<ImageNode>> nodes_;
    DenoisePosition denoise_position_;
    DebugConfig debug_config_;
};

// 节点 0：OB 扣除（Optical Black / Black Level Subtraction）
// 输入 SCM 数据：暗信号、暗电流、系统增益
// 作用：全图逐像素扣除底噪基底，输出绝对线性空间
class OBNode : public ImageNode {
public:
    OBNode();
    ~OBNode() override = default;

    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "OB"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }

    // 设置黑电平偏置（DN）
    void SetBlackLevel(double black_level_dn);
    // 设置暗电流补偿（e-/s）和系统增益（DN/e-）以及曝光时间（秒）
    // 启用后：Offset = black_level + dark_current * gain * exposure_s
    void SetDarkCurrentCompensation(double dark_current_e_per_s,
                                     double system_gain_dn_per_e,
                                     double exposure_s);

    // ====== 2D 暗场扣除（升级）======
    // 设置 2D 暗场图像（逐像素暗信号），优先于标量扣除
    void SetDarkMap(const cv::Mat& dark_map);
    // 计算总偏置（含防呆：下限 0，上限 full_well/2），线程安全读取
    double CalculateTotalOffset() const;

private:
    bool enabled_;
    double black_level_dn_;            // 静态黑电平
    bool use_dark_current_compensation_; // 是否启用暗电流漂移补偿
    double dark_current_e_per_s_;      // 暗电流
    double system_gain_dn_per_e_;      // 系统增益
    double exposure_s_;                // 曝光时间（秒）

    cv::Mat dark_map_;                 // 2D 暗场图像（逐像素暗信号）
    bool use_dark_map_;                // 是否使用 2D 暗场扣除
    std::atomic<double> black_level_atomic_; // 原子黑电平（线程安全，供并行遍历读取）
    double full_well_capacity_dn_;     // 满阱容量（用于偏置上限防呆）
};

// 节点 1：DPC 坏点校正（Dead Pixel Correction）
// 输入 SCM 数据：暗噪声
// 作用：统计学阈值检测坏点（如 5σ），邻域插值修复
class DPCNode : public ImageNode {
public:
    DPCNode();
    ~DPCNode() override = default;

    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "DPC"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }

    // 设置阈值倍数（如 5.0 表示 5σ）
    void SetThresholdSigma(double sigma);
    // 设置暗噪声（DN），用于计算绝对阈值 = sigma * dark_noise
    void SetDarkNoise(double dark_noise_dn);
    // 设置邻域核大小（3 或 5）
    void SetKernelSize(int kernel_size);

private:
    bool enabled_;
    double threshold_sigma_;   // 阈值倍数（默认 5.0）
    double dark_noise_dn_;     // 暗噪声（DN）
    int kernel_size_;          // 邻域核大小（3 或 5）

    // 检测坏点并修复
    void DetectAndCorrect(cv::Mat& img) const;
};

class LSCNode : public ImageNode {

public:
    LSCNode();
    ~LSCNode() override = default;

    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "LSC"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }

    // 设置 Gain Map。use_prnu=true 时保留高频 PRNU 校正（直接使用原始高精度 Gain Map）
    // use_prnu=false 时仅保留低频 LSC（高斯平滑后）
    void SetGainMap(const cv::Mat& gain_map, bool use_prnu = false);
    void SetDarkFrame(const cv::Mat& dark_frame);

private:
    bool enabled_;
    cv::Mat gain_map_;             // 兼容旧接口（保留）
    cv::Mat dark_frame_;
    cv::Mat original_gain_map_;    // 原始高精度 Gain Map（实际校正用）
    cv::Mat lsc_map_;              // 低频 LSC Map（高斯平滑后，仅低频校正时使用）
    bool use_prnu_;                // 是否启用高频 PRNU 校正
};

class DenoiseNode : public ImageNode {
public:
    DenoiseNode();
    ~DenoiseNode() override = default;
    
    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "Denoise"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }
    
    void SetSigma(float sigma);
    
private:
    bool enabled_;
    float sigma_;
};

class InterpolationNode : public ImageNode {
public:
    enum class InterpolationType {
        BILINEAR,
        BICUBIC,
        LANCZOS
    };

    InterpolationNode();
    ~InterpolationNode() override = default;

    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "Interpolation"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }

    void SetType(InterpolationType type);
    void SetOutputSize(const cv::Size& size);

private:
    bool enabled_;
    InterpolationType type_;
    cv::Size output_size_;
};

// 节点 4：Demosaicing 去马赛克插值（Bayer → RGB）
// 输入 SCM 数据：无直接输入，依赖 N0/N2 的线性空间
// 作用：将单通道 Bayer 模式图像插值为三通道 RGB 图像
class DemosaicingNode : public ImageNode {
public:
    enum class BayerPattern {
        BG,
        GB,
        RG,
        GR
    };

    // 去马赛克算法选择
    enum class Algorithm {
        BILINEAR,      // 双线性插值（速度最快）
        VNG,           // Variable Number of Gradients
        EDGE_AWARE     // Edge-Aware（OpenCV EA，伪彩抑制更好）
    };

    DemosaicingNode();
    ~DemosaicingNode() override = default;

    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "Demosaicing"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }

    // 设置 Bayer 排列模式
    void SetPattern(BayerPattern pattern);

    // ====== 算法与色度降噪（升级）======
    // 设置去马赛克算法（默认 EDGE_AWARE）
    void SetAlgorithm(Algorithm algo);
    // 设置色度降噪开关与核大小（默认启用，核大小 3）
    void SetChromaDenoise(bool enabled, int filter_size = 3);

private:
    bool enabled_;
    BayerPattern pattern_;
    Algorithm algorithm_;              // 当前算法
    bool chroma_denoise_enabled_;      // 色度降噪开关
    int chroma_filter_size_;           // 色度滤波核大小
};

class WhiteBalanceNode : public ImageNode {
public:
    WhiteBalanceNode();
    ~WhiteBalanceNode() override = default;
    
    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "WhiteBalance"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }
    
    void SetGains(const cv::Vec3f& gains);
    void SetTemperature(float temperature);
    
private:
    bool enabled_;
    cv::Vec3f gains_;
};

class CCMNode : public ImageNode {
public:
    CCMNode();
    ~CCMNode() override = default;

    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "CCM"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }

    // 设置色彩校正矩阵，含 3x3 校验与每行系数和校验（0.9~1.1）
    // 返回 bool 表示是否成功（矩阵必须为 3x3）
    bool SetMatrix(const cv::Mat& matrix);
    void SetOffset(const cv::Vec3f& offset);

private:
    bool enabled_;
    cv::Mat matrix_;
    cv::Vec3f offset_;
};

// 节点 7：Tone Mapping 动态范围映射（16-bit → 8-bit）
// 作用：在线性空间执行 THRESH_TRUNC 截断 + 线性缩放到 [0, 255]，输出 CV_8U
class ToneMappingNode : public ImageNode {
public:
    ToneMappingNode();
    ~ToneMappingNode() override = default;

    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "ToneMapping"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }

    // 设置输入有效范围 [min, max]（默认 [0, Max_DN]）
    void SetInputRange(float min, float max);
    // 设置 Max_DN（位深自适应，默认 12-bit → 4095）
    void SetMaxDN(int max_dn);

private:
    bool enabled_;
    float input_min_;        // 输入下限（一般为 0）
    float input_max_;        // 输入上限（使用 Max_DN）
    int max_dn_;             // 当前位深的 Max DN
};

// 节点 8：Gamma 校正（8-bit → 8-bit）
// 作用：纯 Gamma 校正，归一化到 [0,1] → pow(1/gamma) → 缩放回 [0, 255]
class GammaNode : public ImageNode {
public:
    GammaNode();
    ~GammaNode() override = default;

    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "Gamma"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }

    void SetGamma(float gamma);
    // 保留向后兼容：旧 Pipeline 可能仍调用此方法设置输入范围
    // 新版 Gamma 为 8-bit 纯校正，此方法仅做兼容存储（不再参与 8-bit gamma 路径）
    void SetInputRange(float min, float max);

private:
    bool enabled_;
    float gamma_;
    float input_min_;        // 兼容旧接口
    float input_max_;        // 兼容旧接口
};

}
}