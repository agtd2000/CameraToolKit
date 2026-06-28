#pragma once

#include <wx/wx.h>
#include <wx/clrpicker.h>
#include <opencv2/core.hpp>
#include <vector>
#include <memory>
#include <string>

#include "algo/image_node.h"
#include "wxui/image_canvas.h"
#include "utils/logger.h"

namespace mvtk {
namespace wxui {

// Pipeline 面板专用控件 ID
enum PipelinePanelIDs {
    PIPELINE_ID_LOAD_INPUT_BTN = wxID_HIGHEST + 2000,
    PIPELINE_ID_RUN_ALL_BTN,
    PIPELINE_ID_RUN_STEP_BTN,
    PIPELINE_ID_RESET_BTN,
    PIPELINE_ID_PREV_STEP_BTN,
    PIPELINE_ID_NODE_ENABLE_CHK_BASE,    // 节点启用复选框起始 ID（动态分配）
    PIPELINE_ID_NODE_CONFIG_BTN_BASE,    // 节点配置按钮起始 ID
    PIPELINE_ID_SAVE_OUTPUT_BTN
};

// 节点定义（顺序固定，对应标准 ISP 流程）
struct PipelineNodeInfo {
    std::string name;            // 节点名称
    std::string display_name;    // 显示名称
    std::string description;     // 节点说明
    std::shared_ptr<algo::ImageNode> node;  // 节点实例
    bool enabled;                // 是否启用
};

class PipelinePanel : public wxPanel {
public:
    PipelinePanel(wxWindow* parent);
    ~PipelinePanel() override;

private:
    // 输入图像
    cv::Mat input_image_;
    cv::Mat current_image_;        // 当前处理进度图像
    cv::Mat final_output_;         // 最终输出

    // Pipeline 节点列表（按标准顺序）
    std::vector<PipelineNodeInfo> nodes_;
    int current_step_;             // 当前执行到的步骤索引（-1 表示未开始）

    // 各节点实例指针（便于配置）
    std::shared_ptr<algo::OBNode> ob_node_;
    std::shared_ptr<algo::DPCNode> dpc_node_;
    std::shared_ptr<algo::LSCNode> lsc_node_;
    std::shared_ptr<algo::DenoiseNode> denoise_node_;
    std::shared_ptr<algo::DemosaicingNode> demosaic_node_;
    std::shared_ptr<algo::WhiteBalanceNode> wb_node_;
    std::shared_ptr<algo::CCMNode> ccm_node_;
    std::shared_ptr<algo::GammaNode> gamma_node_;

    // UI 控件
    wxTextCtrl* path_ctrl_;
    wxButton* load_btn_;
    wxButton* run_all_btn_;
    wxButton* run_step_btn_;
    wxButton* prev_step_btn_;
    wxButton* reset_btn_;
    wxButton* save_btn_;

    wxListBox* node_list_;                 // 节点列表
    wxCheckBox* enable_chk_;               // 当前节点启用复选框
    wxTextCtrl* info_text_;                // 节点说明文本
    wxPanel* config_panel_;                // 当前节点配置参数面板

    // 节点配置参数控件（通用）
    wxTextCtrl* param1_ctrl_;
    wxTextCtrl* param2_ctrl_;
    wxTextCtrl* param3_ctrl_;
    wxStaticText* param1_label_;
    wxStaticText* param2_label_;
    wxStaticText* param3_label_;

    ImageCanvas* input_canvas_;            // 输入图像预览
    ImageCanvas* output_canvas_;           // 当前节点输出预览

    wxStaticText* status_label_;           // 状态栏

    // 初始化
    void InitializePipeline();
    void BuildUI();
    void UpdateNodeList();
    void UpdateConfigPanel(int idx);
    void UpdateStatus(const wxString& msg, bool is_error = false);

    // 事件处理
    void OnLoadInput(wxCommandEvent& event);
    void OnRunAll(wxCommandEvent& event);
    void OnRunStep(wxCommandEvent& event);
    void OnPrevStep(wxCommandEvent& event);
    void OnReset(wxCommandEvent& event);
    void OnSaveOutput(wxCommandEvent& event);
    void OnNodeSelected(wxCommandEvent& event);
    void OnNodeEnabledChanged(wxCommandEvent& event);
    void OnApplyConfig(wxCommandEvent& event);

    // 处理辅助
    bool ProcessSingleStep(int idx);
    void RefreshOutputCanvas();

    DECLARE_EVENT_TABLE()
};

}  // namespace wxui
}  // namespace mvtk
