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

    wxBoxSizer* top_row = new wxBoxSizer(wxHORIZONTAL);

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

    wxBoxSizer* dark_row2 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* dark_exp_label = new wxStaticText(dark_panel, wxID_ANY, "Exposure (us):");
    Style::ApplyNeumorphicStyle(dark_exp_label);
    dark_exp_ctrl_ = new wxTextCtrl(dark_panel, wxID_ANY, "1000", wxDefaultPosition, wxSize(80, -1));
    Style::ApplyNeumorphicStyle(dark_exp_ctrl_);
    dark_row2->Add(dark_exp_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    dark_row2->Add(dark_exp_ctrl_, 0);
    dark_box->Add(dark_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    dark_status_ = new wxStaticText(dark_panel, wxID_ANY, "0 frames loaded");
    Style::ApplyNeumorphicStyle(dark_status_);
    dark_box->Add(dark_status_, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    dark_panel->SetSizer(dark_box);
    top_row->Add(dark_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

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

    wxBoxSizer* bright_row2 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* bright_exp_label = new wxStaticText(bright_panel, wxID_ANY, "Exposure (us):");
    Style::ApplyNeumorphicStyle(bright_exp_label);
    bright_exp_ctrl_ = new wxTextCtrl(bright_panel, wxID_ANY, "1000", wxDefaultPosition, wxSize(80, -1));
    Style::ApplyNeumorphicStyle(bright_exp_ctrl_);
    bright_row2->Add(bright_exp_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    bright_row2->Add(bright_exp_ctrl_, 0);
    bright_box->Add(bright_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    bright_status_ = new wxStaticText(bright_panel, wxID_ANY, "0 frames loaded");
    Style::ApplyNeumorphicStyle(bright_status_);
    bright_box->Add(bright_status_, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    bright_panel->SetSizer(bright_box);
    top_row->Add(bright_panel, 1, wxEXPAND);

    main_sizer->Add(top_row, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* middle_row = new wxBoxSizer(wxHORIZONTAL);

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
    middle_row->Add(info_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    NeumorphicPanel* params_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* params_box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* params_title = new wxStaticText(params_panel, wxID_ANY, "Evaluation Parameters");
    Style::ApplyNeumorphicStyle(params_title, true);
    params_box->Add(params_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxGridSizer* grid = new wxGridSizer(2, 5, Style::SPACING_SMALL);

    wxStaticText* sensor_name_label = new wxStaticText(params_panel, wxID_ANY, "Sensor Name:");
    Style::ApplyNeumorphicStyle(sensor_name_label);
    sensor_name_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "Unknown");
    Style::ApplyNeumorphicStyle(sensor_name_ctrl_);
    grid->Add(sensor_name_label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    grid->Add(sensor_name_ctrl_, 1, wxEXPAND);

    wxStaticText* bits_label = new wxStaticText(params_panel, wxID_ANY, "Bits:");
    Style::ApplyNeumorphicStyle(bits_label);
    bits_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "12");
    Style::ApplyNeumorphicStyle(bits_ctrl_);
    grid->Add(bits_label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    grid->Add(bits_ctrl_, 0);

    wxStaticText* temp_label = new wxStaticText(params_panel, wxID_ANY, "Temp (C):");
    Style::ApplyNeumorphicStyle(temp_label);
    temp_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "25");
    Style::ApplyNeumorphicStyle(temp_ctrl_);
    grid->Add(temp_label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    grid->Add(temp_ctrl_, 0);

    wxStaticText* frames_label = new wxStaticText(params_panel, wxID_ANY, "Min Frames:");
    Style::ApplyNeumorphicStyle(frames_label);
    frames_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "50");
    Style::ApplyNeumorphicStyle(frames_ctrl_);
    grid->Add(frames_label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    grid->Add(frames_ctrl_, 0);

    wxStaticText* roi_label = new wxStaticText(params_panel, wxID_ANY, "ROI (x,y,w,h):");
    Style::ApplyNeumorphicStyle(roi_label);
    roi_ctrl_ = new wxTextCtrl(params_panel, wxID_ANY, "0,0,0,0");
    Style::ApplyNeumorphicStyle(roi_ctrl_);
    grid->Add(roi_label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    grid->Add(roi_ctrl_, 1, wxEXPAND);

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
    middle_row->Add(params_panel, 1, wxEXPAND);

    main_sizer->Add(middle_row, 0, wxEXPAND | wxLEFT | wxRIGHT, Style::SPACING_MEDIUM);

    NeumorphicPanel* result_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* result_box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* result_title = new wxStaticText(result_panel, wxID_ANY, "SCM Results");
    Style::ApplyNeumorphicStyle(result_title, true);
    result_box->Add(result_title, 0, wxALL, Style::SPACING_MEDIUM);

    result_text_ = new wxTextCtrl(result_panel, wxID_ANY, "",
                                   wxDefaultPosition, wxSize(-1, 200),
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
    main_sizer->Add(result_panel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    SetSizer(main_sizer);
    main_sizer->Fit(this);
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
    double exp_us = wxAtof(dark_exp_ctrl_->GetValue());

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
        cv::Mat img = mvtk::utils::imreadUtf8(std::string(full_path.ToUTF8()));
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
    double exp_us = wxAtof(bright_exp_ctrl_->GetValue());

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
        cv::Mat img = mvtk::utils::imreadUtf8(std::string(full_path.ToUTF8()));
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
    params_.sensor_bits = wxAtoi(bits_ctrl_->GetValue());
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

    if (result.is_valid) {
        wxMessageBox("SCM evaluation completed successfully!",
                     "Success", wxOK | wxICON_INFORMATION);
    } else {
        wxMessageBox("Evaluation invalid: " + wxString::FromUTF8(result.message),
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
