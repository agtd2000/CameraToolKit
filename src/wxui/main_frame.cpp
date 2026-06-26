#include "wxui/main_frame.h"
#include "wxui/dead_pixel_panel.h"
#include "wxui/flat_field_panel.h"
#include "wxui/quick_color_calib_panel.h"
#include "wxui/color_error_panel.h"
#include "wxui/color_char_calib_panel.h"
#include "wxui/spectral_calib_panel.h"
#include "utils/logger.h"
#include "utils/tone_mapping.h"

namespace mvtk {
namespace wxui {

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(wxID_EXIT, MainFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)
    EVT_CLOSE(MainFrame::OnClose)
END_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame(nullptr, wxID_ANY, title, pos, size),
      log_text_ctrl_(nullptr), tone_mapping_combo_(nullptr),
      lsc_checkbox_(nullptr), denoise_checkbox_(nullptr),
      interpolation_checkbox_(nullptr), wb_checkbox_(nullptr),
      ccm_checkbox_(nullptr), gamma_checkbox_(nullptr) {

    aui_manager_.SetManagedWindow(this);

    MV_LOG_OPERATION("MainFrame initialization", "Starting CameraToolKit application");

    wxIcon icon(wxT("logo.ico"), wxBITMAP_TYPE_ICO);
    if (icon.IsOk()) {
        SetIcon(icon);
    }

    wxMenu* fileMenu = new wxMenu;
    fileMenu->Append(wxID_EXIT, "E&xit\tAlt-X", "Exit this program");

    wxMenu* viewMenu = new wxMenu;
    viewMenu->Append(wxID_ANY, "Reset Layout", "Reset window layout", wxITEM_NORMAL);

    wxMenu* helpMenu = new wxMenu;
    helpMenu->Append(wxID_ABOUT, "&About\tF1", "Show about dialog");

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(viewMenu, "&View");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);

    CreateStatusBar();
    SetStatusText("Ready");

    notebook_ = new wxNotebook(this, wxID_ANY);

    notebook_->AddPage(new DeadPixelPanel(notebook_), "Dead Pixel");
    notebook_->AddPage(new FlatFieldPanel(notebook_), "Flat Field");
    notebook_->AddPage(new QuickColorCalibPanel(notebook_), "Quick Calib");
    notebook_->AddPage(new ColorErrorPanel(notebook_), "Color Error");
    notebook_->AddPage(new ColorCharCalibPanel(notebook_), "Color Char");
    notebook_->AddPage(new SpectralCalibPanel(notebook_), "Spectral");

    wxPanel* left_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* left_sizer = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* tone_title = new wxStaticText(left_panel, wxID_ANY, "Tone Mapping");
    left_sizer->Add(tone_title, 0, wxALL, 5);
    
    tone_mapping_combo_ = new wxComboBox(left_panel, wxID_ANY, "Percentile", wxDefaultPosition, wxDefaultSize,
                                         {"Linear", "Percentile", "Histogram Equalization", "Gamma"},
                                         wxCB_READONLY);
    left_sizer->Add(tone_mapping_combo_, 0, wxEXPAND | wxALL, 5);
    tone_mapping_combo_->Bind(wxEVT_COMBOBOX, &MainFrame::OnToneMappingChanged, this);
    
    wxStaticLine* line1 = new wxStaticLine(left_panel, wxID_ANY);
    left_sizer->Add(line1, 0, wxEXPAND | wxALL, 5);
    
    wxStaticText* pipeline_title = new wxStaticText(left_panel, wxID_ANY, "Correction Pipeline");
    left_sizer->Add(pipeline_title, 0, wxALL, 5);
    
    lsc_checkbox_ = new wxCheckBox(left_panel, wxID_ANY, "LSC");
    lsc_checkbox_->SetValue(true);
    left_sizer->Add(lsc_checkbox_, 0, wxALL, 2);
    
    denoise_checkbox_ = new wxCheckBox(left_panel, wxID_ANY, "Denoise");
    denoise_checkbox_->SetValue(true);
    left_sizer->Add(denoise_checkbox_, 0, wxALL, 2);
    
    interpolation_checkbox_ = new wxCheckBox(left_panel, wxID_ANY, "Interpolation");
    interpolation_checkbox_->SetValue(true);
    left_sizer->Add(interpolation_checkbox_, 0, wxALL, 2);
    
    wb_checkbox_ = new wxCheckBox(left_panel, wxID_ANY, "White Balance");
    wb_checkbox_->SetValue(true);
    left_sizer->Add(wb_checkbox_, 0, wxALL, 2);
    
    ccm_checkbox_ = new wxCheckBox(left_panel, wxID_ANY, "CCM");
    ccm_checkbox_->SetValue(true);
    left_sizer->Add(ccm_checkbox_, 0, wxALL, 2);
    
    gamma_checkbox_ = new wxCheckBox(left_panel, wxID_ANY, "Gamma");
    gamma_checkbox_->SetValue(true);
    left_sizer->Add(gamma_checkbox_, 0, wxALL, 2);
    
    left_sizer->AddStretchSpacer();
    left_panel->SetSizer(left_sizer);

    wxPanel* right_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* right_sizer = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* right_title = new wxStaticText(right_panel, wxID_ANY, "Log");
    right_sizer->Add(right_title, 0, wxALL, 5);
    
    log_text_ctrl_ = new wxTextCtrl(right_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                    wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH);
    right_sizer->Add(log_text_ctrl_, 1, wxEXPAND | wxALL, 5);
    right_panel->SetSizer(right_sizer);

    utils::Logger::GetInstance().SetLogCallback(
        [this](mvtk::utils::LogLevel level, const std::string& message) {
            this->OnLogUpdate(level, message);
        }
    );

    wxAuiPaneInfo center_pane;
    center_pane.Center().PaneBorder(false).CloseButton(false).MaximizeButton(false);
    aui_manager_.AddPane(notebook_, center_pane);

    wxAuiPaneInfo left_pane;
    left_pane.Left().Floatable(true).Dockable(true).Resizable(true).
        MinSize(wxSize(180, -1)).MaxSize(wxSize(300, -1)).Caption("Tools").CaptionVisible(true);
    aui_manager_.AddPane(left_panel, left_pane);

    wxAuiPaneInfo right_pane;
    right_pane.Right().Floatable(true).Dockable(true).Resizable(true).
        MinSize(wxSize(250, -1)).MaxSize(wxSize(500, -1)).Caption("Log").CaptionVisible(true);
    aui_manager_.AddPane(right_panel, right_pane);

    aui_manager_.Update();

    SetSizeHints(1000, 700);
    Maximize();

    MV_LOG_INFO("MainFrame initialized successfully");
}

MainFrame::~MainFrame() {
    aui_manager_.UnInit();
}

void MainFrame::OnExit(wxCommandEvent& WXUNUSED(event)) {
    MV_LOG_OPERATION("Exit", "User requested application exit");
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& WXUNUSED(event)) {
    wxMessageBox("CameraToolKit v1.0.0\n\nImage Calibration Tool for Industrial Cameras\n\n"
                 "Features:\n"
                 "- 12-bit/16-bit High Bit Depth Support\n"
                 "- Multi-threaded Processing\n"
                 "- Memory Pool Management\n"
                 "- Dockable Panel Layout\n"
                 "- Correction Pipeline (LSC→Denoise→WB→CCM→Gamma)",
                 "About CameraToolKit", wxOK | wxICON_INFORMATION);
}

void MainFrame::OnClose(wxCloseEvent& event) {
    MV_LOG_OPERATION("Close", "Application closing");
    event.Skip();
}

void MainFrame::OnToneMappingChanged(wxCommandEvent& event) {
    wxString selection = event.GetString();
    MV_LOG_OPERATION("Tone Mapping Changed", selection.ToStdString());
}

void MainFrame::OnPipelineNodeChanged(wxCommandEvent& event) {
    wxCheckBox* cb = dynamic_cast<wxCheckBox*>(event.GetEventObject());
    if (cb) {
        std::string name = cb->GetLabel().ToStdString();
        bool enabled = cb->GetValue();
        MV_LOG_OPERATION("Pipeline Node Changed", name + " = " + (enabled ? "Enabled" : "Disabled"));
    }
}

void MainFrame::OnLogUpdate(mvtk::utils::LogLevel level, const std::string& message) {
    if (log_text_ctrl_) {
        wxString wx_msg = wxString::FromUTF8(message.c_str());
        log_text_ctrl_->AppendText(wx_msg);
        
        int lines = log_text_ctrl_->GetNumberOfLines();
        if (lines > 500) {
            log_text_ctrl_->Remove(0, log_text_ctrl_->GetLineLength(0) + 1);
        }
    }
}

}
}
