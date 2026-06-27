#include "neumorphic_panel.h"

NeumorphicPanel::NeumorphicPanel(wxWindow* parent, wxWindowID id,
                                   const wxPoint& pos, const wxSize& size,
                                   long style, const wxString& name)
    : wxPanel(parent, id, pos, size, style | wxFULL_REPAINT_ON_RESIZE, name) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &NeumorphicPanel::OnPaint, this);
    Bind(wxEVT_SIZE, &NeumorphicPanel::OnSize, this);
}

void NeumorphicPanel::SetCornerRadius(int radius) {
    corner_radius_ = radius;
    Refresh();
}

void NeumorphicPanel::SetShadowOffset(int offset) {
    shadow_offset_ = offset;
    Refresh();
}

void NeumorphicPanel::SetColors(const wxColour& bg_color, const wxColour& light_shadow, const wxColour& dark_shadow) {
    bg_color_ = bg_color;
    light_shadow_ = light_shadow;
    dark_shadow_ = dark_shadow;
    Refresh();
}

void NeumorphicPanel::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (!gc) return;

    wxSize size = GetClientSize();
    if (size.GetWidth() <= 0 || size.GetHeight() <= 0) {
        delete gc;
        return;
    }

    int r = corner_radius_;
    int offset = shadow_offset_;

    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

    wxGraphicsPath dark_path = gc->CreatePath();
    dark_path.AddRoundedRectangle(offset, offset, size.GetWidth() - offset, size.GetHeight() - offset, r);
    gc->SetPen(wxPen(dark_shadow_, 0));
    gc->SetBrush(wxBrush(dark_shadow_));
    gc->FillPath(dark_path);

    wxGraphicsPath light_path = gc->CreatePath();
    light_path.AddRoundedRectangle(-offset, -offset, size.GetWidth() - offset, size.GetHeight() - offset, r);
    gc->SetPen(wxPen(light_shadow_, 0));
    gc->SetBrush(wxBrush(light_shadow_));
    gc->FillPath(light_path);

    wxGraphicsPath bg_path = gc->CreatePath();
    bg_path.AddRoundedRectangle(0, 0, size.GetWidth(), size.GetHeight(), r);
    gc->SetPen(wxPen(bg_color_, 0));
    gc->SetBrush(wxBrush(bg_color_));
    gc->FillPath(bg_path);

    delete gc;
}

void NeumorphicPanel::OnSize(wxSizeEvent& event) {
    Refresh();
    event.Skip();
}
