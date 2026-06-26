#include "utils/memory_pool.h"

namespace mvtk {
namespace utils {

MemoryPool::MemoryPool(size_t pool_size, int width, int height, int type, bool use_double_buffer)
    : pool_size_(pool_size), width_(width), height_(height), type_(type),
      use_double_buffer_(use_double_buffer), front_buffer_idx_(0), back_buffer_idx_(1) {
    Init(pool_size, width, height, type);
}

MemoryPool::~MemoryPool() {
    Reset();
}

void MemoryPool::Init(size_t pool_size, int width, int height, int type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    pool_size_ = pool_size;
    width_ = width;
    height_ = height;
    type_ = type;
    
    buffers_.resize(pool_size);
    for (auto& item : buffers_) {
        item.data.create(height, width, type);
        item.data.setTo(0);
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
                buffers_[i].data.release();
            }
        }
        buffers_.resize(pool_size);
    } else {
        size_t old_size = buffers_.size();
        buffers_.resize(pool_size);
        for (size_t i = old_size; i < pool_size; ++i) {
            buffers_[i].data.create(height_, width_, type_);
            buffers_[i].data.setTo(0);
            buffers_[i].in_use = false;
        }
    }
    pool_size_ = pool_size;
}

void MemoryPool::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& item : buffers_) {
        item.in_use = false;
        item.data.release();
    }
    buffers_.clear();
}

cv::Mat MemoryPool::Acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    for (auto& item : buffers_) {
        if (!item.in_use) {
            item.in_use = true;
            return item.data;
        }
    }
    
    cv::Mat new_data(height_, width_, type_);
    new_data.setTo(0);
    buffers_.push_back({new_data, true});
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
    
    mat.release();
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