#include "utils/image_io.h"
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <locale>
#include <codecvt>
#endif

namespace mvtk {
namespace utils {

cv::Mat imreadUtf8(const std::string& filename, int flags) {
#ifdef _WIN32
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring wfilename = converter.from_bytes(filename);

    HANDLE hFile = CreateFileW(wfilename.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return cv::Mat();
    }

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return cv::Mat();
    }

    std::vector<uchar> buffer(fileSize);
    DWORD bytesRead;
    if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr) || bytesRead != fileSize) {
        CloseHandle(hFile);
        return cv::Mat();
    }

    CloseHandle(hFile);

    return cv::imdecode(buffer, flags);
#else
    return cv::imread(filename, flags);
#endif
}

}
}
