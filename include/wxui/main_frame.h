#pragma once

#include <wx/wx.h>
#include <wx/aui/aui.h>
#include <wx/notebook.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include "utils/logger.h"

namespace mvtk {
namespace wxui {

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
    ~MainFrame();

private:
    wxAuiManager aui_manager_;
    wxNotebook* notebook_;
    wxTextCtrl* log_text_ctrl_;
    
    wxComboBox* tone_mapping_combo_;
    wxCheckBox* lsc_checkbox_;
    wxCheckBox* denoise_checkbox_;
    wxCheckBox* interpolation_checkbox_;
    wxCheckBox* wb_checkbox_;
    wxCheckBox* ccm_checkbox_;
    wxCheckBox* gamma_checkbox_;

    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    
    void OnToneMappingChanged(wxCommandEvent& event);
    void OnPipelineNodeChanged(wxCommandEvent& event);
    void OnLogUpdate(mvtk::utils::LogLevel level, const std::string& message);

    DECLARE_EVENT_TABLE()
};

}
}
