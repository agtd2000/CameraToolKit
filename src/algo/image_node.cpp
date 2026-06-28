#include "algo/image_node.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>
#include <algorithm>

namespace mvtk {
namespace algo {

void CorrectionPipeline::AddNode(std::shared_ptr<ImageNode> node) {
    nodes_.push_back(node);
}

void CorrectionPipeline::RemoveNode(const std::string& name) {
    nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
        [&name](const std::shared_ptr<ImageNode>& node) {
            return node->GetName() == name;
        }), nodes_.end());
}

void CorrectionPipeline::Clear() {
    nodes_.clear();
}

void CorrectionPipeline::SetDenoisePosition(DenoisePosition position) {
    denoise_position_ = position;
    UpdatePipelineOrder();
}

DenoisePosition CorrectionPipeline::GetDenoisePosition() const {
    return denoise_position_;
}

void CorrectionPipeline::UpdatePipelineOrder() {
    if (nodes_.empty()) return;
    
    std::vector<std::shared_ptr<ImageNode>> new_order;
    std::shared_ptr<ImageNode> denoise_node = nullptr;
    bool has_lsc = false;
    
    for (const auto& node : nodes_) {
        if (node->GetName() == "Denoise") {
            denoise_node = node;
        } else if (node->GetName() == "LSC") {
            has_lsc = true;
        }
    }
    
    if (!denoise_node) {
        return;
    }
    
    switch (denoise_position_) {
        case DenoisePosition::PRE_LSC:
            new_order.push_back(denoise_node);
            for (const auto& node : nodes_) {
                if (node->GetName() != "Denoise") {
                    new_order.push_back(node);
                }
            }
            break;
            
        case DenoisePosition::POST_LSC:
            for (const auto& node : nodes_) {
                if (node->GetName() != "Denoise") {
                    new_order.push_back(node);
                }
            }
            new_order.push_back(denoise_node);
            break;
            
        case DenoisePosition::BOTH:
            new_order.push_back(denoise_node);
            bool added_post = false;
            for (const auto& node : nodes_) {
                if (node->GetName() != "Denoise") {
                    new_order.push_back(node);
                    if (has_lsc && !added_post && node->GetName() == "LSC") {
                        new_order.push_back(denoise_node);
                        added_post = true;
                    }
                }
            }
            if (!added_post && has_lsc) {
                new_order.push_back(denoise_node);
            }
            break;
    }
    
    if (!new_order.empty()) {
        nodes_ = new_order;
    }
}

cv::Mat CorrectionPipeline::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool CorrectionPipeline::ProcessInPlace(cv::Mat& input) {
    for (const auto& node : nodes_) {
        if (node->IsEnabled()) {
            if (!node->ProcessInPlace(input)) {
                return false;
            }
        }
    }
    return true;
}

size_t CorrectionPipeline::GetNodeCount() const {
    return nodes_.size();
}

std::shared_ptr<ImageNode> CorrectionPipeline::GetNode(const std::string& name) {
    for (const auto& node : nodes_) {
        if (node->GetName() == name) {
            return node;
        }
    }
    return nullptr;
}

std::vector<std::string> CorrectionPipeline::GetNodeNames() const {
    std::vector<std::string> names;
    for (const auto& node : nodes_) {
        names.push_back(node->GetName());
    }
    return names;
}

void CorrectionPipeline::EnableNode(const std::string& name, bool enabled) {
    auto node = GetNode(name);
    if (node) {
        node->SetEnabled(enabled);
    }
}

bool CorrectionPipeline::IsNodeEnabled(const std::string& name) const {
    for (const auto& node : nodes_) {
        if (node->GetName() == name) {
            return node->IsEnabled();
        }
    }
    return false;
}

void CorrectionPipeline::SetPipelineOrder(const std::vector<std::string>& order) {
    std::vector<std::shared_ptr<ImageNode>> new_order;
    for (const auto& name : order) {
        auto node = GetNode(name);
        if (node) {
            new_order.push_back(node);
        }
    }
    nodes_ = new_order;
}

// ===================== OBNode 实现 =====================
OBNode::OBNode()
    : enabled_(true),
      black_level_dn_(100.0),
      use_dark_current_compensation_(false),
      dark_current_e_per_s_(0.0),
      system_gain_dn_per_e_(1.0),
      exposure_s_(0.0) {}

cv::Mat OBNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool OBNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }

    // 计算总偏置：黑电平 + 暗电流漂移补偿
    double offset = black_level_dn_;
    if (use_dark_current_compensation_) {
        offset += dark_current_e_per_s_ * system_gain_dn_per_e_ * exposure_s_;
    }

    // 全图逐像素减去偏置，并裁剪到 [0, max]
    cv::Mat float_img;
    input.convertTo(float_img, CV_32F);
    float_img -= static_cast<float>(offset);

    // 裁剪到非负
    cv::max(float_img, 0.0f, float_img);

    // 转回原类型
    if (input.depth() == CV_16U) {
        float_img.convertTo(input, CV_16U);
    } else if (input.depth() == CV_8U) {
        float_img.convertTo(input, CV_8U);
    } else {
        float_img.copyTo(input);
    }

    return true;
}

void OBNode::SetBlackLevel(double black_level_dn) {
    black_level_dn_ = black_level_dn;
}

void OBNode::SetDarkCurrentCompensation(double dark_current_e_per_s,
                                         double system_gain_dn_per_e,
                                         double exposure_s) {
    dark_current_e_per_s_ = dark_current_e_per_s;
    system_gain_dn_per_e_ = system_gain_dn_per_e;
    exposure_s_ = exposure_s;
    use_dark_current_compensation_ = (dark_current_e_per_s > 0.0 && exposure_s > 0.0);
}

// ===================== DPCNode 实现 =====================
DPCNode::DPCNode()
    : enabled_(true),
      threshold_sigma_(5.0),
      dark_noise_dn_(30.0),
      kernel_size_(3) {}

cv::Mat DPCNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool DPCNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }
    DetectAndCorrect(input);
    return true;
}

void DPCNode::SetThresholdSigma(double sigma) {
    threshold_sigma_ = sigma > 0.0 ? sigma : 5.0;
}

void DPCNode::SetDarkNoise(double dark_noise_dn) {
    dark_noise_dn_ = dark_noise_dn > 0.0 ? dark_noise_dn : 30.0;
}

void DPCNode::SetKernelSize(int kernel_size) {
    kernel_size_ = (kernel_size == 3 || kernel_size == 5) ? kernel_size : 3;
}

void DPCNode::DetectAndCorrect(cv::Mat& img) const {
    // 绝对阈值 = sigma * dark_noise
    const double threshold = threshold_sigma_ * dark_noise_dn_;
    const int k = kernel_size_ / 2;  // 邻域半径

    // 转为浮点处理
    cv::Mat float_img;
    img.convertTo(float_img, CV_32F);

    // 计算邻域均值（排除中心像素）
    cv::Mat mean_img;
    cv::boxFilter(float_img, mean_img, CV_32F, cv::Size(kernel_size_, kernel_size_),
                  cv::Point(-1, -1), true, cv::BORDER_REPLICATE);

    // 检测坏点：与邻域均值差值超过阈值
    cv::Mat diff = cv::abs(float_img - mean_img);
    cv::Mat mask = diff > threshold;

    // 修复：用邻域均值替换坏点
    float_img.copyTo(mean_img, ~mask);  // 非坏点保留原值
    // 坏点位置用 mean_img 的值（邻域均值）

    // 合成结果：坏点位置用均值，非坏点用原值
    cv::Mat result = float_img.clone();
    mean_img.copyTo(result, mask);

    // 转回原类型
    if (img.depth() == CV_16U) {
        result.convertTo(img, CV_16U);
    } else if (img.depth() == CV_8U) {
        result.convertTo(img, CV_8U);
    } else {
        result.copyTo(img);
    }
}

LSCNode::LSCNode() : enabled_(true) {}

cv::Mat LSCNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool LSCNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }
    
    if (!dark_frame_.empty()) {
        cv::subtract(input, dark_frame_, input, cv::noArray(), input.depth());
    }
    
    if (!gain_map_.empty()) {
        cv::Mat gain_map;
        if (gain_map_.type() != input.type()) {
            gain_map_.convertTo(gain_map, input.type());
        } else {
            gain_map = gain_map_;
        }
        
        if (gain_map.size() != input.size()) {
            cv::resize(gain_map, gain_map, input.size());
        }
        
        for (int y = 0; y < input.rows; ++y) {
            for (int x = 0; x < input.cols; ++x) {
                if (input.channels() == 1) {
                    input.at<ushort>(y, x) = static_cast<ushort>(input.at<ushort>(y, x) * gain_map.at<float>(y, x));
                } else if (input.channels() == 3) {
                    cv::Vec3s pix = input.at<cv::Vec3s>(y, x);
                    cv::Vec3f gain = gain_map.at<cv::Vec3f>(y, x);
                    input.at<cv::Vec3s>(y, x) = cv::Vec3s(
                        static_cast<short>(pix[0] * gain[0]),
                        static_cast<short>(pix[1] * gain[1]),
                        static_cast<short>(pix[2] * gain[2])
                    );
                }
            }
        }
    }
    
    return true;
}

void LSCNode::SetGainMap(const cv::Mat& gain_map) {
    gain_map.copyTo(gain_map_);
}

void LSCNode::SetDarkFrame(const cv::Mat& dark_frame) {
    dark_frame.copyTo(dark_frame_);
}

DenoiseNode::DenoiseNode() : enabled_(true), sigma_(1.0f) {}

cv::Mat DenoiseNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool DenoiseNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }
    
    cv::Mat output;
    if (input.depth() == CV_16U) {
        cv::GaussianBlur(input, output, cv::Size(0, 0), sigma_);
    } else {
        cv::fastNlMeansDenoisingColored(input, output, sigma_, sigma_, 7, 21);
    }
    output.copyTo(input);
    
    return true;
}

void DenoiseNode::SetSigma(float sigma) {
    sigma_ = sigma;
}

InterpolationNode::InterpolationNode() : enabled_(true), type_(InterpolationType::BICUBIC) {}

cv::Mat InterpolationNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool InterpolationNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }
    
    int cv_type = cv::INTER_CUBIC;
    switch (type_) {
        case InterpolationType::BILINEAR:
            cv_type = cv::INTER_LINEAR;
            break;
        case InterpolationType::BICUBIC:
            cv_type = cv::INTER_CUBIC;
            break;
        case InterpolationType::LANCZOS:
            cv_type = cv::INTER_LANCZOS4;
            break;
    }
    
    cv::Mat output;
    cv::Size size = output_size_.width > 0 ? output_size_ : input.size();
    cv::resize(input, output, size, 0, 0, cv_type);
    output.copyTo(input);
    
    return true;
}

void InterpolationNode::SetType(InterpolationType type) {
    type_ = type;
}

void InterpolationNode::SetOutputSize(const cv::Size& size) {
    output_size_ = size;
}

// ===================== DemosaicingNode 实现 =====================
DemosaicingNode::DemosaicingNode()
    : enabled_(true), pattern_(BayerPattern::BG) {}

cv::Mat DemosaicingNode::Process(const cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return input.clone();
    }

    // 仅对单通道图像做去马赛克
    if (input.channels() != 1) {
        return input.clone();
    }

    cv::Mat output;
    int code = cv::COLOR_BayerBG2BGR;
    switch (pattern_) {
        case BayerPattern::BG: code = cv::COLOR_BayerBG2BGR; break;
        case BayerPattern::GB: code = cv::COLOR_BayerGB2BGR; break;
        case BayerPattern::RG: code = cv::COLOR_BayerRG2BGR; break;
        case BayerPattern::GR: code = cv::COLOR_BayerGR2BGR; break;
    }

    // OpenCV 的 demosaicing 要求 8-bit 或 16-bit
    if (input.depth() == CV_8U || input.depth() == CV_16U) {
        cv::cvtColor(input, output, code);
    } else {
        // 其它类型先转 16U 再处理
        cv::Mat tmp;
        input.convertTo(tmp, CV_16U);
        cv::cvtColor(tmp, output, code);
    }
    return output;
}

bool DemosaicingNode::ProcessInPlace(cv::Mat& input) {
    // 去马赛克会改变通道数（1→3），不能原地修改，需要替换
    cv::Mat output = Process(input);
    if (!output.empty()) {
        output.copyTo(input);
        return true;
    }
    return false;
}

void DemosaicingNode::SetPattern(BayerPattern pattern) {
    pattern_ = pattern;
}

WhiteBalanceNode::WhiteBalanceNode() : enabled_(true), gains_(1.0f, 1.0f, 1.0f) {}

cv::Mat WhiteBalanceNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool WhiteBalanceNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }
    
    cv::Mat float_img;
    input.convertTo(float_img, CV_32F);
    
    if (input.channels() == 3) {
        std::vector<cv::Mat> channels;
        cv::split(float_img, channels);
        channels[0] *= gains_[0];
        channels[1] *= gains_[1];
        channels[2] *= gains_[2];
        cv::merge(channels, float_img);
    } else if (input.channels() == 1) {
        float_img *= gains_[1];
    }
    
    float_img.convertTo(input, input.type());
    
    return true;
}

void WhiteBalanceNode::SetGains(const cv::Vec3f& gains) {
    gains_ = gains;
}

void WhiteBalanceNode::SetTemperature(float /*temperature*/) {
}

CCMNode::CCMNode() : enabled_(true), offset_(0.0f, 0.0f, 0.0f) {
    matrix_ = cv::Mat::eye(3, 3, CV_32F);
}

cv::Mat CCMNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool CCMNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }
    
    if (input.channels() != 3) {
        return true;
    }
    
    cv::Mat float_img;
    input.convertTo(float_img, CV_32F);
    
    cv::Mat reshaped = float_img.reshape(1, float_img.total());
    cv::Mat transformed = reshaped * matrix_.t();
    
    if (offset_[0] != 0.0f || offset_[1] != 0.0f || offset_[2] != 0.0f) {
        for (int i = 0; i < transformed.rows; ++i) {
            transformed.at<float>(i, 0) += offset_[0];
            transformed.at<float>(i, 1) += offset_[1];
            transformed.at<float>(i, 2) += offset_[2];
        }
    }
    
    cv::Mat result = transformed.reshape(3, float_img.rows);
    result.convertTo(input, input.type());
    
    return true;
}

void CCMNode::SetMatrix(const cv::Mat& matrix) {
    matrix.copyTo(matrix_);
}

void CCMNode::SetOffset(const cv::Vec3f& offset) {
    offset_ = offset;
}

GammaNode::GammaNode() : enabled_(true), gamma_(2.2f), input_min_(0.0f), input_max_(65535.0f) {}

cv::Mat GammaNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool GammaNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }
    
    cv::Mat float_img;
    input.convertTo(float_img, CV_32F);
    
    float_img -= input_min_;
    float_img /= (input_max_ - input_min_);
    float_img = cv::max(float_img, 0.0f);
    float_img = cv::min(float_img, 1.0f);
    
    cv::pow(float_img, 1.0f / gamma_, float_img);
    
    if (input.depth() == CV_16U) {
        float_img *= 65535.0f;
        float_img.convertTo(input, CV_16U);
    } else {
        float_img *= 255.0f;
        float_img.convertTo(input, CV_8U);
    }
    
    return true;
}

void GammaNode::SetGamma(float gamma) {
    gamma_ = gamma;
}

void GammaNode::SetInputRange(float min, float max) {
    input_min_ = min;
    input_max_ = max;
}

}
}