#include "wxui/flat_field_panel.h"
#include "neumorphic_panel.h"
#include "style_defs.h"
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
    SetBackgroundColour(Style::NEU_BG_COLOR);
    SetFont(Style::GetSansFont(10));

    calib_params_.mode = LSCCalibMode::CENTER;
    calib_params_.edge_fill = 2;
    calib_params_.smooth_kernel = 64;
    calib_params_.target_brightness = 128.0;
    calib_params_.channel_mode = 0;

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* top_row = new wxBoxSizer(wxHORIZONTAL);

    NeumorphicPanel* flat_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* flat_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* flat_title = new wxStaticText(flat_panel, wxID_ANY, "Flat Field Image");
    Style::ApplyNeumorphicStyle(flat_title, true);
    flat_box->Add(flat_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* flat_row1 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* flat_source_label = new wxStaticText(flat_panel, wxID_ANY, "Source:");
    Style::ApplyNeumorphicStyle(flat_source_label);
    flat_row1->Add(flat_source_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    flat_source_combo_ = new wxComboBox(flat_panel, wxID_ANY, "Local Image", wxDefaultPosition, wxSize(120, -1),
                                        {"Local Image", "Camera"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(flat_source_combo_);
    flat_row1->Add(flat_source_combo_, 0);
    flat_box->Add(flat_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* flat_row2 = new wxBoxSizer(wxHORIZONTAL);
    load_flat_btn_ = new wxButton(flat_panel, FF_ID_LOAD_FLAT_BTN, "Load");
    Style::ApplyNeumorphicStyle(load_flat_btn_);
    flat_path_ctrl_ = new wxTextCtrl(flat_panel, wxID_ANY, "", wxDefaultPosition, wxSize(150, -1));
    Style::ApplyNeumorphicStyle(flat_path_ctrl_);
    flat_row2->Add(load_flat_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    flat_row2->Add(flat_path_ctrl_, 1);
    flat_box->Add(flat_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* flat_camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_flat_camera_btn_ = new wxButton(flat_panel, FF_ID_OPEN_FLAT_CAMERA_BTN, "Open Camera");
    Style::ApplyNeumorphicStyle(open_flat_camera_btn_);
    capture_flat_btn_ = new wxButton(flat_panel, FF_ID_CAPTURE_FLAT_BTN, "Capture");
    Style::ApplyNeumorphicStyle(capture_flat_btn_);
    close_flat_camera_btn_ = new wxButton(flat_panel, FF_ID_CLOSE_FLAT_CAMERA_BTN, "Close");
    Style::ApplyNeumorphicStyle(close_flat_camera_btn_);
    capture_flat_btn_->Disable();
    close_flat_camera_btn_->Disable();
    flat_camera_row->Add(open_flat_camera_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    flat_camera_row->Add(capture_flat_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    flat_camera_row->Add(close_flat_camera_btn_, 0);
    flat_box->Add(flat_camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    flat_panel->SetSizer(flat_box);
    top_row->Add(flat_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    NeumorphicPanel* dark_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* dark_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* dark_title = new wxStaticText(dark_panel, wxID_ANY, "Dark Frame");
    Style::ApplyNeumorphicStyle(dark_title, true);
    dark_box->Add(dark_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    use_dark_checkbox_ = new wxCheckBox(dark_panel, wxID_ANY, "Use Dark Frame");
    use_dark_checkbox_->SetFont(Style::GetSansFont(9));
    use_dark_checkbox_->SetForegroundColour(Style::NEU_TEXT_COLOR);
    dark_box->Add(use_dark_checkbox_, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* dark_row1 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* dark_source_label = new wxStaticText(dark_panel, wxID_ANY, "Source:");
    Style::ApplyNeumorphicStyle(dark_source_label);
    dark_row1->Add(dark_source_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    dark_source_combo_ = new wxComboBox(dark_panel, wxID_ANY, "Local Image", wxDefaultPosition, wxSize(120, -1),
                                        {"Local Image", "Camera"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(dark_source_combo_);
    dark_row1->Add(dark_source_combo_, 0);
    dark_box->Add(dark_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* dark_row2 = new wxBoxSizer(wxHORIZONTAL);
    load_dark_btn_ = new wxButton(dark_panel, FF_ID_LOAD_DARK_BTN, "Load");
    Style::ApplyNeumorphicStyle(load_dark_btn_);
    dark_path_ctrl_ = new wxTextCtrl(dark_panel, wxID_ANY, "", wxDefaultPosition, wxSize(150, -1));
    Style::ApplyNeumorphicStyle(dark_path_ctrl_);
    dark_row2->Add(load_dark_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    dark_row2->Add(dark_path_ctrl_, 1);
    dark_box->Add(dark_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* dark_camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_dark_camera_btn_ = new wxButton(dark_panel, FF_ID_OPEN_DARK_CAMERA_BTN, "Open Camera");
    Style::ApplyNeumorphicStyle(open_dark_camera_btn_);
    capture_dark_btn_ = new wxButton(dark_panel, FF_ID_CAPTURE_DARK_BTN, "Capture");
    Style::ApplyNeumorphicStyle(capture_dark_btn_);
    capture_dark_btn_->Disable();
    dark_camera_row->Add(open_dark_camera_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    dark_camera_row->Add(capture_dark_btn_, 0);
    dark_box->Add(dark_camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    dark_panel->SetSizer(dark_box);
    top_row->Add(dark_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    NeumorphicPanel* calib_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* calib_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* calib_title = new wxStaticText(calib_panel, wxID_ANY, "Calibration Parameters");
    Style::ApplyNeumorphicStyle(calib_title, true);
    calib_box->Add(calib_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* mode_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* mode_label = new wxStaticText(calib_panel, wxID_ANY, "Mode:");
    Style::ApplyNeumorphicStyle(mode_label);
    mode_row->Add(mode_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    calib_mode_combo_ = new wxComboBox(calib_panel, wxID_ANY, "Center Ref", wxDefaultPosition, wxSize(140, -1),
                                       {"Center Ref", "Brightest Area", "Target Bright"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(calib_mode_combo_);
    mode_row->Add(calib_mode_combo_, 0);
    calib_box->Add(mode_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* calib_box_inner = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* edge_fill_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* edge_fill_label = new wxStaticText(calib_panel, wxID_ANY, "Edge Fill:");
    Style::ApplyNeumorphicStyle(edge_fill_label);
    edge_fill_row->Add(edge_fill_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    edge_fill_slider_ = new wxSlider(calib_panel, wxID_ANY, 2, 0, 10, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(edge_fill_slider_);
    edge_fill_row->Add(edge_fill_slider_, 0, wxALIGN_LEFT);
    calib_box_inner->Add(edge_fill_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* smooth_kernel_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* smooth_kernel_label = new wxStaticText(calib_panel, wxID_ANY, "Smooth:");
    Style::ApplyNeumorphicStyle(smooth_kernel_label);
    smooth_kernel_row->Add(smooth_kernel_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    smooth_kernel_slider_ = new wxSlider(calib_panel, wxID_ANY, 64, 3, 128, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(smooth_kernel_slider_);
    smooth_kernel_row->Add(smooth_kernel_slider_, 0, wxALIGN_LEFT);
    calib_box_inner->Add(smooth_kernel_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* target_bright_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* target_bright_label = new wxStaticText(calib_panel, wxID_ANY, "Target:");
    Style::ApplyNeumorphicStyle(target_bright_label);
    target_bright_row->Add(target_bright_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    target_bright_slider_ = new wxSlider(calib_panel, wxID_ANY, 128, 0, 255, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(target_bright_slider_);
    target_bright_row->Add(target_bright_slider_, 0, wxALIGN_LEFT);
    calib_box_inner->Add(target_bright_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* ch_mode_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* ch_mode_label = new wxStaticText(calib_panel, wxID_ANY, "Channel:");
    Style::ApplyNeumorphicStyle(ch_mode_label);
    ch_mode_row->Add(ch_mode_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    channel_mode_combo_ = new wxComboBox(calib_panel, wxID_ANY, "Independent", wxDefaultPosition, wxSize(120, -1),
                                         {"Independent", "Unified"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(channel_mode_combo_);
    ch_mode_row->Add(channel_mode_combo_, 0);
    calib_box_inner->Add(ch_mode_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    calib_box->Add(calib_box_inner, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    calibrate_btn_ = new wxButton(calib_panel, FF_ID_CALIBRATE_BTN, "Calibrate");
    Style::ApplyNeumorphicStyle(calibrate_btn_);
    calib_box->Add(calibrate_btn_, 0, wxALIGN_CENTER | wxALL, Style::SPACING_MEDIUM);
    calib_panel->SetSizer(calib_box);
    top_row->Add(calib_panel, 1, wxEXPAND);

    main_sizer->Add(top_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, Style::SPACING_LARGE);

    status_text_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(status_text_);
    main_sizer->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    NeumorphicPanel* result_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* result_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* result_title = new wxStaticText(result_panel, wxID_ANY, "Calibration Result");
    Style::ApplyNeumorphicStyle(result_title, true);
    result_box->Add(result_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    result_text_ = new wxStaticText(result_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(result_text_);
    result_box->Add(result_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* result_btn_row = new wxBoxSizer(wxHORIZONTAL);
    apply_btn_ = new wxButton(result_panel, FF_ID_APPLY_BTN, "Apply Correction");
    Style::ApplyNeumorphicStyle(apply_btn_);
    export_gain_btn_ = new wxButton(result_panel, FF_ID_EXPORT_GAIN_BTN, "Export Gain Map");
    Style::ApplyNeumorphicStyle(export_gain_btn_);
    import_gain_btn_ = new wxButton(result_panel, FF_ID_IMPORT_GAIN_BTN, "Import Gain Map");
    Style::ApplyNeumorphicStyle(import_gain_btn_);
    result_btn_row->Add(apply_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    result_btn_row->Add(export_gain_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    result_btn_row->Add(import_gain_btn_, 0);
    result_box->Add(result_btn_row, 0, wxALIGN_CENTER | wxALL, Style::SPACING_MEDIUM);
    result_panel->SetSizer(result_box);
    main_sizer->Add(result_panel, 0, wxEXPAND | wxALL, Style::SPACING_LARGE);

    NeumorphicPanel* image_display_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* image_box = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* flat_img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* flat_img_title = new wxStaticText(image_display_panel, wxID_ANY, "Flat Field Image");
    Style::ApplyNeumorphicStyle(flat_img_title);
    flat_img_box->Add(flat_img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    flat_canvas_ = new ImageCanvas(image_display_panel, wxID_ANY);
    flat_canvas_->SetMinSize(wxSize(350, 250));
    flat_img_box->Add(flat_canvas_, 1, wxEXPAND);
    image_box->Add(flat_img_box, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    wxBoxSizer* dark_img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* dark_img_title = new wxStaticText(image_display_panel, wxID_ANY, "Dark Frame");
    Style::ApplyNeumorphicStyle(dark_img_title);
    dark_img_box->Add(dark_img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    dark_canvas_ = new ImageCanvas(image_display_panel, wxID_ANY);
    dark_canvas_->SetMinSize(wxSize(350, 250));
    dark_img_box->Add(dark_canvas_, 1, wxEXPAND);
    image_box->Add(dark_img_box, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    wxBoxSizer* result_img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* result_img_title = new wxStaticText(image_display_panel, wxID_ANY, "Corrected Image");
    Style::ApplyNeumorphicStyle(result_img_title);
    result_img_box->Add(result_img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    result_canvas_ = new ImageCanvas(image_display_panel, wxID_ANY);
    result_canvas_->SetMinSize(wxSize(350, 250));
    result_img_box->Add(result_canvas_, 1, wxEXPAND);
    image_box->Add(result_img_box, 1, wxEXPAND);
    image_display_panel->SetSizer(image_box);
    main_sizer->Add(image_display_panel, 1, wxEXPAND | wxALL, Style::SPACING_LARGE);

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
        status_text_->SetForegroundColour(*wxBLUE);
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