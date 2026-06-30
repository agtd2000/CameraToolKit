#pragma once

#include <string>
#include <mutex>
#include <optional>
#include <vector>
#include <memory>
#include <toml++/toml.hpp>
#include "utils/logger.h"

namespace mvtk {
namespace utils {

/**
 * @brief 会话配置管理器（单例）
 *
 * 负责读写 pipeline_session.toml 文件，提供类型安全的键值访问接口。
 * 各测试模块通过此类读取输入参数、写入计算结果。
 */
class SessionConfig {
public:
    /// 获取单例
    static SessionConfig& GetInstance();

    /// 初始化：加载指定路径的 TOML 文件；若不存在则使用默认路径
    bool Load(const std::string& filepath = "");

    /// 保存到当前文件路径
    bool Save();

    /// 保存到指定路径
    bool SaveAs(const std::string& filepath);

    /// 获取当前文件路径
    const std::string& GetFilePath() const { return filepath_; }

    /// 是否已加载
    bool IsLoaded() const { return loaded_; }

    // ============ 通用键值读写 ============

    /// 读取标量值
    template <typename T>
    std::optional<T> Get(const std::string& dotted_key) const;

    /// 读取带默认值的标量
    template <typename T>
    T GetOr(const std::string& dotted_key, const T& default_val) const;

    /// 写入标量值（支持 dotted path，如 "scm.system_gain_e_per_dn"）
    template <typename T>
    void Set(const std::string& dotted_key, const T& value);

    /// 读取数组
    template <typename T>
    std::vector<T> GetArray(const std::string& dotted_key) const;

    /// 写入数组
    template <typename T>
    void SetArray(const std::string& dotted_key, const std::vector<T>& values);

    // ============ 便捷方法 ============

    void SetSensorInfo(const std::string& manufacturer,
                       const std::string& model,
                       const std::string& serial_number,
                       int bit_depth,
                       int max_digital_value,
                       const std::string& bayer_pattern = "",
                       int resolution_width = 0,
                       int resolution_height = 0);

    struct SCMResults {
        double system_gain_e_per_dn;
        double system_gain_dn_per_e;
        double dark_noise_e;
        double dark_noise_dn;
        double dark_signal_dn;
        double dark_current_e_per_sec;
        double full_well_capacity_e;
        double full_well_capacity_dn;
        double dynamic_range_db;
        double dynamic_range_bits;
        double snr_at_50_percent_db;
        double peak_snr_db;
        bool is_valid;
        bool is_snr_sufficient_for_ccm;
        double recommended_ccm_exposure_us;
        double recommended_lsc_exposure_us;
        std::string evaluation_message;
    };

    struct ColorCharResults {
        double wb_r_gain = 1.0;
        double wb_g_gain = 1.0;
        double wb_b_gain = 1.0;
        double ccm_matrix[9] = {1,0,0, 0,1,0, 0,0,1}; // 3x3 row-major
        double ccm_offset[3] = {0,0,0};
        double color_temp = 6500.0;
        bool is_valid = false;
    };

    struct QuickCalibResults {
        double wb_r_gain = 1.0;
        double wb_g_gain = 1.0;
        double wb_b_gain = 1.0;
        double ccm_matrix[9] = {1,0,0, 0,1,0, 0,0,1}; // 3x3 row-major
        double ccm_offset[3] = {0,0,0};
        double gamma = 2.2;
        bool is_valid = false;
    };

    void SetSCMResults(const SCMResults& results);
    std::optional<SCMResults> GetSCMResults() const;

    void SetColorCharResults(const ColorCharResults& results);
    std::optional<ColorCharResults> GetColorCharResults() const;

    void SetQuickCalibResults(const QuickCalibResults& results);
    std::optional<QuickCalibResults> GetQuickCalibResults() const;

    std::string GetNodeConfigString(const std::string& node_name) const;

    void SetNodeResult(const std::string& node_name,
                       bool executed,
                       double processing_time_ms,
                       double output_mean = 0.0,
                       double output_std = 0.0);

    void AddCalibrationHistory(const std::string& type,
                               const std::string& description,
                               bool success,
                               double duration_ms,
                               const std::string& notes = "");

    std::string ToString() const;
    bool Reload();

private:
    SessionConfig();
    ~SessionConfig() = default;
    SessionConfig(const SessionConfig&) = delete;
    SessionConfig& operator=(const SessionConfig&) = delete;

    void EnsureLoaded();

    /// 递归获取或创建嵌套 table，返回最后一级父 table 和叶子 key
    toml::table& EnsureNestedTable(const std::string& dotted_key,
                                   std::string& out_leaf_key);

    mutable std::mutex mutex_;
    std::unique_ptr<toml::table> root_;
    std::string filepath_;
    bool loaded_ = false;
};

// ============ 模板实现 ============

template <typename T>
std::optional<T> SessionConfig::Get(const std::string& dotted_key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!loaded_ || !root_) return std::nullopt;
    try {
        auto node = root_->at_path(dotted_key);
        if (!node) return std::nullopt;
        return node.value<T>();
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

template <typename T>
T SessionConfig::GetOr(const std::string& dotted_key, const T& default_val) const {
    auto val = Get<T>(dotted_key);
    return val.value_or(default_val);
}

template <typename T>
void SessionConfig::Set(const std::string& dotted_key, const T& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    EnsureLoaded();
    if (!root_) return;
    try {
        std::string leaf_key;
        auto& parent = EnsureNestedTable(dotted_key, leaf_key);
        parent.insert_or_assign(leaf_key, value);
    } catch (const std::exception& e) {
        MV_LOG_ERROR(std::string("SessionConfig::Set failed: ") + e.what());
    }
}

template <typename T>
std::vector<T> SessionConfig::GetArray(const std::string& dotted_key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!loaded_ || !root_) return {};
    try {
        auto node = root_->at_path(dotted_key);
        if (!node || !node.is_array()) return {};
        std::vector<T> result;
        for (auto& elem : *node.as_array()) {
            auto val = elem.value<T>();
            if (val) result.push_back(*val);
        }
        return result;
    } catch (const std::exception&) {
        return {};
    }
}

template <typename T>
void SessionConfig::SetArray(const std::string& dotted_key, const std::vector<T>& values) {
    std::lock_guard<std::mutex> lock(mutex_);
    EnsureLoaded();
    if (!root_) return;
    try {
        toml::array arr;
        for (const auto& v : values) {
            arr.push_back(v);
        }
        std::string leaf_key;
        auto& parent = EnsureNestedTable(dotted_key, leaf_key);
        parent.insert_or_assign(leaf_key, std::move(arr));
    } catch (const std::exception& e) {
        MV_LOG_ERROR(std::string("SessionConfig::SetArray failed: ") + e.what());
    }
}

}  // namespace utils
}  // namespace mvtk
