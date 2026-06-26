#include "wxui/color_char_calib_panel.h"
#include "algo/quick_color_calib.h"
#include "utils/image_io.h"
#include <fstream>

namespace mvtk {
namespace wxui {

BEGIN_EVENT_TABLE(ColorCharCalibPanel, wxPanel)
    EVT_BUTTON(CC_ID_STEP_BTN_0, ColorCharCalibPanel::OnStepButton)
    EVT_BUTTON(CC_ID_STEP_BTN_1, ColorCharCalibPanel::OnStepButton)
    EVT_BUTTON(CC_ID_STEP_BTN_2, ColorCharCalibPanel::OnStepButton)
    EVT_BUTTON(CC_ID_STEP_BTN_3, ColorCharCalibPanel::OnStepButton)
    EVT_LISTBOX(wxID_ANY, ColorCharCalibPanel::OnLightSelected)
    EVT_COMBOBOX(wxID_ANY, ColorCharCalibPanel::OnImageSourceChanged)
    EVT_BUTTON(CC_ID_LOAD_IMG_BTN, ColorCharCalibPanel::OnLoadImage)
    EVT_BUTTON(CC_ID_OPEN_CAMERA_BTN, ColorCharCalibPanel::OnOpenCamera)
    EVT_BUTTON(CC_ID_CAPTURE_BTN, ColorCharCalibPanel::OnCapture)
    EVT_BUTTON(CC_ID_CLOSE_CAMERA_BTN, ColorCharCalibPanel::OnCloseCamera)
    EVT_BUTTON(CC_ID_REMOVE_BTN, ColorCharCalibPanel::OnRemove)
    EVT_BUTTON(CC_ID_CALIBRATE_BTN, ColorCharCalibPanel::OnCalibrate)
    EVT_BUTTON(CC_ID_EXPORT_BTN, ColorCharCalibPanel::OnExport)
    EVT_BUTTON(CC_ID_BACK_BTN, ColorCharCalibPanel::OnBack)
    EVT_BUTTON(CC_ID_NEXT_BTN, ColorCharCalibPanel::OnNext)
    EVT_BUTTON(CC_ID_RESET_BTN, ColorCharCalibPanel::OnReset)
END_EVENT_TABLE()

ColorCharCalibPanel::ColorCharCalibPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* step_sizer = new wxBoxSizer(wxHORIZONTAL);
    const char* steps[] = {"1. Light Sources", "2. Parameters", "3. Result", "4. Export"};
    const int step_ids[] = {CC_ID_STEP_BTN_0, CC_ID_STEP_BTN_1, CC_ID_STEP_BTN_2, CC_ID_STEP_BTN_3};
    for (int i = 0; i < 4; ++i) {
        step_btns_[i] = new wxButton(this, step_ids[i], steps[i], wxDefaultPosition, wxSize(120, 30));
        step_sizer->Add(step_btns_[i], 0, wxRIGHT, 5);
    }
    main_sizer->Add(step_sizer, 0, wxALIGN_CENTER | wxALL, 5);

    wxStaticLine* sep = new wxStaticLine(this, wxID_ANY);
    main_sizer->Add(sep, 0, wxEXPAND | wxALL, 5);

    notebook_ = new wxNotebook(this, wxID_ANY);

    wxPanel* page0 = new wxPanel(notebook_);
    wxBoxSizer* page0_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* list_static_box = new wxStaticBox(page0, wxID_ANY, "Light Sources List");
    wxStaticBoxSizer* list_box = new wxStaticBoxSizer(list_static_box, wxVERTICAL);
    light_list_ = new wxListBox(list_static_box, wxID_ANY, wxDefaultPosition, wxSize(-1, 150));
    list_box->Add(light_list_, 1, wxEXPAND | wxALL, 5);
    page0_sizer->Add(list_box, 0, wxEXPAND | wxALL, 5);

    wxStaticBox* info_static_box = new wxStaticBox(page0, wxID_ANY, "Current Light Source Info");
    wxStaticBoxSizer* info_box = new wxStaticBoxSizer(info_static_box, wxVERTICAL);
    light_info_text_ = new wxStaticText(info_static_box, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    info_box->Add(light_info_text_, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* img_preview_box = new wxBoxSizer(wxVERTICAL);
    img_preview_box->Add(new wxStaticText(info_static_box, wxID_ANY, "Image Preview"), 0, wxALIGN_CENTER | wxBOTTOM, 5);
    image_canvas_ = new ImageCanvas(page0, wxID_ANY);
    image_canvas_->SetMinSize(wxSize(400, 300));
    img_preview_box->Add(image_canvas_, 1, wxEXPAND);
    info_box->Add(img_preview_box, 1, wxEXPAND | wxALL, 5);
    page0_sizer->Add(info_box, 1, wxEXPAND | wxALL, 5);

    wxStaticBox* add_static_box = new wxStaticBox(page0, wxID_ANY, "Add New Light Source");
    wxStaticBoxSizer* add_box = new wxStaticBoxSizer(add_static_box, wxVERTICAL);
    wxGridSizer* add_grid = new wxGridSizer(2, 2, 5);
    add_grid->Add(new wxStaticText(add_static_box, wxID_ANY, "Light Name:"), 0, wxALIGN_CENTER_VERTICAL);
    new_light_name_ctrl_ = new wxTextCtrl(add_static_box, wxID_ANY, "D65");
    add_grid->Add(new_light_name_ctrl_, 1);
    add_grid->Add(new wxStaticText(add_static_box, wxID_ANY, "Color Temperature:"), 0, wxALIGN_CENTER_VERTICAL);
    new_light_temp_slider_ = new wxSlider(add_static_box, wxID_ANY, 65, 20, 100);
    add_grid->Add(new_light_temp_slider_, 1);
    add_box->Add(add_grid, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* source_row = new wxBoxSizer(wxHORIZONTAL);
    source_row->Add(new wxStaticText(add_static_box, wxID_ANY, "Image Source:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    image_source_combo_ = new wxComboBox(add_static_box, wxID_ANY, "Local Image", wxDefaultPosition, wxDefaultSize,
                                         {"Local Image", "Camera"}, wxCB_READONLY);
    source_row->Add(image_source_combo_, 1);
    add_box->Add(source_row, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* load_row = new wxBoxSizer(wxHORIZONTAL);
    img_path_ctrl_ = new wxTextCtrl(add_static_box, wxID_ANY, "", wxDefaultPosition, wxSize(250, -1));
    load_img_btn_ = new wxButton(add_static_box, CC_ID_LOAD_IMG_BTN, "Load Image");
    load_row->Add(img_path_ctrl_, 1, wxRIGHT, 5);
    load_row->Add(load_img_btn_, 0);
    add_box->Add(load_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_camera_btn_ = new wxButton(add_static_box, CC_ID_OPEN_CAMERA_BTN, "Open Camera");
    capture_btn_ = new wxButton(add_static_box, CC_ID_CAPTURE_BTN, "Capture");
    close_camera_btn_ = new wxButton(add_static_box, CC_ID_CLOSE_CAMERA_BTN, "Close Camera");
    remove_btn_ = new wxButton(add_static_box, CC_ID_REMOVE_BTN, "Remove");
    capture_btn_->Disable();
    close_camera_btn_->Disable();
    remove_btn_->Disable();
    camera_row->Add(open_camera_btn_, 0, wxRIGHT, 5);
    camera_row->Add(capture_btn_, 0, wxRIGHT, 5);
    camera_row->Add(close_camera_btn_, 0, wxRIGHT, 5);
    camera_row->Add(remove_btn_, 0);
    add_box->Add(camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    page0_sizer->Add(add_box, 0, wxEXPAND | wxALL, 5);
    page0->SetSizer(page0_sizer);
    notebook_->AddPage(page0, "Step 1");

    wxPanel* page1 = new wxPanel(notebook_);
    wxBoxSizer* page1_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* params_static_box = new wxStaticBox(page1, wxID_ANY, "Calibration Parameters");
    wxStaticBoxSizer* params_box = new wxStaticBoxSizer(params_static_box, wxVERTICAL);
    wxGridSizer* params_grid = new wxGridSizer(2, 3, 5);
    params_grid->Add(new wxStaticText(params_static_box, wxID_ANY, "WB Method:"), 0, wxALIGN_CENTER_VERTICAL);
    wb_method_combo_ = new wxComboBox(params_static_box, wxID_ANY, "Average", wxDefaultPosition, wxDefaultSize,
                                      {"Average", "Analytic"}, wxCB_READONLY);
    params_grid->Add(wb_method_combo_, 1);
    params_grid->Add(new wxStaticText(params_static_box, wxID_ANY, "CCM Method:"), 0, wxALIGN_CENTER_VERTICAL);
    ccm_method_combo_ = new wxComboBox(params_static_box, wxID_ANY, "Optimal", wxDefaultPosition, wxDefaultSize,
                                       {"Optimal", "Least Squares"}, wxCB_READONLY);
    params_grid->Add(ccm_method_combo_, 1);
    params_grid->Add(new wxStaticText(params_static_box, wxID_ANY, "Standard Colors:"), 0, wxALIGN_CENTER_VERTICAL);
    color_std_combo_ = new wxComboBox(params_static_box, wxID_ANY, "Standard 24 Colors", wxDefaultPosition, wxDefaultSize,
                                      {"Standard 24 Colors", "Custom Colors"}, wxCB_READONLY);
    params_grid->Add(color_std_combo_, 1);
    params_box->Add(params_grid, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* quant_row = new wxBoxSizer(wxHORIZONTAL);
    quantize_checkbox_ = new wxCheckBox(params_static_box, wxID_ANY, "Quantization");
    quant_row->Add(quantize_checkbox_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    quant_row->Add(new wxStaticText(params_static_box, wxID_ANY, "WB Quant:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    wb_quant_ctrl_ = new wxTextCtrl(params_static_box, wxID_ANY, "1024", wxDefaultPosition, wxSize(80, -1));
    quant_row->Add(wb_quant_ctrl_, 0, wxRIGHT, 10);
    quant_row->Add(new wxStaticText(params_static_box, wxID_ANY, "CCM Quant:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    ccm_quant_ctrl_ = new wxTextCtrl(params_static_box, wxID_ANY, "1024", wxDefaultPosition, wxSize(80, -1));
    quant_row->Add(ccm_quant_ctrl_, 0);
    params_box->Add(quant_row, 0, wxEXPAND | wxALL, 5);

    calibrate_btn_ = new wxButton(params_static_box, CC_ID_CALIBRATE_BTN, "Calibrate");
    params_box->Add(calibrate_btn_, 0, wxALIGN_CENTER | wxALL, 5);
    page1_sizer->Add(params_box, 1, wxEXPAND | wxALL, 5);
    page1->SetSizer(page1_sizer);
    notebook_->AddPage(page1, "Step 2");

    wxPanel* page2 = new wxPanel(notebook_);
    wxBoxSizer* page2_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* result_static_box = new wxStaticBox(page2, wxID_ANY, "Calibration Result");
    wxStaticBoxSizer* result_box = new wxStaticBoxSizer(result_static_box, wxVERTICAL);
    result_text_ = new wxStaticText(result_static_box, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    result_box->Add(result_text_, 1, wxEXPAND | wxALL, 5);
    page2_sizer->Add(result_box, 1, wxEXPAND | wxALL, 5);
    page2->SetSizer(page2_sizer);
    notebook_->AddPage(page2, "Step 3");

    wxPanel* page3 = new wxPanel(notebook_);
    wxBoxSizer* page3_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* export_static_box = new wxStaticBox(page3, wxID_ANY, "Export Calibration");
    wxStaticBoxSizer* export_box = new wxStaticBoxSizer(export_static_box, wxVERTICAL);
    export_box->Add(new wxStaticText(export_static_box, wxID_ANY, "Export to: color_char_calib.dat"), 0, wxALL, 5);
    export_btn_ = new wxButton(export_static_box, CC_ID_EXPORT_BTN, "Export");
    export_box->Add(export_btn_, 0, wxALIGN_CENTER | wxALL, 5);
    page3_sizer->Add(export_box, 1, wxEXPAND | wxALL, 5);
    page3->SetSizer(page3_sizer);
    notebook_->AddPage(page3, "Step 4");

    main_sizer->Add(notebook_, 1, wxEXPAND | wxALL, 5);

    wxBoxSizer* nav_sizer = new wxBoxSizer(wxHORIZONTAL);
    back_btn_ = new wxButton(this, CC_ID_BACK_BTN, "<< Back");
    next_btn_ = new wxButton(this, CC_ID_NEXT_BTN, "Next >>");
    reset_btn_ = new wxButton(this, CC_ID_RESET_BTN, "Reset");
    nav_sizer->Add(back_btn_, 0, wxRIGHT, 10);
    nav_sizer->Add(next_btn_, 0, wxRIGHT, 10);
    nav_sizer->Add(reset_btn_, 0);
    main_sizer->Add(nav_sizer, 0, wxALIGN_CENTER | wxALL, 5);

    status_text_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    main_sizer->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    SetSizer(main_sizer);
    UpdateStepButtons();
}

ColorCharCalibPanel::~ColorCharCalibPanel() {
    if (camera_opened_) {
        camera_.release();
    }
}

void ColorCharCalibPanel::UpdateStepButtons() {
    for (int i = 0; i < 4; ++i) {
        if (i == step_) {
            step_btns_[i]->SetBackgroundColour(*wxGREEN);
            step_btns_[i]->SetForegroundColour(*wxWHITE);
        } else {
            step_btns_[i]->SetBackgroundColour(wxNullColour);
            step_btns_[i]->SetForegroundColour(wxNullColour);
        }
    }
}

void ColorCharCalibPanel::UpdateStatus(const wxString& msg, bool is_error) {
    status_text_->SetLabel(msg);
    if (is_error) {
        status_text_->SetForegroundColour(*wxRED);
    } else {
        status_text_->SetForegroundColour(*wxYELLOW);
    }
}

void ColorCharCalibPanel::UpdateLightList() {
    light_list_->Clear();
    for (size_t i = 0; i < light_sources_.size(); ++i) {
        light_list_->Append(wxString::Format("%s (%.0fK)", light_sources_[i].name.c_str(), light_sources_[i].color_temp));
    }
}

void ColorCharCalibPanel::UpdateLightInfo() {
    if (current_light_idx_ >= 0 && current_light_idx_ < static_cast<int>(light_sources_.size())) {
        const auto& ls = light_sources_[current_light_idx_];
        light_info_text_->SetLabel(wxString::Format(
            "Name: %s\nColor Temp: %.0f K\nImage: %dx%d\nROIs: %zu",
            ls.name.c_str(), ls.color_temp, ls.image.cols, ls.image.rows, ls.rois.size()));
        if (!ls.image.empty()) {
            image_canvas_->SetImage(ls.image);
        } else {
            image_canvas_->Clear();
        }
        remove_btn_->Enable();
    } else {
        light_info_text_->SetLabel("No light source selected");
        image_canvas_->Clear();
        remove_btn_->Disable();
    }
}

void ColorCharCalibPanel::OnStepButton(wxCommandEvent& event) {
    for (int i = 0; i < 4; ++i) {
        if (event.GetEventObject() == step_btns_[i]) {
            step_ = i;
            UpdateStepButtons();
            notebook_->SetSelection(step_);
            break;
        }
    }
}

void ColorCharCalibPanel::OnLightSelected(wxCommandEvent& event) {
    current_light_idx_ = light_list_->GetSelection();
    UpdateLightInfo();
}

void ColorCharCalibPanel::OnImageSourceChanged(wxCommandEvent& event) {
    image_source_ = image_source_combo_->GetSelection();
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
        capture_btn_->Disable();
        close_camera_btn_->Disable();
        open_camera_btn_->Enable();
    }
}

void ColorCharCalibPanel::OnLoadImage(wxCommandEvent& event) {
    wxString path = img_path_ctrl_->GetValue();
    if (path.IsEmpty()) {
        UpdateStatus("Please enter a file path", true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Empty file path");
        return;
    }
    MV_LOG_OPERATION("Load Image", "Path: " + path.ToStdString());
    cv::Mat img = mvtk::utils::imreadUtf8(path.ToStdString());
    if (!img.empty()) {
        LightSourceUIData ls;
        ls.name = new_light_name_ctrl_->GetValue().ToStdString();
        ls.color_temp = new_light_temp_slider_->GetValue() * 100.0;
        ls.image = img;
        ls.rois = ROIManager::generate24ColorROI(50, 50, 30, 30, 2);
        light_sources_.push_back(ls);
        current_light_idx_ = static_cast<int>(light_sources_.size()) - 1;
        UpdateLightList();
        UpdateLightInfo();
        UpdateStatus("Light source added from file");
        MV_LOG_DATA("LightSource", "Name: " + ls.name + ", ColorTemp: " + std::to_string(ls.color_temp) + 
                    ", Image: " + std::to_string(img.cols) + "x" + std::to_string(img.rows));
    } else {
        UpdateStatus("Failed to load image", true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Failed to load image: " + path.ToStdString());
    }
}

void ColorCharCalibPanel::OnOpenCamera(wxCommandEvent& event) {
    MV_LOG_OPERATION("Open Camera", "Camera Index: 0");
    camera_.open(0);
    if (camera_.isOpened()) {
        camera_opened_ = true;
        open_camera_btn_->Disable();
        capture_btn_->Enable();
        close_camera_btn_->Enable();
        UpdateStatus("Camera opened successfully");
        MV_LOG_DATA("CameraStatus", "Opened successfully");
    } else {
        UpdateStatus("Failed to open camera", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to open camera index 0");
    }
}

void ColorCharCalibPanel::OnCapture(wxCommandEvent& event) {
    MV_LOG_OPERATION("Capture Image", "Source: Camera");
    cv::Mat img;
    camera_ >> img;
    if (!img.empty()) {
        LightSourceUIData ls;
        ls.name = new_light_name_ctrl_->GetValue().ToStdString();
        ls.color_temp = new_light_temp_slider_->GetValue() * 100.0;
        ls.image = img;
        ls.rois = ROIManager::generate24ColorROI(50, 50, 30, 30, 2);
        light_sources_.push_back(ls);
        current_light_idx_ = static_cast<int>(light_sources_.size()) - 1;
        UpdateLightList();
        UpdateLightInfo();
        UpdateStatus("Light source added from camera");
        MV_LOG_DATA("LightSource", "Name: " + ls.name + ", ColorTemp: " + std::to_string(ls.color_temp) + 
                    ", Image: " + std::to_string(img.cols) + "x" + std::to_string(img.rows));
    } else {
        UpdateStatus("Failed to capture image", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to capture image from camera");
    }
}

void ColorCharCalibPanel::OnCloseCamera(wxCommandEvent& event) {
    camera_.release();
    camera_opened_ = false;
    open_camera_btn_->Enable();
    capture_btn_->Disable();
    close_camera_btn_->Disable();
    UpdateStatus("Camera closed");
}

void ColorCharCalibPanel::OnRemove(wxCommandEvent& event) {
    if (current_light_idx_ >= 0 && current_light_idx_ < static_cast<int>(light_sources_.size())) {
        light_sources_.erase(light_sources_.begin() + current_light_idx_);
        current_light_idx_ = -1;
        UpdateLightList();
        UpdateLightInfo();
        UpdateStatus("Light source removed");
    }
}

void ColorCharCalibPanel::OnCalibrate(wxCommandEvent& event) {
    if (light_sources_.empty()) {
        UpdateStatus("Please add at least one light source", true);
        return;
    }

    wb_method_ = static_cast<mvtk::WBMethod>(wb_method_combo_->GetSelection());
    ccm_method_ = static_cast<mvtk::CCMMethod>(ccm_method_combo_->GetSelection());
    standard_color_ = color_std_combo_->GetSelection();
    quantize_ = quantize_checkbox_->GetValue();
    wb_quant_ctrl_->GetValue().ToInt(&wb_quant_);
    ccm_quant_ctrl_->GetValue().ToInt(&ccm_quant_);

    wb_results_.clear();
    ccm_results_.clear();

    std::vector<cv::Vec3d> ref_colors = StandardColorChart::getXriteClassic24();

    for (const auto& ls : light_sources_) {
        if (ls.image.empty()) continue;

        std::vector<cv::Rect> rects;
        for (const auto& roi : ls.rois) rects.push_back(roi.rect);

        std::vector<cv::Vec3d> src_colors = WhiteBalance::extractColors(ls.image, rects);

        mvtk::WBCalibParams wb_params_;
        wb_params_.method = wb_method_;
        auto wb_params = WhiteBalance::calibrateFromColors(src_colors, ref_colors, wb_params_);
        wb_results_.push_back(wb_params);

        mvtk::CCMCalibParams ccm_params_;
        ccm_params_.method = ccm_method_;
        auto ccm_params = ColorCorrectionMatrix::calibrate(src_colors, ref_colors, ccm_params_);
        ccm_results_.push_back(ccm_params);
    }

    wxString result_str;
    result_str += wxString::Format("Light Sources: %zu\nWB Results: %zu\nCCM Results: %zu\n\n",
                                   light_sources_.size(), wb_results_.size(), ccm_results_.size());

    if (!wb_results_.empty()) {
        result_str += "White Balance Parameters:\n";
        for (size_t i = 0; i < wb_results_.size() && i < light_sources_.size(); ++i) {
            result_str += wxString::Format("  %s: R=%.4f, G=%.4f, B=%.4f\n",
                                           light_sources_[i].name.c_str(),
                                           wb_results_[i].r_gain,
                                           wb_results_[i].g_gain,
                                           wb_results_[i].b_gain);
        }
    }

    if (!ccm_results_.empty()) {
        result_str += "\nCCM Matrix (first light source):\n";
        for (int i = 0; i < 3; ++i) {
            result_str += wxString::Format("  [%.4f, %.4f, %.4f]\n",
                                           ccm_results_[0].ccm(i, 0),
                                           ccm_results_[0].ccm(i, 1),
                                           ccm_results_[0].ccm(i, 2));
        }
    }

    result_text_->SetLabel(result_str);
    UpdateStatus(wxString::Format("Calibration completed: %zu light sources", wb_results_.size()));
    step_ = 2;
    UpdateStepButtons();
    notebook_->SetSelection(step_);
}

void ColorCharCalibPanel::OnExport(wxCommandEvent& event) {
    std::ofstream ofs("color_char_calib.dat");
    if (!ofs) {
        UpdateStatus("Failed to open export file", true);
        return;
    }

    ofs << "COLOR_CHAR_CALIB" << std::endl;
    ofs << light_sources_.size() << std::endl;

    for (size_t i = 0; i < light_sources_.size() && i < wb_results_.size() && i < ccm_results_.size(); ++i) {
        ofs << light_sources_[i].name << std::endl;
        ofs << light_sources_[i].color_temp << std::endl;
        ofs << wb_results_[i].r_gain << " " << wb_results_[i].g_gain << " " << wb_results_[i].b_gain << std::endl;
        for (int j = 0; j < 3; ++j) {
            ofs << ccm_results_[i].ccm(j, 0) << " " << ccm_results_[i].ccm(j, 1) << " " << ccm_results_[i].ccm(j, 2) << std::endl;
        }
    }

    ofs.close();
    UpdateStatus("Calibration data exported");
}

void ColorCharCalibPanel::OnBack(wxCommandEvent& event) {
    if (step_ > 0) {
        step_--;
        UpdateStepButtons();
        notebook_->SetSelection(step_);
    }
}

void ColorCharCalibPanel::OnNext(wxCommandEvent& event) {
    if (step_ == 0 && !light_sources_.empty()) {
        step_ = 1;
        UpdateStepButtons();
        notebook_->SetSelection(step_);
    } else if (step_ == 1) {
        step_ = 2;
        UpdateStepButtons();
        notebook_->SetSelection(step_);
    } else if (step_ == 2) {
        step_ = 3;
        UpdateStepButtons();
        notebook_->SetSelection(step_);
    }
}

void ColorCharCalibPanel::OnReset(wxCommandEvent& event) {
    step_ = 0;
    light_sources_.clear();
    wb_results_.clear();
    ccm_results_.clear();
    current_light_idx_ = -1;
    UpdateLightList();
    UpdateLightInfo();
    result_text_->SetLabel("");
    UpdateStepButtons();
    UpdateStatus("Reset completed");
}

}
}