#include "wxui/spectral_calib_panel.h"
#include "neumorphic_panel.h"
#include "style_defs.h"
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
    SetBackgroundColour(Style::NEU_BG_COLOR);
    SetFont(Style::GetSansFont(10));

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* step_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* step_sizer = new wxBoxSizer(wxHORIZONTAL);
    const char* steps[] = {"1. Config", "2. Capture", "3. Preview", "4. Export"};
    const int step_ids[] = {SP_ID_STEP_BTN_0, SP_ID_STEP_BTN_1, SP_ID_STEP_BTN_2, SP_ID_STEP_BTN_3};
    for (int i = 0; i < 4; ++i) {
        step_btns_[i] = new wxButton(step_panel, step_ids[i], steps[i], wxDefaultPosition, wxSize(120, 32));
        Style::ApplyNeumorphicStyle(step_btns_[i]);
        step_sizer->Add(step_btns_[i], 0, wxRIGHT, Style::SPACING_SMALL);
    }
    step_panel->SetSizer(step_sizer);
    main_sizer->Add(step_panel, 0, wxALIGN_CENTER | wxALL, Style::SPACING_LARGE);

    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);
    notebook->SetBackgroundColour(Style::NEU_BG_COLOR);

    NeumorphicPanel* page0 = new NeumorphicPanel(notebook);
    wxBoxSizer* page0_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* page0_top_row = new wxBoxSizer(wxHORIZONTAL);

    NeumorphicPanel* config_panel = new NeumorphicPanel(page0);
    wxBoxSizer* config_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* config_title = new wxStaticText(config_panel, wxID_ANY, "Configuration");
    Style::ApplyNeumorphicStyle(config_title, true);
    config_box->Add(config_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* mode_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* mode_label = new wxStaticText(config_panel, wxID_ANY, "Mode:");
    Style::ApplyNeumorphicStyle(mode_label);
    mode_row->Add(mode_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    calib_mode_combo_ = new wxComboBox(config_panel, wxID_ANY, "Factory", wxDefaultPosition, wxSize(120, -1),
                                       {"Factory", "User"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(calib_mode_combo_);
    mode_row->Add(calib_mode_combo_, 0);
    config_box->Add(mode_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* comp_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* comp_label = new wxStaticText(config_panel, wxID_ANY, "Complexity:");
    Style::ApplyNeumorphicStyle(comp_label);
    comp_row->Add(comp_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    complexity_combo_ = new wxComboBox(config_panel, wxID_ANY, "Standard", wxDefaultPosition, wxSize(100, -1),
                                        {"Standard", "Simple"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(complexity_combo_);
    comp_row->Add(complexity_combo_, 0);
    config_box->Add(comp_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    config_panel->SetSizer(config_box);
    page0_top_row->Add(config_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    NeumorphicPanel* factory_panel = new NeumorphicPanel(page0);
    wxBoxSizer* factory_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* factory_title = new wxStaticText(factory_panel, wxID_ANY, "Factory Calibration");
    Style::ApplyNeumorphicStyle(factory_title, true);
    factory_box->Add(factory_title, 0, wxALL, Style::SPACING_MEDIUM);
    wxBoxSizer* factory_row = new wxBoxSizer(wxHORIZONTAL);
    import_factory_btn_ = new wxButton(factory_panel, SP_ID_IMPORT_FACTORY_BTN, "Import");
    Style::ApplyNeumorphicStyle(import_factory_btn_);
    factory_path_ctrl_ = new wxTextCtrl(factory_panel, wxID_ANY, "", wxDefaultPosition, wxSize(200, -1));
    Style::ApplyNeumorphicStyle(factory_path_ctrl_);
    factory_row->Add(import_factory_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    factory_row->Add(factory_path_ctrl_, 1);
    factory_box->Add(factory_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    factory_panel->SetSizer(factory_box);
    page0_top_row->Add(factory_panel, 1, wxEXPAND);

    page0_sizer->Add(page0_top_row, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    NeumorphicPanel* user_panel = new NeumorphicPanel(page0);
    wxBoxSizer* user_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* user_title = new wxStaticText(user_panel, wxID_ANY, "User Calibration");
    Style::ApplyNeumorphicStyle(user_title, true);
    user_box->Add(user_title, 0, wxALL, Style::SPACING_MEDIUM);
    wxStaticText* light_text = new wxStaticText(user_panel, wxID_ANY, "Light Sources:");
    Style::ApplyNeumorphicStyle(light_text);
    user_box->Add(light_text, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    light_list_ = new wxListBox(user_panel, wxID_ANY, wxDefaultPosition, wxSize(-1, 100));
    light_list_->SetFont(Style::GetSansFont(9));
    user_box->Add(light_list_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxGridSizer* light_grid = new wxGridSizer(2, 2, Style::SPACING_SMALL);
    wxStaticText* name_label = new wxStaticText(user_panel, wxID_ANY, "New Light Name:");
    Style::ApplyNeumorphicStyle(name_label);
    light_grid->Add(name_label, 0, wxALIGN_CENTER_VERTICAL);
    new_light_name_ctrl_ = new wxTextCtrl(user_panel, wxID_ANY, "A");
    Style::ApplyNeumorphicStyle(new_light_name_ctrl_);
    light_grid->Add(new_light_name_ctrl_, 1);
    wxStaticText* temp_label = new wxStaticText(user_panel, wxID_ANY, "Color Temperature:");
    Style::ApplyNeumorphicStyle(temp_label);
    light_grid->Add(temp_label, 0, wxALIGN_CENTER_VERTICAL);
    new_light_temp_slider_ = new wxSlider(user_panel, wxID_ANY, 29, 20, 100);
    Style::ApplyNeumorphicStyle(new_light_temp_slider_);
    light_grid->Add(new_light_temp_slider_, 1);
    user_box->Add(light_grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    add_light_btn_ = new wxButton(user_panel, SP_ID_ADD_LIGHT_BTN, "Add Light Source");
    Style::ApplyNeumorphicStyle(add_light_btn_);
    user_box->Add(add_light_btn_, 0, wxALIGN_CENTER | wxALL, Style::SPACING_MEDIUM);
    user_panel->SetSizer(user_box);
    page0_sizer->Add(user_panel, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    NeumorphicPanel* region_panel = new NeumorphicPanel(page0);
    wxBoxSizer* region_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* region_title = new wxStaticText(region_panel, wxID_ANY, "Calibration Region");
    Style::ApplyNeumorphicStyle(region_title, true);
    region_box->Add(region_title, 0, wxALL, Style::SPACING_MEDIUM);
    wxBoxSizer* region_box_inner = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* focal_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* focal_label = new wxStaticText(region_panel, wxID_ANY, "Focal Length (mm):");
    Style::ApplyNeumorphicStyle(focal_label);
    focal_row->Add(focal_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    focal_slider_ = new wxSlider(region_panel, wxID_ANY, 25, 1, 100, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(focal_slider_);
    focal_row->Add(focal_slider_, 0, wxALIGN_LEFT);
    region_box_inner->Add(focal_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* dist_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* dist_label = new wxStaticText(region_panel, wxID_ANY, "Working Distance (mm):");
    Style::ApplyNeumorphicStyle(dist_label);
    dist_row->Add(dist_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    distance_slider_ = new wxSlider(region_panel, wxID_ANY, 50, 10, 200, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(distance_slider_);
    dist_row->Add(distance_slider_, 0, wxALIGN_LEFT);
    region_box_inner->Add(dist_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* sw_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* sw_label = new wxStaticText(region_panel, wxID_ANY, "Sensor Width (mm):");
    Style::ApplyNeumorphicStyle(sw_label);
    sw_row->Add(sw_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    sensor_w_slider_ = new wxSlider(region_panel, wxID_ANY, 57, 10, 500, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(sensor_w_slider_);
    sw_row->Add(sensor_w_slider_, 0, wxALIGN_LEFT);
    region_box_inner->Add(sw_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* sh_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* sh_label = new wxStaticText(region_panel, wxID_ANY, "Sensor Height (mm):");
    Style::ApplyNeumorphicStyle(sh_label);
    sh_row->Add(sh_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    sensor_h_slider_ = new wxSlider(region_panel, wxID_ANY, 43, 10, 500, wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(sensor_h_slider_);
    sh_row->Add(sensor_h_slider_, 0, wxALIGN_LEFT);
    region_box_inner->Add(sh_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    region_box->Add(region_box_inner, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    compute_region_btn_ = new wxButton(region_panel, SP_ID_COMPUTE_REGION_BTN, "Compute Calib Region");
    Style::ApplyNeumorphicStyle(compute_region_btn_);
    region_box->Add(compute_region_btn_, 0, wxALIGN_CENTER | wxALL, Style::SPACING_MEDIUM);
    region_panel->SetSizer(region_box);
    page0_sizer->Add(region_panel, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);
    page0->SetSizer(page0_sizer);
    notebook->AddPage(page0, "Step 1");

    NeumorphicPanel* page1 = new NeumorphicPanel(notebook);
    wxBoxSizer* page1_sizer = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* capture_panel = new NeumorphicPanel(page1);
    wxBoxSizer* capture_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* capture_title = new wxStaticText(capture_panel, wxID_ANY, "Capture Spectral Data");
    Style::ApplyNeumorphicStyle(capture_title, true);
    capture_box->Add(capture_title, 0, wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* data_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* data_label = new wxStaticText(capture_panel, wxID_ANY, "Data Source:");
    Style::ApplyNeumorphicStyle(data_label);
    data_row->Add(data_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    data_source_combo_ = new wxComboBox(capture_panel, wxID_ANY, "Camera", wxDefaultPosition, wxSize(140, -1),
                                        {"Camera", "Local Curve"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(data_source_combo_);
    data_row->Add(data_source_combo_, 1);
    capture_box->Add(data_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_camera_btn_ = new wxButton(capture_panel, SP_ID_OPEN_CAMERA_BTN, "Open Camera");
    Style::ApplyNeumorphicStyle(open_camera_btn_);
    capture_image_btn_ = new wxButton(capture_panel, SP_ID_CAPTURE_IMAGE_BTN, "Capture Image");
    Style::ApplyNeumorphicStyle(capture_image_btn_);
    close_camera_btn_ = new wxButton(capture_panel, SP_ID_CLOSE_CAMERA_BTN, "Close Camera");
    Style::ApplyNeumorphicStyle(close_camera_btn_);
    capture_image_btn_->Disable();
    close_camera_btn_->Disable();
    camera_row->Add(open_camera_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    camera_row->Add(capture_image_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    camera_row->Add(close_camera_btn_, 0);
    capture_box->Add(camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* curve_row = new wxBoxSizer(wxHORIZONTAL);
    load_curve_btn_ = new wxButton(capture_panel, SP_ID_LOAD_CURVE_BTN, "Load Curve");
    Style::ApplyNeumorphicStyle(load_curve_btn_);
    curve_path_ctrl_ = new wxTextCtrl(capture_panel, wxID_ANY, "", wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(curve_path_ctrl_);
    curve_row->Add(load_curve_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    curve_row->Add(curve_path_ctrl_, 1);
    capture_box->Add(curve_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* img_title = new wxStaticText(capture_panel, wxID_ANY, "Captured Image");
    Style::ApplyNeumorphicStyle(img_title);
    img_box->Add(img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    image_canvas_ = new ImageCanvas(capture_panel, wxID_ANY);
    image_canvas_->SetMinSize(wxSize(600, 400));
    img_box->Add(image_canvas_, 1, wxEXPAND);
    capture_box->Add(img_box, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    capture_panel->SetSizer(capture_box);
    page1_sizer->Add(capture_panel, 1, wxEXPAND | wxALL, Style::SPACING_MEDIUM);
    page1->SetSizer(page1_sizer);
    notebook->AddPage(page1, "Step 2");

    NeumorphicPanel* page2 = new NeumorphicPanel(notebook);
    wxBoxSizer* page2_sizer = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* preview_panel = new NeumorphicPanel(page2);
    wxBoxSizer* preview_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* preview_title = new wxStaticText(preview_panel, wxID_ANY, "Preview Result");
    Style::ApplyNeumorphicStyle(preview_title, true);
    preview_box->Add(preview_title, 0, wxALL, Style::SPACING_MEDIUM);
    preview_btn_ = new wxButton(preview_panel, SP_ID_PREVIEW_BTN, "Preview");
    Style::ApplyNeumorphicStyle(preview_btn_);
    preview_box->Add(preview_btn_, 0, wxALIGN_CENTER | wxALL, Style::SPACING_MEDIUM);
    preview_panel->SetSizer(preview_box);
    page2_sizer->Add(preview_panel, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    NeumorphicPanel* preview_img_panel = new NeumorphicPanel(page2);
    wxBoxSizer* preview_img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* preview_img_title = new wxStaticText(preview_img_panel, wxID_ANY, "Preview Image");
    Style::ApplyNeumorphicStyle(preview_img_title);
    preview_img_box->Add(preview_img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    ImageCanvas* preview_canvas = new ImageCanvas(preview_img_panel, wxID_ANY);
    preview_canvas->SetMinSize(wxSize(600, 400));
    preview_img_box->Add(preview_canvas, 1, wxEXPAND);
    preview_img_panel->SetSizer(preview_img_box);
    page2_sizer->Add(preview_img_panel, 1, wxEXPAND | wxALL, Style::SPACING_MEDIUM);
    page2->SetSizer(page2_sizer);
    notebook->AddPage(page2, "Step 3");

    NeumorphicPanel* page3 = new NeumorphicPanel(notebook);
    wxBoxSizer* page3_sizer = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* export_panel = new NeumorphicPanel(page3);
    wxBoxSizer* export_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* export_title = new wxStaticText(export_panel, wxID_ANY, "Export Calibration");
    Style::ApplyNeumorphicStyle(export_title, true);
    export_box->Add(export_title, 0, wxALL, Style::SPACING_MEDIUM);
    wxStaticText* export_path = new wxStaticText(export_panel, wxID_ANY, "Export to: spectral_calib.dat");
    Style::ApplyNeumorphicStyle(export_path);
    export_box->Add(export_path, 0, wxALL, Style::SPACING_MEDIUM);
    export_btn_ = new wxButton(export_panel, SP_ID_EXPORT_BTN, "Export");
    Style::ApplyNeumorphicStyle(export_btn_);
    export_box->Add(export_btn_, 0, wxALIGN_CENTER | wxALL, Style::SPACING_MEDIUM);
    export_panel->SetSizer(export_box);
    page3_sizer->Add(export_panel, 1, wxEXPAND | wxALL, Style::SPACING_MEDIUM);
    page3->SetSizer(page3_sizer);
    notebook->AddPage(page3, "Step 4");

    main_sizer->Add(notebook, 1, wxEXPAND | wxALL, Style::SPACING_LARGE);

    NeumorphicPanel* nav_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* nav_sizer = new wxBoxSizer(wxHORIZONTAL);
    back_btn_ = new wxButton(nav_panel, SP_ID_BACK_BTN, "<< Back");
    Style::ApplyNeumorphicStyle(back_btn_);
    next_btn_ = new wxButton(nav_panel, SP_ID_NEXT_BTN, "Next >>");
    Style::ApplyNeumorphicStyle(next_btn_);
    reset_btn_ = new wxButton(nav_panel, SP_ID_RESET_BTN, "Reset");
    Style::ApplyNeumorphicStyle(reset_btn_);
    nav_sizer->Add(back_btn_, 0, wxRIGHT, Style::SPACING_MEDIUM);
    nav_sizer->Add(next_btn_, 0, wxRIGHT, Style::SPACING_MEDIUM);
    nav_sizer->Add(reset_btn_, 0);
    nav_panel->SetSizer(nav_sizer);
    main_sizer->Add(nav_panel, 0, wxALIGN_CENTER | wxALL, Style::SPACING_LARGE);

    status_text_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(status_text_);
    main_sizer->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

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
            step_btns_[i]->SetBackgroundColour(wxColour(82, 196, 26));
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
        status_text_->SetForegroundColour(wxColour(82, 196, 26));
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
    UpdateStatus("Calibration mode changed");
}

void SpectralCalibPanel::OnComplexityChanged(wxCommandEvent& event) {
    UpdateStatus("Complexity changed");
}

void SpectralCalibPanel::OnImportFactory(wxCommandEvent& event) {
    wxFileDialog dialog(this, "Import Factory Calibration", "", "", "*.dat", wxFD_OPEN);
    if (dialog.ShowModal() == wxID_OK) {
        factory_path_ctrl_->SetValue(dialog.GetPath());
        UpdateStatus("Factory calibration imported");
    }
}

void SpectralCalibPanel::OnAddLight(wxCommandEvent& event) {
    mvtk::LightSourceInfo ls;
    ls.name = new_light_name_ctrl_->GetValue().ToStdString();
    ls.color_temperature = new_light_temp_slider_->GetValue() * 100.0;
    light_sources_.push_back(ls);
    UpdateLightList();
    UpdateStatus(wxString::Format("Added light source: %s", ls.name.c_str()));
}

void SpectralCalibPanel::OnComputeRegion(wxCommandEvent& event) {
    float focal = focal_slider_->GetValue();
    float distance = distance_slider_->GetValue();
    float sensor_w = sensor_w_slider_->GetValue() / 1000.0;
    float sensor_h = sensor_h_slider_->GetValue() / 1000.0;
    float field_w = sensor_w * distance / focal;
    float field_h = sensor_h * distance / focal;
    UpdateStatus(wxString::Format("Calib Region: %.2fmm x %.2fmm", field_w * 1000, field_h * 1000));
}

void SpectralCalibPanel::OnDataSourceChanged(wxCommandEvent& event) {
    UpdateStatus("Data source changed");
}

void SpectralCalibPanel::OnOpenCamera(wxCommandEvent& event) {
    camera_.open(0);
    if (camera_.isOpened()) {
        camera_opened_ = true;
        capture_image_btn_->Enable();
        close_camera_btn_->Enable();
        open_camera_btn_->Disable();
        UpdateStatus("Camera opened");
    } else {
        UpdateStatus("Failed to open camera", true);
    }
}

void SpectralCalibPanel::OnCaptureImage(wxCommandEvent& event) {
    if (!camera_opened_) return;
    cv::Mat frame;
    camera_ >> frame;
    if (!frame.empty()) {
        cv::cvtColor(frame, captured_image_, cv::COLOR_BGR2RGB);
        image_canvas_->SetImage(captured_image_);
        UpdateStatus("Image captured");
    }
}

void SpectralCalibPanel::OnCloseCamera(wxCommandEvent& event) {
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
        capture_image_btn_->Disable();
        close_camera_btn_->Disable();
        open_camera_btn_->Enable();
        UpdateStatus("Camera closed");
    }
}

void SpectralCalibPanel::OnLoadCurve(wxCommandEvent& event) {
    wxFileDialog dialog(this, "Load Spectral Curve", "", "", "*.csv", wxFD_OPEN);
    if (dialog.ShowModal() == wxID_OK) {
        curve_path_ctrl_->SetValue(dialog.GetPath());
        UpdateStatus("Spectral curve loaded");
    }
}

void SpectralCalibPanel::OnPreview(wxCommandEvent& event) {
    UpdateStatus("Preview generated");
}

void SpectralCalibPanel::OnExport(wxCommandEvent& event) {
    wxFileDialog dialog(this, "Export Calibration", "", "spectral_calib.dat", "*.dat", wxFD_SAVE);
    if (dialog.ShowModal() == wxID_OK) {
        std::ofstream ofs(dialog.GetPath().ToStdString());
        ofs << "[SpectralCalibration]\n";
        ofs << "Mode=" << calib_mode_combo_->GetValue().ToStdString() << "\n";
        ofs << "Complexity=" << complexity_combo_->GetValue().ToStdString() << "\n";
        ofs.close();
        UpdateStatus("Calibration exported");
    }
}

void SpectralCalibPanel::OnBack(wxCommandEvent& event) {
    if (step_ > 0) {
        step_--;
        UpdateStepButtons();
    }
}

void SpectralCalibPanel::OnNext(wxCommandEvent& event) {
    if (step_ < 3) {
        step_++;
        UpdateStepButtons();
    }
}

void SpectralCalibPanel::OnReset(wxCommandEvent& event) {
    step_ = 0;
    UpdateStepButtons();
    UpdateStatus("Reset");
}

}
}