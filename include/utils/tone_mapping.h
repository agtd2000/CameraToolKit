#pragma once

#include <opencv2/core.hpp>

namespace mvtk {
namespace utils {

enum class ToneMappingMode {
    LINEAR,
    PERCENTILE,
    HISTOGRAM_EQUALIZATION,
    GAMMA
};

class ToneMapping {
public:
    static cv::Mat ConvertTo8Bit(const cv::Mat& input, ToneMappingMode mode = ToneMappingMode::PERCENTILE,
                                  float low_percent = 0.1f, float high_percent = 99.9f, float gamma = 2.2f);

private:
    static cv::Mat LinearMapping(const cv::Mat& input);
    static cv::Mat PercentileMapping(const cv::Mat& input, float low_percent, float high_percent);
    static cv::Mat HistogramEqualization(const cv::Mat& input);
    static cv::Mat GammaMapping(const cv::Mat& input, float gamma);
};

}
}