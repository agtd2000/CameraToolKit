#include "algo/image_node.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/photo.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include "utils/logger.h"

namespace mvtk {
namespace algo {

// ===================== 位深自适应模板分发 =====================
// 使用行指针遍历，支持非连续内存；OpenMP 并行化；saturate_cast 防 UINT16 下溢
template <typename T>
void ProcessChannelData(cv::Mat& img, double offset, double max_dn) {
    int rows = img.rows;
    int cols = img.cols * img.channels();

    if (img.isContinuous()) {
        cols *= rows;
        rows = 1;
    }

    #pragma omp parallel for
    for (int r = 0; r < rows; ++r) {
        T* row_ptr = img.ptr<T>(r);
        for (int c = 0; c < cols; ++c) {
            double val = static_cast<double>(row_ptr[c]) - offset;
            val = std::max(0.0, std::min(val, max_dn));
            row_ptr[c] = static_cast<T>(val);
        }
    }
}

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

    // 调试模式：逐节点处理并导出中间图像/统计信息
    if (debug_config_.enabled) {
        if (debug_config_.save_intermediate_images) {
            // 确保输出目录存在
            std::error_code ec;
            std::filesystem::create_directories(debug_config_.output_dir, ec);
        }

        for (size_t i = 0; i < nodes_.size(); ++i) {
            const auto& node = nodes_[i];
            if (node->IsEnabled()) {
                node->ProcessInPlace(result);
            }

            if (debug_config_.save_intermediate_images && !result.empty()) {
                std::string filename = debug_config_.output_dir +
                                       "N" + std::to_string(i) + "_" +
                                       node->GetName() + "_output.tiff";
                cv::imwrite(filename, result);
            }

            if (debug_config_.save_statistics && !result.empty()) {
                double min_val = 0.0, max_val = 0.0;
                cv::minMaxLoc(result, &min_val, &max_val);
                double mean_val = cv::mean(result)[0];
                MV_LOG_INFO("N" + std::to_string(i) + " " + node->GetName() +
                            ": min=" + std::to_string(min_val) +
                            ", max=" + std::to_string(max_val) +
                            ", mean=" + std::to_string(mean_val));
            }
        }
        return result;
    }

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

void CorrectionPipeline::SetDebugMode(const DebugConfig& config) {
    debug_config_ = config;
}

std::vector<NodeLatency> CorrectionPipeline::ProfileLatency(const cv::Mat& input) {
    std::vector<NodeLatency> latencies;
    double total_time = 0.0;

    cv::Mat current = input.clone();

    for (const auto& node : nodes_) {
        auto start = std::chrono::high_resolution_clock::now();
        if (node->IsEnabled()) {
            node->ProcessInPlace(current);
        }
        auto end = std::chrono::high_resolution_clock::now();

        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        total_time += elapsed_ms;

        latencies.push_back({node->GetName(), elapsed_ms, 0.0});
    }

    // 计算百分比
    for (auto& latency : latencies) {
        latency.percentage = total_time > 0.0
            ? latency.elapsed_ms / total_time * 100.0
            : 0.0;
    }

    return latencies;
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
      exposure_s_(0.0),
      use_dark_map_(false),
      black_level_atomic_(100.0),
      full_well_capacity_dn_(65535.0) {}

cv::Mat OBNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool OBNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }

    // 优先使用 2D 暗场扣除（逐像素暗信号）
    if (use_dark_map_ && !dark_map_.empty()) {
        cv::Mat dark_map;
        if (dark_map_.size() != input.size()) {
            cv::resize(dark_map_, dark_map, input.size());
        } else {
            dark_map = dark_map_;
        }
        // 类型对齐
        if (dark_map.type() != input.type()) {
            dark_map.convertTo(dark_map, input.type());
        }
        // 逐像素矩阵减法（2D 暗场扣除），下限截断到 0 防止 UINT16 下溢
        cv::Mat output;
        cv::subtract(input, dark_map, output, cv::noArray(), input.depth());
        cv::max(output, 0, output);
        output.copyTo(input);
        return true;
    }

    // 回退到标量扣除策略
    // 计算总偏置（含防呆），线程安全读取
    double offset = CalculateTotalOffset();
    double max_dn = (input.depth() == CV_16U) ? 65535.0 : 255.0;

    // 位深自适应模板分发：行指针遍历 + saturate_cast 防 UINT16 下溢
    if (input.depth() == CV_8U) {
        ProcessChannelData<uint8_t>(input, offset, 255.0);
    } else if (input.depth() == CV_16U) {
        ProcessChannelData<uint16_t>(input, offset, max_dn);
    } else {
        // 其它类型回退到浮点路径
        cv::Mat float_img;
        input.convertTo(float_img, CV_64F);
        float_img -= offset;
        cv::max(float_img, 0.0, float_img);
        cv::min(float_img, max_dn, float_img);
        float_img.convertTo(input, input.type());
    }

    return true;
}

void OBNode::SetBlackLevel(double black_level_dn) {
    black_level_dn_ = black_level_dn;
    black_level_atomic_.store(black_level_dn, std::memory_order_relaxed);
}

void OBNode::SetDarkCurrentCompensation(double dark_current_e_per_s,
                                         double system_gain_dn_per_e,
                                         double exposure_s) {
    dark_current_e_per_s_ = dark_current_e_per_s;
    system_gain_dn_per_e_ = system_gain_dn_per_e;
    exposure_s_ = exposure_s;
    use_dark_current_compensation_ = (dark_current_e_per_s > 0.0 && exposure_s > 0.0);
}

void OBNode::SetDarkMap(const cv::Mat& dark_map) {
    dark_map.copyTo(dark_map_);
    use_dark_map_ = !dark_map_.empty();
}

double OBNode::CalculateTotalOffset() const {
    // 线程安全读取黑电平（atomic）
    double offset = black_level_atomic_.load(std::memory_order_relaxed);

    if (use_dark_current_compensation_) {
        offset += dark_current_e_per_s_ * system_gain_dn_per_e_ * exposure_s_;
    }

    // 防呆1：下限截断，确保 Offset 始终为正
    offset = std::max(offset, 0.0);

    // 防呆2：上限截断，防止过度扣除导致图像暗部截断（不超过满阱容量一半）
    double max_offset = full_well_capacity_dn_ * 0.5;
    offset = std::min(offset, max_offset);

    return offset;
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

LSCNode::LSCNode() : enabled_(true), use_prnu_(false) {}

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

    // 选择实际使用的 Gain Map
    // use_prnu_=true  → 直接使用原始高精度 original_gain_map_（数学等价 Input*LSC*PRNU = Input*GainMap）
    // use_prnu_=false → 使用高斯平滑后的 lsc_map_（仅低频阴影校正）
    cv::Mat src_gain_map;
    if (use_prnu_ && !original_gain_map_.empty()) {
        src_gain_map = original_gain_map_;
    } else if (!lsc_map_.empty()) {
        src_gain_map = lsc_map_;
    } else if (!original_gain_map_.empty()) {
        // 兜底：未做平滑分离时直接用原始图
        src_gain_map = original_gain_map_;
    }

    if (!src_gain_map.empty()) {
        cv::Mat gain_map;
        // Gain Map 统一转为浮点，避免 8-bit 图像相乘被截断为整数
        if (src_gain_map.depth() != CV_32F && src_gain_map.depth() != CV_64F) {
            src_gain_map.convertTo(gain_map, CV_32F);
        } else {
            gain_map = src_gain_map;
        }

        if (gain_map.size() != input.size()) {
            cv::resize(gain_map, gain_map, input.size());
        }
        // 通道对齐
        if (gain_map.channels() != input.channels()) {
            std::vector<cv::Mat> chs(input.channels(), gain_map);
            cv::merge(chs, gain_map);
        }

        // 使用 cv::multiply 替代手动循环
        cv::Mat output;
        cv::multiply(input, gain_map, output, 1.0, input.depth());
        output.copyTo(input);
    }

    return true;
}

void LSCNode::SetGainMap(const cv::Mat& gain_map, bool use_prnu) {
    // 兼容旧接口：同步保存到 gain_map_
    gain_map.copyTo(gain_map_);

    // 保留原始高精度 Gain Map（实际校正用）
    gain_map.copyTo(original_gain_map_);
    use_prnu_ = use_prnu;

    if (use_prnu) {
        // 启用高频 PRNU：直接使用原始 Gain Map，无需平滑
        // （标定报告如需 LSC/PRNU 分离，可在此处额外保存 lsc_map_ 与 prnu_map_）
        cv::GaussianBlur(gain_map, lsc_map_, cv::Size(0, 0), 50.0);
    } else {
        // 仅低频 LSC：高斯平滑后作为 lsc_map_ 使用
        cv::GaussianBlur(gain_map, lsc_map_, cv::Size(0, 0), 50.0);
    }
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
    : enabled_(true),
      pattern_(BayerPattern::BG),
      algorithm_(Algorithm::EDGE_AWARE),
      chroma_denoise_enabled_(true),
      chroma_filter_size_(3) {}

cv::Mat DemosaicingNode::Process(const cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return input.clone();
    }

    // 仅对单通道图像做去马赛克
    if (input.channels() != 1) {
        return input.clone();
    }

    cv::Mat output;

    // 根据 Bayer 排列 + 算法选择对应的 cv::cvtColor code
    // BILINEAR → 标准 Bayer→BGR；VNG → _VNG；EDGE_AWARE → _EA
    int code = cv::COLOR_BayerBG2BGR;
    switch (pattern_) {
        case BayerPattern::BG:
            code = (algorithm_ == Algorithm::VNG) ? cv::COLOR_BayerBG2BGR_VNG
                 : (algorithm_ == Algorithm::EDGE_AWARE) ? cv::COLOR_BayerBG2BGR_EA
                 : cv::COLOR_BayerBG2BGR;
            break;
        case BayerPattern::GB:
            code = (algorithm_ == Algorithm::VNG) ? cv::COLOR_BayerGB2BGR_VNG
                 : (algorithm_ == Algorithm::EDGE_AWARE) ? cv::COLOR_BayerGB2BGR_EA
                 : cv::COLOR_BayerGB2BGR;
            break;
        case BayerPattern::RG:
            code = (algorithm_ == Algorithm::VNG) ? cv::COLOR_BayerRG2BGR_VNG
                 : (algorithm_ == Algorithm::EDGE_AWARE) ? cv::COLOR_BayerRG2BGR_EA
                 : cv::COLOR_BayerRG2BGR;
            break;
        case BayerPattern::GR:
            code = (algorithm_ == Algorithm::VNG) ? cv::COLOR_BayerGR2BGR_VNG
                 : (algorithm_ == Algorithm::EDGE_AWARE) ? cv::COLOR_BayerGR2BGR_EA
                 : cv::COLOR_BayerGR2BGR;
            break;
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

    // 色度降噪：RGB→YUV，仅对 U/V 通道平滑，YUV→RGB
    if (chroma_denoise_enabled_ && !output.empty() && output.channels() == 3) {
        int ksize = chroma_filter_size_ | 1;  // 强制奇数
        if (ksize < 3) ksize = 3;

        cv::Mat yuv;
        cv::cvtColor(output, yuv, cv::COLOR_BGR2YUV);

        std::vector<cv::Mat> channels;
        cv::split(yuv, channels);
        // channels[0]=Y, channels[1]=U, channels[2]=V
        cv::GaussianBlur(channels[1], channels[1], cv::Size(ksize, ksize), 0);
        cv::GaussianBlur(channels[2], channels[2], cv::Size(ksize, ksize), 0);

        cv::merge(channels, yuv);
        cv::cvtColor(yuv, output, cv::COLOR_YUV2BGR);
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

void DemosaicingNode::SetAlgorithm(Algorithm algo) {
    algorithm_ = algo;
}

void DemosaicingNode::SetChromaDenoise(bool enabled, int filter_size) {
    chroma_denoise_enabled_ = enabled;
    chroma_filter_size_ = (filter_size >= 3) ? filter_size : 3;
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

    // 当前位深的 Max DN（用于截断）
    double max_dn = (input.depth() == CV_16U) ? 65535.0 : 255.0;

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

    // 溢出保护：使用 cv::saturate_cast 防止溢出，并截断到 [0, max_dn]
    // 先在浮点空间做下限/上限截断，再以 saturate_cast 写回原类型
    cv::max(result, 0.0f, result);
    cv::min(result, static_cast<float>(max_dn), result);

    if (input.depth() == CV_8U) {
        result.convertTo(input, CV_8U);
    } else if (input.depth() == CV_16U) {
        result.convertTo(input, CV_16U);
    } else {
        result.convertTo(input, input.type());
    }

    return true;
}

bool CCMNode::SetMatrix(const cv::Mat& matrix) {
    // 校验1：必须是 3x3 矩阵
    if (matrix.rows != 3 || matrix.cols != 3) {
        MV_LOG_ERROR("CCM SetMatrix failed: matrix must be 3x3, got " +
                     std::to_string(matrix.rows) + "x" + std::to_string(matrix.cols));
        return false;
    }

    // 校验2：每行系数和应接近 1.0
    // 正常范围 0.95~1.05；警告范围 0.9~0.95 或 1.05~1.1；错误范围 <0.9 或 >1.1
    for (int i = 0; i < 3; ++i) {
        double row_sum = cv::sum(matrix.row(i))[0];
        if (row_sum < 0.9 || row_sum > 1.1) {
            MV_LOG_WARN("CCM row " + std::to_string(i) +
                        " sum (" + std::to_string(row_sum) +
                        ") deviates from 1.0, calibration may be affected by ambient light");
        } else if (row_sum < 0.95 || row_sum > 1.05) {
            MV_LOG_WARN("CCM row " + std::to_string(i) +
                        " sum (" + std::to_string(row_sum) +
                        ") slightly deviates from 1.0 (warning range 0.9~1.1)");
        }
    }

    matrix.copyTo(matrix_);
    return true;
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

    // 新版 Gamma：纯 8-bit → 8-bit 校正
    // 归一化到 [0,1] → pow(1/gamma) → 缩放回 [0, 255]
    if (input.depth() == CV_8U) {
        cv::Mat float_img;
        input.convertTo(float_img, CV_32F, 1.0 / 255.0);  // 归一化到 [0,1]

        cv::max(float_img, 0.0f, float_img);
        cv::min(float_img, 1.0f, float_img);

        cv::pow(float_img, 1.0f / gamma_, float_img);

        float_img *= 255.0f;
        float_img.convertTo(input, CV_8U);
        return true;
    }

    // ====== 向后兼容路径 ======
    // 旧 Pipeline 未接入 ToneMappingNode 时，可能直接送入 16-bit 图像。
    // 此时保留旧行为：用 input_min_/input_max_ 归一化后做 Gamma，输出 8-bit。
    cv::Mat float_img;
    input.convertTo(float_img, CV_32F);

    float_img -= input_min_;
    float_img /= (input_max_ - input_min_);
    float_img = cv::max(float_img, 0.0f);
    float_img = cv::min(float_img, 1.0f);

    cv::pow(float_img, 1.0f / gamma_, float_img);

    float_img *= 255.0f;
    float_img.convertTo(input, CV_8U);

    return true;
}

void GammaNode::SetGamma(float gamma) {
    gamma_ = gamma;
}

void GammaNode::SetInputRange(float min, float max) {
    // 兼容旧接口：仅做存储，新版 8-bit gamma 路径不使用；
    // 但当输入仍为 16-bit（旧 Pipeline）时，向后兼容路径会用到。
    input_min_ = min;
    input_max_ = max;
}

// ===================== ToneMappingNode 实现 =====================
ToneMappingNode::ToneMappingNode()
    : enabled_(true), input_min_(0.0f), input_max_(4095.0f), max_dn_(4095) {}

cv::Mat ToneMappingNode::Process(const cv::Mat& input) {
    cv::Mat result = input.clone();
    ProcessInPlace(result);
    return result;
}

bool ToneMappingNode::ProcessInPlace(cv::Mat& input) {
    if (!enabled_ || input.empty()) {
        return true;
    }

    // 阶段1：强制 THRESH_TRUNC 截断，确保输入值严格处于 [0, Max_DN] 区间
    // 防止传感器过饱和或异常值导致后续色彩扭曲
    cv::threshold(input, input, static_cast<double>(max_dn_),
                  static_cast<double>(max_dn_), cv::THRESH_TRUNC);

    // 阶段2：线性缩放到 [0, 255]，输出 CV_8U
    cv::Mat float_img;
    input.convertTo(float_img, CV_32F);

    float range = input_max_ - input_min_;
    if (range <= 0.0f) {
        range = 1.0f;  // 防呆，避免除零
    }

    float_img -= input_min_;
    float_img /= range;
    float_img = cv::max(float_img, 0.0f);
    float_img = cv::min(float_img, 1.0f);

    float_img *= 255.0f;
    float_img.convertTo(input, CV_8U);

    return true;
}

void ToneMappingNode::SetInputRange(float min, float max) {
    input_min_ = min;
    input_max_ = max;
}

void ToneMappingNode::SetMaxDN(int max_dn) {
    max_dn_ = max_dn;
    // 同步更新输入上限，保持一致
    input_max_ = static_cast<float>(max_dn);
}

}
}