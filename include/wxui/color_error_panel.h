#pragma once

#include <wx/wx.h>
#include <wx/grid.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <vector>
#include "algo/color_error.h"
#include "algo/roi.h"
#include "wxui/image_canvas.h"
#include "utils/logger.h"

namespace mvtk {
namespace wxui {

enum {
    CE_ID_SRC_LOAD_BTN = wxID_HIGHEST + 300,
    CE_ID_SRC_OPEN_CAMERA_BTN,
    CE_ID_SRC_CAPTURE_BTN,
    CE_ID_SRC_CLOSE_CAMERA_BTN,
    CE_ID_REF_LOAD_BTN,
    CE_ID_REF_OPEN_CAMERA_BTN,
    CE_ID_REF_CAPTURE_BTN,
    CE_ID_DRAW_24_ROI_BTN,
    CE_ID_DRAW_140_ROI_BTN,
    CE_ID_CLEAR_ROI_BTN,
    CE_ID_ADD_ROW_BTN,
    CE_ID_IMPORT_COLORS_BTN,
    CE_ID_CALCULATE_BTN,
    CE_ID_EXPORT_BTN
};

class ColorErrorPanel : public wxPanel {
public:
    ColorErrorPanel(wxWindow* parent);
    ~ColorErrorPanel();

private:
    cv::Mat src_image_;
    cv::Mat ref_image_;

    mvtk::ColorErrorSettings params_;
    mvtk::ColorErrorResult result_;
    bool has_result_ = false;

    std::vector<mvtk::ROIInfo> rois_;
    std::vector<cv::Vec3d> src_colors_;
    std::vector<cv::Vec3d> ref_colors_;

    int src_source_ = 0;
    int ref_source_ = 0;

    cv::VideoCapture camera_;
    bool camera_opened_ = false;

    wxComboBox* src_source_combo_;
    wxTextCtrl* src_path_ctrl_;
    wxButton* load_src_btn_;
    wxButton* open_src_camera_btn_;
    wxButton* capture_src_btn_;
    wxButton* close_src_camera_btn_;

    wxComboBox* ref_source_combo_;
    wxTextCtrl* ref_path_ctrl_;
    wxButton* load_ref_btn_;
    wxButton* open_ref_camera_btn_;
    wxButton* capture_ref_btn_;

    wxStaticText* status_text_;

    ImageCanvas* src_canvas_;
    ImageCanvas* ref_canvas_;

    wxButton* draw_24_btn_;
    wxButton* draw_140_btn_;
    wxButton* clear_roi_btn_;
    wxStaticText* roi_count_text_;

    wxButton* add_row_btn_;
    wxButton* import_colors_btn_;

    wxGrid* color_grid_;

    wxCheckBox* srgb_checkbox_;
    wxButton* calculate_btn_;

    wxStaticText* result_text_;
    wxGrid* error_grid_;
    wxButton* export_btn_;

    void OnSrcSourceChanged(wxCommandEvent& event);
    void OnLoadSrc(wxCommandEvent& event);
    void OnOpenSrcCamera(wxCommandEvent& event);
    void OnCaptureSrc(wxCommandEvent& event);
    void OnCloseSrcCamera(wxCommandEvent& event);

    void OnRefSourceChanged(wxCommandEvent& event);
    void OnLoadRef(wxCommandEvent& event);
    void OnOpenRefCamera(wxCommandEvent& event);
    void OnCaptureRef(wxCommandEvent& event);

    void OnDraw24ROI(wxCommandEvent& event);
    void OnDraw140ROI(wxCommandEvent& event);
    void OnClearROI(wxCommandEvent& event);

    void OnAddRow(wxCommandEvent& event);
    void OnImportColors(wxCommandEvent& event);

    void OnCalculate(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);

    void UpdateStatus(const wxString& msg, bool is_error = false);
    void UpdateGrid();

    DECLARE_EVENT_TABLE()
};

}
}