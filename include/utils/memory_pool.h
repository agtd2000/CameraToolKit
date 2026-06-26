#pragma once

#include <opencv2/core/mat.hpp>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace mvtk {
namespace utils {

class MemoryPool {
public:
    MemoryPool(size_t pool_size = 8, int width = 1920, int height = 1080, 
               int type = CV_16UC1, bool use_double_buffer = true);
    ~MemoryPool();
    
    void Init(size_t pool_size, int width, int height, int type);
    void Resize(size_t pool_size);
    void Reset();
    
    cv::Mat Acquire();
    void Release(cv::Mat& mat);
    
    size_t GetPoolSize() const;
    size_t GetAvailableCount() const;
    size_t GetUsedCount() const;
    
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    int GetType() const { return type_; }
    
    bool IsDoubleBufferEnabled() const { return use_double_buffer_; }
    cv::Mat GetFrontBuffer();
    cv::Mat GetBackBuffer();
    void SwapBuffers();
    
private:
    struct BufferItem {
        cv::Mat data;
        bool in_use;
    };
    
    std::vector<BufferItem> buffers_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    size_t pool_size_;
    int width_;
    int height_;
    int type_;
    bool use_double_buffer_;
    int front_buffer_idx_;
    int back_buffer_idx_;
};

class ScopedBuffer {
public:
    ScopedBuffer(MemoryPool& pool);
    ~ScopedBuffer();
    
    cv::Mat& Get();
    operator cv::Mat&();
    
private:
    MemoryPool& pool_;
    cv::Mat buffer_;
};

}
}