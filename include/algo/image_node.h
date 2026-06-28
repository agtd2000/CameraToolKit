#pragma once

#include <opencv2/core/mat.hpp>
#include <memory>
#include <vector>
#include <string>

namespace mvtk {
namespace algo {

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

private:
    std::vector<std::shared_ptr<ImageNode>> nodes_;
    DenoisePosition denoise_position_;
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

private:
    bool enabled_;
    double black_level_dn_;            // 静态黑电平
    bool use_dark_current_compensation_; // 是否启用暗电流漂移补偿
    double dark_current_e_per_s_;      // 暗电流
    double system_gain_dn_per_e_;      // 系统增益
    double exposure_s_;                // 曝光时间（秒）
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
    
    void SetGainMap(const cv::Mat& gain_map);
    void SetDarkFrame(const cv::Mat& dark_frame);
    
private:
    bool enabled_;
    cv::Mat gain_map_;
    cv::Mat dark_frame_;
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

    DemosaicingNode();
    ~DemosaicingNode() override = default;

    cv::Mat Process(const cv::Mat& input) override;
    bool ProcessInPlace(cv::Mat& input) override;
    std::string GetName() const override { return "Demosaicing"; }
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool enabled) override { enabled_ = enabled; }

    // 设置 Bayer 排列模式
    void SetPattern(BayerPattern pattern);

private:
    bool enabled_;
    BayerPattern pattern_;
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
    
    void SetMatrix(const cv::Mat& matrix);
    void SetOffset(const cv::Vec3f& offset);
    
private:
    bool enabled_;
    cv::Mat matrix_;
    cv::Vec3f offset_;
};

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
    void SetInputRange(float min, float max);
    
private:
    bool enabled_;
    float gamma_;
    float input_min_;
    float input_max_;
};

}
}