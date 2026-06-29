#include "wxui/color_error_panel.h"
#include "neumorphic_panel.h"
#include "style_defs.h"
#include "algo/white_balance.h"
#include "utils/image_io.h"
#include "utils/session_config.h"
#include <wx/grid.h>

namespace mvtk {
namespace wxui {

BEGIN_EVENT_TABLE(ColorErrorPanel, wxPanel)
    EVT_COMBOBOX(wxID_ANY, ColorErrorPanel::OnSrcSourceChanged)
    EVT_BUTTON(CE_ID_SRC_LOAD_BTN, ColorErrorPanel::OnLoadSrc)
    EVT_BUTTON(CE_ID_SRC_OPEN_CAMERA_BTN, ColorErrorPanel::OnOpenSrcCamera)
    EVT_BUTTON(CE_ID_SRC_CAPTURE_BTN, ColorErrorPanel::OnCaptureSrc)
    EVT_BUTTON(CE_ID_SRC_CLOSE_CAMERA_BTN, ColorErrorPanel::OnCloseSrcCamera)
    EVT_COMBOBOX(wxID_ANY, ColorErrorPanel::OnRefSourceChanged)
    EVT_BUTTON(CE_ID_REF_LOAD_BTN, ColorErrorPanel::OnLoadRef)
    EVT_BUTTON(CE_ID_REF_OPEN_CAMERA_BTN, ColorErrorPanel::OnOpenRefCamera)
    EVT_BUTTON(CE_ID_REF_CAPTURE_BTN, ColorErrorPanel::OnCaptureRef)
    EVT_BUTTON(CE_ID_DRAW_24_ROI_BTN, ColorErrorPanel::OnDraw24ROI)
    EVT_BUTTON(CE_ID_DRAW_140_ROI_BTN, ColorErrorPanel::OnDraw140ROI)
    EVT_BUTTON(CE_ID_CLEAR_ROI_BTN, ColorErrorPanel::OnClearROI)
    EVT_BUTTON(CE_ID_ADD_ROW_BTN, ColorErrorPanel::OnAddRow)
    EVT_BUTTON(CE_ID_IMPORT_COLORS_BTN, ColorErrorPanel::OnImportColors)
    EVT_BUTTON(CE_ID_CALCULATE_BTN, ColorErrorPanel::OnCalculate)
    EVT_BUTTON(CE_ID_EXPORT_BTN, ColorErrorPanel::OnExport)
END_EVENT_TABLE()

ColorErrorPanel::ColorErrorPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    SetBackgroundColour(Style::NEU_BG_COLOR);
    SetFont(Style::GetSansFont(10));

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* top_row = new wxBoxSizer(wxHORIZONTAL);

    NeumorphicPanel* src_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* src_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* src_title = new wxStaticText(src_panel, wxID_ANY, "Source Image");
    Style::ApplyNeumorphicStyle(src_title, true);
    src_box->Add(src_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* src_row1 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* src_label = new wxStaticText(src_panel, wxID_ANY, "Source:");
    Style::ApplyNeumorphicStyle(src_label);
    src_row1->Add(src_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    src_source_combo_ = new wxComboBox(src_panel, wxID_ANY, "Local Image", wxDefaultPosition, wxSize(120, -1),
                                       {"Local Image", "Camera"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(src_source_combo_);
    src_row1->Add(src_source_combo_, 0);
    src_box->Add(src_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* src_row2 = new wxBoxSizer(wxHORIZONTAL);
    load_src_btn_ = new wxButton(src_panel, CE_ID_SRC_LOAD_BTN, "Load");
    Style::ApplyNeumorphicStyle(load_src_btn_);
    src_path_ctrl_ = new wxTextCtrl(src_panel, wxID_ANY, "", wxDefaultPosition, wxSize(200, -1));
    Style::ApplyNeumorphicStyle(src_path_ctrl_);
    src_row2->Add(load_src_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    src_row2->Add(src_path_ctrl_, 1);
    src_box->Add(src_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* src_info_header = new wxStaticText(src_panel, wxID_ANY, "[Info]");
    Style::ApplyNeumorphicStyle(src_info_header, true);
    src_info_header->SetFont(Style::GetSansFont(9));
    src_box->Add(src_info_header, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* src_info_title = new wxStaticText(src_panel, wxID_ANY, "◆ Source Image:");
    Style::ApplyNeumorphicStyle(src_info_title);
    src_info_title->SetFont(Style::GetSansFont(8));
    src_box->Add(src_info_title, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* src_info_item1 = new wxStaticText(src_panel, wxID_ANY, "- Format: .png .jpg .bmp .tif (8/16-bit)");
    src_info_item1->SetFont(Style::GetSansFont(8));
    src_info_item1->SetForegroundColour(wxColour(128, 128, 128));
    src_box->Add(src_info_item1, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* src_info_item2 = new wxStaticText(src_panel, wxID_ANY, "- Captured color checker chart");
    src_info_item2->SetFont(Style::GetSansFont(8));
    src_info_item2->SetForegroundColour(wxColour(128, 128, 128));
    src_box->Add(src_info_item2, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* src_info_item3 = new wxStaticText(src_panel, wxID_ANY, "- Proper lighting and exposure");
    src_info_item3->SetFont(Style::GetSansFont(8));
    src_info_item3->SetForegroundColour(wxColour(128, 128, 128));
    src_box->Add(src_info_item3, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* src_camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_src_camera_btn_ = new wxButton(src_panel, CE_ID_SRC_OPEN_CAMERA_BTN, "Open Camera");
    Style::ApplyNeumorphicStyle(open_src_camera_btn_);
    capture_src_btn_ = new wxButton(src_panel, CE_ID_SRC_CAPTURE_BTN, "Capture");
    Style::ApplyNeumorphicStyle(capture_src_btn_);
    close_src_camera_btn_ = new wxButton(src_panel, CE_ID_SRC_CLOSE_CAMERA_BTN, "Close");
    Style::ApplyNeumorphicStyle(close_src_camera_btn_);
    capture_src_btn_->Disable();
    close_src_camera_btn_->Disable();
    src_camera_row->Add(open_src_camera_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    src_camera_row->Add(capture_src_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    src_camera_row->Add(close_src_camera_btn_, 0);
    src_box->Add(src_camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    src_panel->SetSizer(src_box);
    top_row->Add(src_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    NeumorphicPanel* ref_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* ref_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* ref_title = new wxStaticText(ref_panel, wxID_ANY, "Reference Image");
    Style::ApplyNeumorphicStyle(ref_title, true);
    ref_box->Add(ref_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* ref_row1 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* ref_label = new wxStaticText(ref_panel, wxID_ANY, "Source:");
    Style::ApplyNeumorphicStyle(ref_label);
    ref_row1->Add(ref_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_SMALL);
    ref_source_combo_ = new wxComboBox(ref_panel, wxID_ANY, "Local Image", wxDefaultPosition, wxSize(120, -1),
                                       {"Local Image", "Camera"}, wxCB_READONLY);
    Style::ApplyNeumorphicStyle(ref_source_combo_);
    ref_row1->Add(ref_source_combo_, 0);
    ref_box->Add(ref_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* ref_row2 = new wxBoxSizer(wxHORIZONTAL);
    load_ref_btn_ = new wxButton(ref_panel, CE_ID_REF_LOAD_BTN, "Load");
    Style::ApplyNeumorphicStyle(load_ref_btn_);
    ref_path_ctrl_ = new wxTextCtrl(ref_panel, wxID_ANY, "", wxDefaultPosition, wxSize(200, -1));
    Style::ApplyNeumorphicStyle(ref_path_ctrl_);
    ref_row2->Add(load_ref_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    ref_row2->Add(ref_path_ctrl_, 1);
    ref_box->Add(ref_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* ref_info_header = new wxStaticText(ref_panel, wxID_ANY, "[Info]");
    Style::ApplyNeumorphicStyle(ref_info_header, true);
    ref_info_header->SetFont(Style::GetSansFont(9));
    ref_box->Add(ref_info_header, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* ref_info_title = new wxStaticText(ref_panel, wxID_ANY, "◆ Reference Image:");
    Style::ApplyNeumorphicStyle(ref_info_title);
    ref_info_title->SetFont(Style::GetSansFont(8));
    ref_box->Add(ref_info_title, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* ref_info_item1 = new wxStaticText(ref_panel, wxID_ANY, "- Format: .png .jpg .bmp .tif (8/16-bit)");
    ref_info_item1->SetFont(Style::GetSansFont(8));
    ref_info_item1->SetForegroundColour(wxColour(128, 128, 128));
    ref_box->Add(ref_info_item1, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* ref_info_item2 = new wxStaticText(ref_panel, wxID_ANY, "- Standard color chart or calibrated image");
    ref_info_item2->SetFont(Style::GetSansFont(8));
    ref_info_item2->SetForegroundColour(wxColour(128, 128, 128));
    ref_box->Add(ref_info_item2, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxStaticText* ref_info_item3 = new wxStaticText(ref_panel, wxID_ANY, "- Same size/resolution as source");
    ref_info_item3->SetFont(Style::GetSansFont(8));
    ref_info_item3->SetForegroundColour(wxColour(128, 128, 128));
    ref_box->Add(ref_info_item3, 0, wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* ref_camera_row = new wxBoxSizer(wxHORIZONTAL);
    open_ref_camera_btn_ = new wxButton(ref_panel, CE_ID_REF_OPEN_CAMERA_BTN, "Open Camera");
    Style::ApplyNeumorphicStyle(open_ref_camera_btn_);
    capture_ref_btn_ = new wxButton(ref_panel, CE_ID_REF_CAPTURE_BTN, "Capture");
    Style::ApplyNeumorphicStyle(capture_ref_btn_);
    capture_ref_btn_->Disable();
    ref_camera_row->Add(open_ref_camera_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    ref_camera_row->Add(capture_ref_btn_, 0);
    ref_box->Add(ref_camera_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    ref_panel->SetSizer(ref_box);
    top_row->Add(ref_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    NeumorphicPanel* roi_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* roi_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* roi_title = new wxStaticText(roi_panel, wxID_ANY, "ROI Settings");
    Style::ApplyNeumorphicStyle(roi_title, true);
    roi_box->Add(roi_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* roi_btn_row1 = new wxBoxSizer(wxHORIZONTAL);
    draw_24_btn_ = new wxButton(roi_panel, CE_ID_DRAW_24_ROI_BTN, "Draw 24-Color");
    Style::ApplyNeumorphicStyle(draw_24_btn_);
    draw_140_btn_ = new wxButton(roi_panel, CE_ID_DRAW_140_ROI_BTN, "Draw 140-Color");
    Style::ApplyNeumorphicStyle(draw_140_btn_);
    roi_btn_row1->Add(draw_24_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    roi_btn_row1->Add(draw_140_btn_, 0);
    roi_box->Add(roi_btn_row1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_SMALL);

    wxBoxSizer* roi_btn_row2 = new wxBoxSizer(wxHORIZONTAL);
    clear_roi_btn_ = new wxButton(roi_panel, CE_ID_CLEAR_ROI_BTN, "Clear ROI");
    Style::ApplyNeumorphicStyle(clear_roi_btn_);
    roi_btn_row2->Add(clear_roi_btn_, 0);
    roi_box->Add(roi_btn_row2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    roi_count_text_ = new wxStaticText(roi_panel, wxID_ANY, "ROI Count: 0");
    Style::ApplyNeumorphicStyle(roi_count_text_);
    roi_box->Add(roi_count_text_, 0, wxALL, Style::SPACING_MEDIUM);
    roi_panel->SetSizer(roi_box);
    top_row->Add(roi_panel, 1, wxEXPAND);

    main_sizer->Add(top_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, Style::SPACING_LARGE);

    status_text_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(status_text_);
    main_sizer->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    NeumorphicPanel* image_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* image_box = new wxBoxSizer(wxHORIZONTAL);
    
    wxBoxSizer* src_img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* src_img_title = new wxStaticText(image_panel, wxID_ANY, "Source Image");
    Style::ApplyNeumorphicStyle(src_img_title);
    src_img_box->Add(src_img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    src_canvas_ = new ImageCanvas(image_panel, wxID_ANY);
    src_canvas_->SetMinSize(wxSize(350, 350));
    src_img_box->Add(src_canvas_, 1, wxEXPAND);
    image_box->Add(src_img_box, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);

    wxBoxSizer* ref_img_box = new wxBoxSizer(wxVERTICAL);
    wxStaticText* ref_img_title = new wxStaticText(image_panel, wxID_ANY, "Reference Image");
    Style::ApplyNeumorphicStyle(ref_img_title);
    ref_img_box->Add(ref_img_title, 0, wxALIGN_CENTER | wxBOTTOM, Style::SPACING_MEDIUM);
    ref_canvas_ = new ImageCanvas(image_panel, wxID_ANY);
    ref_canvas_->SetMinSize(wxSize(350, 350));
    ref_img_box->Add(ref_canvas_, 1, wxEXPAND);
    image_box->Add(ref_img_box, 1, wxEXPAND);
    image_panel->SetSizer(image_box);
    main_sizer->Add(image_panel, 1, wxEXPAND | wxALL, Style::SPACING_LARGE);

    NeumorphicPanel* color_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* color_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* color_title = new wxStaticText(color_panel, wxID_ANY, "Color Pairs");
    Style::ApplyNeumorphicStyle(color_title, true);
    color_box->Add(color_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* color_btn_row = new wxBoxSizer(wxHORIZONTAL);
    add_row_btn_ = new wxButton(color_panel, CE_ID_ADD_ROW_BTN, "+ Add Row");
    Style::ApplyNeumorphicStyle(add_row_btn_);
    import_colors_btn_ = new wxButton(color_panel, CE_ID_IMPORT_COLORS_BTN, "Import Color File");
    Style::ApplyNeumorphicStyle(import_colors_btn_);
    color_btn_row->Add(add_row_btn_, 0, wxRIGHT, Style::SPACING_SMALL);
    color_btn_row->Add(import_colors_btn_, 0);
    color_box->Add(color_btn_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    color_grid_ = new wxGrid(color_panel, wxID_ANY);
    color_grid_->CreateGrid(0, 7);
    color_grid_->SetColLabelValue(0, "#");
    color_grid_->SetColLabelValue(1, "Src R");
    color_grid_->SetColLabelValue(2, "Src G");
    color_grid_->SetColLabelValue(3, "Src B");
    color_grid_->SetColLabelValue(4, "Ref R");
    color_grid_->SetColLabelValue(5, "Ref G");
    color_grid_->SetColLabelValue(6, "Ref B");
    color_grid_->EnableEditing(false);
    color_grid_->SetFont(Style::GetSansFont(8));
    color_box->Add(color_grid_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    color_panel->SetSizer(color_box);

    NeumorphicPanel* calc_panel = new NeumorphicPanel(this, wxID_ANY);
    wxBoxSizer* calc_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* calc_title = new wxStaticText(calc_panel, wxID_ANY, "Calculation");
    Style::ApplyNeumorphicStyle(calc_title, true);
    calc_box->Add(calc_title, 0, wxALL, Style::SPACING_MEDIUM);
    
    wxBoxSizer* calc_row = new wxBoxSizer(wxHORIZONTAL);
    srgb_checkbox_ = new wxCheckBox(calc_panel, wxID_ANY, "sRGB Color Space");
    srgb_checkbox_->SetFont(Style::GetSansFont(9));
    srgb_checkbox_->SetForegroundColour(Style::NEU_TEXT_COLOR);
    calculate_btn_ = new wxButton(calc_panel, CE_ID_CALCULATE_BTN, "Calculate");
    Style::ApplyNeumorphicStyle(calculate_btn_);
    calc_row->Add(srgb_checkbox_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Style::SPACING_MEDIUM);
    calc_row->Add(calculate_btn_, 0);
    calc_box->Add(calc_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);

    wxBoxSizer* result_box = new wxBoxSizer(wxHORIZONTAL);

    NeumorphicPanel* error_panel = new NeumorphicPanel(calc_panel, wxID_ANY);
    wxBoxSizer* error_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* error_title = new wxStaticText(error_panel, wxID_ANY, "Color Error");
    Style::ApplyNeumorphicStyle(error_title, true);
    error_box->Add(error_title, 0, wxALL, Style::SPACING_SMALL);
    
    error_grid_ = new wxGrid(error_panel, wxID_ANY);
    error_grid_->CreateGrid(0, 5);
    error_grid_->SetColLabelValue(0, "#");
    error_grid_->SetColLabelValue(1, "DeltaE00");
    error_grid_->SetColLabelValue(2, "DeltaE76");
    error_grid_->SetColLabelValue(3, "DeltaL");
    error_grid_->SetColLabelValue(4, "DeltaC");
    error_grid_->EnableEditing(false);
    error_grid_->SetFont(Style::GetSansFont(8));
    error_box->Add(error_grid_, 1, wxEXPAND | wxALL, Style::SPACING_SMALL);
    error_panel->SetSizer(error_box);
    result_box->Add(error_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_MEDIUM);

    NeumorphicPanel* stats_panel = new NeumorphicPanel(calc_panel, wxID_ANY);
    wxBoxSizer* stats_box = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* stats_title = new wxStaticText(stats_panel, wxID_ANY, "Statistics");
    Style::ApplyNeumorphicStyle(stats_title, true);
    stats_box->Add(stats_title, 0, wxALL, Style::SPACING_SMALL);
    
    result_text_ = new wxStaticText(stats_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    Style::ApplyNeumorphicStyle(result_text_);
    stats_box->Add(result_text_, 0, wxEXPAND | wxALL, Style::SPACING_SMALL);

    export_btn_ = new wxButton(stats_panel, CE_ID_EXPORT_BTN, "Export Result");
    Style::ApplyNeumorphicStyle(export_btn_);
    export_btn_->Disable();
    stats_box->Add(export_btn_, 0, wxALIGN_CENTER | wxALL, Style::SPACING_SMALL);
    stats_panel->SetSizer(stats_box);
    result_box->Add(stats_panel, 0, wxEXPAND);

    calc_box->Add(result_box, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, Style::SPACING_MEDIUM);
    calc_panel->SetSizer(calc_box);

    wxBoxSizer* bottom_row = new wxBoxSizer(wxHORIZONTAL);
    bottom_row->Add(color_panel, 1, wxEXPAND | wxRIGHT, Style::SPACING_LARGE);
    bottom_row->Add(calc_panel, 1, wxEXPAND);
    main_sizer->Add(bottom_row, 1, wxEXPAND | wxALL, Style::SPACING_LARGE);

    SetSizer(main_sizer);
}

ColorErrorPanel::~ColorErrorPanel() {
    if (camera_opened_) {
        camera_.release();
    }
}

void ColorErrorPanel::UpdateStatus(const wxString& msg, bool is_error) {
    status_text_->SetLabel(msg);
    if (is_error) {
        status_text_->SetForegroundColour(*wxRED);
    } else {
        status_text_->SetForegroundColour(*wxBLUE);
    }
}

void ColorErrorPanel::UpdateGrid() {
    color_grid_->ClearGrid();
    int row_count = color_grid_->GetNumberRows();
    if (row_count > 0) {
        color_grid_->DeleteRows(0, row_count);
    }
    color_grid_->InsertRows(0, src_colors_.size());

    for (size_t i = 0; i < src_colors_.size() && i < ref_colors_.size(); ++i) {
        color_grid_->SetCellValue(i, 0, wxString::Format("%zu", i));
        color_grid_->SetCellValue(i, 1, wxString::Format("%.0f", src_colors_[i][0]));
        color_grid_->SetCellValue(i, 2, wxString::Format("%.0f", src_colors_[i][1]));
        color_grid_->SetCellValue(i, 3, wxString::Format("%.0f", src_colors_[i][2]));
        color_grid_->SetCellValue(i, 4, wxString::Format("%.0f", ref_colors_[i][0]));
        color_grid_->SetCellValue(i, 5, wxString::Format("%.0f", ref_colors_[i][1]));
        color_grid_->SetCellValue(i, 6, wxString::Format("%.0f", ref_colors_[i][2]));
    }
}

void ColorErrorPanel::OnSrcSourceChanged(wxCommandEvent& event) {
    src_source_ = src_source_combo_->GetSelection();
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
        capture_src_btn_->Disable();
        close_src_camera_btn_->Disable();
        open_src_camera_btn_->Enable();
    }
}

void ColorErrorPanel::OnLoadSrc(wxCommandEvent& event) {
    wxString path = src_path_ctrl_->GetValue();
    if (path.IsEmpty()) {
        UpdateStatus("Please enter a file path", true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Empty source file path");
        return;
    }
    MV_LOG_OPERATION("Load Source Image", "Path: " + path.ToStdString());
    src_image_ = mvtk::utils::imreadUtf8(path.ToStdString());
    if (!src_image_.empty()) {
        src_canvas_->SetImage(src_image_);
        UpdateStatus(wxString::Format("Source image loaded: %dx%d", src_image_.cols, src_image_.rows));
        MV_LOG_DATA("ImageInfo", "Size: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows));
    } else {
        UpdateStatus("Failed to load source image: " + path, true);
        MV_LOG_ERROR_DETAIL("ImageLoad", "Failed to load source image: " + path.ToStdString());
    }
}

void ColorErrorPanel::OnOpenSrcCamera(wxCommandEvent& event) {
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

void ColorErrorPanel::OnCaptureSrc(wxCommandEvent& event) {
    MV_LOG_OPERATION("Capture Image", "Source: Camera");
    camera_ >> src_image_;
    if (!src_image_.empty()) {
        src_canvas_->SetImage(src_image_);
        UpdateStatus(wxString::Format("Image captured: %dx%d", src_image_.cols, src_image_.rows));
        MV_LOG_DATA("CapturedImage", "Size: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows));
    } else {
        UpdateStatus("Failed to capture image", true);
        MV_LOG_ERROR_DETAIL("Camera", "Failed to capture image from camera");
    }
}

void ColorErrorPanel::OnCloseSrcCamera(wxCommandEvent& event) {
    camera_.release();
    camera_opened_ = false;
    open_src_camera_btn_->Enable();
    capture_src_btn_->Disable();
    close_src_camera_btn_->Disable();
    UpdateStatus("Camera closed");
}

void ColorErrorPanel::OnRefSourceChanged(wxCommandEvent& event) {
    ref_source_ = ref_source_combo_->GetSelection();
    if (camera_opened_) {
        camera_.release();
        camera_opened_ = false;
        capture_ref_btn_->Disable();
        open_ref_camera_btn_->Enable();
    }
}

void ColorErrorPanel::OnLoadRef(wxCommandEvent& event) {
    wxString path = ref_path_ctrl_->GetValue();
    if (path.IsEmpty()) {
        UpdateStatus("Please enter a file path", true);
        return;
    }
    ref_image_ = mvtk::utils::imreadUtf8(path.ToStdString());
    if (!ref_image_.empty()) {
        ref_canvas_->SetImage(ref_image_);
        UpdateStatus(wxString::Format("Reference image loaded: %dx%d", ref_image_.cols, ref_image_.rows));
    } else {
        UpdateStatus("Failed to load reference image: " + path, true);
    }
}

void ColorErrorPanel::OnOpenRefCamera(wxCommandEvent& event) {
    camera_.open(0);
    if (camera_.isOpened()) {
        camera_opened_ = true;
        open_ref_camera_btn_->Disable();
        capture_ref_btn_->Enable();
        UpdateStatus("Camera opened successfully");
    } else {
        UpdateStatus("Failed to open camera", true);
    }
}

void ColorErrorPanel::OnCaptureRef(wxCommandEvent& event) {
    camera_ >> ref_image_;
    if (!ref_image_.empty()) {
        ref_canvas_->SetImage(ref_image_);
        UpdateStatus("Reference image captured");
    } else {
        UpdateStatus("Failed to capture reference image", true);
    }
}

void ColorErrorPanel::OnDraw24ROI(wxCommandEvent& event) {
    if (src_image_.empty()) {
        UpdateStatus("Please load source image first", true);
        return;
    }

    rois_ = ROIManager::detectColorChecker(src_image_, mvtk::ROIType::COLOR_24);
    
    if (rois_.empty()) {
        UpdateStatus("Failed to detect 24-color checker, trying fallback...", true);
        
        int img_w = src_image_.cols;
        int img_h = src_image_.rows;
        int margin_x = img_w * 0.1;
        int margin_y = img_h * 0.1;
        int roi_w = (img_w - margin_x * 2) / 6;
        int roi_h = (img_h - margin_y * 2) / 4;
        rois_ = ROIManager::generate24ColorROI(margin_x, margin_y, roi_w, roi_h, 2);
    }

    roi_count_text_->SetLabel(wxString::Format("ROI Count: %zu", rois_.size()));
    UpdateStatus(wxString::Format("24-Color ROI detected: %zu patches", rois_.size()));

    cv::Mat src_with_roi = ROIManager::drawROIs(src_image_, rois_, -1);
    src_canvas_->SetImage(src_with_roi);

    if (!rois_.empty() && !src_image_.empty()) {
        std::vector<cv::Rect> rects;
        for (const auto& roi : rois_) rects.push_back(roi.rect);
        src_colors_ = WhiteBalance::extractColors(src_image_, rects, mvtk::RGBStatMethod::MEDIAN);
    }
    if (!rois_.empty() && !ref_image_.empty()) {
        std::vector<ROIInfo> ref_rois = ROIManager::detectColorChecker(ref_image_, mvtk::ROIType::COLOR_24);
        if (ref_rois.empty()) {
            int ref_margin_x = ref_image_.cols * 0.1;
            int ref_margin_y = ref_image_.rows * 0.1;
            int ref_roi_w = (ref_image_.cols - ref_margin_x * 2) / 6;
            int ref_roi_h = (ref_image_.rows - ref_margin_y * 2) / 4;
            ref_rois = ROIManager::generate24ColorROI(ref_margin_x, ref_margin_y, ref_roi_w, ref_roi_h, 2);
        }
        std::vector<cv::Rect> rects;
        for (const auto& roi : ref_rois) rects.push_back(roi.rect);
        ref_colors_ = WhiteBalance::extractColors(ref_image_, rects, mvtk::RGBStatMethod::MEDIAN);
        
        cv::Mat ref_with_roi = ROIManager::drawROIs(ref_image_, ref_rois, -1);
        ref_canvas_->SetImage(ref_with_roi);
    }
    UpdateGrid();
}

void ColorErrorPanel::OnDraw140ROI(wxCommandEvent& event) {
    if (src_image_.empty()) {
        UpdateStatus("Please load source image first", true);
        return;
    }

    rois_ = ROIManager::detectColorChecker(src_image_, mvtk::ROIType::COLOR_140);
    
    if (rois_.empty()) {
        UpdateStatus("Failed to detect 140-color checker, trying fallback...", true);
        
        int img_w = src_image_.cols;
        int img_h = src_image_.rows;
        int margin_x = img_w * 0.05;
        int margin_y = img_h * 0.05;
        int roi_w = (img_w - margin_x * 2) / 14;
        int roi_h = (img_h - margin_y * 2) / 10;
        rois_ = ROIManager::generate140ColorROI(margin_x, margin_y, roi_w, roi_h, 1);
    }

    roi_count_text_->SetLabel(wxString::Format("ROI Count: %zu", rois_.size()));
    UpdateStatus(wxString::Format("140-Color ROI detected: %zu patches", rois_.size()));

    cv::Mat src_with_roi = ROIManager::drawROIs(src_image_, rois_, -1);
    src_canvas_->SetImage(src_with_roi);

    if (!rois_.empty() && !src_image_.empty()) {
        std::vector<cv::Rect> rects;
        for (const auto& roi : rois_) rects.push_back(roi.rect);
        src_colors_ = WhiteBalance::extractColors(src_image_, rects, mvtk::RGBStatMethod::MEDIAN);
    }
    if (!rois_.empty() && !ref_image_.empty()) {
        std::vector<ROIInfo> ref_rois = ROIManager::detectColorChecker(ref_image_, mvtk::ROIType::COLOR_140);
        if (ref_rois.empty()) {
            int ref_margin_x = ref_image_.cols * 0.05;
            int ref_margin_y = ref_image_.rows * 0.05;
            int ref_roi_w = (ref_image_.cols - ref_margin_x * 2) / 14;
            int ref_roi_h = (ref_image_.rows - ref_margin_y * 2) / 10;
            ref_rois = ROIManager::generate140ColorROI(ref_margin_x, ref_margin_y, ref_roi_w, ref_roi_h, 1);
        }
        std::vector<cv::Rect> rects;
        for (const auto& roi : ref_rois) rects.push_back(roi.rect);
        ref_colors_ = WhiteBalance::extractColors(ref_image_, rects, mvtk::RGBStatMethod::MEDIAN);
        
        cv::Mat ref_with_roi = ROIManager::drawROIs(ref_image_, ref_rois, -1);
        ref_canvas_->SetImage(ref_with_roi);
    }
    UpdateGrid();
}

void ColorErrorPanel::OnClearROI(wxCommandEvent& event) {
    rois_.clear();
    src_colors_.clear();
    ref_colors_.clear();
    roi_count_text_->SetLabel("ROI Count: 0");
    if (!src_image_.empty()) {
        src_canvas_->SetImage(src_image_);
    }
    if (!ref_image_.empty()) {
        ref_canvas_->SetImage(ref_image_);
    }
    UpdateGrid();
    UpdateStatus("ROI cleared");
}

void ColorErrorPanel::OnAddRow(wxCommandEvent& event) {
    src_colors_.push_back(cv::Vec3d(128, 128, 128));
    ref_colors_.push_back(cv::Vec3d(128, 128, 128));
    UpdateGrid();
}

void ColorErrorPanel::OnImportColors(wxCommandEvent& event) {
    auto colors = ColorErrorCalculator::importColorsFromIni("");
    if (!colors.empty()) {
        ref_colors_ = colors;
        UpdateGrid();
        UpdateStatus(wxString::Format("Colors imported: %zu", colors.size()));
    } else {
        UpdateStatus("Failed to import colors", true);
    }
}

void ColorErrorPanel::OnCalculate(wxCommandEvent& event) {
    if (src_colors_.empty() || ref_colors_.empty()) {
        UpdateStatus("Please provide source and reference colors", true);
        return;
    }
    if (src_colors_.size() != ref_colors_.size()) {
        UpdateStatus("Source and reference color counts must match", true);
        return;
    }

    params_.use_srgb = srgb_checkbox_->GetValue();
    result_ = ColorErrorCalculator::calculate(src_colors_, ref_colors_, params_);
    has_result_ = true;

    result_text_->SetLabel(wxString::Format(
        "Mean DeltaE00: %.2f\nMax DeltaE00: %.2f\nMin DeltaE00: %.2f",
        result_.mean_deltaE00, result_.max_deltaE00, result_.min_deltaE00));

    error_grid_->ClearGrid();
    int error_row_count = error_grid_->GetNumberRows();
    if (error_row_count > 0) {
        error_grid_->DeleteRows(0, error_row_count);
    }
    error_grid_->InsertRows(0, result_.pairs.size());

    for (size_t i = 0; i < result_.pairs.size(); ++i) {
        error_grid_->SetCellValue(i, 0, wxString::Format("%zu", i));
        double de = result_.pairs[i].deltaE00;
        error_grid_->SetCellValue(i, 1, wxString::Format("%.2f", de));
        if (de >= 6.0) error_grid_->SetCellTextColour(i, 1, *wxRED);
        else if (de >= 3.0) error_grid_->SetCellTextColour(i, 1, *wxBLUE);
        else error_grid_->SetCellTextColour(i, 1, *wxGREEN);
        error_grid_->SetCellValue(i, 2, wxString::Format("%.2f", result_.pairs[i].deltaE76));
        error_grid_->SetCellValue(i, 3, wxString::Format("%.2f", result_.pairs[i].deltaL));
        error_grid_->SetCellValue(i, 4, wxString::Format("%.2f", result_.pairs[i].deltaC));
    }

    export_btn_->Enable();
    UpdateStatus(wxString::Format("Calculation completed: %zu color pairs", src_colors_.size()));

    // Save calculation results to TOML
    auto& cfg = utils::SessionConfig::GetInstance();
    cfg.Set("color_error.use_srgb", params_.use_srgb);
    cfg.Set("color_error.color_count", static_cast<int>(src_colors_.size()));
    cfg.Set("color_error.delta_e00_mean", result_.mean_deltaE00);
    cfg.Set("color_error.delta_e00_max", result_.max_deltaE00);
    cfg.Set("color_error.delta_e00_min", result_.min_deltaE00);
    // Compute mean DeltaE76, DeltaL, DeltaC from pairs
    double sum_e76 = 0.0, sum_l = 0.0, sum_c = 0.0;
    for (const auto& p : result_.pairs) {
        sum_e76 += p.deltaE76;
        sum_l += p.deltaL;
        sum_c += p.deltaC;
    }
    size_t n = result_.pairs.size();
    cfg.Set("color_error.delta_e76_mean", n > 0 ? sum_e76 / n : 0.0);
    cfg.Set("color_error.delta_l_mean", n > 0 ? sum_l / n : 0.0);
    cfg.Set("color_error.delta_c_mean", n > 0 ? sum_c / n : 0.0);
    cfg.Set("color_error.executed", true);
    cfg.AddCalibrationHistory("ColorError", "Color error calculation", true, 0.0,
                              "pairs=" + std::to_string(n) +
                              ",mean_de=" + std::to_string(result_.mean_deltaE00));
    cfg.Save();
}

void ColorErrorPanel::OnExport(wxCommandEvent& event) {
    std::string export_path = "config/color_error_result.csv";
    ColorErrorCalculator::exportResult(result_, export_path);
    UpdateStatus("Result exported to " + wxString(export_path));

    // Save export path to TOML
    auto& cfg = utils::SessionConfig::GetInstance();
    cfg.Set("color_error.csv_export_path", export_path);
    cfg.Save();
}

}
}