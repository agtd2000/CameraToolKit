#include "wxui/quick_color_calib_panel.h"
#include "neumorphic_panel.h"
#include "style_defs.h"
#include "utils/image_io.h"
#include "algo/roi.h"
#include <wx/grid.h>
#include <opencv2/imgproc.hpp>

namespace mvtk {
namespace wxui {

BEGIN_EVENT_TABLE(QuickColorCalibPanel, wxPanel)
    EVT_BUTTON(ID_STEP_BTN_0, QuickColorCalibPanel::OnStepButton)
    EVT_BUTTON(ID_STEP_BTN_1, QuickColorCalibPanel::OnStepButton)
    EVT_BUTTON(ID_STEP_BTN_2, QuickColorCalibPanel::OnStepButton)
    EVT_BUTTON(ID_STEP_BTN_3, QuickColorCalibPanel::OnStepButton)
    EVT_COMBOBOX(ID_CALIB_MODE_COMBO, QuickColorCalibPanel::OnCalibModeChanged)
    EVT_COMBOBOX(ID_SRC_SOURCE_COMBO, QuickColorCalibPanel::OnSrcSourceChanged)
    EVT_BUTTON(ID_SRC_LOAD_BTN, QuickColorCalibPanel::OnLoadSrc)
    EVT_BUTTON(ID_SRC_OPEN_CAMERA_BTN, QuickColorCalibPanel::OnOpenSrcCamera)
    EVT_BUTTON(ID_SRC_CAPTURE_BTN, QuickColorCalibPanel::OnCaptureSrc)
    EVT_BUTTON(ID_SRC_CLOSE_CAMERA_BTN, QuickColorCalibPanel::OnCloseSrcCamera)
    EVT_BUTTON(ID_AUTO_EXPOSURE_BTN, QuickColorCalibPanel::OnAutoExposure)
    EVT_COMBOBOX(ID_ROI_MODE_COMBO, QuickColorCalibPanel::OnRoiModeChanged)
    EVT_BUTTON(ID_GENERATE_ROI_BTN, QuickColorCalibPanel::OnGenerateROI)
    EVT_BUTTON(ID_DETECT_ROI_BTN, QuickColorCalibPanel::OnDetectROI)
    EVT_BUTTON(ID_DETECT_REF_ROI_BTN, QuickColorCalibPanel::OnDetectRefROI)
    EVT_COMBOBOX(ID_REF_SOURCE_COMBO, QuickColorCalibPanel::OnRefSourceChanged)
    EVT_BUTTON(ID_REF_LOAD_BTN, QuickColorCalibPanel::OnLoadRef)
    EVT_BUTTON(ID_REF_OPEN_CAMERA_BTN, QuickColorCalibPanel::OnOpenRefCamera)
    EVT_BUTTON(ID_REF_CAPTURE_BTN, QuickColorCalibPanel::OnCaptureRef)
    EVT_BUTTON(ID_REF_CLOSE_CAMERA_BTN, QuickColorCalibPanel::OnCloseRefCamera)
    EVT_BUTTON(ID_CALIBRATE_BTN, QuickColorCalibPanel::OnCalibrate)
    EVT_BUTTON(ID_BACK_BTN, QuickColorCalibPanel::OnBack)
    EVT_BUTTON(ID_NEXT_BTN, QuickColorCalibPanel::OnNext)
    EVT_BUTTON(ID_RESET_BTN, QuickColorCalibPanel::OnReset)
END_EVENT_TABLE()

QuickColorCalibPanel::QuickColorCalibPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    SetBackgroundColour(Style::NEU_BG_COLOR);
    SetFont(Style::GetSansFont(10));

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* step_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* step_sizer = new wxBoxSizer(wxHORIZONTAL);
    const char* steps[] = {"1. Acquire", "2. Draw ROI", "3. Reference", "4. Result"};
    const int step_ids[] = {ID_STEP_BTN_0, ID_STEP_BTN_1, ID_STEP_BTN_2, ID_STEP_BTN_3};
    for (int i = 0; i < 4; ++i) {
        step_btns_[i] = new wxButton(step_panel, step_ids[i], steps[i], wxDefaultPosition, wxSize(120, 32));
        Style::ApplyNeumorphicStyle(step_btns_[i]);
        step_sizer->Add(step_btns_[i], 0, wxRIGHT, Style::SPACING_SMALL);
    }
    step_panel->SetSizer(step_sizer);
    main_sizer->Add(step_panel, 0, wxALIGN_CENTER | wxALL, Style::SPACING_LARGE);

    NeumorphicPanel* mode_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* mode_box = new wxBoxSizer(wxHORIZONTAL);
    
    wxStaticText* mode_label = new wxStaticText(mode_panel, wxID_ANY, "Calibration Mode:");
    Style::ApplyNeumorphicStyle(mode_label);
    mode_box->Add(mode_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    calib_mode_combo_ = new wxComboBox(mode_panel, ID_CALIB_MODE_COMBO, "Standard Mode", wxDefaultPosition, wxSize(160, -1),
                                       {"Standard Mode", "Reference Mode"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(calib_mode_combo_);
    mode_box->Add(calib_mode_combo_, 0);
    mode_panel->SetSizer(mode_box);
    main_sizer->Add(mode_panel, 0, wxEXPAND | wxALL, Style::SPACING_LARGE);

    notebook_ = new wxNotebook(this, wxID_ANY);
    notebook_->SetBackgroundColour(Style::NEU_BG_COLOR);

    NeumorphicPanel* page0 = new NeumorphicPanel(notebook_);
    wxBoxSizer* page0_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* page0_top_row = new wxBoxSizer(wxHORIZONTAL);

    NeumorphicPanel* src_panel = new NeumorphicPanel(page0);
    wxBoxSizer* src_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* src_title = new wxStaticText(src_panel, wxID_ANY, "Source Image");
    Style::ApplyNeumorphicStyle(src_title, true);
    src_box->Add(src_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* src_row1 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* src_label = new wxStaticText(src_panel, wxID_ANY, "Source:");
    Style::ApplyNeumorphicStyle(src_label);
    src_row1->Add(src_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    src_source_combo_ = new wxComboBox(src_panel, ID_SRC_SOURCE_COMBO, "Local Image", wxDefaultPosition, wxSize(120, -1),
                                       {"Local Image", "Camera"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(src_source_combo_);
    src_row1->Add(src_source_combo_, 0);
    src_box->Add(src_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* src_row2 = new wxBoxSizer(wxHORIZONTAL);
    load_src_btn_ = new wxButton(src_panel, ID_SRC_LOAD_BTN, "Load");
    Style::ApplyNeumorphicStyle(load_src_btn_);
    src_path_ctrl_ = new wxTextCtrl(src_panel, wxID_ANY, "", wxDefaultPosition, wxSize(200, -1));
    Style::ApplyNeumorphicStyle(src_path_ctrl_);
    src_row2->Add(load_src_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    src_row2->Add(src_path_ctrl_, 1);
    src_box->Add(src_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* src_camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_src_camera_btn_ = new wxButton(src_panel, ID_SRC_OPEN_CAMERA_BTN, "Open Camera");
    Style::ApplyNeumorphicStyle(open_src_camera_btn_);
    capture_src_btn_ = new wxButton(src_panel, ID_SRC_CAPTURE_BTN, "Capture");
    Style::ApplyNeumorphicStyle(capture_src_btn_);
    close_src_camera_btn_ = new wxButton(src_panel, ID_SRC_CLOSE_CAMERA_BTN, "Close");
    Style::ApplyNeumorphicStyle(close_src_camera_btn_);
    capture_src_btn_->Disable();
    close_src_camera_btn_->Disable();
    src_camera_row->Add(open_src_camera_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    src_camera_row->Add(capture_src_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    src_camera_row->Add(close_src_camera_btn_, 0);
    src_box->Add(src_camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    src_panel->SetSizer(src_box);
    page0_top_row->Add(src_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    NeumorphicPanel* exposure_panel = new NeumorphicPanel(page0);
    wxBoxSizer* exposure_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* exp_title = new wxStaticText(exposure_panel, wxID_ANY, "Exposure & Gain");
    Style::ApplyNeumorphicStyle(exp_title, true);
    exposure_box->Add(exp_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxGridSizer* exp_grid = new wxGridSizer(2, 1, Style::SPACING_SMALL);
    
    wxBoxSizer* exp_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* exp_label = new wxStaticText(exposure_panel, wxID_ANY, "Exposure:");
    Style::ApplyNeumorphicStyle(exp_label);
    exp_row->Add(exp_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    exposure_slider_ = new wxSlider(exposure_panel, wxID_ANY, 100, 1, 1000);
    Style::ApplyNeumorphicStyle(exposure_slider_);
    exp_row->Add(exposure_slider_, 1);
    exp_grid->Add(exp_row, 1, wxEXPAND);
    
    wxBoxSizer* gain_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* gain_label = new wxStaticText(exposure_panel, wxID_ANY, "Gain:");
    Style::ApplyNeumorphicStyle(gain_label);
    gain_row->Add(gain_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    gain_slider_ = new wxSlider(exposure_panel, wxID_ANY, 10, 1, 100);
    Style::ApplyNeumorphicStyle(gain_slider_);
    gain_row->Add(gain_slider_, 1);
    exp_grid->Add(gain_row, 1, wxEXPAND);
    
    exposure_box->Add(exp_grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    auto_exposure_btn_ = new wxButton(exposure_panel, ID_AUTO_EXPOSURE_BTN, "Auto");
    Style::ApplyNeumorphicStyle(auto_exposure_btn_);
    exposure_box->Add(auto_exposure_btn_, 0, wxALIGN_CENTER | wxALL, Style::SPACING_MEDIUM);
    exposure_panel->SetSizer(exposure_box);
    page0_top_row->Add(exposure_panel, 1, wxEXPAND);

    page0_sizer->Add(page0_top_row, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    NeumorphicPanel* src_img_panel = new NeumorphicPanel(page0);
    wxBoxSizer* src_img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* src_img_title = new wxStaticText(src_img_panel, wxID_ANY, "Source Image");
    Style::ApplyNeumorphicStyle(src_img_title);
    src_img_box->Add(src_img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    src_canvas_ = new ImageCanvas(src_img_panel, wxID_ANY);
    src_canvas_->SetMinSize(wxSize(550, 350));
    src_img_box->Add(src_canvas_, 1, wxEXPAND);
    src_img_panel->SetSizer(src_img_box);
    page0_sizer->Add(src_img_panel, 1, wxEXPAND | wxALL, Style::SPACING_MEDIUM);
    
    page0->SetSizer(page0_sizer);
    notebook_->AddPage(page0, "Step 1");

    NeumorphicPanel* page1 = new NeumorphicPanel(notebook_);
    wxBoxSizer* page1_sizer = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* roi_panel = new NeumorphicPanel(page1);
    wxBoxSizer* roi_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* roi_title = new wxStaticText(roi_panel, wxID_ANY, "ROI Settings");
    Style::ApplyNeumorphicStyle(roi_title, true);
    roi_box->Add(roi_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* roi_mode_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* roi_mode_label = new wxStaticText(roi_panel, wxID_ANY, "Mode:");
    Style::ApplyNeumorphicStyle(roi_mode_label);
    roi_mode_row->Add(roi_mode_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    roi_mode_combo_ = new wxComboBox(roi_panel, ID_ROI_MODE_COMBO, "Auto Detect", wxDefaultPosition, wxSize(140, -1),
                                       {"Manual", "Auto Detect"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(roi_mode_combo_);
    roi_mode_row->Add(roi_mode_combo_, 0);
    roi_box->Add(roi_mode_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxGridSizer* roi_grid = new wxGridSizer(2, 4, Style::SPACING_SMALL);
    
    wxStaticText* chart_x_label = new wxStaticText(roi_panel, wxID_ANY, "Chart X:");
    Style::ApplyNeumorphicStyle(chart_x_label);
    roi_grid->Add(chart_x_label, 0, wxALIGN_CENTER_VERTICAL);
    chart_x_ctrl_ = new wxTextCtrl(roi_panel, wxID_ANY, "100", wxDefaultPosition, wxSize(80, -1));
    Style::ApplyNeumorphicStyle(chart_x_ctrl_);
    chart_x_ctrl_->Disable();
    roi_grid->Add(chart_x_ctrl_, 1);
    
    wxStaticText* chart_y_label = new wxStaticText(roi_panel, wxID_ANY, "Chart Y:");
    Style::ApplyNeumorphicStyle(chart_y_label);
    roi_grid->Add(chart_y_label, 0, wxALIGN_CENTER_VERTICAL);
    chart_y_ctrl_ = new wxTextCtrl(roi_panel, wxID_ANY, "50", wxDefaultPosition, wxSize(80, -1));
    Style::ApplyNeumorphicStyle(chart_y_ctrl_);
    chart_y_ctrl_->Disable();
    roi_grid->Add(chart_y_ctrl_, 1);
    
    wxStaticText* chart_w_label = new wxStaticText(roi_panel, wxID_ANY, "Width:");
    Style::ApplyNeumorphicStyle(chart_w_label);
    roi_grid->Add(chart_w_label, 0, wxALIGN_CENTER_VERTICAL);
    chart_w_ctrl_ = new wxTextCtrl(roi_panel, wxID_ANY, "400", wxDefaultPosition, wxSize(80, -1));
    Style::ApplyNeumorphicStyle(chart_w_ctrl_);
    chart_w_ctrl_->Disable();
    roi_grid->Add(chart_w_ctrl_, 1);
    
    wxStaticText* chart_h_label = new wxStaticText(roi_panel, wxID_ANY, "Height:");
    Style::ApplyNeumorphicStyle(chart_h_label);
    roi_grid->Add(chart_h_label, 0, wxALIGN_CENTER_VERTICAL);
    chart_h_ctrl_ = new wxTextCtrl(roi_panel, wxID_ANY, "600", wxDefaultPosition, wxSize(80, -1));
    Style::ApplyNeumorphicStyle(chart_h_ctrl_);
    chart_h_ctrl_->Disable();
    roi_grid->Add(chart_h_ctrl_, 1);
    
    roi_box->Add(roi_grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxGridSizer* brightness_grid = new wxGridSizer(2, 2, Style::SPACING_SMALL);
    
    wxStaticText* bright_min_label = new wxStaticText(roi_panel, wxID_ANY, "Brightness Min:");
    Style::ApplyNeumorphicStyle(bright_min_label);
    brightness_grid->Add(bright_min_label, 0, wxALIGN_CENTER_VERTICAL);
    brightness_min_ctrl_ = new wxTextCtrl(roi_panel, wxID_ANY, "0.55", wxDefaultPosition, wxSize(80, -1));
    Style::ApplyNeumorphicStyle(brightness_min_ctrl_);
    brightness_grid->Add(brightness_min_ctrl_, 1);
    
    wxStaticText* bright_max_label = new wxStaticText(roi_panel, wxID_ANY, "Brightness Max:");
    Style::ApplyNeumorphicStyle(bright_max_label);
    brightness_grid->Add(bright_max_label, 0, wxALIGN_CENTER_VERTICAL);
    brightness_max_ctrl_ = new wxTextCtrl(roi_panel, wxID_ANY, "0.65", wxDefaultPosition, wxSize(80, -1));
    Style::ApplyNeumorphicStyle(brightness_max_ctrl_);
    brightness_grid->Add(brightness_max_ctrl_, 1);
    
    roi_box->Add(brightness_grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* roi_btn_row = new wxBoxSizer(wxHORIZONTAL);
    generate_roi_btn_ = new wxButton(roi_panel, ID_GENERATE_ROI_BTN, "Generate ROI (Manual)");
    Style::ApplyNeumorphicStyle(generate_roi_btn_);
    detect_roi_btn_ = new wxButton(roi_panel, ID_DETECT_ROI_BTN, "Detect Color Checker");
    Style::ApplyNeumorphicStyle(detect_roi_btn_);
    roi_btn_row->Add(generate_roi_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    roi_btn_row->Add(detect_roi_btn_, 0);
    roi_box->Add(roi_btn_row, 0, wxALIGN_CENTER | wxALL, Style::SPACING_MEDIUM);

    roi_count_text_ = new wxStaticText(roi_panel, wxID_ANY, "ROI Count: 0");
    Style::ApplyNeumorphicStyle(roi_count_text_);
    roi_box->Add(roi_count_text_, 0, wxALL, Style::SPACING_MEDIUM);

    brightness_text_ = new wxStaticText(roi_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(brightness_text_);
    roi_box->Add(brightness_text_, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);
    roi_panel->SetSizer(roi_box);
    page1_sizer->Add(roi_panel, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    NeumorphicPanel* roi_img_panel = new NeumorphicPanel(page1);
    wxBoxSizer* roi_img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* roi_img_title = new wxStaticText(roi_img_panel, wxID_ANY, "Source Image");
    Style::ApplyNeumorphicStyle(roi_img_title);
    roi_img_box->Add(roi_img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    roi_canvas_ = new ImageCanvas(roi_img_panel, wxID_ANY);
    roi_canvas_->SetMinSize(wxSize(550, 350));
    roi_img_box->Add(roi_canvas_, 1, wxEXPAND);
    roi_img_panel->SetSizer(roi_img_box);
    page1_sizer->Add(roi_img_panel, 1, wxEXPAND | wxALL, Style::SPACING_MEDIUM);
    
    page1->SetSizer(page1_sizer);
    notebook_->AddPage(page1, "Step 2");

    NeumorphicPanel* page2 = new NeumorphicPanel(notebook_);
    wxBoxSizer* page2_sizer = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* ref_panel = new NeumorphicPanel(page2);
    wxBoxSizer* ref_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* ref_title = new wxStaticText(ref_panel, wxID_ANY, "Reference Settings");
    Style::ApplyNeumorphicStyle(ref_title, true);
    ref_box->Add(ref_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxStaticText* ref_mode_text = new wxStaticText(ref_panel, wxID_ANY, "Standard Mode: Using X-Rite Classic 24 reference colors");
    Style::ApplyNeumorphicStyle(ref_mode_text);
    ref_box->Add(ref_mode_text, 0, wxALL, Style::SPACING_MEDIUM);

    wxBoxSizer* ref_row1 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* ref_label = new wxStaticText(ref_panel, wxID_ANY, "Source:");
    Style::ApplyNeumorphicStyle(ref_label);
    ref_row1->Add(ref_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    ref_source_combo_ = new wxComboBox(ref_panel, ID_REF_SOURCE_COMBO, "Local Image", wxDefaultPosition, wxSize(160, -1),
                                       {"Local Image", "Camera"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(ref_source_combo_);
    ref_row1->Add(ref_source_combo_, 0);
    ref_box->Add(ref_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* ref_row2 = new wxBoxSizer(wxHORIZONTAL);
    load_ref_btn_ = new wxButton(ref_panel, ID_REF_LOAD_BTN, "Load Reference");
    Style::ApplyNeumorphicStyle(load_ref_btn_);
    ref_path_ctrl_ = new wxTextCtrl(ref_panel, wxID_ANY, "", wxDefaultPosition, wxSize(300, -1));
    Style::ApplyNeumorphicStyle(ref_path_ctrl_);
    ref_row2->Add(load_ref_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    ref_row2->Add(ref_path_ctrl_, 1);
    ref_box->Add(ref_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* ref_camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_ref_camera_btn_ = new wxButton(ref_panel, ID_REF_OPEN_CAMERA_BTN, "Open Camera");
    Style::ApplyNeumorphicStyle(open_ref_camera_btn_);
    capture_ref_btn_ = new wxButton(ref_panel, ID_REF_CAPTURE_BTN, "Capture Reference");
    Style::ApplyNeumorphicStyle(capture_ref_btn_);
    close_ref_camera_btn_ = new wxButton(ref_panel, ID_REF_CLOSE_CAMERA_BTN, "Close Camera");
    Style::ApplyNeumorphicStyle(close_ref_camera_btn_);
    capture_ref_btn_->Disable();
    close_ref_camera_btn_->Disable();
    ref_camera_row->Add(open_ref_camera_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    ref_camera_row->Add(capture_ref_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    ref_camera_row->Add(close_ref_camera_btn_, 0);
    ref_box->Add(ref_camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* ref_roi_row = new wxBoxSizer(wxHORIZONTAL);
    detect_ref_roi_btn_ = new wxButton(ref_panel, ID_DETECT_REF_ROI_BTN, "Detect Color Checker");
    Style::ApplyNeumorphicStyle(detect_ref_roi_btn_);
    ref_roi_count_text_ = new wxStaticText(ref_panel, wxID_ANY, "ROI Count: 0");
    Style::ApplyNeumorphicStyle(ref_roi_count_text_);
    ref_roi_row->Add(detect_ref_roi_btn_, 0, wxRIGHT, Style::SPACING_MEDIUM);
    ref_roi_row->Add(ref_roi_count_text_, 0, wxALIGN_CENTER_VERTICAL);
    ref_box->Add(ref_roi_row, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);
    ref_panel->SetSizer(ref_box);
    page2_sizer->Add(ref_panel, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    NeumorphicPanel* ref_img_panel = new NeumorphicPanel(page2);
    wxBoxSizer* ref_img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* ref_img_title = new wxStaticText(ref_img_panel, wxID_ANY, "Reference Image");
    Style::ApplyNeumorphicStyle(ref_img_title);
    ref_img_box->Add(ref_img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    ref_canvas_ = new ImageCanvas(ref_img_panel, wxID_ANY);
    ref_canvas_->SetMinSize(wxSize(550, 350));
    ref_img_box->Add(ref_canvas_, 1, wxEXPAND);
    ref_img_panel->SetSizer(ref_img_box);
    page2_sizer->Add(ref_img_panel, 1, wxEXPAND | wxALL, Style::SPACING_MEDIUM);

    NeumorphicPanel* adv_panel = new NeumorphicPanel(page2);
    wxBoxSizer* adv_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* adv_title = new wxStaticText(adv_panel, wxID_ANY, "Advanced Settings");
    Style::ApplyNeumorphicStyle(adv_title, true);
    adv_box->Add(adv_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* stat_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* stat_label = new wxStaticText(adv_panel, wxID_ANY, "RGB Stat:");
    Style::ApplyNeumorphicStyle(stat_label);
    stat_row->Add(stat_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    stat_method_combo_ = new wxComboBox(adv_panel, wxID_ANY, "Median", wxDefaultPosition, wxSize(120, -1),
                                        {"Mean", "Median"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(stat_method_combo_);
    stat_row->Add(stat_method_combo_, 0);
    adv_box->Add(stat_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* wb_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* wb_label = new wxStaticText(adv_panel, wxID_ANY, "WB Method:");
    Style::ApplyNeumorphicStyle(wb_label);
    wb_row->Add(wb_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    wb_method_combo_ = new wxComboBox(adv_panel, wxID_ANY, "Average", wxDefaultPosition, wxSize(120, -1),
                                      {"Average", "Analytic"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(wb_method_combo_);
    wb_row->Add(wb_method_combo_, 0);
    adv_box->Add(wb_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* ccm_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* ccm_label = new wxStaticText(adv_panel, wxID_ANY, "CCM Method:");
    Style::ApplyNeumorphicStyle(ccm_label);
    ccm_row->Add(ccm_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    ccm_method_combo_ = new wxComboBox(adv_panel, wxID_ANY, "Optimal", wxDefaultPosition, wxSize(140, -1),
                                       {"Optimal", "Least Squares"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(ccm_method_combo_);
    ccm_row->Add(ccm_method_combo_, 0);
    adv_box->Add(ccm_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    calibrate_btn_ = new wxButton(adv_panel, ID_CALIBRATE_BTN, "Calibrate");
    Style::ApplyNeumorphicStyle(calibrate_btn_);
    adv_box->Add(calibrate_btn_, 0, wxALIGN_CENTER | wxALL, Style::SPACING_MEDIUM);
    adv_panel->SetSizer(adv_box);
    page2_sizer->Add(adv_panel, 0, wxEXPAND | wxALL, Style::SPACING_MEDIUM);
    
    page2->SetSizer(page2_sizer);
    notebook_->AddPage(page2, "Step 3");

    NeumorphicPanel* page3 = new NeumorphicPanel(notebook_);
    wxBoxSizer* page3_sizer = new wxBoxSizer(wxHORIZONTAL);

    NeumorphicPanel* deltae_panel = new NeumorphicPanel(page3);
    wxBoxSizer* deltae_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* deltae_title = new wxStaticText(deltae_panel, wxID_ANY, "Color Error (DeltaE00)");
    Style::ApplyNeumorphicStyle(deltae_title, true);
    deltae_box->Add(deltae_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* deltae_top_box = new wxBoxSizer(wxHORIZONTAL);
    deltae_mean_text_ = new wxStaticText(deltae_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(deltae_mean_text_);
    deltae_top_box->Add(deltae_mean_text_, 0, wxALIGN_CENTER_VERTICAL);
    deltae_box->Add(deltae_top_box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    deltae_grid_ = new wxGrid(deltae_panel, wxID_ANY);
    deltae_grid_->CreateGrid(0, 2);
    deltae_grid_->SetColLabelValue(0, "Patch");
    deltae_grid_->SetColLabelValue(1, "DeltaE00");
    deltae_grid_->EnableEditing(false);
    deltae_grid_->SetFont(Style::GetSansFont(8));
    deltae_grid_->SetMinSize(wxSize(280, 350));
    deltae_box->Add(deltae_grid_, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);
    deltae_panel->SetSizer(deltae_box);
    page3_sizer->Add(deltae_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_MEDIUM);

    wxBoxSizer* right_box = new wxBoxSizer(wxVERTICAL);

    NeumorphicPanel* wb_panel = new NeumorphicPanel(page3);
    wxBoxSizer* wb_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* wb_title = new wxStaticText(wb_panel, wxID_ANY, "White Balance Parameters");
    Style::ApplyNeumorphicStyle(wb_title, true);
    wb_box->Add(wb_title, 0, wxALL, Style::SPACING_SMALL);
    
    wb_result_text_ = new wxStaticText(wb_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(wb_result_text_);
    wb_box->Add(wb_result_text_, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);
    wb_panel->SetSizer(wb_box);
    right_box->Add(wb_panel, 1, wxEXPAND | wxBOTTOM, Style::SPACING_MEDIUM);

    NeumorphicPanel* ccm_panel = new NeumorphicPanel(page3);
    wxBoxSizer* ccm_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* ccm_title = new wxStaticText(ccm_panel, wxID_ANY, "CCM Matrix");
    Style::ApplyNeumorphicStyle(ccm_title, true);
    ccm_box->Add(ccm_title, 0, wxALL, Style::SPACING_SMALL);
    
    ccm_result_text_ = new wxStaticText(ccm_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(ccm_result_text_);
    ccm_box->Add(ccm_result_text_, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);
    ccm_panel->SetSizer(ccm_box);
    right_box->Add(ccm_panel, 1, wxEXPAND);

    page3_sizer->Add(right_box, 1, wxEXPAND);
    page3->SetSizer(page3_sizer);
    notebook_->AddPage(page3, "Step 4");

    main_sizer->Add(notebook_, 1, wxEXPAND | wxALL, Style::SPACING_LARGE);

    NeumorphicPanel* nav_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* nav_sizer = new wxBoxSizer(wxHORIZONTAL);
    back_btn_ = new wxButton(nav_panel, ID_BACK_BTN, "<< Back");
    Style::ApplyNeumorphicStyle(back_btn_);
    next_btn_ = new wxButton(nav_panel, ID_NEXT_BTN, "Next >>");
    Style::ApplyNeumorphicStyle(next_btn_);
    reset_btn_ = new wxButton(nav_panel, ID_RESET_BTN, "Reset");
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

QuickColorCalibPanel::~QuickColorCalibPanel() {
    if (camera_opened_) {
        camera_.release();
    }
}

void QuickColorCalibPanel::UpdateStepButtons() {
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

void QuickColorCalibPanel::UpdateStatus(const wxString& msg, bool is_error) {
    status_text_->SetLabel(msg);
    if (is_error) {
        status_text_->SetForegroundColour(*wxRED);
    } else {
        status_text_->SetForegroundColour(*wxBLUE);
    }
}

void QuickColorCalibPanel::UpdateCanvasImages() {
    if (!src_image_.empty() && src_canvas_) {
        src_canvas_->SetImage(src_image_);
    }
    if (!src_image_.empty() && roi_canvas_) {
        roi_canvas_->SetImage(src_image_);
    }
    if (!ref_image_.empty() && ref_canvas_) {
        ref_canvas_->SetImage(ref_image_);
    }
}

void QuickColorCalibPanel::OnStepButton(wxCommandEvent& event) {
    for (int i = 0; i < 4; ++i) {
        if (event.GetEventObject() == step_btns_[i]) {
            step_ = i;
            UpdateStepButtons();
            notebook_->SetSelection(i);
            UpdateCanvasImages();
            break;
        }
    }
}

void QuickColorCalibPanel::OnCalibModeChanged(wxCommandEvent& event) {
    calib_mode_ = calib_mode_combo_->GetSelection();
}

void QuickColorCalibPanel::OnSrcSourceChanged(wxCommandEvent& event) {
    src_source_ = src_source_combo_->GetSelection();
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
        capture_src_btn_->Disable();
        close_src_camera_btn_->Disable();
        open_src_camera_btn_->Enable();
    }
}

void QuickColorCalibPanel::OnLoadSrc(wxCommandEvent& event) {
    wxString path = src_path_ctrl_->GetValue();
    if (path.IsEmpty()) {
        UpdateStatus("Please enter a file path", true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Empty file path");
        return;
    }
    MV_LOG_OPERATION("Load Source Image", "Path: " + path.ToStdString());
    src_image_ = mvtk::utils::imreadUtf8(path.ToStdString());
    if (!src_image_.empty()) {
        src_canvas_->SetImage(src_image_);
        UpdateStatus(wxString::Format("Image loaded: %dx%d", src_image_.cols, src_image_.rows));
        MV_LOG_DATA("ImageInfo", "Size: " + std::to_string(src_image_.cols) + "x" + 
                    std::to_string(src_image_.rows) + ", Channels: " + std::to_string(src_image_.channels()) +
                    ", Depth: " + std::to_string(src_image_.depth()));
        next_btn_->Enable();
    } else {
        UpdateStatus("Failed to load image: " + path, true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Failed to load image from path: " + path.ToStdString());
    }
}

void QuickColorCalibPanel::OnOpenSrcCamera(wxCommandEvent& event) {
    MV_LOG_OPERATION("Open Camera", "Camera Index: 0");
    camera_.open(0);
    if (camera_.isOpened()) {
        camera_opened_ = true;
        open_src_camera_btn_->Disable();
        capture_src_btn_->Enable();
        close_src_camera_btn_->Enable();
        UpdateStatus("Camera opened successfully");
        MV_LOG_DATA("CameraStatus", "Opened successfully");
    } else {
        UpdateStatus("Failed to open camera", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to open camera index 0");
    }
}

void QuickColorCalibPanel::OnCaptureSrc(wxCommandEvent& event) {
    MV_LOG_OPERATION("Capture Image", "Source: Camera");
    camera_ >> src_image_;
    if (!src_image_.empty()) {
        src_canvas_->SetImage(src_image_);
        UpdateStatus(wxString::Format("Image captured: %dx%d", src_image_.cols, src_image_.rows));
        MV_LOG_DATA("CapturedImage", "Size: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows));
        next_btn_->Enable();
    } else {
        UpdateStatus("Failed to capture image", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to capture image from camera");
    }
}

void QuickColorCalibPanel::OnCloseSrcCamera(wxCommandEvent& event) {
    camera_.release();
    camera_opened_ = false;
    open_src_camera_btn_->Enable();
    capture_src_btn_->Disable();
    close_src_camera_btn_->Disable();
    UpdateStatus("Camera closed");
}

void QuickColorCalibPanel::OnAutoExposure(wxCommandEvent& event) {
    if (src_image_.empty() || rois_.empty()) {
        UpdateStatus("Please load image and generate ROIs first", true);
        return;
    }
    cv::Rect check_roi = rois_.back().rect;
    double factor = QuickColorCalibrator::autoExposure(src_image_, check_roi);
    UpdateStatus(wxString::Format("Auto exposure factor: %.2f", factor));
}

void QuickColorCalibPanel::OnRoiModeChanged(wxCommandEvent& event) {
    int mode = roi_mode_combo_->GetSelection();
    bool is_manual = (mode == 0);
    chart_x_ctrl_->Enable(is_manual);
    chart_y_ctrl_->Enable(is_manual);
    chart_w_ctrl_->Enable(is_manual);
    chart_h_ctrl_->Enable(is_manual);
}

void QuickColorCalibPanel::OnGenerateROI(wxCommandEvent& event) {
    if (src_image_.empty()) {
        UpdateStatus("Please load or capture an image first", true);
        MV_LOG_ERROR_DETAIL("ROIGeneration", "No source image loaded");
        return;
    }
    chart_x_ctrl_->GetValue().ToInt(&chart_x_);
    chart_y_ctrl_->GetValue().ToInt(&chart_y_);
    chart_w_ctrl_->GetValue().ToInt(&chart_w_);
    chart_h_ctrl_->GetValue().ToInt(&chart_h_);
    brightness_min_ctrl_->GetValue().ToDouble(&brightness_min_);
    brightness_max_ctrl_->GetValue().ToDouble(&brightness_max_);
    MV_LOG_OPERATION("Generate ROI (Manual)", "Chart position: (" + std::to_string(chart_x_) + "," + 
                     std::to_string(chart_y_) + "), Grid: " + std::to_string(chart_w_) + "x" + std::to_string(chart_h_) +
                     ", Brightness Range: [" + std::to_string(brightness_min_) + ", " + std::to_string(brightness_max_) + "]");

    const int rows = 4;
    const int cols = 6;
    
    int roi_width = chart_w_ / cols / 2;
    int roi_height = chart_h_ / rows / 2;
    
    float cell_w = static_cast<float>(chart_w_) / cols;
    float cell_h = static_cast<float>(chart_h_) / rows;

    rois_.clear();
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            float center_x = chart_x_ + cell_w * (col + 0.5f);
            float center_y = chart_y_ + cell_h * (row + 0.5f);
            
            int x = static_cast<int>(center_x - roi_width / 2);
            int y = static_cast<int>(center_y - roi_height / 2);
            
            mvtk::ROIInfo roi_info;
            roi_info.rect = cv::Rect(x, y, roi_width, roi_height);
            roi_info.label = "";
            rois_.push_back(roi_info);
        }
    }

    roi_count_text_->SetLabel(wxString::Format("ROI Count: %zu", rois_.size()));
    MV_LOG_DATA("ROIResult", "Generated " + std::to_string(rois_.size()) + " ROIs (Manual)");

    if (!rois_.empty()) {
        cv::Rect check_roi;
        if (rois_.size() >= 23) check_roi = rois_[22].rect;
        current_brightness_ = mvtk::WhiteBalance::computeBrightness(src_image_, check_roi);
        bool in_range = (current_brightness_ >= brightness_min_ && current_brightness_ <= brightness_max_);
        brightness_text_->SetLabel(wxString::Format("Current Brightness: %.3f %s\nRecommended: %.2f ~ %.2f",
                                                    current_brightness_, in_range ? "(OK)" : "(Out of Range)",
                                                    brightness_min_, brightness_max_));
        brightness_text_->SetForegroundColour(in_range ? *wxGREEN : *wxRED);
        MV_LOG_DATA("BrightnessCheck", "Value: " + std::to_string(current_brightness_) + 
                    ", Range: [" + std::to_string(brightness_min_) + ", " + std::to_string(brightness_max_) + "]" +
                    ", Status: " + (in_range ? "OK" : "Out of Range"));
    }

    cv::Mat display_img = src_image_.clone();
    
    cv::Scalar colors[] = {
        cv::Scalar(255, 0, 0),    cv::Scalar(0, 255, 0),    cv::Scalar(0, 0, 255),    cv::Scalar(255, 255, 0),
        cv::Scalar(255, 0, 255),  cv::Scalar(0, 255, 255),  cv::Scalar(128, 0, 0),    cv::Scalar(0, 128, 0),
        cv::Scalar(0, 0, 128),    cv::Scalar(128, 128, 0),  cv::Scalar(128, 0, 128),  cv::Scalar(0, 128, 128),
        cv::Scalar(255, 128, 0),  cv::Scalar(255, 0, 128),  cv::Scalar(128, 255, 0),  cv::Scalar(0, 255, 128),
        cv::Scalar(128, 128, 255), cv::Scalar(255, 128, 128), cv::Scalar(128, 255, 128), cv::Scalar(255, 255, 255),
        cv::Scalar(192, 192, 192), cv::Scalar(64, 64, 64),   cv::Scalar(128, 64, 0),   cv::Scalar(64, 0, 128)
    };

    cv::Rect overall_roi(chart_x_, chart_y_, chart_w_, chart_h_);
    cv::rectangle(display_img, overall_roi, cv::Scalar(255, 255, 255), 2);

    for (size_t i = 0; i < rois_.size() && i < 24; ++i) {
        cv::rectangle(display_img, rois_[i].rect, colors[i], 2);
    }

    roi_canvas_->SetImage(display_img);

    UpdateStatus(wxString::Format("ROI generated: %zu regions (Manual)", rois_.size()));
    next_btn_->Enable();
}

void QuickColorCalibPanel::OnDetectROI(wxCommandEvent& event) {
    if (src_image_.empty()) {
        UpdateStatus("Please load or capture an image first", true);
        MV_LOG_ERROR_DETAIL("ROIDetection", "No source image loaded");
        return;
    }

    brightness_min_ctrl_->GetValue().ToDouble(&brightness_min_);
    brightness_max_ctrl_->GetValue().ToDouble(&brightness_max_);
    MV_LOG_OPERATION("Detect Color Checker", "Starting auto detection...");

    rois_ = mvtk::ROIManager::detectColorChecker(src_image_, mvtk::ROIType::COLOR_24);

    if (rois_.empty()) {
        UpdateStatus("Failed to detect color checker", true);
        MV_LOG_ERROR_DETAIL("ROIDetection", "MCC module failed to detect color checker");
        return;
    }

    roi_count_text_->SetLabel(wxString::Format("ROI Count: %zu", rois_.size()));
    MV_LOG_DATA("ROIResult", "Detected " + std::to_string(rois_.size()) + " ROIs (Auto)");

    if (!rois_.empty()) {
        cv::Rect check_roi;
        if (rois_.size() >= 23) check_roi = rois_[22].rect;
        current_brightness_ = mvtk::WhiteBalance::computeBrightness(src_image_, check_roi);
        bool in_range = (current_brightness_ >= brightness_min_ && current_brightness_ <= brightness_max_);
        brightness_text_->SetLabel(wxString::Format("Current Brightness: %.3f %s\nRecommended: %.2f ~ %.2f",
                                                    current_brightness_, in_range ? "(OK)" : "(Out of Range)",
                                                    brightness_min_, brightness_max_));
        brightness_text_->SetForegroundColour(in_range ? *wxGREEN : *wxRED);
        MV_LOG_DATA("BrightnessCheck", "Value: " + std::to_string(current_brightness_) + 
                    ", Range: [" + std::to_string(brightness_min_) + ", " + std::to_string(brightness_max_) + "]" +
                    ", Status: " + (in_range ? "OK" : "Out of Range"));
    }

    cv::Mat display_img = src_image_.clone();

    cv::Scalar colors[] = {
        cv::Scalar(255, 0, 0),    cv::Scalar(0, 255, 0),    cv::Scalar(0, 0, 255),    cv::Scalar(255, 255, 0),
        cv::Scalar(255, 0, 255),  cv::Scalar(0, 255, 255),  cv::Scalar(128, 0, 0),    cv::Scalar(0, 128, 0),
        cv::Scalar(0, 0, 128),    cv::Scalar(128, 128, 0),  cv::Scalar(128, 0, 128),  cv::Scalar(0, 128, 128),
        cv::Scalar(255, 128, 0),  cv::Scalar(255, 0, 128),  cv::Scalar(128, 255, 0),  cv::Scalar(0, 255, 128),
        cv::Scalar(128, 128, 255), cv::Scalar(255, 128, 128), cv::Scalar(128, 255, 128), cv::Scalar(255, 255, 255),
        cv::Scalar(192, 192, 192), cv::Scalar(64, 64, 64),   cv::Scalar(128, 64, 0),   cv::Scalar(64, 0, 128)
    };

    cv::Rect overall_roi;
    if (!rois_.empty()) {
        overall_roi = rois_[0].rect;
        for (const auto& roi : rois_) {
            overall_roi.x = std::min(overall_roi.x, roi.rect.x);
            overall_roi.y = std::min(overall_roi.y, roi.rect.y);
            overall_roi.width = std::max(overall_roi.x + overall_roi.width, roi.rect.x + roi.rect.width) - overall_roi.x;
            overall_roi.height = std::max(overall_roi.y + overall_roi.height, roi.rect.y + roi.rect.height) - overall_roi.y;
        }
        overall_roi.x -= 10;
        overall_roi.y -= 10;
        overall_roi.width += 20;
        overall_roi.height += 20;
        cv::rectangle(display_img, overall_roi, cv::Scalar(255, 255, 255), 2);
    }

    for (size_t i = 0; i < rois_.size() && i < 24; ++i) {
        cv::rectangle(display_img, rois_[i].rect, colors[i], 2);
    }

    roi_canvas_->SetImage(display_img);

    UpdateStatus(wxString::Format("Color checker detected: %zu regions", rois_.size()));
    next_btn_->Enable();
}

void QuickColorCalibPanel::OnDetectRefROI(wxCommandEvent& event) {
    if (ref_image_.empty()) {
        UpdateStatus("Please load or capture a reference image first", true);
        MV_LOG_ERROR_DETAIL("RefROIDetection", "No reference image loaded");
        return;
    }

    MV_LOG_OPERATION("Detect Reference Color Checker", "Starting auto detection...");

    ref_rois_ = mvtk::ROIManager::detectColorChecker(ref_image_, mvtk::ROIType::COLOR_24);

    if (ref_rois_.empty()) {
        UpdateStatus("Failed to detect color checker in reference image", true);
        MV_LOG_ERROR_DETAIL("RefROIDetection", "MCC module failed to detect color checker");
        return;
    }

    ref_roi_count_text_->SetLabel(wxString::Format("ROI Count: %zu", ref_rois_.size()));
    MV_LOG_DATA("RefROIResult", "Detected " + std::to_string(ref_rois_.size()) + " ROIs (Auto)");

    cv::Mat display_img = ref_image_.clone();

    cv::Scalar colors[] = {
        cv::Scalar(255, 0, 0),    cv::Scalar(0, 255, 0),    cv::Scalar(0, 0, 255),    cv::Scalar(255, 255, 0),
        cv::Scalar(255, 0, 255),  cv::Scalar(0, 255, 255),  cv::Scalar(128, 0, 0),    cv::Scalar(0, 128, 0),
        cv::Scalar(0, 0, 128),    cv::Scalar(128, 128, 0),  cv::Scalar(128, 0, 128),  cv::Scalar(0, 128, 128),
        cv::Scalar(255, 128, 0),  cv::Scalar(255, 0, 128),  cv::Scalar(128, 255, 0),  cv::Scalar(0, 255, 128),
        cv::Scalar(128, 128, 255), cv::Scalar(255, 128, 128), cv::Scalar(128, 255, 128), cv::Scalar(255, 255, 255),
        cv::Scalar(192, 192, 192), cv::Scalar(64, 64, 64),   cv::Scalar(128, 64, 0),   cv::Scalar(64, 0, 128)
    };

    cv::Rect overall_roi;
    if (!ref_rois_.empty()) {
        overall_roi = ref_rois_[0].rect;
        for (const auto& roi : ref_rois_) {
            overall_roi.x = std::min(overall_roi.x, roi.rect.x);
            overall_roi.y = std::min(overall_roi.y, roi.rect.y);
            overall_roi.width = std::max(overall_roi.x + overall_roi.width, roi.rect.x + roi.rect.width) - overall_roi.x;
            overall_roi.height = std::max(overall_roi.y + overall_roi.height, roi.rect.y + roi.rect.height) - overall_roi.y;
        }
        overall_roi.x -= 10;
        overall_roi.y -= 10;
        overall_roi.width += 20;
        overall_roi.height += 20;
        cv::rectangle(display_img, overall_roi, cv::Scalar(255, 255, 255), 2);
    }

    for (size_t i = 0; i < ref_rois_.size() && i < 24; ++i) {
        cv::rectangle(display_img, ref_rois_[i].rect, colors[i], 2);
    }

    ref_canvas_->SetImage(display_img);

    UpdateStatus(wxString::Format("Reference color checker detected: %zu regions", ref_rois_.size()));
}

void QuickColorCalibPanel::OnRefSourceChanged(wxCommandEvent& event) {
    ref_source_ = ref_source_combo_->GetSelection();
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
        capture_ref_btn_->Disable();
        close_ref_camera_btn_->Disable();
        open_ref_camera_btn_->Enable();
    }
}

void QuickColorCalibPanel::OnLoadRef(wxCommandEvent& event) {
    wxString path = ref_path_ctrl_->GetValue();
    if (path.IsEmpty()) {
        UpdateStatus("Please enter a file path", true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Empty reference file path");
        return;
    }
    MV_LOG_OPERATION("Load Reference Image", "Path: " + path.ToStdString());
    ref_image_ = mvtk::utils::imreadUtf8(path.ToStdString());
    if (!ref_image_.empty()) {
        ref_canvas_->SetImage(ref_image_);
        UpdateStatus("Reference image loaded");
        MV_LOG_DATA("RefImageInfo", "Size: " + std::to_string(ref_image_.cols) + "x" + std::to_string(ref_image_.rows));
    } else {
        UpdateStatus("Failed to load reference image", true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Failed to load reference image: " + path.ToStdString());
    }
}

void QuickColorCalibPanel::OnOpenRefCamera(wxCommandEvent& event) {
    camera_.open(0);
    if (camera_.isOpened()) {
        camera_opened_ = true;
        open_ref_camera_btn_->Disable();
        capture_ref_btn_->Enable();
        close_ref_camera_btn_->Enable();
        UpdateStatus("Camera opened successfully");
    } else {
        UpdateStatus("Failed to open camera", true);
    }
}

void QuickColorCalibPanel::OnCaptureRef(wxCommandEvent& event) {
    camera_ >> ref_image_;
    if (!ref_image_.empty()) {
        ref_canvas_->SetImage(ref_image_);
        UpdateStatus("Reference image captured");
    } else {
        UpdateStatus("Failed to capture reference image", true);
    }
}

void QuickColorCalibPanel::OnCloseRefCamera(wxCommandEvent& event) {
    camera_.release();
    camera_opened_ = false;
    open_ref_camera_btn_->Enable();
    capture_ref_btn_->Disable();
    close_ref_camera_btn_->Disable();
    UpdateStatus("Camera closed");
}

void QuickColorCalibPanel::OnCalibrate(wxCommandEvent& event) {
    if (src_image_.empty()) {
        UpdateStatus("Please load or capture an image first", true);
        MV_LOG_ERROR_DETAIL("Calibration", "No source image");
        return;
    }
    if (rois_.empty()) {
        UpdateStatus("Please generate or draw ROIs first", true);
        MV_LOG_ERROR_DETAIL("Calibration", "No ROIs generated");
        return;
    }

    std::vector<cv::Rect> rects;
    for (const auto& r : rois_) rects.push_back(r.rect);

    stat_method_ = static_cast<mvtk::RGBStatMethod>(stat_method_combo_->GetSelection());
    wb_method_ = static_cast<mvtk::WBMethod>(wb_method_combo_->GetSelection());
    ccm_method_ = static_cast<mvtk::CCMMethod>(ccm_method_combo_->GetSelection());

    MV_LOG_OPERATION("Calibrate", "Mode: " + std::string(calib_mode_ == 0 ? "Standard" : "Reference") +
                     ", ROIs: " + std::to_string(rois_.size()) +
                     ", WB Method: " + std::to_string(static_cast<int>(wb_method_)) +
                     ", CCM Method: " + std::to_string(static_cast<int>(ccm_method_)) +
                     ", Stat Method: " + std::to_string(static_cast<int>(stat_method_)));

    if (calib_mode_ == 0) {
        calib_result_ = QuickColorCalibrator::calibrateStandard(src_image_, rects, wb_method_, ccm_method_, stat_method_);
    } else {
        if (ref_image_.empty()) {
            UpdateStatus("Please load or capture a reference image", true);
            MV_LOG_ERROR_DETAIL("Calibration", "No reference image for reference mode");
            return;
        }
        std::vector<cv::Rect> ref_rects;
        for (const auto& r : ref_rois_) ref_rects.push_back(r.rect);
        calib_result_ = QuickColorCalibrator::calibrateReference(src_image_, ref_image_, rects, ref_rects, wb_method_, ccm_method_, stat_method_);
    }

    MV_LOG_DATA("WhiteBalance", "R Gain: " + std::to_string(calib_result_.wb_params.r_gain) + 
                ", G Gain: " + std::to_string(calib_result_.wb_params.g_gain) + 
                ", B Gain: " + std::to_string(calib_result_.wb_params.b_gain));
    MV_LOG_DATA("CCMMatrix", "[" + std::to_string(calib_result_.ccm_params.ccm(0, 0)) + "," + 
                std::to_string(calib_result_.ccm_params.ccm(0, 1)) + "," + 
                std::to_string(calib_result_.ccm_params.ccm(0, 2)) + "] [" +
                std::to_string(calib_result_.ccm_params.ccm(1, 0)) + "," + 
                std::to_string(calib_result_.ccm_params.ccm(1, 1)) + "," + 
                std::to_string(calib_result_.ccm_params.ccm(1, 2)) + "] [" +
                std::to_string(calib_result_.ccm_params.ccm(2, 0)) + "," + 
                std::to_string(calib_result_.ccm_params.ccm(2, 1)) + "," + 
                std::to_string(calib_result_.ccm_params.ccm(2, 2)) + "]");
    MV_LOG_DATA("DeltaE00", "Mean: " + std::to_string(calib_result_.mean_deltaE00) +
                ", Max: " + std::to_string(*std::max_element(calib_result_.deltaE00.begin(), calib_result_.deltaE00.end())));

    wb_result_text_->SetLabel(wxString::Format(
        "  R Gain: %.4f\n  G Gain: %.4f\n  B Gain: %.4f",
        calib_result_.wb_params.r_gain,
        calib_result_.wb_params.g_gain,
        calib_result_.wb_params.b_gain));

    ccm_result_text_->SetLabel(wxString::Format(
        "  [%.4f, %.4f, %.4f]\n  [%.4f, %.4f, %.4f]\n  [%.4f, %.4f, %.4f]",
        calib_result_.ccm_params.ccm(0, 0), calib_result_.ccm_params.ccm(0, 1), calib_result_.ccm_params.ccm(0, 2),
        calib_result_.ccm_params.ccm(1, 0), calib_result_.ccm_params.ccm(1, 1), calib_result_.ccm_params.ccm(1, 2),
        calib_result_.ccm_params.ccm(2, 0), calib_result_.ccm_params.ccm(2, 1), calib_result_.ccm_params.ccm(2, 2)));

    deltae_mean_text_->SetLabel(wxString::Format("Mean DeltaE00: %.2f", calib_result_.mean_deltaE00));

    deltae_grid_->ClearGrid();
    int row_count = deltae_grid_->GetNumberRows();
    if (row_count > 0) {
        deltae_grid_->DeleteRows(0, row_count);
    }
    deltae_grid_->InsertRows(0, calib_result_.deltaE00.size());
    for (size_t i = 0; i < calib_result_.deltaE00.size(); ++i) {
        deltae_grid_->SetCellValue(i, 0, wxString::Format("Patch %zu", i));
        double de = calib_result_.deltaE00[i];
        deltae_grid_->SetCellValue(i, 1, wxString::Format("%.2f", de));
        if (de >= 6.0) deltae_grid_->SetCellTextColour(i, 1, *wxRED);
        else if (de >= 3.0) deltae_grid_->SetCellTextColour(i, 1, *wxBLUE);
        else deltae_grid_->SetCellTextColour(i, 1, *wxGREEN);
    }

    UpdateStatus("Calibration completed");
    MV_LOG_OPERATION("Calibration", "Completed successfully");
    step_ = 3;
    UpdateStepButtons();
    notebook_->SetSelection(3);
    UpdateCanvasImages();
}

void QuickColorCalibPanel::OnBack(wxCommandEvent& event) {
    if (step_ > 0) {
        step_--;
        UpdateStepButtons();
        notebook_->SetSelection(step_);
        UpdateCanvasImages();
    }
}

void QuickColorCalibPanel::OnNext(wxCommandEvent& event) {
    if (step_ == 0 && !src_image_.empty()) {
        step_ = 1;
        UpdateStepButtons();
        notebook_->SetSelection(1);
        UpdateCanvasImages();
    } else if (step_ == 1 && !rois_.empty()) {
        step_ = 2;
        UpdateStepButtons();
        notebook_->SetSelection(2);
        UpdateCanvasImages();
    } else if (step_ == 2) {
        step_ = 3;
        UpdateStepButtons();
        notebook_->SetSelection(3);
        UpdateCanvasImages();
    }
}

void QuickColorCalibPanel::OnReset(wxCommandEvent& event) {
    step_ = 0;
    rois_.clear();
    ref_rois_.clear();
    calib_result_ = mvtk::QuickCalibResult();
    src_image_ = cv::Mat();
    ref_image_ = cv::Mat();
    src_canvas_->Clear();
    ref_canvas_->Clear();
    deltae_grid_->ClearGrid();
    int row_count = deltae_grid_->GetNumberRows();
    if (row_count > 0) {
        deltae_grid_->DeleteRows(0, row_count);
    }
    wb_result_text_->SetLabel("");
    ccm_result_text_->SetLabel("");
    deltae_mean_text_->SetLabel("");
    UpdateStepButtons();
    UpdateStatus("Reset completed");
}

}
}