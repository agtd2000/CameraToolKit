#include "wxui/main_app.h"
#include "wxui/main_frame.h"
#include "utils/session_config.h"
#include "utils/logger.h"
#include <wx/filename.h>
#include <wx/filefn.h>

wxIMPLEMENT_APP(CameraToolKitApp);

bool CameraToolKitApp::OnInit() {
    if (!wxApp::OnInit()) return false;

    // Resolve config/pipeline_session.toml in the current working directory.
    // Ensure the config/ subfolder exists so Save() can write into it.
    wxString cwd = wxGetCwd();
    wxString config_dir = cwd + wxFILE_SEP_PATH + "config";
    if (!wxDirExists(config_dir)) {
        wxMkdir(config_dir);
    }
    wxString config_path = config_dir + wxFILE_SEP_PATH + "pipeline_session.toml";
    MV_LOG_INFO("CameraToolKitApp: config file path: " + std::string(config_path.ToUTF8()));
    mvtk::utils::SessionConfig::GetInstance().Load(std::string(config_path.ToUTF8()));

    mvtk::wxui::MainFrame* frame = new mvtk::wxui::MainFrame("CameraToolKit - Image Calibration Tool", wxPoint(50, 50), wxSize(1280, 720));
    frame->Show(true);
    SetTopWindow(frame);

    return true;
}
