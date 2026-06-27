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