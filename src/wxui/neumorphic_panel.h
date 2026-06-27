#pragma once

#include <wx/wx.h>
#include <wx/graphics.h>

class NeumorphicPanel : public wxPanel {
public:
    NeumorphicPanel(wxWindow* parent, wxWindowID id = wxID_ANY,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxTAB_TRAVERSAL,
                    const wxString& name = wxPanelNameStr);

    void SetCornerRadius(int radius);
    void SetShadowOffset(int offset);
    void SetColors(const wxColour& bg_color, const wxColour& light_shadow, const wxColour& dark_shadow);

protected:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

private:
    int corner_radius_ = 0;
    int shadow_offset_ = 5;
    wxColour bg_color_ = wxColour(224, 229, 236);
    wxColour light_shadow_ = wxColour(255, 255, 255);
    wxColour dark_shadow_ = wxColour(163, 177, 198);
};
