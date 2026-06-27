#include "utils/memory_pool.h"
#include <cstdlib>
#include <cstdint>

namespace mvtk {
namespace utils {

#if defined(_WIN32) || defined(_WIN64)
#include <malloc.h>
#define aligned_malloc(size, alignment) _aligned_malloc(size, alignment)
#define aligned_free(ptr) _aligned_free(ptr)
#else
#include <stdlib.h>
#define aligned_malloc(size, alignment) posix_memalign((void**)&ptr, alignment, size)
#define aligned_free(ptr) free(ptr)
#endif

MemoryPool::MemoryPool(size_t pool_size, int width, int height, int type, bool use_double_buffer, int alignment)
    : pool_size_(pool_size), width_(width), height_(height), type_(type),
      use_double_buffer_(use_double_buffer), front_buffer_idx_(0), back_buffer_idx_(1), alignment_(alignment) {
    Init(pool_size, width, height, type);
}

MemoryPool::~MemoryPool() {
    Reset();
}

cv::Mat MemoryPool::createAlignedMat(int rows, int cols, int type, int alignment, void*& out_ptr, size_t& out_size) {
    cv::Mat mat;
    int depth = CV_MAT_DEPTH(type);
    int channels = CV_MAT_CN(type);
    size_t elem_size;
    
    if (depth == CV_8U || depth == CV_8S) {
        elem_size = 1 * channels;
    } else if (depth == CV_16U || depth == CV_16S) {
        elem_size = 2 * channels;
    } else if (depth == CV_32S) {
        elem_size = 4 * channels;
    } else if (depth == CV_32F) {
        elem_size = 4 * channels;
    } else if (depth == CV_64F) {
        elem_size = 8 * channels;
    } else {
        elem_size = 1 * channels;
    }
    
    size_t row_size = (cols * elem_size + alignment - 1) & ~(alignment - 1);
    out_size = rows * row_size;
    
    void* ptr = aligned_malloc(out_size, alignment);
    out_ptr = ptr;
    
    if (!ptr) {
        mat.create(rows, cols, type);
        return mat;
    }
    
    mat = cv::Mat(rows, cols, type, ptr, row_size);
    mat.setTo(0);
    
    return mat;
}

void MemoryPool::Init(size_t pool_size, int width, int height, int type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    pool_size_ = pool_size;
    width_ = width;
    height_ = height;
    type_ = type;
    
    buffers_.resize(pool_size);
    for (auto& item : buffers_) {
        void* ptr = nullptr;
        size_t size = 0;
        item.data = createAlignedMat(height, width, type, alignment_, ptr, size);
        item.aligned_ptr = ptr;
        item.allocated_size = size;
        item.in_use = false;
    }
    
    if (use_double_buffer_ && pool_size >= 2) {
        front_buffer_idx_ = 0;
        back_buffer_idx_ = 1;
    }
}

void MemoryPool::Resize(size_t pool_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pool_size < pool_size_) {
        for (size_t i = pool_size; i < pool_size_; ++i) {
            if (!buffers_[i].in_use) {
                buffers_[i].data = cv::Mat();
                if (buffers_[i].aligned_ptr) {
                    aligned_free(buffers_[i].aligned_ptr);
                    buffers_[i].aligned_ptr = nullptr;
                }
            }
        }
        buffers_.resize(pool_size);
    } else {
        size_t old_size = buffers_.size();
        buffers_.resize(pool_size);
        for (size_t i = old_size; i < pool_size; ++i) {
            void* ptr = nullptr;
            size_t size = 0;
            buffers_[i].data = createAlignedMat(height_, width_, type_, alignment_, ptr, size);
            buffers_[i].aligned_ptr = ptr;
            buffers_[i].allocated_size = size;
            buffers_[i].in_use = false;
        }
    }
    pool_size_ = pool_size;
}

void MemoryPool::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& item : buffers_) {
        item.in_use = false;
        item.data = cv::Mat();
        if (item.aligned_ptr) {
            aligned_free(item.aligned_ptr);
            item.aligned_ptr = nullptr;
        }
    }
    buffers_.clear();
}

void MemoryPool::SetAlignment(int alignment) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (alignment != alignment_) {
        alignment_ = alignment;
        Reset();
        Init(pool_size_, width_, height_, type_);
    }
}

cv::Mat MemoryPool::Acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    for (auto& item : buffers_) {
        if (!item.in_use) {
            item.in_use = true;
            return item.data;
        }
    }
    
    BufferItem new_item;
    void* ptr = nullptr;
    size_t size = 0;
    new_item.data = createAlignedMat(height_, width_, type_, alignment_, ptr, size);
    new_item.aligned_ptr = ptr;
    new_item.allocated_size = size;
    new_item.in_use = true;
    
    buffers_.push_back(new_item);
    pool_size_++;
    
    return buffers_.back().data;
}

void MemoryPool::Release(cv::Mat& mat) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& item : buffers_) {
        if (item.data.data == mat.data) {
            item.in_use = false;
            break;
        }
    }
    
    mat = cv::Mat();
}

size_t MemoryPool::GetPoolSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_size_;
}

size_t MemoryPool::GetAvailableCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& item : buffers_) {
        if (!item.in_use) {
            count++;
        }
    }
    return count;
}

size_t MemoryPool::GetUsedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& item : buffers_) {
        if (item.in_use) {
            count++;
        }
    }
    return count;
}

cv::Mat MemoryPool::GetFrontBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffers_.size() > front_buffer_idx_) {
        return buffers_[front_buffer_idx_].data;
    }
    return cv::Mat();
}

cv::Mat MemoryPool::GetBackBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffers_.size() > back_buffer_idx_) {
        return buffers_[back_buffer_idx_].data;
    }
    return cv::Mat();
}

void MemoryPool::SwapBuffers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::swap(front_buffer_idx_, back_buffer_idx_);
}

ScopedBuffer::ScopedBuffer(MemoryPool& pool) : pool_(pool) {
    buffer_ = pool_.Acquire();
}

ScopedBuffer::~ScopedBuffer() {
    pool_.Release(buffer_);
}

cv::Mat& ScopedBuffer::Get() {
    return buffer_;
}

ScopedBuffer::operator cv::Mat&() {
    return buffer_;
}

}
}