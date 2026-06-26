#pragma once

#include <wx/wx.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include "algo/flat_field.h"
#include "wxui/image_canvas.h"
#include "utils/logger.h"

namespace mvtk {
namespace wxui {

enum {
    FF_ID_LOAD_FLAT_BTN = wxID_HIGHEST + 400,
    FF_ID_OPEN_FLAT_CAMERA_BTN,
    FF_ID_CAPTURE_FLAT_BTN,
    FF_ID_CLOSE_FLAT_CAMERA_BTN,
    FF_ID_LOAD_DARK_BTN,
    FF_ID_OPEN_DARK_CAMERA_BTN,
    FF_ID_CAPTURE_DARK_BTN,
    FF_ID_CALIBRATE_BTN,
    FF_ID_APPLY_BTN,
    FF_ID_EXPORT_GAIN_BTN,
    FF_ID_IMPORT_GAIN_BTN
};

class FlatFieldPanel : public wxPanel {
public:
    FlatFieldPanel(wxWindow* parent);
    ~FlatFieldPanel();

private:
    cv::Mat src_image_;
    cv::Mat dark_image_;
    cv::Mat corrected_image_;

    mvtk::FlatFieldCalibParams calib_params_;
    mvtk::FlatFieldResult calib_result_;

    bool use_dark_ = false;

    int flat_source_ = 0;
    int dark_source_ = 0;

    cv::VideoCapture camera_;
    bool camera_opened_ = false;

    wxComboBox* flat_source_combo_;
    wxTextCtrl* flat_path_ctrl_;
    wxButton* load_flat_btn_;
    wxButton* open_flat_camera_btn_;
    wxButton* capture_flat_btn_;
    wxButton* close_flat_camera_btn_;

    wxCheckBox* use_dark_checkbox_;
    wxComboBox* dark_source_combo_;
    wxTextCtrl* dark_path_ctrl_;
    wxButton* load_dark_btn_;
    wxButton* open_dark_camera_btn_;
    wxButton* capture_dark_btn_;

    wxStaticText* status_text_;

    wxComboBox* calib_mode_combo_;
    wxSlider* edge_fill_slider_;
    wxSlider* smooth_kernel_slider_;
    wxSlider* target_bright_slider_;
    wxComboBox* channel_mode_combo_;
    wxButton* calibrate_btn_;

    wxStaticText* result_text_;

    wxButton* apply_btn_;
    wxButton* export_gain_btn_;
    wxButton* import_gain_btn_;

    ImageCanvas* flat_canvas_;
    ImageCanvas* dark_canvas_;
    ImageCanvas* result_canvas_;

    void OnFlatSourceChanged(wxCommandEvent& event);
    void OnLoadFlat(wxCommandEvent& event);
    void OnOpenFlatCamera(wxCommandEvent& event);
    void OnCaptureFlat(wxCommandEvent& event);
    void OnCloseFlatCamera(wxCommandEvent& event);

    void OnUseDarkChanged(wxCommandEvent& event);
    void OnDarkSourceChanged(wxCommandEvent& event);
    void OnLoadDark(wxCommandEvent& event);
    void OnOpenDarkCamera(wxCommandEvent& event);
    void OnCaptureDark(wxCommandEvent& event);

    void OnCalibrate(wxCommandEvent& event);
    void OnApply(wxCommandEvent& event);
    void OnExportGain(wxCommandEvent& event);
    void OnImportGain(wxCommandEvent& event);

    void UpdateStatus(const wxString& msg, bool is_error = false);
    void UpdateUI();

    DECLARE_EVENT_TABLE()
};

}
}