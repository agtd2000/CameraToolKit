#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace mvtk {
namespace utils {

cv::Mat imreadUtf8(const std::string& filename);

}
}
