#include "wxui/dead_pixel_panel.h"
#include "neumorphic_panel.h"
#include "style_defs.h"
#include "utils/image_io.h"
#include <wx/grid.h>

namespace mvtk {
namespace wxui {

BEGIN_EVENT_TABLE(DeadPixelPanel, wxPanel)
    EVT_COMBOBOX(wxID_ANY, DeadPixelPanel::OnSourceChanged)
    EVT_BUTTON(DP_ID_LOAD_BTN, DeadPixelPanel::OnLoadImage)
    EVT_BUTTON(DP_ID_OPEN_CAMERA_BTN, DeadPixelPanel::OnOpenCamera)
    EVT_BUTTON(DP_ID_CAPTURE_BTN, DeadPixelPanel::OnCapture)
    EVT_BUTTON(DP_ID_CLOSE_CAMERA_BTN, DeadPixelPanel::OnCloseCamera)
    EVT_BUTTON(DP_ID_DETECT_BTN, DeadPixelPanel::OnDetect)
    EVT_BUTTON(DP_ID_CORRECT_BTN, DeadPixelPanel::OnCorrect)
    EVT_BUTTON(DP_ID_EXPORT_BTN, DeadPixelPanel::OnExport)
    EVT_BUTTON(DP_ID_IMPORT_BTN, DeadPixelPanel::OnImport)
    EVT_BUTTON(DP_ID_CLEAR_BTN, DeadPixelPanel::OnClear)
END_EVENT_TABLE()

DeadPixelPanel::DeadPixelPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    SetBackgroundColour(Style::NEU_BG_COLOR);
    SetFont(Style::GetSansFont(10));

    detect_params_.type = DeadPixelType::BRIGHT;
    detect_params_.threshold = 30;
    detect_params_.avg_brightness_min = 0;
    detect_params_.avg_brightness_max = 255;
    detect_params_.kernel_size = 3;

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* top_row = new wxBoxSizer(wxHORIZONTAL);

    NeumorphicPanel* image_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* image_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* image_title = new wxStaticText(image_panel, wxID_ANY, "Image Source");
    Style::ApplyNeumorphicStyle(image_title, true);
    image_box->Add(image_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* image_row1 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* source_label = new wxStaticText(image_panel, wxID_ANY, "Source:");
    Style::ApplyNeumorphicStyle(source_label);
    image_row1->Add(source_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    source_combo_ = new wxComboBox(image_panel, wxID_ANY, "Local Image", wxDefaultPosition, wxSize(120, -1),
                                   {"Local Image", "Camera"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(source_combo_);
    image_row1->Add(source_combo_, 0);
    image_box->Add(image_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* image_row2 = new wxBoxSizer(wxHORIZONTAL);
    load_btn_ = new wxButton(image_panel, DP_ID_LOAD_BTN, "Load");
    Style::ApplyNeumorphicStyle(load_btn_);
    path_ctrl_ = new wxTextCtrl(image_panel, wxID_ANY, "", wxDefaultPosition, wxSize(200, -1));
    Style::ApplyNeumorphicStyle(path_ctrl_);
    image_row2->Add(load_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    image_row2->Add(path_ctrl_, 1);
    image_box->Add(image_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_camera_btn_ = new wxButton(image_panel, DP_ID_OPEN_CAMERA_BTN, "Open Camera");
    Style::ApplyNeumorphicStyle(open_camera_btn_);
    capture_btn_ = new wxButton(image_panel, DP_ID_CAPTURE_BTN, "Capture");
    Style::ApplyNeumorphicStyle(capture_btn_);
    close_camera_btn_ = new wxButton(image_panel, DP_ID_CLOSE_CAMERA_BTN, "Close");
    Style::ApplyNeumorphicStyle(close_camera_btn_);
    capture_btn_->Disable();
    close_camera_btn_->Disable();
    camera_row->Add(open_camera_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    camera_row->Add(capture_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    camera_row->Add(close_camera_btn_, 0);
    image_box->Add(camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    status_text_ = new wxStaticText(image_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(status_text_);
    image_box->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    image_panel->SetSizer(image_box);
    top_row->Add(image_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    NeumorphicPanel* detect_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* detect_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* detect_title = new wxStaticText(detect_panel, wxID_ANY, "Detection Parameters");
    Style::ApplyNeumorphicStyle(detect_title, true);
    detect_box->Add(detect_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* type_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* type_label = new wxStaticText(detect_panel, wxID_ANY, "Type:");
    Style::ApplyNeumorphicStyle(type_label);
    type_row->Add(type_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    dp_type_combo_ = new wxComboBox(detect_panel, wxID_ANY, "Bright Dead Pixel", wxDefaultPosition, wxSize(140, -1),
                                     {"Bright Dead Pixel", "Dark Dead Pixel"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(dp_type_combo_);
    type_row->Add(dp_type_combo_, 0);
    detect_box->Add(type_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* detect_box_inner = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* thresh_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* thresh_label = new wxStaticText(detect_panel, wxID_ANY, "Threshold:");
    Style::ApplyNeumorphicStyle(thresh_label);
    thresh_row->Add(thresh_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    threshold_slider_ = new wxSlider(detect_panel, wxID_ANY, 30, 1, 100, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(threshold_slider_);
    thresh_row->Add(threshold_slider_, 0, wxALIGN_LEFT);
    detect_box_inner->Add(thresh_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* min_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* min_label = new wxStaticText(detect_panel, wxID_ANY, "Min Bright:");
    Style::ApplyNeumorphicStyle(min_label);
    min_row->Add(min_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    min_bright_slider_ = new wxSlider(detect_panel, wxID_ANY, 0, 0, 255, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(min_bright_slider_);
    min_row->Add(min_bright_slider_, 0, wxALIGN_LEFT);
    detect_box_inner->Add(min_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* max_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* max_label = new wxStaticText(detect_panel, wxID_ANY, "Max Bright:");
    Style::ApplyNeumorphicStyle(max_label);
    max_row->Add(max_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    max_bright_slider_ = new wxSlider(detect_panel, wxID_ANY, 255, 0, 255, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(max_bright_slider_);
    max_row->Add(max_bright_slider_, 0, wxALIGN_LEFT);
    detect_box_inner->Add(max_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* kernel_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* kernel_label = new wxStaticText(detect_panel, wxID_ANY, "Kernel:");
    Style::ApplyNeumorphicStyle(kernel_label);
    kernel_row->Add(kernel_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    kernel_slider_ = new wxSlider(detect_panel, wxID_ANY, 3, 3, 5, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(kernel_slider_);
    kernel_row->Add(kernel_slider_, 0, wxALIGN_LEFT);
    detect_box_inner->Add(kernel_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    detect_box->Add(detect_box_inner, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* detect_btn_row = new wxBoxSizer(wxHORIZONTAL);
    detect_btn_ = new wxButton(detect_panel, DP_ID_DETECT_BTN, "Auto Detect");
    Style::ApplyNeumorphicStyle(detect_btn_);
    multi_frame_btn_ = new wxButton(detect_panel, wxID_ANY, "Multi-Frame");
    Style::ApplyNeumorphicStyle(multi_frame_btn_);
    detect_btn_row->Add(detect_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    detect_btn_row->Add(multi_frame_btn_, 0);
    detect_box->Add(detect_btn_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    detect_panel->SetSizer(detect_box);
    top_row->Add(detect_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    NeumorphicPanel* correct_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* correct_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* correct_title = new wxStaticText(correct_panel, wxID_ANY, "Correction");
    Style::ApplyNeumorphicStyle(correct_title, true);
    correct_box->Add(correct_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* correct_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* method_label = new wxStaticText(correct_panel, wxID_ANY, "Method:");
    Style::ApplyNeumorphicStyle(method_label);
    correct_row->Add(method_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    correct_method_combo_ = new wxComboBox(correct_panel, wxID_ANY, "Mean Interpolation", wxDefaultPosition, wxSize(180, -1),
                                            {"Mean Interpolation", "Median Interpolation"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(correct_method_combo_);
    correct_row->Add(correct_method_combo_, 0, wxRIGHT, Style::SPACING_LARGE);
    correct_btn_ = new wxButton(correct_panel, DP_ID_CORRECT_BTN, "Correct");
    Style::ApplyNeumorphicStyle(correct_btn_);
    correct_row->Add(correct_btn_, 0);
    correct_box->Add(correct_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    correct_panel->SetSizer(correct_box);
    top_row->Add(correct_panel, 1, wxEXPAND);

    main_sizer->Add(top_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, Style::SPACING_LARGE);

    NeumorphicPanel* list_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* list_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* list_title = new wxStaticText(list_panel, wxID_ANY, "Dead Pixel List");
    Style::ApplyNeumorphicStyle(list_title, true);
    list_box->Add(list_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    dead_pixel_grid_ = new wxGrid(list_panel, wxID_ANY);
    dead_pixel_grid_->CreateGrid(0, 4);
    dead_pixel_grid_->SetColLabelValue(0, "X");
    dead_pixel_grid_->SetColLabelValue(1, "Y");
    dead_pixel_grid_->SetColLabelValue(2, "Type");
    dead_pixel_grid_->SetColLabelValue(3, "Value");
    dead_pixel_grid_->SetColSize(0, 60);
    dead_pixel_grid_->SetColSize(1, 60);
    dead_pixel_grid_->SetColSize(2, 80);
    dead_pixel_grid_->SetColSize(3, 100);
    dead_pixel_grid_->EnableEditing(false);
    dead_pixel_grid_->SetFont(Style::GetSansFont(9));
    dead_pixel_grid_->SetMaxSize(wxSize(-1, 200));
    dead_pixel_grid_->SetMinSize(wxSize(-1, 200));
    list_box->Add(dead_pixel_grid_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* list_btn_row = new wxBoxSizer(wxHORIZONTAL);
    show_detected_checkbox_ = new wxCheckBox(list_panel, wxID_ANY, "Show Detected");
    show_detected_checkbox_->SetFont(Style::GetSansFont(9));
    show_detected_checkbox_->SetForegroundColour(Style::NEU_TEXT_COLOR);
    show_detected_checkbox_->SetValue(true);
    export_btn_ = new wxButton(list_panel, DP_ID_EXPORT_BTN, "Export CSV");
    Style::ApplyNeumorphicStyle(export_btn_);
    import_btn_ = new wxButton(list_panel, DP_ID_IMPORT_BTN, "Import CSV");
    Style::ApplyNeumorphicStyle(import_btn_);
    clear_btn_ = new wxButton(list_panel, DP_ID_CLEAR_BTN, "Clear");
    Style::ApplyNeumorphicStyle(clear_btn_);
    list_btn_row->Add(show_detected_checkbox_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_LARGE);
    list_btn_row->Add(export_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    list_btn_row->Add(import_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    list_btn_row->Add(clear_btn_, 0);
    list_box->Add(list_btn_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    list_panel->SetSizer(list_box);
    list_panel->SetMaxSize(wxSize(-1, 280));
    list_panel->SetMinSize(wxSize(-1, 280));
    main_sizer->Add(list_panel, 0, wxEXPAND | wxALL, Style::SPACING_LARGE);

    NeumorphicPanel* image_display_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* image_box2 = new wxBoxSizer(wxHORIZONTAL);
    
    wxBoxSizer* src_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* src_title = new wxStaticText(image_display_panel, wxID_ANY, "Source Image");
    Style::ApplyNeumorphicStyle(src_title);
    src_box->Add(src_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    src_canvas_ = new ImageCanvas(image_display_panel, wxID_ANY);
    src_canvas_->SetMinSize(wxSize(400, 300));
    src_box->Add(src_canvas_, 1, wxEXPAND);
    image_box2->Add(src_box, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    wxBoxSizer* result_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* result_title = new wxStaticText(image_display_panel, wxID_ANY, "Corrected Image");
    Style::ApplyNeumorphicStyle(result_title);
    result_box->Add(result_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    result_canvas_ = new ImageCanvas(image_display_panel, wxID_ANY);
    result_canvas_->SetMinSize(wxSize(400, 300));
    result_box->Add(result_canvas_, 1, wxEXPAND);
    image_box2->Add(result_box, 1, wxEXPAND);
    image_display_panel->SetSizer(image_box2);
    main_sizer->Add(image_display_panel, 1, wxEXPAND | wxALL, Style::SPACING_LARGE);

    SetSizer(main_sizer);
    UpdateUI();
}

DeadPixelPanel::~DeadPixelPanel() {
    if (camera_opened_) {
        camera_.release();
    }
}

void DeadPixelPanel::UpdateStatus(const wxString& msg, bool is_error) {
    status_text_->SetLabel(msg);
    if (is_error) {
        status_text_->SetForegroundColour(*wxRED);
    } else {
        status_text_->SetForegroundColour(*wxBLUE);
    }
}

void DeadPixelPanel::UpdateUI() {
    bool is_local = (image_source_ == 0);
    path_ctrl_->Show(is_local);
    load_btn_->Show(is_local);
    open_camera_btn_->Show(!is_local && !camera_opened_);
    capture_btn_->Show(!is_local && camera_opened_);
    close_camera_btn_->Show(!is_local && camera_opened_);
    Layout();
}

void DeadPixelPanel::UpdateGrid() {
    dead_pixel_grid_->ClearGrid();
    int current_rows = dead_pixel_grid_->GetNumberRows();
    if (current_rows > 0) {
        dead_pixel_grid_->DeleteRows(0, current_rows);
    }
    dead_pixel_grid_->InsertRows(0, dead_pixels_.size());

    for (size_t i = 0; i < dead_pixels_.size(); ++i) {
        dead_pixel_grid_->SetCellValue(i, 0, wxString::Format("%d", dead_pixels_[i].x));
        dead_pixel_grid_->SetCellValue(i, 1, wxString::Format("%d", dead_pixels_[i].y));
        dead_pixel_grid_->SetCellValue(i, 2, dead_pixels_[i].type == DeadPixelType::BRIGHT ? "Bright" : "Dark");
        dead_pixel_grid_->SetCellValue(i, 3, wxString::Format("%.1f", dead_pixels_[i].value));
    }
}

void DeadPixelPanel::OnSourceChanged(wxCommandEvent& event) {
    image_source_ = source_combo_->GetSelection();
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
    }
    UpdateUI();
}

void DeadPixelPanel::OnLoadImage(wxCommandEvent& event) {
    wxString path = path_ctrl_->GetValue();
    if (path.IsEmpty()) {
        UpdateStatus("Please enter a file path", true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Empty file path");
        return;
    }
    MV_LOG_OPERATION("Load Image", "Path: " + path.ToStdString());
    src_image_ = mvtk::utils::imreadUtf8(path.ToStdString());
    if (!src_image_.empty()) {
        vis_image_ = src_image_.clone();
        src_canvas_->SetImage(vis_image_);
        UpdateStatus(wxString::Format("Image loaded: %dx%d", src_image_.cols, src_image_.rows));
        MV_LOG_DATA("ImageInfo", "Size: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows));
    } else {
        UpdateStatus("Failed to load image: " + path, true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Failed to load image: " + path.ToStdString());
    }
}

void DeadPixelPanel::OnOpenCamera(wxCommandEvent& event) {
    MV_LOG_OPERATION("Open Camera", "Camera Index: 0");
    camera_.open(0);
    if (camera_.isOpened()) {
        camera_opened_ = true;
        UpdateStatus("Camera opened successfully");
        MV_LOG_DATA("CameraStatus", "Opened successfully");
    } else {
        UpdateStatus("Failed to open camera", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to open camera index 0");
    }
    UpdateUI();
}

void DeadPixelPanel::OnCapture(wxCommandEvent& event) {
    MV_LOG_OPERATION("Capture Image", "Source: Camera");
    camera_ >> src_image_;
    if (!src_image_.empty()) {
        vis_image_ = src_image_.clone();
        src_canvas_->SetImage(vis_image_);
        UpdateStatus(wxString::Format("Image captured: %dx%d", src_image_.cols, src_image_.rows));
        MV_LOG_DATA("CapturedImage", "Size: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows));
    } else {
        UpdateStatus("Failed to capture image", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to capture image from camera");
    }
}

void DeadPixelPanel::OnCloseCamera(wxCommandEvent& event) {
    camera_.release();
    camera_opened_ = false;
    UpdateStatus("Camera closed");
    UpdateUI();
}

void DeadPixelPanel::OnDetect(wxCommandEvent& event) {
    if (src_image_.empty()) {
        UpdateStatus("Please load or capture an image first", true);
        return;
    }

    detect_params_.type = static_cast<DeadPixelType>(dp_type_combo_->GetSelection());
    detect_params_.threshold = threshold_slider_->GetValue();
    detect_params_.avg_brightness_min = min_bright_slider_->GetValue();
    detect_params_.avg_brightness_max = max_bright_slider_->GetValue();
    detect_params_.kernel_size = kernel_slider_->GetValue();

    dead_pixels_ = DeadPixelDetector::detect(src_image_, detect_params_);
    vis_image_ = DeadPixelDetector::visualize(src_image_, dead_pixels_);
    src_canvas_->SetImage(vis_image_);
    UpdateGrid();
    UpdateStatus(wxString::Format("Detected %zu dead pixels", dead_pixels_.size()));
}

void DeadPixelPanel::OnCorrect(wxCommandEvent& event) {
    if (src_image_.empty()) {
        UpdateStatus("Please load or capture an image first", true);
        return;
    }
    if (dead_pixels_.empty()) {
        UpdateStatus("No dead pixels to correct", true);
        return;
    }

    correct_params_.method = correct_method_combo_->GetSelection();
    corrected_image_ = DeadPixelDetector::correct(src_image_, dead_pixels_, correct_params_);
    result_canvas_->SetImage(corrected_image_);
    UpdateStatus("Correction completed");
}

void DeadPixelPanel::OnExport(wxCommandEvent& event) {
    DeadPixelDetector::exportCSV(dead_pixels_, csv_path_);
    UpdateStatus("Dead pixels exported to " + wxString(csv_path_));
}

void DeadPixelPanel::OnImport(wxCommandEvent& event) {
    dead_pixels_ = DeadPixelDetector::importCSV(csv_path_);
    UpdateGrid();
    if (!src_image_.empty()) {
        vis_image_ = DeadPixelDetector::visualize(src_image_, dead_pixels_);
        src_canvas_->SetImage(vis_image_);
    }
    UpdateStatus(wxString::Format("Dead pixels imported: %zu", dead_pixels_.size()));
}

void DeadPixelPanel::OnClear(wxCommandEvent& event) {
    dead_pixels_.clear();
    vis_image_ = src_image_.clone();
    src_canvas_->SetImage(vis_image_);
    UpdateGrid();
    UpdateStatus("Dead pixel list cleared");
}

}
}