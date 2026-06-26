#include "utils/tone_mapping.h"
#include <opencv2/imgproc.hpp>

namespace mvtk {
namespace utils {

cv::Mat ToneMapping::ConvertTo8Bit(const cv::Mat& input, ToneMappingMode mode,
                                    float low_percent, float high_percent, float gamma) {
    if (input.empty()) {
        return cv::Mat();
    }

    cv::Mat result;
    switch (mode) {
        case ToneMappingMode::LINEAR:
            result = LinearMapping(input);
            break;
        case ToneMappingMode::PERCENTILE:
            result = PercentileMapping(input, low_percent, high_percent);
            break;
        case ToneMappingMode::HISTOGRAM_EQUALIZATION:
            result = HistogramEqualization(input);
            break;
        case ToneMappingMode::GAMMA:
            result = GammaMapping(input, gamma);
            break;
        default:
            result = LinearMapping(input);
            break;
    }
    return result;
}

cv::Mat ToneMapping::LinearMapping(const cv::Mat& input) {
    cv::Mat result;
    double min_val, max_val;
    cv::minMaxLoc(input, &min_val, &max_val);
    
    if (max_val == min_val) {
        input.convertTo(result, CV_8U);
        return result;
    }

    input.convertTo(result, CV_8U, 255.0 / (max_val - min_val), -255.0 * min_val / (max_val - min_val));
    return result;
}

cv::Mat ToneMapping::PercentileMapping(const cv::Mat& input, float low_percent, float high_percent) {
    cv::Mat result;
    cv::Mat flat;
    
    if (input.channels() > 1) {
        cv::Mat hsv;
        cv::cvtColor(input, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> channels;
        cv::split(hsv, channels);
        flat = channels[2];
    } else {
        flat = input;
    }

    flat = flat.reshape(1, 1);
    cv::sort(flat, flat, cv::SORT_EVERY_ROW + cv::SORT_ASCENDING);
    
    int low_idx = static_cast<int>(flat.cols * low_percent / 100.0);
    int high_idx = static_cast<int>(flat.cols * high_percent / 100.0);
    
    double low_val = flat.at<double>(0, low_idx);
    double high_val = flat.at<double>(0, high_idx);
    
    if (high_val == low_val) {
        input.convertTo(result, CV_8U);
        return result;
    }

    cv::Mat normalized;
    input.convertTo(normalized, CV_64F);
    normalized = (normalized - low_val) / (high_val - low_val);
    normalized = cv::max(cv::min(normalized, 1.0), 0.0);
    normalized.convertTo(result, CV_8U, 255.0);
    
    return result;
}

cv::Mat ToneMapping::HistogramEqualization(const cv::Mat& input) {
    cv::Mat result;
    
    if (input.channels() == 1) {
        cv::equalizeHist(input, result);
    } else {
        cv::Mat ycrcb;
        cv::cvtColor(input, ycrcb, cv::COLOR_BGR2YCrCb);
        std::vector<cv::Mat> channels;
        cv::split(ycrcb, channels);
        cv::equalizeHist(channels[0], channels[0]);
        cv::merge(channels, ycrcb);
        cv::cvtColor(ycrcb, result, cv::COLOR_YCrCb2BGR);
    }
    
    return result;
}

cv::Mat ToneMapping::GammaMapping(const cv::Mat& input, float gamma) {
    cv::Mat result;
    cv::Mat normalized;
    
    if (input.depth() == CV_16U) {
        input.convertTo(normalized, CV_64F, 1.0 / 65535.0);
    } else {
        input.convertTo(normalized, CV_64F, 1.0 / 255.0);
    }
    
    cv::pow(normalized, 1.0 / gamma, normalized);
    normalized.convertTo(result, CV_8U, 255.0);
    
    return result;
}

}
}