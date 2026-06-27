#pragma once

#include <wx/wx.h>

namespace Style {

const wxColour NEU_BG_COLOR = wxColour(224, 229, 236);
const wxColour NEU_LIGHT_SHADOW = wxColour(255, 255, 255);
const wxColour NEU_DARK_SHADOW = wxColour(163, 177, 198);
const wxColour NEU_TEXT_COLOR = wxColour(60, 66, 82);
const wxColour NEU_TEXT_LIGHT = wxColour(100, 106, 122);

const int NEU_CORNER_RADIUS = 12;
const int NEU_SHADOW_OFFSET = 5;

const int SPACING_SMALL = 5;
const int SPACING_MEDIUM = 10;
const int SPACING_LARGE = 15;
const int SPACING_XLARGE = 20;

inline wxFont GetSansFont(int size = 10, bool bold = false) {
    return wxFont(size, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                  bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL, false,
                  "Segoe UI");
}

inline void ApplyNeumorphicStyle(wxWindow* win) {
    win->SetFont(GetSansFont());
    win->SetForegroundColour(NEU_TEXT_COLOR);
}

inline void ApplyNeumorphicStyle(wxStaticText* text, bool bold = false) {
    text->SetFont(GetSansFont(10, bold));
    text->SetForegroundColour(NEU_TEXT_COLOR);
}

inline void ApplyNeumorphicStyle(wxButton* btn) {
    btn->SetFont(GetSansFont(10, true));
    btn->SetBackgroundColour(NEU_BG_COLOR);
    btn->SetForegroundColour(NEU_TEXT_COLOR);
}

inline void ApplyNeumorphicStyle(wxComboBox* combo) {
    combo->SetFont(GetSansFont());
    combo->SetBackgroundColour(wxColour(255, 255, 255));
    combo->SetForegroundColour(NEU_TEXT_COLOR);
}

inline void ApplyNeumorphicStyle(wxTextCtrl* ctrl) {
    ctrl->SetFont(GetSansFont());
    ctrl->SetBackgroundColour(wxColour(255, 255, 255));
    ctrl->SetForegroundColour(NEU_TEXT_COLOR);
}

inline void ApplyNeumorphicStyle(wxSlider* slider) {
    slider->SetForegroundColour(NEU_TEXT_COLOR);
}

}
