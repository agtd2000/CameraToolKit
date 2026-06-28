#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <string>

namespace mvtk {
namespace utils {

// UTF-8 路径图像读取
// flags: 读取标志，默认 cv::IMREAD_COLOR（8-bit 3 通道，向后兼容）
//        16-bit Raw 图请传 cv::IMREAD_UNCHANGED 以保留位深
cv::Mat imreadUtf8(const std::string& filename, int flags = cv::IMREAD_COLOR);

}
}
