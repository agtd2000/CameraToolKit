#pragma once

#include <wx/wx.h>
#include <opencv2/core.hpp>
#include <map>
#include <vector>

#include "algo/scm.h"

namespace mvtk {
namespace wxui {

enum SCMPanelIDs {
    SCM_ID_LOAD_DARK_BTN = wxID_HIGHEST + 1000,
    SCM_ID_LOAD_BRIGHT_BTN,
    SCM_ID_CLEAR_DATA_BTN,
    SCM_ID_EVALUATE_BTN,
    SCM_ID_EXPORT_REPORT_BTN,
    SCM_ID_EXPORT_RESULT_BTN
};

class SCMPanel : public wxPanel {
public:
    SCMPanel(wxWindow* parent);

private:
    void OnLoadDark(wxCommandEvent& event);
    void OnLoadBright(wxCommandEvent& event);
    void OnClearData(wxCommandEvent& event);
    void OnEvaluate(wxCommandEvent& event);
    void OnExportReport(wxCommandEvent& event);
    void OnExportResult(wxCommandEvent& event);

    void UpdateStatus();

    SCMParams params_;
    SCMResult current_result_;

    std::map<double, std::vector<cv::Mat>> dark_images_;
    std::map<double, std::vector<cv::Mat>> bright_images_;

    wxButton* load_dark_btn_;
    wxButton* load_bright_btn_;
    wxButton* clear_data_btn_;
    wxButton* evaluate_btn_;
    wxButton* export_report_btn_;
    wxButton* export_result_btn_;

    wxTextCtrl* dark_path_ctrl_;
    wxTextCtrl* bright_path_ctrl_;
    wxTextCtrl* dark_exp_ctrl_;
    wxTextCtrl* bright_exp_ctrl_;
    wxTextCtrl* sensor_name_ctrl_;
    wxComboBox* bits_ctrl_;
    wxTextCtrl* temp_ctrl_;
    wxTextCtrl* frames_ctrl_;
    wxTextCtrl* roi_ctrl_;
    wxTextCtrl* result_text_;
    // Sensor info controls (manufacturer / model / SN / bayer / resolution)
    wxTextCtrl* manufacturer_ctrl_;
    wxTextCtrl* model_ctrl_;
    wxTextCtrl* sn_ctrl_;
    wxComboBox* bayer_ctrl_;
    wxTextCtrl* res_w_ctrl_;
    wxTextCtrl* res_h_ctrl_;

    wxStaticText* dark_status_;
    wxStaticText* bright_status_;

    DECLARE_EVENT_TABLE()
};

}
}