#pragma once

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/grid.h>
#include <wx/statline.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <vector>
#include "algo/quick_color_calib.h"
#include "algo/roi.h"
#include "wxui/image_canvas.h"
#include "utils/logger.h"

namespace mvtk {
namespace wxui {

enum {
    ID_STEP_BTN_0 = wxID_HIGHEST + 1,
    ID_STEP_BTN_1,
    ID_STEP_BTN_2,
    ID_STEP_BTN_3,
    ID_CALIB_MODE_COMBO,
    ID_SRC_SOURCE_COMBO,
    ID_SRC_LOAD_BTN,
    ID_SRC_OPEN_CAMERA_BTN,
    ID_SRC_CAPTURE_BTN,
    ID_SRC_CLOSE_CAMERA_BTN,
    ID_AUTO_EXPOSURE_BTN,
    ID_ROI_MODE_COMBO,
    ID_GENERATE_ROI_BTN,
    ID_DETECT_ROI_BTN,
    ID_DETECT_REF_ROI_BTN,
    ID_REF_SOURCE_COMBO,
    ID_REF_LOAD_BTN,
    ID_REF_OPEN_CAMERA_BTN,
    ID_REF_CAPTURE_BTN,
    ID_REF_CLOSE_CAMERA_BTN,
    ID_CALIBRATE_BTN,
    ID_BACK_BTN,
    ID_NEXT_BTN,
    ID_RESET_BTN
};

class QuickColorCalibPanel : public wxPanel {
public:
    QuickColorCalibPanel(wxWindow* parent);
    ~QuickColorCalibPanel();

private:
    cv::Mat src_image_;
    cv::Mat ref_image_;

    int calib_mode_ = 0;
    int step_ = 0;

    mvtk::WBMethod wb_method_ = mvtk::WBMethod::AVERAGE;
    mvtk::CCMMethod ccm_method_ = mvtk::CCMMethod::OPTIMAL;
    mvtk::RGBStatMethod stat_method_ = mvtk::RGBStatMethod::MEDIAN;

    std::vector<mvtk::ROIInfo> rois_;
    std::vector<mvtk::ROIInfo> ref_rois_;
    mvtk::QuickCalibResult calib_result_;
    double current_brightness_ = 0.0;

    int chart_x_ = 100, chart_y_ = 50;
    int chart_w_ = 400, chart_h_ = 600;
    bool auto_roi_ = true;
    
    double brightness_min_ = 0.55;
    double brightness_max_ = 0.65;

    int src_source_ = 0;
    int ref_source_ = 0;

    cv::VideoCapture camera_;
    bool camera_opened_ = false;

    wxButton* step_btns_[4];
    wxComboBox* calib_mode_combo_;

    wxComboBox* src_source_combo_;
    wxTextCtrl* src_path_ctrl_;
    wxButton* load_src_btn_;
    wxButton* open_src_camera_btn_;
    wxButton* capture_src_btn_;
    wxButton* close_src_camera_btn_;

    wxSlider* exposure_slider_;
    wxSlider* gain_slider_;
    wxButton* auto_exposure_btn_;

    wxComboBox* roi_mode_combo_;
    wxTextCtrl* chart_x_ctrl_;
    wxTextCtrl* chart_y_ctrl_;
    wxTextCtrl* chart_w_ctrl_;
    wxTextCtrl* chart_h_ctrl_;
    wxTextCtrl* brightness_min_ctrl_;
    wxTextCtrl* brightness_max_ctrl_;
    wxButton* generate_roi_btn_;
    wxButton* detect_roi_btn_;
    wxButton* detect_ref_roi_btn_;
    wxStaticText* brightness_text_;
    wxStaticText* roi_count_text_;
    wxStaticText* ref_roi_count_text_;

    wxComboBox* ref_source_combo_;
    wxTextCtrl* ref_path_ctrl_;
    wxButton* load_ref_btn_;
    wxButton* open_ref_camera_btn_;
    wxButton* capture_ref_btn_;
    wxButton* close_ref_camera_btn_;

    wxComboBox* stat_method_combo_;
    wxComboBox* wb_method_combo_;
    wxComboBox* ccm_method_combo_;
    wxButton* calibrate_btn_;

    wxStaticText* wb_result_text_;
    wxStaticText* ccm_result_text_;
    wxStaticText* deltae_mean_text_;
    wxGrid* deltae_grid_;

    wxButton* back_btn_;
    wxButton* next_btn_;
    wxButton* reset_btn_;

    wxStaticText* status_text_;

    ImageCanvas* src_canvas_;
    ImageCanvas* roi_canvas_;
    ImageCanvas* ref_canvas_;
    wxNotebook* notebook_;

    void UpdateStepButtons();
    void ShowStep(int step);
    void UpdateCanvasImages();

    void OnStepButton(wxCommandEvent& event);
    void OnCalibModeChanged(wxCommandEvent& event);
    void OnSrcSourceChanged(wxCommandEvent& event);
    void OnLoadSrc(wxCommandEvent& event);
    void OnOpenSrcCamera(wxCommandEvent& event);
    void OnCaptureSrc(wxCommandEvent& event);
    void OnCloseSrcCamera(wxCommandEvent& event);
    void OnAutoExposure(wxCommandEvent& event);

    void OnRoiModeChanged(wxCommandEvent& event);
    void OnGenerateROI(wxCommandEvent& event);
    void OnDetectROI(wxCommandEvent& event);
    void OnDetectRefROI(wxCommandEvent& event);

    void OnRefSourceChanged(wxCommandEvent& event);
    void OnLoadRef(wxCommandEvent& event);
    void OnOpenRefCamera(wxCommandEvent& event);
    void OnCaptureRef(wxCommandEvent& event);
    void OnCloseRefCamera(wxCommandEvent& event);

    void OnCalibrate(wxCommandEvent& event);
    void OnBack(wxCommandEvent& event);
    void OnNext(wxCommandEvent& event);
    void OnReset(wxCommandEvent& event);

    void UpdateStatus(const wxString& msg, bool is_error = false);

    DECLARE_EVENT_TABLE()
};

}
}