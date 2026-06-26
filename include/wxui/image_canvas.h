#pragma once

#include <wx/panel.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <opencv2/core.hpp>
#include "utils/tone_mapping.h"

namespace mvtk {
namespace wxui {

class ImageCanvas : public wxPanel {
public:
    ImageCanvas(wxWindow* parent, wxWindowID id = wxID_ANY);
    ~ImageCanvas();

    void SetImage(const cv::Mat& image);
    void Clear();
    void SetToneMappingMode(utils::ToneMappingMode mode);

private:
    cv::Mat image_;
    wxBitmap bitmap_;
    float zoom_;
    float offset_x_;
    float offset_y_;
    bool has_image_;
    bool dragging_;
    int drag_start_x_;
    int drag_start_y_;
    utils::ToneMappingMode tone_mapping_mode_;

    cv::Mat ConvertToDisplayable(const cv::Mat& image);

    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnSetFocus(wxFocusEvent& event);

    DECLARE_EVENT_TABLE()
};

}
}