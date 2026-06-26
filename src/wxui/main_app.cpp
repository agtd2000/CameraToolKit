#include "wxui/main_app.h"
#include "wxui/main_frame.h"

wxIMPLEMENT_APP(CameraToolKitApp);

bool CameraToolKitApp::OnInit() {
    if (!wxApp::OnInit()) return false;

    mvtk::wxui::MainFrame* frame = new mvtk::wxui::MainFrame("CameraToolKit - Image Calibration Tool", wxPoint(50, 50), wxSize(1280, 720));
    frame->Show(true);
    SetTopWindow(frame);

    return true;
}
