#pragma once

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/statline.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <vector>
#include "algo/spectral.h"
#include "wxui/image_canvas.h"
#include "utils/logger.h"

namespace mvtk {
namespace wxui {

enum {
    SP_ID_STEP_BTN_0 = wxID_HIGHEST + 100,
    SP_ID_STEP_BTN_1,
    SP_ID_STEP_BTN_2,
    SP_ID_STEP_BTN_3,
    SP_ID_IMPORT_FACTORY_BTN,
    SP_ID_ADD_LIGHT_BTN,
    SP_ID_COMPUTE_REGION_BTN,
    SP_ID_OPEN_CAMERA_BTN,
    SP_ID_CAPTURE_IMAGE_BTN,
    SP_ID_CLOSE_CAMERA_BTN,
    SP_ID_LOAD_CURVE_BTN,
    SP_ID_PREVIEW_BTN,
    SP_ID_EXPORT_BTN,
    SP_ID_BACK_BTN,
    SP_ID_NEXT_BTN,
    SP_ID_RESET_BTN
};

class SpectralCalibPanel : public wxPanel {
public:
    SpectralCalibPanel(wxWindow* parent);
    ~SpectralCalibPanel();

private:
    int step_ = 0;
    int calib_mode_ = 0;
    int complexity_ = 0;
    int image_source_ = 0;

    std::string factory_file_path_;
    std::vector<mvtk::LightSourceInfo> light_sources_;
    std::vector<mvtk::LightSourceInfo> factory_sources_;

    wxTextCtrl* new_light_name_ctrl_;
    wxSlider* new_light_temp_slider_;

    double focal_length_ = 25.0;
    double working_distance_ = 500.0;
    double sensor_width_ = 5.7;
    double sensor_height_ = 4.3;
    mvtk::CalibRegionResult calib_region_;

    cv::Mat spectral_preview_;

    cv::VideoCapture camera_;
    bool camera_opened_ = false;
    cv::Mat captured_image_;

    wxButton* step_btns_[4];

    wxComboBox* calib_mode_combo_;
    wxComboBox* complexity_combo_;

    wxTextCtrl* factory_path_ctrl_;
    wxButton* import_factory_btn_;

    wxListBox* light_list_;

    wxButton* add_light_btn_;

    wxSlider* focal_slider_;
    wxSlider* distance_slider_;
    wxSlider* sensor_w_slider_;
    wxSlider* sensor_h_slider_;
    wxButton* compute_region_btn_;

    wxComboBox* data_source_combo_;
    wxButton* open_camera_btn_;
    wxButton* capture_image_btn_;
    wxButton* close_camera_btn_;

    wxTextCtrl* curve_path_ctrl_;
    wxButton* load_curve_btn_;

    wxButton* preview_btn_;

    wxButton* export_btn_;

    wxButton* back_btn_;
    wxButton* next_btn_;
    wxButton* reset_btn_;

    wxStaticText* status_text_;

    ImageCanvas* image_canvas_;

    void UpdateStepButtons();

    void OnStepButton(wxCommandEvent& event);

    void OnCalibModeChanged(wxCommandEvent& event);
    void OnComplexityChanged(wxCommandEvent& event);

    void OnImportFactory(wxCommandEvent& event);

    void OnAddLight(wxCommandEvent& event);

    void OnComputeRegion(wxCommandEvent& event);

    void OnDataSourceChanged(wxCommandEvent& event);
    void OnOpenCamera(wxCommandEvent& event);
    void OnCaptureImage(wxCommandEvent& event);
    void OnCloseCamera(wxCommandEvent& event);

    void OnLoadCurve(wxCommandEvent& event);

    void OnPreview(wxCommandEvent& event);

    void OnExport(wxCommandEvent& event);

    void OnBack(wxCommandEvent& event);
    void OnNext(wxCommandEvent& event);
    void OnReset(wxCommandEvent& event);

    void UpdateStatus(const wxString& msg, bool is_error = false);
    void UpdateLightList();

    DECLARE_EVENT_TABLE()
};

}
}