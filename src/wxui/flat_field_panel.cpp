#include "wxui/flat_field_panel.h"
#include "utils/image_io.h"

namespace mvtk {
namespace wxui {

BEGIN_EVENT_TABLE(FlatFieldPanel, wxPanel)
    EVT_COMBOBOX(wxID_ANY, FlatFieldPanel::OnFlatSourceChanged)
    EVT_BUTTON(FF_ID_LOAD_FLAT_BTN, FlatFieldPanel::OnLoadFlat)
    EVT_BUTTON(FF_ID_OPEN_FLAT_CAMERA_BTN, FlatFieldPanel::OnOpenFlatCamera)
    EVT_BUTTON(FF_ID_CAPTURE_FLAT_BTN, FlatFieldPanel::OnCaptureFlat)
    EVT_BUTTON(FF_ID_CLOSE_FLAT_CAMERA_BTN, FlatFieldPanel::OnCloseFlatCamera)
    EVT_CHECKBOX(wxID_ANY, FlatFieldPanel::OnUseDarkChanged)
    EVT_COMBOBOX(wxID_ANY, FlatFieldPanel::OnDarkSourceChanged)
    EVT_BUTTON(FF_ID_LOAD_DARK_BTN, FlatFieldPanel::OnLoadDark)
    EVT_BUTTON(FF_ID_OPEN_DARK_CAMERA_BTN, FlatFieldPanel::OnOpenDarkCamera)
    EVT_BUTTON(FF_ID_CAPTURE_DARK_BTN, FlatFieldPanel::OnCaptureDark)
    EVT_BUTTON(FF_ID_CALIBRATE_BTN, FlatFieldPanel::OnCalibrate)
    EVT_BUTTON(FF_ID_APPLY_BTN, FlatFieldPanel::OnApply)
    EVT_BUTTON(FF_ID_EXPORT_GAIN_BTN, FlatFieldPanel::OnExportGain)
    EVT_BUTTON(FF_ID_IMPORT_GAIN_BTN, FlatFieldPanel::OnImportGain)
END_EVENT_TABLE()

FlatFieldPanel::FlatFieldPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    calib_params_.mode = LSCCalibMode::CENTER;
    calib_params_.edge_fill = 2;
    calib_params_.smooth_kernel = 64;
    calib_params_.target_brightness = 128.0;
    calib_params_.channel_mode = 0;

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* flat_static_box = new wxStaticBox(this, wxID_ANY, "Flat Field Image");
    wxStaticBoxSizer* flat_box = new wxStaticBoxSizer(flat_static_box, wxVERTICAL);
    wxBoxSizer* flat_row1 = new wxBoxSizer(wxHORIZONTAL);
    flat_row1->Add(new wxStaticText(flat_static_box, wxID_ANY, "Source:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    flat_source_combo_ = new wxComboBox(flat_static_box, wxID_ANY, "Local Image", wxDefaultPosition, wxDefaultSize,
                                        {"Local Image", "Camera"}, wxCB_READONLY);
    flat_row1->Add(flat_source_combo_, 1);
    flat_box->Add(flat_row1, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* flat_row2 = new wxBoxSizer(wxHORIZONTAL);
    flat_path_ctrl_ = new wxTextCtrl(flat_static_box, wxID_ANY, "", wxDefaultPosition, wxSize(300, -1));
    load_flat_btn_ = new wxButton(flat_static_box, FF_ID_LOAD_FLAT_BTN, "Load Flat");
    flat_row2->Add(flat_path_ctrl_, 1, wxRIGHT, 5);
    flat_row2->Add(load_flat_btn_, 0);
    flat_box->Add(flat_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* flat_camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_flat_camera_btn_ = new wxButton(flat_static_box, FF_ID_OPEN_FLAT_CAMERA_BTN, "Open Camera");
    capture_flat_btn_ = new wxButton(flat_static_box, FF_ID_CAPTURE_FLAT_BTN, "Capture Flat");
    close_flat_camera_btn_ = new wxButton(flat_static_box, FF_ID_CLOSE_FLAT_CAMERA_BTN, "Close Camera");
    capture_flat_btn_->Disable();
    close_flat_camera_btn_->Disable();
    flat_camera_row->Add(open_flat_camera_btn_, 0, wxRIGHT, 5);
    flat_camera_row->Add(capture_flat_btn_, 0, wxRIGHT, 5);
    flat_camera_row->Add(close_flat_camera_btn_, 0);
    flat_box->Add(flat_camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    main_sizer->Add(flat_box, 0, wxEXPAND | wxALL, 5);

    wxStaticBox* dark_static_box = new wxStaticBox(this, wxID_ANY, "Dark Frame");
    wxStaticBoxSizer* dark_box = new wxStaticBoxSizer(dark_static_box, wxVERTICAL);
    use_dark_checkbox_ = new wxCheckBox(dark_static_box, wxID_ANY, "Use Dark Frame");
    dark_box->Add(use_dark_checkbox_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* dark_row1 = new wxBoxSizer(wxHORIZONTAL);
    dark_row1->Add(new wxStaticText(dark_static_box, wxID_ANY, "Source:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    dark_source_combo_ = new wxComboBox(dark_static_box, wxID_ANY, "Local Image", wxDefaultPosition, wxDefaultSize,
                                        {"Local Image", "Camera"}, wxCB_READONLY);
    dark_row1->Add(dark_source_combo_, 1);
    dark_box->Add(dark_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* dark_row2 = new wxBoxSizer(wxHORIZONTAL);
    dark_path_ctrl_ = new wxTextCtrl(dark_static_box, wxID_ANY, "", wxDefaultPosition, wxSize(300, -1));
    load_dark_btn_ = new wxButton(dark_static_box, FF_ID_LOAD_DARK_BTN, "Load Dark");
    dark_row2->Add(dark_path_ctrl_, 1, wxRIGHT, 5);
    dark_row2->Add(load_dark_btn_, 0);
    dark_box->Add(dark_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* dark_camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_dark_camera_btn_ = new wxButton(dark_static_box, FF_ID_OPEN_DARK_CAMERA_BTN, "Open Camera");
    capture_dark_btn_ = new wxButton(dark_static_box, FF_ID_CAPTURE_DARK_BTN, "Capture Dark");
    capture_dark_btn_->Disable();
    dark_camera_row->Add(open_dark_camera_btn_, 0, wxRIGHT, 5);
    dark_camera_row->Add(capture_dark_btn_, 0);
    dark_box->Add(dark_camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    main_sizer->Add(dark_box, 0, wxEXPAND | wxALL, 5);

    status_text_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    main_sizer->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxStaticBox* calib_static_box = new wxStaticBox(this, wxID_ANY, "Calibration Parameters");
    wxStaticBoxSizer* calib_box = new wxStaticBoxSizer(calib_static_box, wxVERTICAL);
    wxGridSizer* calib_grid = new wxGridSizer(2, 4, 5);

    calib_grid->Add(new wxStaticText(calib_static_box, wxID_ANY, "Mode:"), 0, wxALIGN_CENTER_VERTICAL);
    calib_mode_combo_ = new wxComboBox(calib_static_box, wxID_ANY, "Center Reference", wxDefaultPosition, wxDefaultSize,
                                       {"Center Reference", "Brightest Area", "Target Brightness"}, wxCB_READONLY);
    calib_grid->Add(calib_mode_combo_, 1);

    calib_grid->Add(new wxStaticText(calib_static_box, wxID_ANY, "Edge Fill:"), 0, wxALIGN_CENTER_VERTICAL);
    edge_fill_slider_ = new wxSlider(calib_static_box, wxID_ANY, 2, 0, 10);
    calib_grid->Add(edge_fill_slider_, 1);

    calib_grid->Add(new wxStaticText(calib_static_box, wxID_ANY, "Smooth Kernel:"), 0, wxALIGN_CENTER_VERTICAL);
    smooth_kernel_slider_ = new wxSlider(calib_static_box, wxID_ANY, 64, 3, 128);
    calib_grid->Add(smooth_kernel_slider_, 1);

    calib_grid->Add(new wxStaticText(calib_static_box, wxID_ANY, "Target Bright:"), 0, wxALIGN_CENTER_VERTICAL);
    target_bright_slider_ = new wxSlider(calib_static_box, wxID_ANY, 128, 0, 255);
    calib_grid->Add(target_bright_slider_, 1);

    calib_box->Add(calib_grid, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* ch_mode_row = new wxBoxSizer(wxHORIZONTAL);
    ch_mode_row->Add(new wxStaticText(calib_static_box, wxID_ANY, "Channel Mode:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    channel_mode_combo_ = new wxComboBox(calib_static_box, wxID_ANY, "Independent Channels", wxDefaultPosition, wxDefaultSize,
                                         {"Independent Channels", "Unified Gain"}, wxCB_READONLY);
    ch_mode_row->Add(channel_mode_combo_, 1);
    calib_box->Add(ch_mode_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    calibrate_btn_ = new wxButton(calib_static_box, FF_ID_CALIBRATE_BTN, "Calibrate");
    calib_box->Add(calibrate_btn_, 0, wxALIGN_CENTER | wxALL, 5);
    main_sizer->Add(calib_box, 0, wxEXPAND | wxALL, 5);

    wxStaticBox* result_static_box = new wxStaticBox(this, wxID_ANY, "Calibration Result");
    wxStaticBoxSizer* result_box = new wxStaticBoxSizer(result_static_box, wxVERTICAL);
    result_text_ = new wxStaticText(result_static_box, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    result_box->Add(result_text_, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* result_btn_row = new wxBoxSizer(wxHORIZONTAL);
    apply_btn_ = new wxButton(result_static_box, FF_ID_APPLY_BTN, "Apply Correction");
    export_gain_btn_ = new wxButton(result_static_box, FF_ID_EXPORT_GAIN_BTN, "Export Gain Map");
    import_gain_btn_ = new wxButton(result_static_box, FF_ID_IMPORT_GAIN_BTN, "Import Gain Map");
    result_btn_row->Add(apply_btn_, 0, wxRIGHT, 5);
    result_btn_row->Add(export_gain_btn_, 0, wxRIGHT, 5);
    result_btn_row->Add(import_gain_btn_, 0);
    result_box->Add(result_btn_row, 0, wxALIGN_CENTER | wxALL, 5);
    main_sizer->Add(result_box, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* image_box = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* flat_img_box = new wxBoxSizer(wxVERTICAL);
    flat_img_box->Add(new wxStaticText(this, wxID_ANY, "Flat Field Image"), 0, wxALIGN_CENTER | wxBOTTOM, 5);
    flat_canvas_ = new ImageCanvas(this, wxID_ANY);
    flat_canvas_->SetMinSize(wxSize(400, 300));
    flat_img_box->Add(flat_canvas_, 1, wxEXPAND);
    image_box->Add(flat_img_box, 1, wxEXPAND | wxRIGHT, 10);

    wxBoxSizer* dark_img_box = new wxBoxSizer(wxVERTICAL);
    dark_img_box->Add(new wxStaticText(this, wxID_ANY, "Dark Frame"), 0, wxALIGN_CENTER | wxBOTTOM, 5);
    dark_canvas_ = new ImageCanvas(this, wxID_ANY);
    dark_canvas_->SetMinSize(wxSize(400, 300));
    dark_img_box->Add(dark_canvas_, 1, wxEXPAND);
    image_box->Add(dark_img_box, 1, wxEXPAND | wxRIGHT, 10);

    wxBoxSizer* result_img_box = new wxBoxSizer(wxVERTICAL);
    result_img_box->Add(new wxStaticText(this, wxID_ANY, "Corrected Image"), 0, wxALIGN_CENTER | wxBOTTOM, 5);
    result_canvas_ = new ImageCanvas(this, wxID_ANY);
    result_canvas_->SetMinSize(wxSize(400, 300));
    result_img_box->Add(result_canvas_, 1, wxEXPAND);
    image_box->Add(result_img_box, 1, wxEXPAND);
    main_sizer->Add(image_box, 1, wxEXPAND | wxALL, 5);

    SetSizer(main_sizer);
    UpdateUI();
}

FlatFieldPanel::~FlatFieldPanel() {
    if (camera_opened_) {
        camera_.release();
    }
}

void FlatFieldPanel::UpdateStatus(const wxString& msg, bool is_error) {
    status_text_->SetLabel(msg);
    if (is_error) {
        status_text_->SetForegroundColour(*wxRED);
    } else {
        status_text_->SetForegroundColour(*wxYELLOW);
    }
}

void FlatFieldPanel::UpdateUI() {
    bool is_flat_local = (flat_source_ == 0);
    flat_path_ctrl_->Show(is_flat_local);
    load_flat_btn_->Show(is_flat_local);
    open_flat_camera_btn_->Show(!is_flat_local && !camera_opened_);
    capture_flat_btn_->Show(!is_flat_local && camera_opened_);
    close_flat_camera_btn_->Show(!is_flat_local && camera_opened_);

    bool is_dark_local = (dark_source_ == 0);
    dark_source_combo_->Enable(use_dark_);
    dark_path_ctrl_->Show(use_dark_ && is_dark_local);
    load_dark_btn_->Show(use_dark_ && is_dark_local);
    open_dark_camera_btn_->Show(use_dark_ && !is_dark_local && !camera_opened_);
    capture_dark_btn_->Show(use_dark_ && !is_dark_local && camera_opened_);

    Layout();
}

void FlatFieldPanel::OnFlatSourceChanged(wxCommandEvent& event) {
    flat_source_ = flat_source_combo_->GetSelection();
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
    }
    UpdateUI();
}

void FlatFieldPanel::OnLoadFlat(wxCommandEvent& event) {
    wxString path = flat_path_ctrl_->GetValue();
    if (path.IsEmpty()) {
        UpdateStatus("Please enter a file path", true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Empty flat file path");
        return;
    }
    MV_LOG_OPERATION("Load Flat Image", "Path: " + path.ToStdString());
    src_image_ = mvtk::utils::imreadUtf8(path.ToStdString());
    if (!src_image_.empty()) {
        flat_canvas_->SetImage(src_image_);
        UpdateStatus(wxString::Format("Flat image loaded: %dx%d", src_image_.cols, src_image_.rows));
        MV_LOG_DATA("ImageInfo", "Size: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows));
    } else {
        UpdateStatus("Failed to load flat image: " + path, true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Failed to load flat image: " + path.ToStdString());
    }
}

void FlatFieldPanel::OnOpenFlatCamera(wxCommandEvent& event) {
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

void FlatFieldPanel::OnCaptureFlat(wxCommandEvent& event) {
    MV_LOG_OPERATION("Capture Flat Image", "Source: Camera");
    camera_ >> src_image_;
    if (!src_image_.empty()) {
        flat_canvas_->SetImage(src_image_);
        UpdateStatus("Flat image captured");
        MV_LOG_DATA("CapturedImage", "Size: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows));
    } else {
        UpdateStatus("Failed to capture flat image", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to capture flat image from camera");
    }
}

void FlatFieldPanel::OnCloseFlatCamera(wxCommandEvent& event) {
    camera_.release();
    camera_opened_ = false;
    UpdateStatus("Camera closed");
    UpdateUI();
}

void FlatFieldPanel::OnUseDarkChanged(wxCommandEvent& event) {
    use_dark_ = use_dark_checkbox_->GetValue();
    UpdateUI();
}

void FlatFieldPanel::OnDarkSourceChanged(wxCommandEvent& event) {
    dark_source_ = dark_source_combo_->GetSelection();
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
    }
    UpdateUI();
}

void FlatFieldPanel::OnLoadDark(wxCommandEvent& event) {
    wxString path = dark_path_ctrl_->GetValue();
    if (path.IsEmpty()) {
        UpdateStatus("Please enter a file path", true);
        return;
    }
    dark_image_ = mvtk::utils::imreadUtf8(path.ToStdString());
    if (!dark_image_.empty()) {
        dark_canvas_->SetImage(dark_image_);
        UpdateStatus("Dark image loaded");
    } else {
        UpdateStatus("Failed to load dark image", true);
    }
}

void FlatFieldPanel::OnOpenDarkCamera(wxCommandEvent& event) {
    camera_.open(0);
    if (camera_.isOpened()) {
        camera_opened_ = true;
        UpdateStatus("Camera opened successfully");
    } else {
        UpdateStatus("Failed to open camera", true);
    }
    UpdateUI();
}

void FlatFieldPanel::OnCaptureDark(wxCommandEvent& event) {
    camera_ >> dark_image_;
    if (!dark_image_.empty()) {
        dark_canvas_->SetImage(dark_image_);
        UpdateStatus("Dark image captured");
    } else {
        UpdateStatus("Failed to capture dark image", true);
    }
}

void FlatFieldPanel::OnCalibrate(wxCommandEvent& event) {
    if (src_image_.empty()) {
        UpdateStatus("Please load or capture a flat field image first", true);
        return;
    }

    calib_params_.mode = static_cast<LSCCalibMode>(calib_mode_combo_->GetSelection());
    calib_params_.edge_fill = edge_fill_slider_->GetValue();
    calib_params_.smooth_kernel = smooth_kernel_slider_->GetValue();
    calib_params_.target_brightness = target_bright_slider_->GetValue();
    calib_params_.channel_mode = channel_mode_combo_->GetSelection();

    if (use_dark_ && !dark_image_.empty()) {
        calib_result_ = FlatFieldCorrector::calibrateWithDark(src_image_, dark_image_, calib_params_);
    } else {
        calib_result_ = FlatFieldCorrector::calibrate(src_image_, calib_params_);
    }

    double min_g, max_g;
    cv::minMaxLoc(calib_result_.gain_map, &min_g, &max_g);
    result_text_->SetLabel(wxString::Format(
        "Uniformity: %.2f%%\nCenter Brightness: %.1f\nEdge Brightness: %.1f\nGain Range: [%.3f, %.3f]",
        calib_result_.uniformity * 100.0,
        calib_result_.center_brightness,
        calib_result_.edge_brightness,
        min_g, max_g));
    UpdateStatus("Calibration completed");
}

void FlatFieldPanel::OnApply(wxCommandEvent& event) {
    if (src_image_.empty()) {
        UpdateStatus("Please load or capture an image first", true);
        return;
    }
    if (calib_result_.gain_map.empty()) {
        UpdateStatus("Please calibrate first", true);
        return;
    }
    corrected_image_ = FlatFieldCorrector::apply(src_image_, calib_result_.gain_map, calib_params_.edge_fill);
    result_canvas_->SetImage(corrected_image_);
    UpdateStatus("Correction applied");
}

void FlatFieldPanel::OnExportGain(wxCommandEvent& event) {
    if (calib_result_.gain_map.empty()) {
        UpdateStatus("Please calibrate first", true);
        return;
    }
    FlatFieldCorrector::exportGainMap(calib_result_.gain_map, "gain_map.dat");
    UpdateStatus("Gain map exported");
}

void FlatFieldPanel::OnImportGain(wxCommandEvent& event) {
    calib_result_.gain_map = FlatFieldCorrector::importGainMap("gain_map.dat");
    if (!calib_result_.gain_map.empty()) {
        UpdateStatus("Gain map imported");
    } else {
        UpdateStatus("Failed to import gain map", true);
    }
}

}
}