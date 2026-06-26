#include "wxui/spectral_calib_panel.h"
#include <opencv2/imgproc.hpp>

namespace mvtk {
namespace wxui {

BEGIN_EVENT_TABLE(SpectralCalibPanel, wxPanel)
    EVT_BUTTON(SP_ID_STEP_BTN_0, SpectralCalibPanel::OnStepButton)
    EVT_BUTTON(SP_ID_STEP_BTN_1, SpectralCalibPanel::OnStepButton)
    EVT_BUTTON(SP_ID_STEP_BTN_2, SpectralCalibPanel::OnStepButton)
    EVT_BUTTON(SP_ID_STEP_BTN_3, SpectralCalibPanel::OnStepButton)
    EVT_COMBOBOX(wxID_ANY, SpectralCalibPanel::OnCalibModeChanged)
    EVT_COMBOBOX(wxID_ANY, SpectralCalibPanel::OnComplexityChanged)
    EVT_BUTTON(SP_ID_IMPORT_FACTORY_BTN, SpectralCalibPanel::OnImportFactory)
    EVT_BUTTON(SP_ID_ADD_LIGHT_BTN, SpectralCalibPanel::OnAddLight)
    EVT_BUTTON(SP_ID_COMPUTE_REGION_BTN, SpectralCalibPanel::OnComputeRegion)
    EVT_COMBOBOX(wxID_ANY, SpectralCalibPanel::OnDataSourceChanged)
    EVT_BUTTON(SP_ID_OPEN_CAMERA_BTN, SpectralCalibPanel::OnOpenCamera)
    EVT_BUTTON(SP_ID_CAPTURE_IMAGE_BTN, SpectralCalibPanel::OnCaptureImage)
    EVT_BUTTON(SP_ID_CLOSE_CAMERA_BTN, SpectralCalibPanel::OnCloseCamera)
    EVT_BUTTON(SP_ID_LOAD_CURVE_BTN, SpectralCalibPanel::OnLoadCurve)
    EVT_BUTTON(SP_ID_PREVIEW_BTN, SpectralCalibPanel::OnPreview)
    EVT_BUTTON(SP_ID_EXPORT_BTN, SpectralCalibPanel::OnExport)
    EVT_BUTTON(SP_ID_BACK_BTN, SpectralCalibPanel::OnBack)
    EVT_BUTTON(SP_ID_NEXT_BTN, SpectralCalibPanel::OnNext)
    EVT_BUTTON(SP_ID_RESET_BTN, SpectralCalibPanel::OnReset)
END_EVENT_TABLE()

SpectralCalibPanel::SpectralCalibPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* step_sizer = new wxBoxSizer(wxHORIZONTAL);
    const char* steps[] = {"1. Config", "2. Capture", "3. Preview", "4. Export"};
    const int step_ids[] = {SP_ID_STEP_BTN_0, SP_ID_STEP_BTN_1, SP_ID_STEP_BTN_2, SP_ID_STEP_BTN_3};
    for (int i = 0; i < 4; ++i) {
        step_btns_[i] = new wxButton(this, step_ids[i], steps[i], wxDefaultPosition, wxSize(120, 30));
        step_sizer->Add(step_btns_[i], 0, wxRIGHT, 5);
    }
    main_sizer->Add(step_sizer, 0, wxALIGN_CENTER | wxALL, 5);

    wxStaticLine* sep = new wxStaticLine(this, wxID_ANY);
    main_sizer->Add(sep, 0, wxEXPAND | wxALL, 5);

    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

    wxPanel* page0 = new wxPanel(notebook);
    wxBoxSizer* page0_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* config_static_box = new wxStaticBox(page0, wxID_ANY, "Configuration");
    wxStaticBoxSizer* config_box = new wxStaticBoxSizer(config_static_box, wxVERTICAL);
    wxGridSizer* config_grid = new wxGridSizer(2, 2, 5);
    config_grid->Add(new wxStaticText(config_static_box, wxID_ANY, "Calibration Mode:"), 0, wxALIGN_CENTER_VERTICAL);
    calib_mode_combo_ = new wxComboBox(config_static_box, wxID_ANY, "Factory Calibration", wxDefaultPosition, wxDefaultSize,
                                       {"Factory Calibration", "User Calibration"}, wxCB_READONLY);
    config_grid->Add(calib_mode_combo_, 1);
    config_grid->Add(new wxStaticText(config_static_box, wxID_ANY, "Complexity:"), 0, wxALIGN_CENTER_VERTICAL);
    complexity_combo_ = new wxComboBox(config_static_box, wxID_ANY, "Standard", wxDefaultPosition, wxDefaultSize,
                                        {"Standard", "Simple"}, wxCB_READONLY);
    config_grid->Add(complexity_combo_, 1);
    config_box->Add(config_grid, 0, wxEXPAND | wxALL, 5);

    wxStaticBox* factory_static_box = new wxStaticBox(page0, wxID_ANY, "Factory Calibration");
    wxStaticBoxSizer* factory_box = new wxStaticBoxSizer(factory_static_box, wxVERTICAL);
    wxBoxSizer* factory_row = new wxBoxSizer(wxHORIZONTAL);
    factory_path_ctrl_ = new wxTextCtrl(factory_static_box, wxID_ANY, "", wxDefaultPosition, wxSize(250, -1));
    import_factory_btn_ = new wxButton(factory_static_box, SP_ID_IMPORT_FACTORY_BTN, "Import");
    factory_row->Add(factory_path_ctrl_, 1, wxRIGHT, 5);
    factory_row->Add(import_factory_btn_, 0);
    factory_box->Add(factory_row, 0, wxEXPAND | wxALL, 5);
    page0_sizer->Add(factory_box, 0, wxEXPAND | wxALL, 5);

    wxStaticBox* user_static_box = new wxStaticBox(page0, wxID_ANY, "User Calibration");
    wxStaticBoxSizer* user_box = new wxStaticBoxSizer(user_static_box, wxVERTICAL);
    wxStaticText* light_text = new wxStaticText(user_static_box, wxID_ANY, "Light Sources:");
    user_box->Add(light_text, 0, wxALL, 5);

    light_list_ = new wxListBox(user_static_box, wxID_ANY, wxDefaultPosition, wxSize(-1, 100));
    user_box->Add(light_list_, 1, wxEXPAND | wxALL, 5);

    wxGridSizer* light_grid = new wxGridSizer(2, 2, 5);
    light_grid->Add(new wxStaticText(user_static_box, wxID_ANY, "New Light Name:"), 0, wxALIGN_CENTER_VERTICAL);
    new_light_name_ctrl_ = new wxTextCtrl(user_static_box, wxID_ANY, "A");
    light_grid->Add(new_light_name_ctrl_, 1);
    light_grid->Add(new wxStaticText(user_static_box, wxID_ANY, "Color Temperature:"), 0, wxALIGN_CENTER_VERTICAL);
    new_light_temp_slider_ = new wxSlider(user_static_box, wxID_ANY, 29, 20, 100);
    light_grid->Add(new_light_temp_slider_, 1);
    user_box->Add(light_grid, 0, wxEXPAND | wxALL, 5);

    add_light_btn_ = new wxButton(user_static_box, SP_ID_ADD_LIGHT_BTN, "Add Light Source");
    user_box->Add(add_light_btn_, 0, wxALIGN_CENTER | wxALL, 5);
    page0_sizer->Add(user_box, 0, wxEXPAND | wxALL, 5);

    wxStaticBox* region_static_box = new wxStaticBox(page0, wxID_ANY, "Calibration Region");
    wxStaticBoxSizer* region_box = new wxStaticBoxSizer(region_static_box, wxVERTICAL);
    wxGridSizer* region_grid = new wxGridSizer(2, 2, 5);
    region_grid->Add(new wxStaticText(region_static_box, wxID_ANY, "Focal Length (mm):"), 0, wxALIGN_CENTER_VERTICAL);
    focal_slider_ = new wxSlider(region_static_box, wxID_ANY, 25, 1, 100);
    region_grid->Add(focal_slider_, 1);
    region_grid->Add(new wxStaticText(region_static_box, wxID_ANY, "Working Distance (mm):"), 0, wxALIGN_CENTER_VERTICAL);
    distance_slider_ = new wxSlider(region_static_box, wxID_ANY, 50, 10, 200);
    region_grid->Add(distance_slider_, 1);
    region_grid->Add(new wxStaticText(region_static_box, wxID_ANY, "Sensor Width (mm):"), 0, wxALIGN_CENTER_VERTICAL);
    sensor_w_slider_ = new wxSlider(region_static_box, wxID_ANY, 57, 10, 500);
    region_grid->Add(sensor_w_slider_, 1);
    region_grid->Add(new wxStaticText(region_static_box, wxID_ANY, "Sensor Height (mm):"), 0, wxALIGN_CENTER_VERTICAL);
    sensor_h_slider_ = new wxSlider(region_static_box, wxID_ANY, 43, 10, 500);
    region_grid->Add(sensor_h_slider_, 1);
    region_box->Add(region_grid, 0, wxEXPAND | wxALL, 5);

    compute_region_btn_ = new wxButton(region_static_box, SP_ID_COMPUTE_REGION_BTN, "Compute Calib Region");
    region_box->Add(compute_region_btn_, 0, wxALIGN_CENTER | wxALL, 5);
    page0_sizer->Add(region_box, 0, wxEXPAND | wxALL, 5);
    page0->SetSizer(page0_sizer);
    notebook->AddPage(page0, "Step 1");

    wxPanel* page1 = new wxPanel(notebook);
    wxBoxSizer* page1_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* capture_static_box = new wxStaticBox(page1, wxID_ANY, "Capture Spectral Data");
    wxStaticBoxSizer* capture_box = new wxStaticBoxSizer(capture_static_box, wxVERTICAL);
    wxBoxSizer* data_row = new wxBoxSizer(wxHORIZONTAL);
    data_row->Add(new wxStaticText(capture_static_box, wxID_ANY, "Data Source:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    data_source_combo_ = new wxComboBox(capture_static_box, wxID_ANY, "Camera", wxDefaultPosition, wxDefaultSize,
                                        {"Camera", "Local Curve"}, wxCB_READONLY);
    data_row->Add(data_source_combo_, 1);
    capture_box->Add(data_row, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_camera_btn_ = new wxButton(capture_static_box, SP_ID_OPEN_CAMERA_BTN, "Open Camera");
    capture_image_btn_ = new wxButton(capture_static_box, SP_ID_CAPTURE_IMAGE_BTN, "Capture Image");
    close_camera_btn_ = new wxButton(capture_static_box, SP_ID_CLOSE_CAMERA_BTN, "Close Camera");
    capture_image_btn_->Disable();
    close_camera_btn_->Disable();
    camera_row->Add(open_camera_btn_, 0, wxRIGHT, 5);
    camera_row->Add(capture_image_btn_, 0, wxRIGHT, 5);
    camera_row->Add(close_camera_btn_, 0);
    capture_box->Add(camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* curve_row = new wxBoxSizer(wxHORIZONTAL);
    curve_path_ctrl_ = new wxTextCtrl(capture_static_box, wxID_ANY, "", wxDefaultPosition, wxSize(250, -1));
    load_curve_btn_ = new wxButton(capture_static_box, SP_ID_LOAD_CURVE_BTN, "Load Curve");
    curve_row->Add(curve_path_ctrl_, 1, wxRIGHT, 5);
    curve_row->Add(load_curve_btn_, 0);
    capture_box->Add(curve_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* img_box = new wxBoxSizer(wxVERTICAL);
    img_box->Add(new wxStaticText(capture_static_box, wxID_ANY, "Captured Image"), 0, wxALIGN_CENTER | wxBOTTOM, 5);
    image_canvas_ = new ImageCanvas(page1, wxID_ANY);
    image_canvas_->SetMinSize(wxSize(600, 400));
    img_box->Add(image_canvas_, 1, wxEXPAND);
    capture_box->Add(img_box, 1, wxEXPAND | wxALL, 5);
    page1_sizer->Add(capture_box, 1, wxEXPAND | wxALL, 5);
    page1->SetSizer(page1_sizer);
    notebook->AddPage(page1, "Step 2");

    wxPanel* page2 = new wxPanel(notebook);
    wxBoxSizer* page2_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* preview_static_box = new wxStaticBox(page2, wxID_ANY, "Preview Result");
    wxStaticBoxSizer* preview_box = new wxStaticBoxSizer(preview_static_box, wxVERTICAL);
    preview_btn_ = new wxButton(preview_static_box, SP_ID_PREVIEW_BTN, "Preview");
    preview_box->Add(preview_btn_, 0, wxALIGN_CENTER | wxALL, 5);
    page2_sizer->Add(preview_box, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* preview_img_box = new wxBoxSizer(wxVERTICAL);
    preview_img_box->Add(new wxStaticText(page2, wxID_ANY, "Preview Image"), 0, wxALIGN_CENTER | wxBOTTOM, 5);
    ImageCanvas* preview_canvas = new ImageCanvas(page2, wxID_ANY);
    preview_canvas->SetMinSize(wxSize(600, 400));
    preview_img_box->Add(preview_canvas, 1, wxEXPAND);
    page2_sizer->Add(preview_img_box, 1, wxEXPAND | wxALL, 5);
    page2->SetSizer(page2_sizer);
    notebook->AddPage(page2, "Step 3");

    wxPanel* page3 = new wxPanel(notebook);
    wxBoxSizer* page3_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBox* export_static_box = new wxStaticBox(page3, wxID_ANY, "Export Calibration");
    wxStaticBoxSizer* export_box = new wxStaticBoxSizer(export_static_box, wxVERTICAL);
    export_box->Add(new wxStaticText(export_static_box, wxID_ANY, "Export to: spectral_calib.dat"), 0, wxALL, 5);
    export_btn_ = new wxButton(export_static_box, SP_ID_EXPORT_BTN, "Export");
    export_box->Add(export_btn_, 0, wxALIGN_CENTER | wxALL, 5);
    page3_sizer->Add(export_box, 1, wxEXPAND | wxALL, 5);
    page3->SetSizer(page3_sizer);
    notebook->AddPage(page3, "Step 4");

    main_sizer->Add(notebook, 1, wxEXPAND | wxALL, 5);

    wxBoxSizer* nav_sizer = new wxBoxSizer(wxHORIZONTAL);
    back_btn_ = new wxButton(this, SP_ID_BACK_BTN, "<< Back");
    next_btn_ = new wxButton(this, SP_ID_NEXT_BTN, "Next >>");
    reset_btn_ = new wxButton(this, SP_ID_RESET_BTN, "Reset");
    nav_sizer->Add(back_btn_, 0, wxRIGHT, 10);
    nav_sizer->Add(next_btn_, 0, wxRIGHT, 10);
    nav_sizer->Add(reset_btn_, 0);
    main_sizer->Add(nav_sizer, 0, wxALIGN_CENTER | wxALL, 5);

    status_text_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    main_sizer->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    SetSizer(main_sizer);
    UpdateStepButtons();
}

SpectralCalibPanel::~SpectralCalibPanel() {
    if (camera_opened_) {
        camera_.release();
    }
}

void SpectralCalibPanel::UpdateStepButtons() {
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

void SpectralCalibPanel::UpdateStatus(const wxString& msg, bool is_error) {
    status_text_->SetLabel(msg);
    if (is_error) {
        status_text_->SetForegroundColour(*wxRED);
    } else {
        status_text_->SetForegroundColour(*wxYELLOW);
    }
}

void SpectralCalibPanel::UpdateLightList() {
    light_list_->Clear();
    for (size_t i = 0; i < light_sources_.size(); ++i) {
        light_list_->Append(wxString::Format("%s (%.0fK)", light_sources_[i].name.c_str(), light_sources_[i].color_temperature));
    }
}

void SpectralCalibPanel::OnStepButton(wxCommandEvent& event) {
    for (int i = 0; i < 4; ++i) {
        if (event.GetEventObject() == step_btns_[i]) {
            step_ = i;
            UpdateStepButtons();
            break;
        }
    }
}

void SpectralCalibPanel::OnCalibModeChanged(wxCommandEvent& event) {
    calib_mode_ = calib_mode_combo_->GetSelection();
}

void SpectralCalibPanel::OnComplexityChanged(wxCommandEvent& event) {
    complexity_ = complexity_combo_->GetSelection();
}

void SpectralCalibPanel::OnImportFactory(wxCommandEvent& event) {
    wxString path = factory_path_ctrl_->GetValue();
    factory_sources_ = SpectralCalibrator::importFactoryCalibFile(path.ToStdString());
    UpdateStatus(wxString::Format("Factory calibration imported: %zu light sources", factory_sources_.size()));
}

void SpectralCalibPanel::OnAddLight(wxCommandEvent& event) {
    SpectralCalibrator::addLightSource(light_sources_, 
        new_light_name_ctrl_->GetValue().ToStdString(), 
        new_light_temp_slider_->GetValue() * 100.0, {});
    UpdateLightList();
    UpdateStatus("Light source added: " + new_light_name_ctrl_->GetValue());
}

void SpectralCalibPanel::OnComputeRegion(wxCommandEvent& event) {
    focal_length_ = focal_slider_->GetValue();
    working_distance_ = distance_slider_->GetValue() * 10.0;
    sensor_width_ = sensor_w_slider_->GetValue() / 10.0;
    sensor_height_ = sensor_h_slider_->GetValue() / 10.0;

    calib_region_ = SpectralCalibrator::computeCalibRegion(
        focal_length_, working_distance_, sensor_width_, sensor_height_);
    UpdateStatus("Calibration region computed");
}

void SpectralCalibPanel::OnDataSourceChanged(wxCommandEvent& event) {
    image_source_ = data_source_combo_->GetSelection();
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
        capture_image_btn_->Disable();
        close_camera_btn_->Disable();
        open_camera_btn_->Enable();
    }
}

void SpectralCalibPanel::OnOpenCamera(wxCommandEvent& event) {
    MV_LOG_OPERATION("Open Camera", "Camera Index: 0");
    camera_.open(0);
    if (camera_.isOpened()) {
        camera_opened_ = true;
        open_camera_btn_->Disable();
        capture_image_btn_->Enable();
        close_camera_btn_->Enable();
        UpdateStatus("Camera opened successfully");
        MV_LOG_DATA("CameraStatus", "Opened successfully");
    } else {
        UpdateStatus("Failed to open camera", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to open camera index 0");
    }
}

void SpectralCalibPanel::OnCaptureImage(wxCommandEvent& event) {
    MV_LOG_OPERATION("Capture Image", "Source: Camera");
    camera_ >> captured_image_;
    if (!captured_image_.empty()) {
        image_canvas_->SetImage(captured_image_);
        UpdateStatus(wxString::Format("Image captured: %dx%d", captured_image_.cols, captured_image_.rows));
        MV_LOG_DATA("CapturedImage", "Size: " + std::to_string(captured_image_.cols) + "x" + std::to_string(captured_image_.rows));
    } else {
        UpdateStatus("Failed to capture image", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to capture image from camera");
    }
}

void SpectralCalibPanel::OnCloseCamera(wxCommandEvent& event) {
    camera_.release();
    camera_opened_ = false;
    open_camera_btn_->Enable();
    capture_image_btn_->Disable();
    close_camera_btn_->Disable();
    UpdateStatus("Camera closed");
}

void SpectralCalibPanel::OnLoadCurve(wxCommandEvent& event) {
    UpdateStatus("Curve loaded from file");
}

void SpectralCalibPanel::OnPreview(wxCommandEvent& event) {
    spectral_preview_ = cv::Mat(400, 400, CV_8UC3, cv::Scalar(50, 50, 50));
    cv::putText(spectral_preview_, "Spectral Preview", cv::Point(50, 200),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
    UpdateStatus("Preview generated");
}

void SpectralCalibPanel::OnExport(wxCommandEvent& event) {
    SpectralCalibrator::generateCalibFile(light_sources_, 
        static_cast<mvtk::SpectralCalibMode>(calib_mode_),
        static_cast<mvtk::SpectralComplexity>(complexity_),
        "spectral_calib.dat");
    UpdateStatus("Calibration data exported");
}

void SpectralCalibPanel::OnBack(wxCommandEvent& event) {
    if (step_ > 0) {
        step_--;
        UpdateStepButtons();
    }
}

void SpectralCalibPanel::OnNext(wxCommandEvent& event) {
    if (step_ == 0) {
        step_ = 1;
        UpdateStepButtons();
    } else if (step_ == 1 && (!captured_image_.empty() || image_source_ == 1)) {
        step_ = 2;
        UpdateStepButtons();
    } else if (step_ == 2) {
        step_ = 3;
        UpdateStepButtons();
    }
}

void SpectralCalibPanel::OnReset(wxCommandEvent& event) {
    step_ = 0;
    light_sources_.clear();
    captured_image_ = cv::Mat();
    spectral_preview_ = cv::Mat();
    image_canvas_->Clear();
    UpdateLightList();
    UpdateStepButtons();
    UpdateStatus("Reset completed");
}

}
}