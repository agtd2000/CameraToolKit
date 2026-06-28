#include "wxui/pipeline_panel.h"
#include "style_defs.h"
#include "utils/image_io.h"
#include "utils/session_config.h"
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace mvtk {
namespace wxui {

// Button ID for "Apply" on the config panel
constexpr int PIPELINE_ID_APPLY_CONFIG_BTN = wxID_HIGHEST + 2100;

BEGIN_EVENT_TABLE(PipelinePanel, wxPanel)
    EVT_BUTTON(PIPELINE_ID_LOAD_INPUT_BTN, PipelinePanel::OnLoadInput)
    EVT_BUTTON(PIPELINE_ID_RUN_ALL_BTN, PipelinePanel::OnRunAll)
    EVT_BUTTON(PIPELINE_ID_RUN_STEP_BTN, PipelinePanel::OnRunStep)
    EVT_BUTTON(PIPELINE_ID_PREV_STEP_BTN, PipelinePanel::OnPrevStep)
    EVT_BUTTON(PIPELINE_ID_RESET_BTN, PipelinePanel::OnReset)
    EVT_BUTTON(PIPELINE_ID_SAVE_OUTPUT_BTN, PipelinePanel::OnSaveOutput)
    EVT_BUTTON(PIPELINE_ID_APPLY_CONFIG_BTN, PipelinePanel::OnApplyConfig)
    EVT_LISTBOX(wxID_ANY, PipelinePanel::OnNodeSelected)
    EVT_CHECKBOX(wxID_ANY, PipelinePanel::OnNodeEnabledChanged)
END_EVENT_TABLE()

PipelinePanel::PipelinePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY),
      current_step_(-1),
      path_ctrl_(nullptr), load_btn_(nullptr),
      run_all_btn_(nullptr), run_step_btn_(nullptr),
      prev_step_btn_(nullptr), reset_btn_(nullptr), save_btn_(nullptr),
      node_list_(nullptr), enable_chk_(nullptr), info_text_(nullptr),
      config_panel_(nullptr),
      param1_ctrl_(nullptr), param2_ctrl_(nullptr), param3_ctrl_(nullptr),
      param1_label_(nullptr), param2_label_(nullptr), param3_label_(nullptr),
      input_canvas_(nullptr), output_canvas_(nullptr),
      status_label_(nullptr) {

    SetBackgroundColour(Style::NEU_BG_COLOR);

    InitializePipeline();
    BuildUI();
    UpdateNodeList();
    UpdateStatus("Ready. Please load an input image.");
}

PipelinePanel::~PipelinePanel() = default;

void PipelinePanel::InitializePipeline() {
    // Create 8 standard node instances in ISP-standard order
    ob_node_ = std::make_shared<algo::OBNode>();
    dpc_node_ = std::make_shared<algo::DPCNode>();
    lsc_node_ = std::make_shared<algo::LSCNode>();
    denoise_node_ = std::make_shared<algo::DenoiseNode>();
    demosaic_node_ = std::make_shared<algo::DemosaicingNode>();
    wb_node_ = std::make_shared<algo::WhiteBalanceNode>();
    ccm_node_ = std::make_shared<algo::CCMNode>();
    gamma_node_ = std::make_shared<algo::GammaNode>();

    // Disable some nodes by default (require user-configured parameters)
    lsc_node_->SetEnabled(false);       // requires gain map
    wb_node_->SetEnabled(false);        // requires WB gains
    ccm_node_->SetEnabled(false);       // requires CCM matrix

    // ============ Load SCM results from TOML to auto-configure node parameters ============
    auto& cfg = utils::SessionConfig::GetInstance();
    cfg.Load();
    auto scm = cfg.GetSCMResults();
    if (scm) {
        // Node 0 (OB): use SCM dark signal as black level, configure dark current compensation
        ob_node_->SetBlackLevel(scm->dark_signal_dn > 0 ? scm->dark_signal_dn : 100.0);
        if (scm->dark_current_e_per_sec > 0 && scm->system_gain_dn_per_e > 0) {
            ob_node_->SetDarkCurrentCompensation(scm->dark_current_e_per_sec,
                                                  scm->system_gain_dn_per_e, 0.001); // default 1ms
        }

        // Node 1 (DPC): use SCM dark noise to compute absolute threshold
        if (scm->dark_noise_dn > 0) {
            dpc_node_->SetDarkNoise(scm->dark_noise_dn);
        }

        // Node 3 (Denoise): save noise-variance model parameters (for debugging)
        // Noise model: sigma^2(I) = dark_noise^2 + system_gain * I

        // Node 6 (CCM): use SCM 50% SNR to decide whether to enable
        // Node 7 (Gamma): use full-well capacity and dynamic range to configure mapping range
        if (scm->full_well_capacity_dn > 0) {
            double effective_max = scm->full_well_capacity_dn - scm->dark_signal_dn;
            gamma_node_->SetInputRange(0.0, effective_max > 0 ? effective_max : 65535.0);
        }
        if (scm->dynamic_range_bits > 0 && scm->dynamic_range_bits < 16) {
            // Dynamic range info can be used for debugging
        }
        // Write SCM recommended exposures to TOML pipeline section (for debugging reference)
        cfg.Set("pipeline.recommended_ccm_exposure_us", scm->recommended_ccm_exposure_us);
        cfg.Set("pipeline.recommended_lsc_exposure_us", scm->recommended_lsc_exposure_us);
    }

    nodes_.clear();
    nodes_.push_back({"OB",          "N0: OB Subtraction",
                      "Optical black / black level subtraction. SCM inputs: dark signal, dark current, system gain.",
                      ob_node_, true});
    nodes_.push_back({"DPC",         "N1: DPC Defective Pixel Correction",
                      "Statistical threshold (5-sigma) defect detection and neighbor interpolation. SCM inputs: dark noise.",
                      dpc_node_, true});
    nodes_.push_back({"LSC",         "N2: LSC Flat-Field Correction",
                      "Lens shading correction, removes vignetting. SCM inputs: full-well capacity, recommended LSC exposure.",
                      lsc_node_, false});
    nodes_.push_back({"Denoise",     "N3: Denoise",
                      "Noise-variance-model based denoising. SCM inputs: system gain, dark noise.",
                      denoise_node_, true});
    nodes_.push_back({"Demosaicing", "N4: Demosaicing",
                      "Bayer pattern interpolation to RGB. SCM inputs: none (depends on N0/N2).",
                      demosaic_node_, true});
    nodes_.push_back({"WhiteBalance","N5: White Balance",
                      "RGB channel gain application. SCM inputs: none (depends on N0/N2).",
                      wb_node_, false});
    nodes_.push_back({"CCM",         "N6: CCM Color Correction",
                      "3x3 color correction matrix. SCM inputs: 50% SNR, recommended CCM exposure.",
                      ccm_node_, false});
    nodes_.push_back({"Gamma",       "N7: Gamma / Tone Mapping",
                      "High bit-depth mapped to 8-bit for display. SCM inputs: dynamic range, full-well capacity.",
                      gamma_node_, true});

    // Sync enabled state
    for (auto& info : nodes_) {
        info.enabled = info.node->IsEnabled();
    }

    // Read each node's enabled state from TOML (overrides defaults)
    for (auto& info : nodes_) {
        std::string key = "pipeline.nodes." + info.name + ".enabled";
        auto enabled = cfg.Get<bool>(key);
        if (enabled) {
            info.enabled = *enabled;
            info.node->SetEnabled(*enabled);
        }
    }
}

void PipelinePanel::BuildUI() {
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    // ============ Top toolbar ============
    wxPanel* toolbar = new wxPanel(this, wxID_ANY);
    toolbar->SetBackgroundColour(Style::NEU_BG_COLOR);
    wxBoxSizer* toolbar_sizer = new wxBoxSizer(wxHORIZONTAL);

    auto make_btn = [this, toolbar](const wxString& label, int id) {
        wxButton* btn = new wxButton(toolbar, id, label);
        Style::ApplyNeumorphicStyle(btn);
        return btn;
    };

    wxStaticText* path_label = new wxStaticText(toolbar, wxID_ANY, "Input:");
    Style::ApplyNeumorphicStyle(path_label);
    toolbar_sizer->Add(path_label, 0, wxALL | wxALIGN_CENTER_VERTICAL, Style::SPACING_SMALL);

    path_ctrl_ = new wxTextCtrl(toolbar, wxID_ANY, "", wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(path_ctrl_);
    toolbar_sizer->Add(path_ctrl_, 1, wxALL | wxALIGN_CENTER_VERTICAL, Style::SPACING_SMALL);

    load_btn_ = make_btn("Load", PIPELINE_ID_LOAD_INPUT_BTN);
    toolbar_sizer->Add(load_btn_, 0, wxALL, Style::SPACING_SMALL);

    run_all_btn_ = make_btn("Run All", PIPELINE_ID_RUN_ALL_BTN);
    toolbar_sizer->Add(run_all_btn_, 0, wxALL, Style::SPACING_SMALL);

    run_step_btn_ = make_btn("Step", PIPELINE_ID_RUN_STEP_BTN);
    toolbar_sizer->Add(run_step_btn_, 0, wxALL, Style::SPACING_SMALL);

    prev_step_btn_ = make_btn("Prev", PIPELINE_ID_PREV_STEP_BTN);
    toolbar_sizer->Add(prev_step_btn_, 0, wxALL, Style::SPACING_SMALL);

    reset_btn_ = make_btn("Reset", PIPELINE_ID_RESET_BTN);
    toolbar_sizer->Add(reset_btn_, 0, wxALL, Style::SPACING_SMALL);

    save_btn_ = make_btn("Save Output", PIPELINE_ID_SAVE_OUTPUT_BTN);
    toolbar_sizer->Add(save_btn_, 0, wxALL, Style::SPACING_SMALL);

    toolbar->SetSizer(toolbar_sizer);
    main_sizer->Add(toolbar, 0, wxEXPAND | wxALL, Style::SPACING_SMALL);

    // ============ Middle three-column area ============
    wxPanel* middle_panel = new wxPanel(this, wxID_ANY);
    middle_panel->SetBackgroundColour(Style::NEU_BG_COLOR);
    wxBoxSizer* middle_sizer = new wxBoxSizer(wxHORIZONTAL);

    // Left column: node list
    wxPanel* left_panel = new wxPanel(middle_panel, wxID_ANY);
    left_panel->SetBackgroundColour(Style::NEU_BG_COLOR);
    wxBoxSizer* left_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* list_title = new wxStaticText(left_panel, wxID_ANY, "Pipeline Nodes");
    Style::ApplyNeumorphicStyle(list_title, true);
    left_sizer->Add(list_title, 0, wxALL, Style::SPACING_SMALL);

    node_list_ = new wxListBox(left_panel, wxID_ANY, wxDefaultPosition, wxSize(200, 200));
    Style::ApplyNeumorphicStyle(node_list_);
    left_sizer->Add(node_list_, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);

    enable_chk_ = new wxCheckBox(left_panel, wxID_ANY, "Enable this node");
    Style::ApplyNeumorphicStyle(enable_chk_);
    left_sizer->Add(enable_chk_, 0, wxALL, Style::SPACING_SMALL);

    info_text_ = new wxTextCtrl(left_panel, wxID_ANY, "",
                                wxDefaultPosition, wxSize(200, 100),
                                wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
    Style::ApplyNeumorphicStyle(info_text_);
    left_sizer->Add(info_text_, 0, wxEXPAND | wxALL, Style::SPACING_SMALL);

    left_panel->SetSizer(left_sizer);
    middle_sizer->Add(left_panel, 0, wxEXPAND | wxALL, Style::SPACING_SMALL);

    // Center column: node parameter configuration
    wxPanel* center_panel = new wxPanel(middle_panel, wxID_ANY);
    center_panel->SetBackgroundColour(Style::NEU_BG_COLOR);
    wxBoxSizer* center_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* config_title = new wxStaticText(center_panel, wxID_ANY, "Node Parameters");
    Style::ApplyNeumorphicStyle(config_title, true);
    center_sizer->Add(config_title, 0, wxALL, Style::SPACING_SMALL);

    config_panel_ = new wxPanel(center_panel, wxID_ANY);
    config_panel_->SetBackgroundColour(Style::NEU_BG_COLOR);
    wxFlexGridSizer* config_grid = new wxFlexGridSizer(3, 2, 5, 5);

    param1_label_ = new wxStaticText(config_panel_, wxID_ANY, "Param 1:");
    Style::ApplyNeumorphicStyle(param1_label_);
    param1_ctrl_ = new wxTextCtrl(config_panel_, wxID_ANY, "");
    Style::ApplyNeumorphicStyle(param1_ctrl_);
    config_grid->Add(param1_label_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    config_grid->Add(param1_ctrl_, 1, wxEXPAND | wxALL, 2);

    param2_label_ = new wxStaticText(config_panel_, wxID_ANY, "Param 2:");
    Style::ApplyNeumorphicStyle(param2_label_);
    param2_ctrl_ = new wxTextCtrl(config_panel_, wxID_ANY, "");
    Style::ApplyNeumorphicStyle(param2_ctrl_);
    config_grid->Add(param2_label_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    config_grid->Add(param2_ctrl_, 1, wxEXPAND | wxALL, 2);

    param3_label_ = new wxStaticText(config_panel_, wxID_ANY, "Param 3:");
    Style::ApplyNeumorphicStyle(param3_label_);
    param3_ctrl_ = new wxTextCtrl(config_panel_, wxID_ANY, "");
    Style::ApplyNeumorphicStyle(param3_ctrl_);
    config_grid->Add(param3_label_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    config_grid->Add(param3_ctrl_, 1, wxEXPAND | wxALL, 2);

    config_grid->AddGrowableCol(1, 1);
    config_panel_->SetSizer(config_grid);
    center_sizer->Add(config_panel_, 0, wxEXPAND | wxALL, Style::SPACING_SMALL);

    wxButton* apply_btn = new wxButton(center_panel, PIPELINE_ID_APPLY_CONFIG_BTN, "Apply");
    Style::ApplyNeumorphicStyle(apply_btn);
    center_sizer->Add(apply_btn, 0, wxALL, Style::SPACING_SMALL);

    wxStaticText* hint = new wxStaticText(center_panel, wxID_ANY,
        "Hints:\n"
        "* OB: P1=Black Level (DN), P2=Dark Current (e-/s), P3=Exposure (us)\n"
        "* DPC: P1=Threshold Sigma, P2=Dark Noise (DN), P3=Kernel Size (3/5)\n"
        "* Denoise: P1=Sigma Strength\n"
        "* Demosaicing: P1=Pattern (BG/GB/RG/GR)\n"
        "* WB: P1=R Gain, P2=G Gain, P3=B Gain\n"
        "* CCM: P1=Gamma\n"
        "* Gamma: P1=Gamma Value, P2=Input Min, P3=Input Max");
    hint->SetFont(Style::GetSansFont(8));
    hint->SetForegroundColour(Style::NEU_TEXT_LIGHT);
    center_sizer->Add(hint, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);

    center_panel->SetSizer(center_sizer);
    middle_sizer->Add(center_panel, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);

    // Right column: image preview (input + output stacked vertically)
    wxPanel* right_panel = new wxPanel(middle_panel, wxID_ANY);
    right_panel->SetBackgroundColour(Style::NEU_BG_COLOR);
    wxBoxSizer* right_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* input_title = new wxStaticText(right_panel, wxID_ANY, "Input Image");
    Style::ApplyNeumorphicStyle(input_title, true);
    right_sizer->Add(input_title, 0, wxALL, Style::SPACING_SMALL);

    input_canvas_ = new ImageCanvas(right_panel, wxID_ANY);
    input_canvas_->SetMinSize(wxSize(320, 240));
    right_sizer->Add(input_canvas_, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);

    wxStaticText* output_title = new wxStaticText(right_panel, wxID_ANY, "Current Node Output");
    Style::ApplyNeumorphicStyle(output_title, true);
    right_sizer->Add(output_title, 0, wxALL, Style::SPACING_SMALL);

    output_canvas_ = new ImageCanvas(right_panel, wxID_ANY);
    output_canvas_->SetMinSize(wxSize(320, 240));
    right_sizer->Add(output_canvas_, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);

    right_panel->SetSizer(right_sizer);
    middle_sizer->Add(right_panel, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);

    middle_panel->SetSizer(middle_sizer);
    main_sizer->Add(middle_panel, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);

    // ============ Bottom status bar ============
    wxPanel* status_panel = new wxPanel(this, wxID_ANY);
    status_panel->SetBackgroundColour(Style::NEU_BG_COLOR);
    wxBoxSizer* status_sizer = new wxBoxSizer(wxHORIZONTAL);
    status_label_ = new wxStaticText(status_panel, wxID_ANY, "Status: Ready");
    Style::ApplyNeumorphicStyle(status_label_);
    status_sizer->Add(status_label_, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);
    status_panel->SetSizer(status_sizer);
    main_sizer->Add(status_panel, 0, wxEXPAND | wxALL, Style::SPACING_SMALL);

    SetSizer(main_sizer);
}

void PipelinePanel::UpdateNodeList() {
    if (!node_list_) return;
    node_list_->Clear();
    for (size_t i = 0; i < nodes_.size(); ++i) {
        wxString item = wxString::Format("[%zu] %s %s",
            i, nodes_[i].display_name,
            nodes_[i].enabled ? "(ON)" : "(OFF)");
        node_list_->Append(item);
    }
    // Highlight current step
    if (current_step_ >= 0 && current_step_ < static_cast<int>(nodes_.size())) {
        node_list_->SetSelection(current_step_);
        UpdateConfigPanel(current_step_);
    } else if (!nodes_.empty()) {
        node_list_->SetSelection(0);
        UpdateConfigPanel(0);
    }
}

void PipelinePanel::UpdateConfigPanel(int idx) {
    if (idx < 0 || idx >= static_cast<int>(nodes_.size())) return;

    const auto& info = nodes_[idx];
    if (enable_chk_) {
        // Temporarily unbind to avoid triggering event
        enable_chk_->Unbind(wxEVT_CHECKBOX, &PipelinePanel::OnNodeEnabledChanged, this);
        enable_chk_->SetValue(info.enabled);
        enable_chk_->Bind(wxEVT_CHECKBOX, &PipelinePanel::OnNodeEnabledChanged, this);
    }
    if (info_text_) {
        info_text_->SetValue(info.description);
    }

    // Set parameter labels and default values based on node type
    auto set_param = [this](const wxString& l1, const wxString& v1,
                            const wxString& l2 = "", const wxString& v2 = "",
                            const wxString& l3 = "", const wxString& v3 = "") {
        param1_label_->SetLabel(l1);
        param1_ctrl_->SetValue(v1);
        param2_label_->SetLabel(l2);
        param2_ctrl_->SetValue(v2);
        param3_label_->SetLabel(l3);
        param3_ctrl_->SetValue(v3);
        param2_label_->Show(!l2.empty());
        param2_ctrl_->Show(!v2.empty());
        param3_label_->Show(!l3.empty());
        param3_ctrl_->Show(!v3.empty());
        config_panel_->Layout();
    };

    if (info.name == "OB") {
        set_param("Black Level (DN):", "100.0",
                  "Dark Current (e-/s):", "0.0",
                  "Exposure (us):", "1000.0");
    } else if (info.name == "DPC") {
        set_param("Threshold Sigma:", "5.0",
                  "Dark Noise (DN):", "30.0",
                  "Kernel Size (3/5):", "3");
    } else if (info.name == "LSC") {
        set_param("Gain Map Path:", "",
                  "Dark Field Path:", "", "");
    } else if (info.name == "Denoise") {
        set_param("Sigma Strength:", "1.0", "", "", "", "");
    } else if (info.name == "Demosaicing") {
        set_param("Bayer Pattern (BG/GB/RG/GR):", "BG", "", "", "", "");
    } else if (info.name == "WhiteBalance") {
        set_param("R Gain:", "1.0",
                  "G Gain:", "1.0",
                  "B Gain:", "1.0");
    } else if (info.name == "CCM") {
        set_param("Gamma:", "1.0", "", "", "", "");
    } else if (info.name == "Gamma") {
        set_param("Gamma Value:", "2.2",
                  "Input Min:", "0.0",
                  "Input Max:", "65535.0");
    }
}

void PipelinePanel::UpdateStatus(const wxString& msg, bool is_error) {
    if (status_label_) {
        if (is_error) {
            status_label_->SetForegroundColour(*wxRED);
        } else {
            status_label_->SetForegroundColour(Style::NEU_TEXT_COLOR);
        }
        status_label_->SetLabel("Status: " + msg);
    }
    if (is_error) {
        MV_LOG_ERROR(msg.ToUTF8().data());
    } else {
        MV_LOG_INFO(msg.ToUTF8().data());
    }
}

void PipelinePanel::OnLoadInput(wxCommandEvent& event) {
    wxString filter = "Image Files (*.png;*.jpg;*.bmp;*.tif;*.tiff)|*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff|All Files (*.*)|*.*";
    wxFileDialog dlg(this, "Select Input Image", "", "", filter, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    wxString path = dlg.GetPath();
    path_ctrl_->SetValue(path);

    // Use IMREAD_UNCHANGED to preserve 16-bit data
    std::string utf8_path = std::string(path.ToUTF8());
    input_image_ = utils::imreadUtf8(utf8_path, cv::IMREAD_UNCHANGED);

    if (input_image_.empty()) {
        UpdateStatus("Cannot load image: " + path, true);
        return;
    }

    current_image_ = input_image_.clone();
    final_output_ = cv::Mat();
    current_step_ = -1;

    if (input_canvas_) {
        input_canvas_->SetImage(input_image_);
    }
    if (output_canvas_) {
        output_canvas_->SetImage(input_image_);
    }

    UpdateNodeList();
    wxString msg = wxString::Format("Image loaded: %dx%d, %d channels, %d-bit",
        input_image_.cols, input_image_.rows,
        input_image_.channels(),
        input_image_.depth() == CV_16U ? 16 : (input_image_.depth() == CV_8U ? 8 : 32));
    UpdateStatus(msg);

    // ============ Write input image info to TOML ============
    auto& cfg = utils::SessionConfig::GetInstance();
    cfg.Set("pipeline.input_image_path", std::string(path.ToUTF8()));
    cfg.Set("pipeline.input_width", input_image_.cols);
    cfg.Set("pipeline.input_height", input_image_.rows);
    cfg.Set("pipeline.input_channels", input_image_.channels());
    cfg.Set("pipeline.input_bit_depth",
            input_image_.depth() == CV_16U ? 16 : (input_image_.depth() == CV_8U ? 8 : 32));
    cfg.Save();
}

void PipelinePanel::OnRunAll(wxCommandEvent& event) {
    if (input_image_.empty()) {
        UpdateStatus("Please load an input image first", true);
        return;
    }

    current_image_ = input_image_.clone();
    current_step_ = -1;

    int processed = 0;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (!nodes_[i].enabled) continue;
        if (!ProcessSingleStep(static_cast<int>(i))) {
            UpdateStatus(wxString::Format("Node %s failed", nodes_[i].name), true);
            return;
        }
        processed++;
    }

    final_output_ = current_image_.clone();
    RefreshOutputCanvas();

    // ============ Write final output statistics to TOML ============
    auto& cfg = utils::SessionConfig::GetInstance();
    cfg.Set("pipeline.enabled_nodes", processed);
    cfg.Set("pipeline.total_nodes", static_cast<int>(nodes_.size()));
    if (!final_output_.empty()) {
        cv::Scalar mean, stddev;
        cv::meanStdDev(final_output_, mean, stddev);
        cfg.Set("output.output_width", final_output_.cols);
        cfg.Set("output.output_height", final_output_.rows);
        cfg.Set("output.output_channels", final_output_.channels());
        cfg.Set("output.output_bit_depth",
                final_output_.depth() == CV_16U ? 16 :
                (final_output_.depth() == CV_8U ? 8 : 32));
        if (final_output_.channels() >= 3) {
            cfg.Set("output.output_mean_r", mean[2]);  // OpenCV BGR
            cfg.Set("output.output_mean_g", mean[1]);
            cfg.Set("output.output_mean_b", mean[0]);
            cfg.Set("output.output_std_r", stddev[2]);
            cfg.Set("output.output_std_g", stddev[1]);
            cfg.Set("output.output_std_b", stddev[0]);
        } else {
            cfg.Set("output.output_mean_r", mean[0]);
            cfg.Set("output.output_std_r", stddev[0]);
        }
    }
    cfg.AddCalibrationHistory("Pipeline", "Pipeline chain processing", true, 0.0,
                              "processed_nodes=" + std::to_string(processed));
    cfg.Save();

    UpdateStatus(wxString::Format("Pipeline done: %d nodes processed, results saved to TOML", processed));
}

void PipelinePanel::OnRunStep(wxCommandEvent& event) {
    if (input_image_.empty()) {
        UpdateStatus("Please load an input image first", true);
        return;
    }

    // Find next enabled node
    int next = current_step_ + 1;
    while (next < static_cast<int>(nodes_.size()) && !nodes_[next].enabled) {
        next++;
    }

    if (next >= static_cast<int>(nodes_.size())) {
        UpdateStatus("Reached end of pipeline");
        return;
    }

    if (!ProcessSingleStep(next)) {
        UpdateStatus(wxString::Format("Node %s failed", nodes_[next].name), true);
        return;
    }

    current_step_ = next;
    final_output_ = current_image_.clone();
    RefreshOutputCanvas();
    UpdateNodeList();
    UpdateStatus(wxString::Format("Completed node [%d] %s",
        next, nodes_[next].name));
}

void PipelinePanel::OnPrevStep(wxCommandEvent& event) {
    if (current_step_ < 0) {
        UpdateStatus("No step to go back to");
        return;
    }

    // Step back: reprocess from original image to previous enabled node
    int target = current_step_ - 1;
    while (target >= 0 && !nodes_[target].enabled) {
        target--;
    }

    current_image_ = input_image_.clone();
    current_step_ = -1;

    if (target >= 0) {
        for (int i = 0; i <= target; ++i) {
            if (!nodes_[i].enabled) continue;
            if (!ProcessSingleStep(i)) {
                UpdateStatus("Step back failed", true);
                return;
            }
            current_step_ = i;
        }
    }

    final_output_ = current_image_.clone();
    RefreshOutputCanvas();
    UpdateNodeList();
    UpdateStatus(target >= 0 ?
        wxString::Format("Stepped back to node [%d] %s", target, nodes_[target].name) :
        "Stepped back to input image");
}

void PipelinePanel::OnReset(wxCommandEvent& event) {
    if (input_image_.empty()) {
        UpdateStatus("No image to reset");
        return;
    }
    current_image_ = input_image_.clone();
    final_output_ = cv::Mat();
    current_step_ = -1;
    if (output_canvas_) {
        output_canvas_->SetImage(input_image_);
    }
    UpdateNodeList();
    UpdateStatus("Reset to input image");
}

void PipelinePanel::OnSaveOutput(wxCommandEvent& event) {
    if (final_output_.empty()) {
        UpdateStatus("No output to save", true);
        return;
    }

    wxString filter = "PNG Image (*.png)|*.png|BMP Image (*.bmp)|*.bmp|TIFF Image (*.tif)|*.tif";
    wxFileDialog dlg(this, "Save Output Image", "", "pipeline_output",
                     filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    wxString path = dlg.GetPath();
    if (cv::imwrite(std::string(path.ToUTF8()), final_output_)) {
        UpdateStatus("Output saved: " + path);
        // Write output path to TOML
        auto& cfg = utils::SessionConfig::GetInstance();
        cfg.Set("output.output_image_path", std::string(path.ToUTF8()));
        cfg.Save();
    } else {
        UpdateStatus("Save failed: " + path, true);
    }
}

void PipelinePanel::OnNodeSelected(wxCommandEvent& event) {
    int idx = event.GetSelection();
    if (idx >= 0 && idx < static_cast<int>(nodes_.size())) {
        UpdateConfigPanel(idx);
    }
}

void PipelinePanel::OnNodeEnabledChanged(wxCommandEvent& event) {
    int idx = node_list_ ? node_list_->GetSelection() : -1;
    if (idx < 0 || idx >= static_cast<int>(nodes_.size())) return;

    bool enabled = enable_chk_ ? enable_chk_->GetValue() : true;
    nodes_[idx].enabled = enabled;
    nodes_[idx].node->SetEnabled(enabled);
    UpdateNodeList();
    UpdateConfigPanel(idx);

    // ============ Sync enabled state to TOML ============
    auto& cfg = utils::SessionConfig::GetInstance();
    cfg.Set("pipeline.nodes." + nodes_[idx].name + ".enabled", enabled);
    cfg.Save();

    UpdateStatus(wxString::Format("Node %s %s",
        nodes_[idx].name, enabled ? "enabled" : "disabled"));
}

void PipelinePanel::OnApplyConfig(wxCommandEvent& event) {
    int idx = node_list_ ? node_list_->GetSelection() : -1;
    if (idx < 0 || idx >= static_cast<int>(nodes_.size())) return;

    const auto& info = nodes_[idx];
    double p1 = 0.0, p2 = 0.0, p3 = 0.0;
    param1_ctrl_->GetValue().ToDouble(&p1);
    param2_ctrl_->GetValue().ToDouble(&p2);
    param3_ctrl_->GetValue().ToDouble(&p3);

    if (info.name == "OB") {
        ob_node_->SetBlackLevel(p1);
        // p2=dark current, p3=exposure time (us), enable dark current compensation
        if (p2 > 0 && p3 > 0) {
            ob_node_->SetDarkCurrentCompensation(p2, 1.0, p3 / 1e6); // system gain default 1, exposure in seconds
        }
    } else if (info.name == "DPC") {
        dpc_node_->SetThresholdSigma(p1);
        dpc_node_->SetDarkNoise(p2);
        dpc_node_->SetKernelSize(static_cast<int>(p3));
    } else if (info.name == "Denoise") {
        denoise_node_->SetSigma(static_cast<float>(p1));
    } else if (info.name == "Demosaicing") {
        wxString mode = param1_ctrl_->GetValue().Upper();
        if (mode == "BG") demosaic_node_->SetPattern(algo::DemosaicingNode::BayerPattern::BG);
        else if (mode == "GB") demosaic_node_->SetPattern(algo::DemosaicingNode::BayerPattern::GB);
        else if (mode == "RG") demosaic_node_->SetPattern(algo::DemosaicingNode::BayerPattern::RG);
        else if (mode == "GR") demosaic_node_->SetPattern(algo::DemosaicingNode::BayerPattern::GR);
    } else if (info.name == "WhiteBalance") {
        wb_node_->SetGains(cv::Vec3f(static_cast<float>(p1),
                                      static_cast<float>(p2),
                                      static_cast<float>(p3)));
    } else if (info.name == "CCM") {
        // Simple example: use gamma as diagonal scaling
        cv::Mat m = cv::Mat::eye(3, 3, CV_32F) * p1;
        ccm_node_->SetMatrix(m);
    } else if (info.name == "Gamma") {
        gamma_node_->SetGamma(p1);
        gamma_node_->SetInputRange(p2, p3);
    }

    // ============ Sync parameters to TOML (for debugging and reproducibility) ============
    auto& cfg = utils::SessionConfig::GetInstance();
    std::string prefix = "pipeline.nodes." + info.name + ".";
    if (info.name == "OB") {
        cfg.Set(prefix + "black_level_dn", p1);
        cfg.Set(prefix + "dark_current_e_per_s", p2);
        cfg.Set(prefix + "exposure_time_us", p3);
        cfg.Set(prefix + "use_dark_current_compensation", (p2 > 0 && p3 > 0));
    } else if (info.name == "DPC") {
        cfg.Set(prefix + "threshold_sigma", p1);
        cfg.Set(prefix + "dark_noise_dn", p2);
        cfg.Set(prefix + "kernel_size", static_cast<int>(p3));
        cfg.Set(prefix + "absolute_threshold_dn", p1 * p2);
    } else if (info.name == "Denoise") {
        cfg.Set(prefix + "sigma", p1);
    } else if (info.name == "Demosaicing") {
        cfg.Set(prefix + "bayer_pattern", std::string(param1_ctrl_->GetValue().ToUTF8()));
    } else if (info.name == "WhiteBalance") {
        cfg.Set(prefix + "r_gain", p1);
        cfg.Set(prefix + "g_gain", p2);
        cfg.Set(prefix + "b_gain", p3);
    } else if (info.name == "CCM") {
        cfg.Set(prefix + "gamma", p1);
    } else if (info.name == "Gamma") {
        cfg.Set(prefix + "gamma_value", p1);
        cfg.Set(prefix + "input_min_dn", p2);
        cfg.Set(prefix + "input_max_dn", p3);
    }
    cfg.Save();

    UpdateStatus(wxString::Format("Node %s params applied and saved to TOML", info.name));
}

bool PipelinePanel::ProcessSingleStep(int idx) {
    if (idx < 0 || idx >= static_cast<int>(nodes_.size())) return false;
    if (current_image_.empty()) return false;

    const auto& info = nodes_[idx];
    cv::Mat before = current_image_.clone();

    // Record processing start time
    auto t_start = std::chrono::high_resolution_clock::now();

    try {
        // Process: call Process instead of ProcessInPlace because Demosaicing changes channel count
        cv::Mat output = info.node->Process(current_image_);
        if (output.empty()) {
            return false;
        }
        current_image_ = output;
        MV_LOG_INFO("Pipeline step [" + info.name + "] processed");
    } catch (const cv::Exception& e) {
        MV_LOG_ERROR(std::string("OpenCV error in node ") + info.name + ": " + e.what());
        return false;
    } catch (const std::exception& e) {
        MV_LOG_ERROR(std::string("Error in node ") + info.name + ": " + e.what());
        return false;
    }

    // Compute processing duration
    auto t_end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // Compute output image statistics (for debugging)
    double output_mean = 0.0, output_std = 0.0;
    if (!current_image_.empty()) {
        cv::Scalar mean, stddev;
        cv::meanStdDev(current_image_, mean, stddev);
        output_mean = mean[0];
        output_std = stddev[0];
    }

    // ============ Write node execution results to TOML ============
    auto& cfg = utils::SessionConfig::GetInstance();
    cfg.SetNodeResult(info.name, true, duration_ms, output_mean, output_std);

    // Write node-specific debug info
    if (info.name == "OB") {
        // OB node: record black level and dark current compensation parameters
        cfg.Set("pipeline.nodes.OB.black_level_dn", ob_node_->IsEnabled() ? 100.0 : 0.0);
    } else if (info.name == "DPC") {
        // DPC node: record absolute threshold
        cfg.Set("pipeline.nodes.DPC.absolute_threshold_dn",
                5.0 * 30.0);  // threshold_sigma * dark_noise
    } else if (info.name == "Demosaicing") {
        cfg.Set("pipeline.nodes.Demosaicing.output_channels", current_image_.channels());
    }

    return true;
}

void PipelinePanel::RefreshOutputCanvas() {
    if (output_canvas_) {
        output_canvas_->SetImage(current_image_);
    }
}

}  // namespace wxui
}  // namespace mvtk
