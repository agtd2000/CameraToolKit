#include "utils/session_config.h"

#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>

namespace mvtk {
namespace utils {

namespace {
constexpr const char* DEFAULT_CONFIG_PATH = "config/pipeline_session.toml";
constexpr const char* HISTORY_CONFIG_PATH = "config/pipeline_session_history.toml";

std::string CurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
    return oss.str();
}
}  // namespace

SessionConfig& SessionConfig::GetInstance() {
    static SessionConfig instance;
    return instance;
}

SessionConfig::SessionConfig() {
    root_ = std::make_unique<toml::table>();
}

void SessionConfig::EnsureLoaded() {
    if (!loaded_) {
        Load(DEFAULT_CONFIG_PATH);
    }
}

bool SessionConfig::Load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!filepath.empty()) {
        filepath_ = filepath;
    } else if (filepath_.empty()) {
        filepath_ = DEFAULT_CONFIG_PATH;
    }
    try {
        std::ifstream ifs(filepath_);
        if (!ifs.is_open()) {
            MV_LOG_WARN("SessionConfig: config file not found, using empty config: " + filepath_);
            root_ = std::make_unique<toml::table>();
        } else {
            std::ostringstream oss;
            oss << ifs.rdbuf();
            auto result = toml::parse(oss.str());
            root_ = std::make_unique<toml::table>(std::move(result));
        }

        // Load history from separate file
        std::ifstream history_ifs(HISTORY_CONFIG_PATH);
        if (history_ifs.is_open()) {
            try {
                std::ostringstream history_oss;
                history_oss << history_ifs.rdbuf();
                auto history_result = toml::parse(history_oss.str());
                auto history_calibration = history_result.at_path("history.calibration");
                if (history_calibration && history_calibration.is_array()) {
                    std::string leaf;
                    auto& parent = EnsureNestedTable("history.calibration", leaf);
                    parent.insert_or_assign(leaf, std::move(*history_calibration.as_array()));
                }
                MV_LOG_INFO("SessionConfig: loaded history from " + std::string(HISTORY_CONFIG_PATH));
            } catch (const std::exception&) {
                MV_LOG_WARN("SessionConfig: failed to load history file");
            }
        }

        loaded_ = true;
        MV_LOG_INFO("SessionConfig: loaded config file " + filepath_);
        return true;
    } catch (const std::exception& e) {
        MV_LOG_ERROR(std::string("SessionConfig: load failed: ") + e.what());
        root_ = std::make_unique<toml::table>();
        loaded_ = true;
        return false;
    }
}

bool SessionConfig::Save() {
    return SaveAs(filepath_.empty() ? DEFAULT_CONFIG_PATH : filepath_);
}

bool SessionConfig::SaveAs(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!root_) {
        MV_LOG_ERROR("SessionConfig: not loaded, cannot save");
        return false;
    }
    try {
        // Save history.calibration to separate file
        auto history_node = root_->at_path("history.calibration");
        if (history_node && history_node.is_array()) {
            toml::table history_table;
            toml::array history_copy = *history_node.as_array();
            history_table.insert("history", toml::table{ {"calibration", std::move(history_copy)} });
            std::ofstream history_ofs(HISTORY_CONFIG_PATH);
            if (history_ofs.is_open()) {
                history_ofs << toml::toml_formatter{ history_table };
                MV_LOG_INFO("SessionConfig: history saved to " + std::string(HISTORY_CONFIG_PATH));
            }
        }

        // Create a copy of root_ without history.calibration for main config file
        toml::table save_table = *root_;
        auto history_table_node = save_table.at_path("history");
        if (history_table_node && history_table_node.is_table()) {
            history_table_node.as_table()->erase("calibration");
            if (history_table_node.as_table()->empty()) {
                save_table.erase("history");
            }
        }

        std::ofstream ofs(filepath);
        if (!ofs.is_open()) {
            MV_LOG_ERROR("SessionConfig: cannot open file for writing: " + filepath);
            return false;
        }
        ofs << toml::toml_formatter{ save_table };
        filepath_ = filepath;
        MV_LOG_INFO("SessionConfig: saved to " + filepath);
        return true;
    } catch (const std::exception& e) {
        MV_LOG_ERROR(std::string("SessionConfig: save failed: ") + e.what());
        return false;
    }
}

void SessionConfig::SetSensorInfo(const std::string& manufacturer,
                                   const std::string& model,
                                   const std::string& serial_number,
                                   int bit_depth,
                                   int max_digital_value,
                                   const std::string& bayer_pattern,
                                   int resolution_width,
                                   int resolution_height) {
    std::lock_guard<std::mutex> lock(mutex_);
    EnsureLoaded();
    if (!root_) return;
    try {
        std::string leaf;
        auto set_field = [&](const std::string& key, auto&& val) {
            std::string lk;
            auto& p = EnsureNestedTable(key, lk);
            p.insert_or_assign(lk, std::forward<decltype(val)>(val));
        };
        set_field("sensor.manufacturer", manufacturer);
        set_field("sensor.model", model);
        set_field("sensor.serial_number", serial_number);
        set_field("sensor.bit_depth", bit_depth);
        set_field("sensor.max_digital_value", max_digital_value);
        set_field("sensor.bayer_pattern", bayer_pattern);
        set_field("sensor.resolution_width", resolution_width);
        set_field("sensor.resolution_height", resolution_height);
    } catch (const std::exception& e) {
        MV_LOG_ERROR(std::string("SessionConfig::SetSensorInfo failed: ") + e.what());
    }
}

void SessionConfig::SetSCMResults(const SCMResults& r) {
    Set("scm.system_gain_e_per_dn", r.system_gain_e_per_dn);
    Set("scm.system_gain_dn_per_e", r.system_gain_dn_per_e);
    Set("scm.dark_noise_e", r.dark_noise_e);
    Set("scm.dark_noise_dn", r.dark_noise_dn);
    Set("scm.dark_signal_dn", r.dark_signal_dn);
    Set("scm.dark_current_e_per_sec", r.dark_current_e_per_sec);
    Set("scm.full_well_capacity_e", r.full_well_capacity_e);
    Set("scm.full_well_capacity_dn", r.full_well_capacity_dn);
    Set("scm.dynamic_range_db", r.dynamic_range_db);
    Set("scm.dynamic_range_bits", r.dynamic_range_bits);
    Set("scm.snr_at_50_percent_db", r.snr_at_50_percent_db);
    Set("scm.peak_snr_db", r.peak_snr_db);
    Set("scm.is_valid", r.is_valid);
    Set("scm.is_snr_sufficient_for_ccm", r.is_snr_sufficient_for_ccm);
    Set("scm.recommended_ccm_exposure_us", r.recommended_ccm_exposure_us);
    Set("scm.recommended_lsc_exposure_us", r.recommended_lsc_exposure_us);
    Set("scm.evaluation_message", r.evaluation_message);
}

std::optional<SessionConfig::SCMResults> SessionConfig::GetSCMResults() const {
    SCMResults r;
    auto get_or_fail = [&](const std::string& key, double& out) -> bool {
        auto v = Get<double>(key);
        if (!v) return false;
        out = *v;
        return true;
    };
    bool ok = true;
    ok &= get_or_fail("scm.system_gain_e_per_dn", r.system_gain_e_per_dn);
    ok &= get_or_fail("scm.system_gain_dn_per_e", r.system_gain_dn_per_e);
    ok &= get_or_fail("scm.dark_noise_e", r.dark_noise_e);
    ok &= get_or_fail("scm.dark_noise_dn", r.dark_noise_dn);
    ok &= get_or_fail("scm.dark_signal_dn", r.dark_signal_dn);
    ok &= get_or_fail("scm.dark_current_e_per_sec", r.dark_current_e_per_sec);
    ok &= get_or_fail("scm.full_well_capacity_e", r.full_well_capacity_e);
    ok &= get_or_fail("scm.full_well_capacity_dn", r.full_well_capacity_dn);
    ok &= get_or_fail("scm.dynamic_range_db", r.dynamic_range_db);
    ok &= get_or_fail("scm.dynamic_range_bits", r.dynamic_range_bits);
    ok &= get_or_fail("scm.snr_at_50_percent_db", r.snr_at_50_percent_db);
    ok &= get_or_fail("scm.peak_snr_db", r.peak_snr_db);
    r.is_valid = GetOr<bool>("scm.is_valid", false);
    r.is_snr_sufficient_for_ccm = GetOr<bool>("scm.is_snr_sufficient_for_ccm", false);
    r.recommended_ccm_exposure_us = GetOr<double>("scm.recommended_ccm_exposure_us", 0.0);
    r.recommended_lsc_exposure_us = GetOr<double>("scm.recommended_lsc_exposure_us", 0.0);
    r.evaluation_message = GetOr<std::string>("scm.evaluation_message", "");
    if (!ok) return std::nullopt;
    return r;
}

void SessionConfig::SetColorCharResults(const ColorCharResults& results) {
    Set("color_char.wb_r_gain", results.wb_r_gain);
    Set("color_char.wb_g_gain", results.wb_g_gain);
    Set("color_char.wb_b_gain", results.wb_b_gain);
    SetArray<double>("color_char.ccm_row_0", std::vector<double>{results.ccm_matrix[0], results.ccm_matrix[1], results.ccm_matrix[2]});
    SetArray<double>("color_char.ccm_row_1", std::vector<double>{results.ccm_matrix[3], results.ccm_matrix[4], results.ccm_matrix[5]});
    SetArray<double>("color_char.ccm_row_2", std::vector<double>{results.ccm_matrix[6], results.ccm_matrix[7], results.ccm_matrix[8]});
    SetArray<double>("color_char.ccm_offset", std::vector<double>{results.ccm_offset[0], results.ccm_offset[1], results.ccm_offset[2]});
    Set("color_char.color_temp", results.color_temp);
    Set("color_char.is_valid", results.is_valid);
}

std::optional<SessionConfig::ColorCharResults> SessionConfig::GetColorCharResults() const {
    auto is_valid = Get<bool>("color_char.is_valid");
    if (!is_valid || !*is_valid) return std::nullopt;

    ColorCharResults results;
    results.wb_r_gain = GetOr<double>("color_char.wb_r_gain", 1.0);
    results.wb_g_gain = GetOr<double>("color_char.wb_g_gain", 1.0);
    results.wb_b_gain = GetOr<double>("color_char.wb_b_gain", 1.0);

    auto row0 = GetArray<double>("color_char.ccm_row_0");
    auto row1 = GetArray<double>("color_char.ccm_row_1");
    auto row2 = GetArray<double>("color_char.ccm_row_2");
    if (row0.size() == 3) { results.ccm_matrix[0] = row0[0]; results.ccm_matrix[1] = row0[1]; results.ccm_matrix[2] = row0[2]; }
    if (row1.size() == 3) { results.ccm_matrix[3] = row1[0]; results.ccm_matrix[4] = row1[1]; results.ccm_matrix[5] = row1[2]; }
    if (row2.size() == 3) { results.ccm_matrix[6] = row2[0]; results.ccm_matrix[7] = row2[1]; results.ccm_matrix[8] = row2[2]; }

    auto offset = GetArray<double>("color_char.ccm_offset");
    if (offset.size() == 3) { results.ccm_offset[0] = offset[0]; results.ccm_offset[1] = offset[1]; results.ccm_offset[2] = offset[2]; }

    results.color_temp = GetOr<double>("color_char.color_temp", 6500.0);
    results.is_valid = true;
    return results;
}

void SessionConfig::SetQuickCalibResults(const QuickCalibResults& results) {
    Set("quick_calib.wb_r_gain", results.wb_r_gain);
    Set("quick_calib.wb_g_gain", results.wb_g_gain);
    Set("quick_calib.wb_b_gain", results.wb_b_gain);
    SetArray<double>("quick_calib.ccm_row_0", std::vector<double>{results.ccm_matrix[0], results.ccm_matrix[1], results.ccm_matrix[2]});
    SetArray<double>("quick_calib.ccm_row_1", std::vector<double>{results.ccm_matrix[3], results.ccm_matrix[4], results.ccm_matrix[5]});
    SetArray<double>("quick_calib.ccm_row_2", std::vector<double>{results.ccm_matrix[6], results.ccm_matrix[7], results.ccm_matrix[8]});
    SetArray<double>("quick_calib.ccm_offset", std::vector<double>{results.ccm_offset[0], results.ccm_offset[1], results.ccm_offset[2]});
    Set("quick_calib.gamma", results.gamma);
    Set("quick_calib.is_valid", results.is_valid);
}

std::optional<SessionConfig::QuickCalibResults> SessionConfig::GetQuickCalibResults() const {
    auto is_valid = Get<bool>("quick_calib.is_valid");
    if (!is_valid || !*is_valid) return std::nullopt;

    QuickCalibResults results;
    results.wb_r_gain = GetOr<double>("quick_calib.wb_r_gain", 1.0);
    results.wb_g_gain = GetOr<double>("quick_calib.wb_g_gain", 1.0);
    results.wb_b_gain = GetOr<double>("quick_calib.wb_b_gain", 1.0);

    auto row0 = GetArray<double>("quick_calib.ccm_row_0");
    auto row1 = GetArray<double>("quick_calib.ccm_row_1");
    auto row2 = GetArray<double>("quick_calib.ccm_row_2");
    if (row0.size() == 3) { results.ccm_matrix[0] = row0[0]; results.ccm_matrix[1] = row0[1]; results.ccm_matrix[2] = row0[2]; }
    if (row1.size() == 3) { results.ccm_matrix[3] = row1[0]; results.ccm_matrix[4] = row1[1]; results.ccm_matrix[5] = row1[2]; }
    if (row2.size() == 3) { results.ccm_matrix[6] = row2[0]; results.ccm_matrix[7] = row2[1]; results.ccm_matrix[8] = row2[2]; }

    auto offset = GetArray<double>("quick_calib.ccm_offset");
    if (offset.size() == 3) { results.ccm_offset[0] = offset[0]; results.ccm_offset[1] = offset[1]; results.ccm_offset[2] = offset[2]; }

    results.gamma = GetOr<double>("quick_calib.gamma", 2.2);
    results.is_valid = true;
    return results;
}

std::string SessionConfig::GetNodeConfigString(const std::string& node_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!loaded_ || !root_) return "";
    try {
        auto node = root_->at_path("pipeline.nodes." + node_name);
        if (!node) return "";
        std::ostringstream oss;
        oss << toml::toml_formatter{ *node.node() };
        return oss.str();
    } catch (const std::exception&) {
        return "";
    }
}

void SessionConfig::SetNodeResult(const std::string& node_name,
                                   bool executed,
                                   double processing_time_ms,
                                   double output_mean,
                                   double output_std) {
    std::lock_guard<std::mutex> lock(mutex_);
    EnsureLoaded();
    if (!root_) return;
    try {
        std::string base = "pipeline.nodes." + node_name;
        auto set_field = [&](const std::string& key, auto&& val) {
            std::string leaf;
            auto& parent = EnsureNestedTable(key, leaf);
            parent.insert_or_assign(leaf, std::forward<decltype(val)>(val));
        };
        set_field(base + ".executed", executed);
        set_field(base + ".processing_time_ms", processing_time_ms);
        set_field(base + ".output_mean", output_mean);
        set_field(base + ".output_std", output_std);
        set_field(base + ".last_run_at", CurrentTimestamp());
    } catch (const std::exception& e) {
        MV_LOG_ERROR(std::string("SessionConfig::SetNodeResult failed: ") + e.what());
    }
}

void SessionConfig::AddCalibrationHistory(const std::string& type,
                                          const std::string& description,
                                          bool success,
                                          double duration_ms,
                                          const std::string& notes) {
    std::lock_guard<std::mutex> lock(mutex_);
    EnsureLoaded();
    if (!root_) return;
    try {
        toml::table entry;
        entry.insert("timestamp", CurrentTimestamp());
        entry.insert("type", type);
        entry.insert("description", description);
        entry.insert("success", success);
        entry.insert("duration_ms", duration_ms);
        entry.insert("notes", notes);

        auto existing = root_->at_path("history.calibration");
        toml::array arr;
        if (existing && existing.is_array()) {
            arr = std::move(*existing.as_array());
        }
        arr.push_back(std::move(entry));

        std::string leaf;
        auto& parent = EnsureNestedTable("history.calibration", leaf);
        parent.insert_or_assign(leaf, std::move(arr));
    } catch (const std::exception& e) {
        MV_LOG_ERROR(std::string("SessionConfig::AddCalibrationHistory failed: ") + e.what());
    }
}

toml::table& SessionConfig::EnsureNestedTable(const std::string& dotted_key,
                                              std::string& out_leaf_key) {
    if (!root_) {
        root_ = std::make_unique<toml::table>();
    }
    auto pos = dotted_key.find_last_of('.');
    if (pos == std::string::npos) {
        out_leaf_key = dotted_key;
        return *root_;
    }
    out_leaf_key = dotted_key.substr(pos + 1);
    std::string prefix = dotted_key.substr(0, pos);

    toml::table* current = root_.get();
    size_t start = 0;
    while (start <= prefix.size()) {
        size_t end = prefix.find('.', start);
        std::string segment = (end == std::string::npos)
                                  ? prefix.substr(start)
                                  : prefix.substr(start, end - start);
        if (segment.empty()) break;

        auto node = current->at_path(segment);
        if (!node || !node.is_table()) {
            toml::table new_table;
            current->insert_or_assign(segment, std::move(new_table));
            node = current->at_path(segment);
        }
        current = node.as_table();
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return *current;
}

std::string SessionConfig::ToString() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!loaded_ || !root_) return "";
    std::ostringstream oss;
    oss << toml::toml_formatter{ *root_ };
    return oss.str();
}

bool SessionConfig::Reload() {
    return Load(filepath_);
}

}  // namespace utils
}  // namespace mvtk
