#pragma once

#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <wx/grid.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <vector>
#include "algo/dead_pixel.h"
#include "wxui/image_canvas.h"
#include "utils/logger.h"

namespace mvtk {
namespace wxui {

enum {
    DP_ID_LOAD_BTN = wxID_HIGHEST + 500,
    DP_ID_OPEN_CAMERA_BTN,
    DP_ID_CAPTURE_BTN,
    DP_ID_CLOSE_CAMERA_BTN,
    DP_ID_DETECT_BTN,
    DP_ID_CORRECT_BTN,
    DP_ID_EXPORT_BTN,
    DP_ID_IMPORT_BTN,
    DP_ID_CLEAR_BTN
};

class DeadPixelPanel : public wxPanel {
public:
    DeadPixelPanel(wxWindow* parent);
    ~DeadPixelPanel();

private:
    cv::Mat src_image_;
    cv::Mat corrected_image_;
    cv::Mat vis_image_;

    mvtk::DeadPixelDetectParams detect_params_;
    mvtk::DeadPixelCorrectParams correct_params_;
    std::vector<mvtk::DeadPixel> dead_pixels_;

    bool show_detected_ = true;
    int image_source_ = 0;
    std::string csv_path_ = "dead_pixels.csv";

    cv::VideoCapture camera_;
    bool camera_opened_ = false;

    wxComboBox* source_combo_;
    wxTextCtrl* path_ctrl_;
    wxButton* load_btn_;
    wxButton* open_camera_btn_;
    wxButton* capture_btn_;
    wxButton* close_camera_btn_;
    wxStaticText* status_text_;

    wxComboBox* dp_type_combo_;
    wxSlider* threshold_slider_;
    wxSlider* min_bright_slider_;
    wxSlider* max_bright_slider_;
    wxSlider* kernel_slider_;
    wxButton* detect_btn_;
    wxButton* multi_frame_btn_;

    wxCheckBox* show_detected_checkbox_;
    wxButton* export_btn_;
    wxButton* import_btn_;
    wxButton* clear_btn_;

    wxComboBox* correct_method_combo_;
    wxButton* correct_btn_;

    ImageCanvas* src_canvas_;
    ImageCanvas* result_canvas_;

    wxGrid* dead_pixel_grid_;

    void OnSourceChanged(wxCommandEvent& event);
    void OnLoadImage(wxCommandEvent& event);
    void OnOpenCamera(wxCommandEvent& event);
    void OnCapture(wxCommandEvent& event);
    void OnCloseCamera(wxCommandEvent& event);
    void OnDetect(wxCommandEvent& event);
    void OnCorrect(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnImport(wxCommandEvent& event);
    void OnClear(wxCommandEvent& event);

    void UpdateStatus(const wxString& msg, bool is_error = false);
    void UpdateUI();
    void UpdateGrid();

    DECLARE_EVENT_TABLE()
};

}
}