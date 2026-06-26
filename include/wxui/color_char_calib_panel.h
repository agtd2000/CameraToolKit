#pragma once

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/grid.h>
#include <wx/statline.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <vector>
#include "algo/white_balance.h"
#include "algo/ccm.h"
#include "algo/roi.h"
#include "wxui/image_canvas.h"
#include "utils/logger.h"

namespace mvtk {
namespace wxui {

enum {
    CC_ID_STEP_BTN_0 = wxID_HIGHEST + 200,
    CC_ID_STEP_BTN_1,
    CC_ID_STEP_BTN_2,
    CC_ID_STEP_BTN_3,
    CC_ID_LOAD_IMG_BTN,
    CC_ID_OPEN_CAMERA_BTN,
    CC_ID_CAPTURE_BTN,
    CC_ID_CLOSE_CAMERA_BTN,
    CC_ID_REMOVE_BTN,
    CC_ID_CALIBRATE_BTN,
    CC_ID_EXPORT_BTN,
    CC_ID_BACK_BTN,
    CC_ID_NEXT_BTN,
    CC_ID_RESET_BTN
};

struct LightSourceUIData {
    std::string name;
    double color_temp = 5000.0;
    cv::Mat image;
    std::vector<mvtk::ROIInfo> rois;
    double brightness = 0.0;
};

class ColorCharCalibPanel : public wxPanel {
public:
    ColorCharCalibPanel(wxWindow* parent);
    ~ColorCharCalibPanel();

private:
    int step_ = 0;
    int current_light_idx_ = -1;

    std::vector<LightSourceUIData> light_sources_;
    wxTextCtrl* new_light_name_ctrl_;
    wxSlider* new_light_temp_slider_;

    mvtk::WBMethod wb_method_ = mvtk::WBMethod::AVERAGE;
    mvtk::CCMMethod ccm_method_ = mvtk::CCMMethod::OPTIMAL;
    int standard_color_ = 0;

    bool quantize_ = false;
    int wb_quant_ = 1024;
    int ccm_quant_ = 1024;

    std::vector<mvtk::WBCorrectParams> wb_results_;
    std::vector<mvtk::CCMCorrectParams> ccm_results_;

    int image_source_ = 0;

    cv::VideoCapture camera_;
    bool camera_opened_ = false;

    wxButton* step_btns_[4];
    wxNotebook* notebook_;

    wxListBox* light_list_;
    wxStaticText* light_info_text_;

    wxComboBox* image_source_combo_;
    wxTextCtrl* img_path_ctrl_;
    wxButton* load_img_btn_;
    wxButton* open_camera_btn_;
    wxButton* capture_btn_;
    wxButton* close_camera_btn_;
    wxButton* remove_btn_;

    wxComboBox* wb_method_combo_;
    wxComboBox* ccm_method_combo_;
    wxComboBox* color_std_combo_;
    wxCheckBox* quantize_checkbox_;
    wxTextCtrl* wb_quant_ctrl_;
    wxTextCtrl* ccm_quant_ctrl_;
    wxButton* calibrate_btn_;

    wxStaticText* result_text_;

    wxButton* export_btn_;

    wxButton* back_btn_;
    wxButton* next_btn_;
    wxButton* reset_btn_;

    wxStaticText* status_text_;

    ImageCanvas* image_canvas_;

    void UpdateStepButtons();
    void ShowStep(int step);

    void OnStepButton(wxCommandEvent& event);
    void OnLightSelected(wxCommandEvent& event);

    void OnImageSourceChanged(wxCommandEvent& event);
    void OnLoadImage(wxCommandEvent& event);
    void OnOpenCamera(wxCommandEvent& event);
    void OnCapture(wxCommandEvent& event);
    void OnCloseCamera(wxCommandEvent& event);
    void OnRemove(wxCommandEvent& event);

    void OnCalibrate(wxCommandEvent& event);

    void OnExport(wxCommandEvent& event);

    void OnBack(wxCommandEvent& event);
    void OnNext(wxCommandEvent& event);
    void OnReset(wxCommandEvent& event);

    void UpdateStatus(const wxString& msg, bool is_error = false);
    void UpdateLightList();
    void UpdateLightInfo();

    DECLARE_EVENT_TABLE()
};

}
}