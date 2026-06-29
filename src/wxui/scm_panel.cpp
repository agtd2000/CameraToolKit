#include <fstream>
#include <sstream>
#include <algorithm>
#include <wx/dir.h>
#include <wx/filename.h>
#include "wxui/scm_panel.h"
#include "neumorphic_panel.h"
#include "style_defs.h"
#include "utils/image_io.h"
#include "utils/tone_mapping.h"
#include "utils/logger.h"
#include "utils/session_config.h"
#include "algo/scm.h"

namespace mvtk {
namespace wxui {

BEGIN_EVENT_TABLE(SCMPanel, wxPanel)
    EVT_BUTTON(SCM_ID_LOAD_DARK_BTN, SCMPanel::OnLoadDark)
    EVT_BUTTON(SCM_ID_LOAD_BRIGHT_BTN, SCMPanel::OnLoadBright)
    EVT_BUTTON(SCM_ID_CLEAR_DATA_BTN, SCMPanel::OnClearData)
    EVT_BUTTON(SCM_ID_EVALUATE_BTN, SCMPanel::OnEvaluate)
    EVT_BUTTON(SCM_ID_EXPORT_REPORT_BTN, SCMPanel::OnExportReport)
    EVT_BUTTON(SCM_ID_EXPORT_RESULT_BTN, SCMPanel::OnExportResult)
END_EVENT_TABLE()

SCMPanel::SCMPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    SetBackgroundColour(Style::NEU_BG_COLOR);
    SetFont(Style::GetSansFont(10));

    params_.min_frame_count = 50;
    params_.sensor_bits = 12;
    params_.temperature_c = 25.0;

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* main_row = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* left_col = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* dark_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* dark_box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* dark_title = new wxStaticText(dark_panel, wxID_ANY, "Dark Field Images");
    Style::ApplyNeumorphicStyle(dark_title, true);
    dark_box->Add(dark_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* dark_row1 = new wxBoxSizer(wxHORIZONTAL);
    load_dark_btn_ = new wxButton(dark_panel, SCM_ID_LOAD_DARK_BTN, "Load");
    Style::ApplyNeumorphicStyle(load_dark_btn_);
    dark_path_ctrl_ = new wxTextCtrl(dark_panel, wxID_ANY, "", wxDefaultPosition, wxSize(180, -1));
    Style::ApplyNeumorphicStyle(dark_path_ctrl_);
    dark_row1->Add(load_dark_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    dark_row1->Add(dark_path_ctrl_, 1);
    dark_box->Add(dark_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    dark_status_ = new wxStaticText(dark_panel, wxID_ANY, "0 frames loaded");
    Style::ApplyNeumorphicStyle(dark_status_);
    dark_box->Add(dark_status_, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    dark_panel->SetSizer(dark_box);
    left_col->Add(dark_panel, 0, wxEXPAND | wxBOTTOM, Style::SPACING_LARGE);

    NeumorphicPanel* bright_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* bright_box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* bright_title = new wxStaticText(bright_panel, wxID_ANY, "Bright Field Images");
    Style::ApplyNeumorphicStyle(bright_title, true);
    bright_box->Add(bright_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* bright_row1 = new wxBoxSizer(wxHORIZONTAL);
    load_bright_btn_ = new wxButton(bright_panel, SCM_ID_LOAD_BRIGHT_BTN, "Load");
    Style::ApplyNeumorphicStyle(load_bright_btn_);
    bright_path_ctrl_ = new wxTextCtrl(bright_panel, wxID_ANY, "", wxDefaultPosition, wxSize(180, -1));
    Style::ApplyNeumorphicStyle(bright_path_ctrl_);
    bright_row1->Add(load_bright_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    bright_row1->Add(bright_path_ctrl_, 1);
    bright_box->Add(bright_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    bright_status_ = new wxStaticText(bright_panel, wxID_ANY, "0 frames loaded");
    Style::ApplyNeumorphicStyle(bright_status_);
    bright_box->Add(bright_status_, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    bright_panel->SetSizer(bright_box);
    left_col->Add(bright_panel, 0, wxEXPAND | wxBOTTOM, Style::SPACING_LARGE);

    NeumorphicPanel* exp_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* exp_box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* exp_title = new wxStaticText(exp_panel, wxID_ANY, "Exposure Settings");
    Style::ApplyNeumorphicStyle(exp_title, true);
    exp_box->Add(exp_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* exp_row1 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* exp_label = new wxStaticText(exp_panel, wxID_ANY, "Exposure (us):");
    Style::ApplyNeumorphicStyle(exp_label);
    exp_ctrl_ = new wxTextCtrl(exp_panel, wxID_ANY, "1000", wxDefaultPosition, wxSize(100, -1));
    Style::ApplyNeumorphicStyle(exp_ctrl_);
    exp_row1->Add(exp_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    exp_row1->Add(exp_ctrl_, 0);
    exp_box->Add(exp_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxStaticText* exp_note = new wxStaticText(exp_panel, wxID_ANY, "Note: Same exposure for dark and bright frames");
    exp_note->SetFont(Style::GetSansFont(8));
    exp_note->SetForegroundColour(wxColour(128, 128, 128));
    exp_box->Add(exp_note, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    exp_panel->SetSizer(exp_box);
    left_col->Add(exp_panel, 0, wxEXPAND | wxBOTTOM, Style::SPACING_LARGE);

    NeumorphicPanel* info_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* info_box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* info_title = new wxStaticText(info_panel, wxID_ANY, "Info");
    Style::ApplyNeumorphicStyle(info_title, true);
    info_box->Add(info_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxStaticText* info_header = new wxStaticText(info_panel, wxID_ANY, "[Capture Requirements]");
    Style::ApplyNeumorphicStyle(info_header, true);
    info_box->Add(info_header, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_dark_title = new wxStaticText(info_panel, wxID_ANY, "◆ Dark Field Images:");
    Style::ApplyNeumorphicStyle(info_dark_title);
    info_box->Add(info_dark_title, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_dark_item1 = new wxStaticText(info_panel, wxID_ANY, "- Turn off light source or cap lens");
    Style::ApplyNeumorphicStyle(info_dark_item1);
    info_box->Add(info_dark_item1, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_dark_item2 = new wxStaticText(info_panel, wxID_ANY, "- Use same exposure as bright images");
    Style::ApplyNeumorphicStyle(info_dark_item2);
    info_box->Add(info_dark_item2, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_dark_item3 = new wxStaticText(info_panel, wxID_ANY, "- Capture at least 50 frames");
    Style::ApplyNeumorphicStyle(info_dark_item3);
    info_box->Add(info_dark_item3, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_dark_item4 = new wxStaticText(info_panel, wxID_ANY, "- Keep temperature stable");
    Style::ApplyNeumorphicStyle(info_dark_item4);
    info_box->Add(info_dark_item4, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_bright_title = new wxStaticText(info_panel, wxID_ANY, "◆ Bright Field Images:");
    Style::ApplyNeumorphicStyle(info_bright_title);
    info_box->Add(info_bright_title, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_bright_item1 = new wxStaticText(info_panel, wxID_ANY, "- Uniform light source (integrating sphere)");
    Style::ApplyNeumorphicStyle(info_bright_item1);
    info_box->Add(info_bright_item1, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_bright_item2 = new wxStaticText(info_panel, wxID_ANY, "- Image brightness: 30-70% of full scale");
    Style::ApplyNeumorphicStyle(info_bright_item2);
    info_box->Add(info_bright_item2, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_bright_item3 = new wxStaticText(info_panel, wxID_ANY, "- Avoid saturation areas");
    Style::ApplyNeumorphicStyle(info_bright_item3);
    info_box->Add(info_bright_item3, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_bright_item4 = new wxStaticText(info_panel, wxID_ANY, "- Capture at least 50 frames");
    Style::ApplyNeumorphicStyle(info_bright_item4);
    info_box->Add(info_bright_item4, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_common_title = new wxStaticText(info_panel, wxID_ANY, "◆ General Requirements:");
    Style::ApplyNeumorphicStyle(info_common_title);
    info_box->Add(info_common_title, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_common_item1 = new wxStaticText(info_panel, wxID_ANY, "- Capture in Raw format");
    Style::ApplyNeumorphicStyle(info_common_item1);
    info_box->Add(info_common_item1, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_common_item2 = new wxStaticText(info_panel, wxID_ANY, "- Keep camera parameters consistent");
    Style::ApplyNeumorphicStyle(info_common_item2);
    info_box->Add(info_common_item2, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* info_common_item3 = new wxStaticText(info_panel, wxID_ANY, "- Dark and bright images must be paired");
    Style::ApplyNeumorphicStyle(info_common_item3);
    info_box->Add(info_common_item3, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    info_panel->SetSizer(info_box);
    left_col->Add(info_panel, 1, wxEXPAND);

    main_row->Add(left_col, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    wxBoxSizer* right_col = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* params_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* params_box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* params_title = new wxStaticText(params_panel, wxID_ANY, "Evaluation Parameters");
    Style::ApplyNeumorphicStyle(params_title, true);
    params_box->Add(params_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxGridSizer* grid = new wxGridSizer(2, 11, Style::SPACING_SMALL);

    wxStaticText* sensor_name_label = new wxStaticText(params_panel, wxID_ANY, "Sensor Name:");
    Style::ApplyNeumorphicStyle(sensor_name_label);
    sensor_name_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "Unknown");
    Style::ApplyNeumorphicStyle(sensor_name_ctrl_);
    grid->Add(sensor_name_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(sensor_name_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* manufacturer_label = new wxStaticText(params_panel, wxID_ANY, "Manufacturer:");
    Style::ApplyNeumorphicStyle(manufacturer_label);
    manufacturer_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "");
    Style::ApplyNeumorphicStyle(manufacturer_ctrl_);
    grid->Add(manufacturer_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(manufacturer_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* model_label = new wxStaticText(params_panel, wxID_ANY, "Model:");
    Style::ApplyNeumorphicStyle(model_label);
    model_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "");
    Style::ApplyNeumorphicStyle(model_ctrl_);
    grid->Add(model_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(model_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* sn_label = new wxStaticText(params_panel, wxID_ANY, "Serial Number:");
    Style::ApplyNeumorphicStyle(sn_label);
    sn_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "");
    Style::ApplyNeumorphicStyle(sn_ctrl_);
    grid->Add(sn_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(sn_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* bits_label = new wxStaticText(params_panel, wxID_ANY, "Bits:");
    Style::ApplyNeumorphicStyle(bits_label);
    bits_ctrl_ = new wxComboBox(params_panel, wxID_ANY, "12", wxDefaultPosition, wxDefaultSize,
                                {"8", "10", "12", "16"});
    bits_ctrl_->SetSelection(2);
    Style::ApplyNeumorphicStyle(bits_ctrl_);
    grid->Add(bits_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(bits_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* bayer_label = new wxStaticText(params_panel, wxID_ANY, "Bayer Pattern:");
    Style::ApplyNeumorphicStyle(bayer_label);
    bayer_ctrl_ = new wxComboBox(params_panel, wxID_ANY, "RGGB", wxDefaultPosition, wxDefaultSize,
                                 {"RGGB", "BGGR", "GRBG", "GBRG"});
    bayer_ctrl_->SetSelection(0);
    Style::ApplyNeumorphicStyle(bayer_ctrl_);
    grid->Add(bayer_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(bayer_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* res_w_label = new wxStaticText(params_panel, wxID_ANY, "Resolution W:");
    Style::ApplyNeumorphicStyle(res_w_label);
    res_w_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "0");
    Style::ApplyNeumorphicStyle(res_w_ctrl_);
    grid->Add(res_w_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(res_w_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* res_h_label = new wxStaticText(params_panel, wxID_ANY, "Resolution H:");
    Style::ApplyNeumorphicStyle(res_h_label);
    res_h_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "0");
    Style::ApplyNeumorphicStyle(res_h_ctrl_);
    grid->Add(res_h_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(res_h_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* temp_label = new wxStaticText(params_panel, wxID_ANY, "Temp (C):");
    Style::ApplyNeumorphicStyle(temp_label);
    temp_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "25");
    Style::ApplyNeumorphicStyle(temp_ctrl_);
    grid->Add(temp_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(temp_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* frames_label = new wxStaticText(params_panel, wxID_ANY, "Min Frames:");
    Style::ApplyNeumorphicStyle(frames_label);
    frames_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "50");
    Style::ApplyNeumorphicStyle(frames_ctrl_);
    grid->Add(frames_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(frames_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* roi_label = new wxStaticText(params_panel, wxID_ANY, "ROI (x,y,w,h):");
    Style::ApplyNeumorphicStyle(roi_label);
    roi_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "0,0,0,0");
    Style::ApplyNeumorphicStyle(roi_ctrl_);
    grid->Add(roi_label, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    grid->Add(roi_ctrl_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    params_box->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* btn_row = new wxBoxSizer(wxHORIZONTAL);
    clear_data_btn_ = new wxButton(params_panel, SCM_ID_CLEAR_DATA_BTN, "Clear");
    Style::ApplyNeumorphicStyle(clear_data_btn_);
    evaluate_btn_ = new wxButton(params_panel, SCM_ID_EVALUATE_BTN, "Evaluate");
    Style::ApplyNeumorphicStyle(evaluate_btn_);
    btn_row->Add(clear_data_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    btn_row->Add(evaluate_btn_, 0);
    params_box->Add(btn_row, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    params_panel->SetSizer(params_box);
    right_col->Add(params_panel, 1, wxEXPAND);

    main_row->Add(right_col, 1, wxEXPAND);

    main_sizer->Add(main_row, 1, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    NeumorphicPanel* result_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* result_box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* result_title = new wxStaticText(result_panel, wxID_ANY, "SCM Results");
    Style::ApplyNeumorphicStyle(result_title, true);
    result_box->Add(result_title, 0, wxALL, Style::SPACING_MEDIUM);

    result_text_ = new wxTextCtrl(result_panel, wxID_ANY, "",
                                   wxDefaultPosition, wxSize(-1, 300),
                                   wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH);
    result_text_->SetFont(Style::GetSansFont(9));
    result_text_->SetBackgroundColour(wxColour(255, 255, 255));
    result_box->Add(result_text_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* export_row = new wxBoxSizer(wxHORIZONTAL);
    export_report_btn_ = new wxButton(result_panel, SCM_ID_EXPORT_REPORT_BTN, "Export Report");
    Style::ApplyNeumorphicStyle(export_report_btn_);
    export_result_btn_ = new wxButton(result_panel, SCM_ID_EXPORT_RESULT_BTN, "Export JSON");
    Style::ApplyNeumorphicStyle(export_result_btn_);
    export_row->Add(export_report_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    export_row->Add(export_result_btn_, 0);
    result_box->Add(export_row, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    result_panel->SetSizer(result_box);
    main_sizer->Add(result_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    SetSizer(main_sizer);
    main_sizer->Fit(this);

    // ============ 从 TOML 加载上次的传感器信息 ============
    auto& cfg = utils::SessionConfig::GetInstance();
    cfg.Load();
    auto sensor_name = cfg.Get<std::string>("sensor.sensor_name");
    if (sensor_name && !sensor_name->empty()) {
        sensor_name_ctrl_->SetValue(wxString::FromUTF8(*sensor_name));
    }
    auto manufacturer = cfg.Get<std::string>("sensor.manufacturer");
    if (manufacturer) {
        manufacturer_ctrl_->SetValue(wxString::FromUTF8(*manufacturer));
    }
    auto model = cfg.Get<std::string>("sensor.model");
    if (model) {
        model_ctrl_->SetValue(wxString::FromUTF8(*model));
    }
    auto sn = cfg.Get<std::string>("sensor.serial_number");
    if (sn) {
        sn_ctrl_->SetValue(wxString::FromUTF8(*sn));
    }
    auto bayer = cfg.Get<std::string>("sensor.bayer_pattern");
    if (bayer && !bayer->empty()) {
        bayer_ctrl_->SetValue(wxString::FromUTF8(*bayer));
    }
    auto res_w = cfg.Get<int>("sensor.resolution_width");
    if (res_w && *res_w > 0) {
        res_w_ctrl_->SetValue(wxString::Format("%d", *res_w));
    }
    auto res_h = cfg.Get<int>("sensor.resolution_height");
    if (res_h && *res_h > 0) {
        res_h_ctrl_->SetValue(wxString::Format("%d", *res_h));
    }
    auto bits = cfg.Get<int>("sensor.bit_depth");
    if (bits && *bits >= 1 && *bits <= 16) {
        bits_ctrl_->SetValue(wxString::Format("%d", *bits));
    }
    auto temp = cfg.Get<double>("scm.test_temperature_c");
    if (temp) {
        temp_ctrl_->SetValue(wxString::Format("%.1f", *temp));
    }
    auto frames = cfg.Get<int>("scm.min_frame_count");
    if (frames && *frames > 0) {
        frames_ctrl_->SetValue(wxString::Format("%d", *frames));
    }
}

void SCMPanel::OnLoadDark(wxCommandEvent& event) {
    wxString folder_path = dark_path_ctrl_->GetValue();
    if (folder_path.IsEmpty()) {
        wxMessageBox("Please enter a folder path!", "Error", wxOK | wxICON_ERROR);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Empty dark folder path");
        return;
    }

    if (!wxDir::Exists(folder_path)) {
        wxMessageBox("Folder does not exist: " + folder_path, "Error", wxOK | wxICON_ERROR);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Dark folder does not exist: " + folder_path.ToStdString());
        return;
    }

    MV_LOG_OPERATION("Load Dark Images", "Folder: " + folder_path.ToStdString());
    double exp_us = wxAtof(exp_ctrl_->GetValue());

    wxArrayString files;
    wxDir dir(folder_path);
    wxString file;
    bool cont = dir.GetFirst(&file, wxEmptyString, wxDIR_FILES);
    while (cont) {
        wxString ext = file.AfterLast('.').Lower();
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tif" || ext == "tiff" || ext == "bmp") {
            files.Add(file);
        }
        cont = dir.GetNext(&file);
    }

    std::sort(files.begin(), files.end());

    int loaded_count = 0;
    for (const wxString& filename : files) {
        wxString full_path = wxFileName(folder_path, filename).GetFullPath();
        // SCM 需要 Raw 数据，使用 IMREAD_UNCHANGED 保留 16-bit 位深和单通道
        cv::Mat img = mvtk::utils::imreadUtf8(std::string(full_path.ToUTF8()), cv::IMREAD_UNCHANGED);
        if (!img.empty()) {
            dark_images_[exp_us].push_back(img);
            loaded_count++;
        } else {
            MV_LOG_ERROR_DETAIL("ImageLoad", "Failed to load dark image: " + full_path.ToStdString());
        }
    }

    UpdateStatus();

    if (loaded_count > 0) {
        wxMessageBox(wxString::Format("Successfully loaded %d dark field images!", loaded_count),
                     "Success", wxOK | wxICON_INFORMATION);
        MV_LOG_DATA("ImageInfo", "Loaded " + std::to_string(loaded_count) + " dark field images from folder");
    } else {
        wxMessageBox("No valid image files found in the folder!", "Warning", wxOK | wxICON_WARNING);
        MV_LOG_ERROR_DETAIL("ImageLoad", "No valid image files found in dark folder");
    }
}

void SCMPanel::OnLoadBright(wxCommandEvent& event) {
    wxString folder_path = bright_path_ctrl_->GetValue();
    if (folder_path.IsEmpty()) {
        wxMessageBox("Please enter a folder path!", "Error", wxOK | wxICON_ERROR);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Empty bright folder path");
        return;
    }

    if (!wxDir::Exists(folder_path)) {
        wxMessageBox("Folder does not exist: " + folder_path, "Error", wxOK | wxICON_ERROR);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Bright folder does not exist: " + folder_path.ToStdString());
        return;
    }

    MV_LOG_OPERATION("Load Bright Images", "Folder: " + folder_path.ToStdString());
    double exp_us = wxAtof(exp_ctrl_->GetValue());

    wxArrayString files;
    wxDir dir(folder_path);
    wxString file;
    bool cont = dir.GetFirst(&file, wxEmptyString, wxDIR_FILES);
    while (cont) {
        wxString ext = file.AfterLast('.').Lower();
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tif" || ext == "tiff" || ext == "bmp") {
            files.Add(file);
        }
        cont = dir.GetNext(&file);
    }

    std::sort(files.begin(), files.end());

    int loaded_count = 0;
    for (const wxString& filename : files) {
        wxString full_path = wxFileName(folder_path, filename).GetFullPath();
        // SCM 需要 Raw 数据，使用 IMREAD_UNCHANGED 保留 16-bit 位深和单通道
        cv::Mat img = mvtk::utils::imreadUtf8(std::string(full_path.ToUTF8()), cv::IMREAD_UNCHANGED);
        if (!img.empty()) {
            bright_images_[exp_us].push_back(img);
            loaded_count++;
        } else {
            MV_LOG_ERROR_DETAIL("ImageLoad", "Failed to load bright image: " + full_path.ToStdString());
        }
    }

    UpdateStatus();

    if (loaded_count > 0) {
        wxMessageBox(wxString::Format("Successfully loaded %d bright field images!", loaded_count),
                     "Success", wxOK | wxICON_INFORMATION);
        MV_LOG_DATA("ImageInfo", "Loaded " + std::to_string(loaded_count) + " bright field images from folder");
    } else {
        wxMessageBox("No valid image files found in the folder!", "Warning", wxOK | wxICON_WARNING);
        MV_LOG_ERROR_DETAIL("ImageLoad", "No valid image files found in bright folder");
    }
}

void SCMPanel::OnClearData(wxCommandEvent& event) {
    dark_images_.clear();
    bright_images_.clear();
    UpdateStatus();
    result_text_->Clear();
}

void SCMPanel::OnEvaluate(wxCommandEvent& event) {
    if (dark_images_.empty() || bright_images_.empty()) {
        wxMessageBox("Please load dark field and bright field images first!",
                     "Error", wxOK | wxICON_ERROR);
        return;
    }

    params_.sensor_name = std::string(sensor_name_ctrl_->GetValue().ToUTF8());

    // 位深校验：支持 8/10/12/16 位预设及自定义值（限定 1-16 位）
    long bits_val = 0;
    if (!bits_ctrl_->GetValue().ToLong(&bits_val) || bits_val < 1 || bits_val > 16) {
        wxMessageBox("Sensor bits must be an integer between 1 and 16!\n"
                     "Common values: 8, 10, 12, 16.",
                     "Invalid Bits", wxOK | wxICON_WARNING);
        return;
    }
    params_.sensor_bits = static_cast<int>(bits_val);

    // Read sensor identity / spec fields
    params_.manufacturer = std::string(manufacturer_ctrl_->GetValue().ToUTF8());
    params_.model = std::string(model_ctrl_->GetValue().ToUTF8());
    params_.serial_number = std::string(sn_ctrl_->GetValue().ToUTF8());
    params_.bayer_pattern = std::string(bayer_ctrl_->GetValue().ToUTF8());
    long res_w = 0, res_h = 0;
    res_w_ctrl_->GetValue().ToLong(&res_w);
    res_h_ctrl_->GetValue().ToLong(&res_h);
    params_.resolution_width = static_cast<int>(res_w > 0 ? res_w : 0);
    params_.resolution_height = static_cast<int>(res_h > 0 ? res_h : 0);

    params_.temperature_c = wxAtof(temp_ctrl_->GetValue());
    params_.min_frame_count = wxAtoi(frames_ctrl_->GetValue());

    wxString roi_str = roi_ctrl_->GetValue();
    int x = 0, y = 0, w = 0, h = 0;
    sscanf(roi_str.ToUTF8(), "%d,%d,%d,%d", &x, &y, &w, &h);
    if (w > 0 && h > 0) {
        params_.roi = cv::Rect(x, y, w, h);
    } else {
        params_.roi = cv::Rect();
    }

    SCMResult result = SCMEvaluator::evaluate(dark_images_, bright_images_, params_);
    current_result_ = result;

    std::string report = SCMEvaluator::generateReport(result);
    result_text_->SetValue(wxString::FromUTF8(report));

    // ============ 写入 TOML 会话配置 ============
    auto& cfg = utils::SessionConfig::GetInstance();
    // 确保已加载（若未加载则使用默认路径）
    cfg.Load();

    // 1. 写入传感器信息（包含厂商/型号/SN/Bayer/分辨率）
    cfg.SetSensorInfo(params_.manufacturer,
                      params_.model,
                      params_.serial_number,
                      params_.sensor_bits,
                      static_cast<int>((1 << params_.sensor_bits) - 1),
                      params_.bayer_pattern,
                      params_.resolution_width,
                      params_.resolution_height);
    cfg.Set("sensor.sensor_name", params_.sensor_name);
    cfg.Set("sensor.temperature_c", params_.temperature_c);

    // 2. 写入 SCM 测试参数
    cfg.Set("scm.test_temperature_c", params_.temperature_c);
    cfg.Set("scm.min_frame_count", params_.min_frame_count);
    cfg.Set("scm.saturation_threshold", params_.saturation_threshold);
    cfg.Set("scm.roi_x", params_.roi.x);
    cfg.Set("scm.roi_y", params_.roi.y);
    cfg.Set("scm.roi_width", params_.roi.width);
    cfg.Set("scm.roi_height", params_.roi.height);

    // 3. 写入 SCM 核心指标
    utils::SessionConfig::SCMResults scm_r;
    scm_r.system_gain_e_per_dn = result.system_gain_e_per_dn;
    scm_r.system_gain_dn_per_e = result.system_gain_dn_per_e;
    scm_r.dark_noise_e = result.dark_noise_e;
    scm_r.dark_noise_dn = result.dark_noise_dn;
    scm_r.dark_signal_dn = result.dark_signal_dn;
    scm_r.dark_current_e_per_sec = result.dark_current_e_per_sec;
    scm_r.full_well_capacity_e = result.full_well_capacity_e;
    scm_r.full_well_capacity_dn = result.full_well_capacity_dn;
    scm_r.dynamic_range_db = result.dynamic_range_db;
    scm_r.dynamic_range_bits = result.dynamic_range_bits;
    scm_r.snr_at_50_percent_db = result.snr_at_50_percent;
    scm_r.peak_snr_db = result.peak_snr_db;
    scm_r.is_valid = result.is_valid;
    scm_r.is_snr_sufficient_for_ccm = result.is_snr_sufficient_for_ccm;
    scm_r.recommended_ccm_exposure_us = result.recommended_ccm_exposure_us;
    scm_r.recommended_lsc_exposure_us = result.recommended_lsc_exposure_us;
    scm_r.evaluation_message = result.message;
    cfg.SetSCMResults(scm_r);

    // 4. 写入 SNR 其它指标（调试用）
    cfg.Set("scm.snr_at_10_percent_db", result.snr_at_10_percent);
    cfg.Set("scm.snr_at_90_percent_db", result.snr_at_90_percent);
    cfg.Set("scm.is_uniformity_sufficient_for_lsc", result.is_uniformity_sufficient_for_lsc);
    cfg.Set("scm.dark_current_e_per_sec_per_mm2", result.dark_current_e_per_sec_per_mm2);

    // 5. 写入曝光时间序列和 PTC 数据点（便于调试）
    cfg.SetArray("scm.exposure_times_us", result.exposure_times_us);
    cfg.SetArray("scm.dark_means", result.dark_means);
    cfg.SetArray("scm.dark_variances", result.dark_variances);
    cfg.SetArray("scm.bright_means", result.bright_means);
    cfg.SetArray("scm.bright_variances", result.bright_variances);

    // 6. 记录标定历史
    cfg.AddCalibrationHistory(
        "SCM",
        "Sensor characterization evaluation (" + params_.sensor_name + ")",
        result.is_valid,
        0.0,
        "exposure_groups=" + std::to_string(result.exposure_times_us.size()) +
        ", frames=" + std::to_string(params_.min_frame_count));

    // 7. 保存
    cfg.Save();

    if (result.is_valid) {
        wxMessageBox("SCM evaluation completed successfully!\n\nResults saved to pipeline_session.toml",
                     "Success", wxOK | wxICON_INFORMATION);
    } else {
        wxMessageBox("Evaluation invalid: " + wxString::FromUTF8(result.message) +
                     "\n\nResults saved to pipeline_session.toml",
                     "Warning", wxOK | wxICON_WARNING);
    }
}

void SCMPanel::OnExportReport(wxCommandEvent& event) {
    if (!current_result_.is_valid) {
        wxMessageBox("No valid evaluation result to export!", "Error", wxOK | wxICON_ERROR);
        return;
    }

    wxFileDialog dlg(this, "Save Report", "", "scm_report.txt",
                      "Text Files (*.txt)|*.txt",
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() == wxID_OK) {
        std::string report = SCMEvaluator::generateReport(current_result_);
        std::ofstream ofs(std::string(dlg.GetPath().ToUTF8()));
        if (ofs) {
            ofs << report;
            wxMessageBox("Report exported successfully!", "Success", wxOK | wxICON_INFORMATION);
        }
    }
}

void SCMPanel::OnExportResult(wxCommandEvent& event) {
    if (!current_result_.is_valid) {
        wxMessageBox("No valid evaluation result to export!", "Error", wxOK | wxICON_ERROR);
        return;
    }

    wxFileDialog dlg(this, "Save Result", "", "scm_result.json",
                      "JSON Files (*.json)|*.json",
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() == wxID_OK) {
        bool success = SCMEvaluator::exportResult(current_result_, std::string(dlg.GetPath().ToUTF8()));
        if (success) {
            wxMessageBox("Result exported successfully!", "Success", wxOK | wxICON_INFORMATION);
        }
    }
}

void SCMPanel::UpdateStatus() {
    int dark_total = 0;
    for (const auto& entry : dark_images_) {
        dark_total += entry.second.size();
    }
    dark_status_->SetLabel(wxString::Format("%d frames (%d exposures)", dark_total, (int)dark_images_.size()));

    int bright_total = 0;
    for (const auto& entry : bright_images_) {
        bright_total += entry.second.size();
    }
    bright_status_->SetLabel(wxString::Format("%d frames (%d exposures)", bright_total, (int)bright_images_.size()));
}

}
}
