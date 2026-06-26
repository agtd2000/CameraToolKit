#include "wxui/dead_pixel_panel.h"
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
    detect_params_.type = DeadPixelType::BRIGHT;
    detect_params_.threshold = 30;
    detect_params_.avg_brightness_min = 0;
    detect_params_.avg_brightness_max = 255;
    detect_params_.kernel_size = 3;

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* image_static_box = new wxStaticBox(this, wxID_ANY, "Image Source");
    wxStaticBoxSizer* image_box = new wxStaticBoxSizer(image_static_box, wxVERTICAL);
    wxBoxSizer* image_row1 = new wxBoxSizer(wxHORIZONTAL);
    image_row1->Add(new wxStaticText(image_static_box, wxID_ANY, "Source:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    source_combo_ = new wxComboBox(image_static_box, wxID_ANY, "Local Image", wxDefaultPosition, wxDefaultSize,
                                   {"Local Image", "Camera"}, wxCB_READONLY);
    image_row1->Add(source_combo_, 1);
    image_box->Add(image_row1, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* image_row2 = new wxBoxSizer(wxHORIZONTAL);
    path_ctrl_ = new wxTextCtrl(image_static_box, wxID_ANY, "", wxDefaultPosition, wxSize(300, -1));
    load_btn_ = new wxButton(image_static_box, DP_ID_LOAD_BTN, "Load");
    image_row2->Add(path_ctrl_, 1, wxRIGHT, 5);
    image_row2->Add(load_btn_, 0);
    image_box->Add(image_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_camera_btn_ = new wxButton(image_static_box, DP_ID_OPEN_CAMERA_BTN, "Open Camera");
    capture_btn_ = new wxButton(image_static_box, DP_ID_CAPTURE_BTN, "Capture");
    close_camera_btn_ = new wxButton(image_static_box, DP_ID_CLOSE_CAMERA_BTN, "Close Camera");
    capture_btn_->Disable();
    close_camera_btn_->Disable();
    camera_row->Add(open_camera_btn_, 0, wxRIGHT, 5);
    camera_row->Add(capture_btn_, 0, wxRIGHT, 5);
    camera_row->Add(close_camera_btn_, 0);
    image_box->Add(camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    status_text_ = new wxStaticText(image_static_box, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    image_box->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    main_sizer->Add(image_box, 0, wxEXPAND | wxALL, 5);

    wxStaticBox* detect_static_box = new wxStaticBox(this, wxID_ANY, "Detection Parameters");
    wxStaticBoxSizer* detect_box = new wxStaticBoxSizer(detect_static_box, wxVERTICAL);
    wxGridSizer* detect_grid = new wxGridSizer(2, 5, 5);

    detect_grid->Add(new wxStaticText(detect_static_box, wxID_ANY, "Type:"), 0, wxALIGN_CENTER_VERTICAL);
    dp_type_combo_ = new wxComboBox(detect_static_box, wxID_ANY, "Bright Dead Pixel", wxDefaultPosition, wxDefaultSize,
                                     {"Bright Dead Pixel", "Dark Dead Pixel"}, wxCB_READONLY);
    detect_grid->Add(dp_type_combo_, 1);

    detect_grid->Add(new wxStaticText(detect_static_box, wxID_ANY, "Threshold:"), 0, wxALIGN_CENTER_VERTICAL);
    threshold_slider_ = new wxSlider(detect_static_box, wxID_ANY, 30, 1, 100);
    detect_grid->Add(threshold_slider_, 1);

    detect_grid->Add(new wxStaticText(detect_static_box, wxID_ANY, "Min Bright:"), 0, wxALIGN_CENTER_VERTICAL);
    min_bright_slider_ = new wxSlider(detect_static_box, wxID_ANY, 0, 0, 255);
    detect_grid->Add(min_bright_slider_, 1);

    detect_grid->Add(new wxStaticText(detect_static_box, wxID_ANY, "Max Bright:"), 0, wxALIGN_CENTER_VERTICAL);
    max_bright_slider_ = new wxSlider(detect_static_box, wxID_ANY, 255, 0, 255);
    detect_grid->Add(max_bright_slider_, 1);

    detect_grid->Add(new wxStaticText(detect_static_box, wxID_ANY, "Kernel:"), 0, wxALIGN_CENTER_VERTICAL);
    kernel_slider_ = new wxSlider(detect_static_box, wxID_ANY, 3, 3, 5);
    detect_grid->Add(kernel_slider_, 1);

    detect_box->Add(detect_grid, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* detect_btn_row = new wxBoxSizer(wxHORIZONTAL);
    detect_btn_ = new wxButton(detect_static_box, DP_ID_DETECT_BTN, "Auto Detect");
    multi_frame_btn_ = new wxButton(detect_static_box, wxID_ANY, "Detect (Multi-Frame)");
    detect_btn_row->Add(detect_btn_, 0, wxRIGHT, 5);
    detect_btn_row->Add(multi_frame_btn_, 0);
    detect_box->Add(detect_btn_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    main_sizer->Add(detect_box, 0, wxEXPAND | wxALL, 5);

    wxStaticBox* list_static_box = new wxStaticBox(this, wxID_ANY, "Dead Pixel List");
    wxStaticBoxSizer* list_box = new wxStaticBoxSizer(list_static_box, wxVERTICAL);
    dead_pixel_grid_ = new wxGrid(list_static_box, wxID_ANY);
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
    list_box->Add(dead_pixel_grid_, 1, wxEXPAND | wxALL, 5);

    wxBoxSizer* list_btn_row = new wxBoxSizer(wxHORIZONTAL);
    show_detected_checkbox_ = new wxCheckBox(list_static_box, wxID_ANY, "Show Detected");
    show_detected_checkbox_->SetValue(true);
    export_btn_ = new wxButton(list_static_box, DP_ID_EXPORT_BTN, "Export CSV");
    import_btn_ = new wxButton(list_static_box, DP_ID_IMPORT_BTN, "Import CSV");
    clear_btn_ = new wxButton(list_static_box, DP_ID_CLEAR_BTN, "Clear");
    list_btn_row->Add(show_detected_checkbox_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    list_btn_row->Add(export_btn_, 0, wxRIGHT, 5);
    list_btn_row->Add(import_btn_, 0, wxRIGHT, 5);
    list_btn_row->Add(clear_btn_, 0);
    list_box->Add(list_btn_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    main_sizer->Add(list_box, 0, wxEXPAND | wxALL, 5);

    wxStaticBox* correct_static_box = new wxStaticBox(this, wxID_ANY, "Correction");
    wxStaticBoxSizer* correct_box = new wxStaticBoxSizer(correct_static_box, wxVERTICAL);
    wxBoxSizer* correct_row = new wxBoxSizer(wxHORIZONTAL);
    correct_row->Add(new wxStaticText(correct_static_box, wxID_ANY, "Method:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    correct_method_combo_ = new wxComboBox(correct_static_box, wxID_ANY, "Mean Interpolation", wxDefaultPosition, wxDefaultSize,
                                            {"Mean Interpolation", "Median Interpolation"}, wxCB_READONLY);
    correct_row->Add(correct_method_combo_, 1, wxRIGHT, 10);
    correct_btn_ = new wxButton(correct_static_box, DP_ID_CORRECT_BTN, "Correct");
    correct_row->Add(correct_btn_, 0);
    correct_box->Add(correct_row, 0, wxEXPAND | wxALL, 5);
    main_sizer->Add(correct_box, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* image_box2 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* src_box = new wxBoxSizer(wxVERTICAL);
    src_box->Add(new wxStaticText(this, wxID_ANY, "Source Image"), 0, wxALIGN_CENTER | wxBOTTOM, 5);
    src_canvas_ = new ImageCanvas(this, wxID_ANY);
    src_canvas_->SetMinSize(wxSize(400, 300));
    src_box->Add(src_canvas_, 1, wxEXPAND);
    image_box2->Add(src_box, 1, wxEXPAND | wxRIGHT, 10);

    wxBoxSizer* result_box = new wxBoxSizer(wxVERTICAL);
    result_box->Add(new wxStaticText(this, wxID_ANY, "Corrected Image"), 0, wxALIGN_CENTER | wxBOTTOM, 5);
    result_canvas_ = new ImageCanvas(this, wxID_ANY);
    result_canvas_->SetMinSize(wxSize(400, 300));
    result_box->Add(result_canvas_, 1, wxEXPAND);
    image_box2->Add(result_box, 1, wxEXPAND);
    main_sizer->Add(image_box2, 1, wxEXPAND | wxALL, 5);

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
        status_text_->SetForegroundColour(*wxYELLOW);
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
    dead_pixel_grid_->DeleteRows(0, dead_pixel_grid_->GetNumberRows());
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