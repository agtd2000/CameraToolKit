#include "wxui/image_canvas.h"
#include <wx/dcclient.h>
#include <wx/image.h>
#include <wx/graphics.h>
#include <opencv2/imgproc.hpp>

namespace mvtk {
namespace wxui {

BEGIN_EVENT_TABLE(ImageCanvas, wxPanel)
    EVT_PAINT(ImageCanvas::OnPaint)
    EVT_SIZE(ImageCanvas::OnSize)
    EVT_MOUSEWHEEL(ImageCanvas::OnMouseWheel)
    EVT_LEFT_DOWN(ImageCanvas::OnLeftDown)
    EVT_LEFT_UP(ImageCanvas::OnLeftUp)
    EVT_MOTION(ImageCanvas::OnMouseMove)
    EVT_SET_FOCUS(ImageCanvas::OnSetFocus)
END_EVENT_TABLE()

ImageCanvas::ImageCanvas(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS),
      zoom_(1.0f), offset_x_(0), offset_y_(0), has_image_(false), 
      dragging_(false), tone_mapping_mode_(utils::ToneMappingMode::PERCENTILE) {
    SetFocus();
    SetCursor(wxCursor(wxCURSOR_HAND));
}

ImageCanvas::~ImageCanvas() {}

void ImageCanvas::SetToneMappingMode(utils::ToneMappingMode mode) {
    tone_mapping_mode_ = mode;
    if (has_image_ && !image_.empty()) {
        SetImage(image_);
    }
}

cv::Mat ImageCanvas::ConvertToDisplayable(const cv::Mat& image) {
    cv::Mat display_img;
    
    if (image.depth() == CV_16U || image.depth() == CV_16S) {
        display_img = utils::ToneMapping::ConvertTo8Bit(image, tone_mapping_mode_);
    } else {
        image.convertTo(display_img, CV_8U);
    }
    
    return display_img;
}

void ImageCanvas::SetImage(const cv::Mat& image) {
    image_ = image.clone();
    
    if (image.empty()) {
        has_image_ = false;
        bitmap_ = wxBitmap();
        Refresh();
        return;
    }

    cv::Mat display_img = ConvertToDisplayable(image);
    
    cv::Mat rgb;
    if (display_img.channels() == 1) {
        cv::cvtColor(display_img, rgb, cv::COLOR_GRAY2RGB);
    } else if (display_img.channels() == 4) {
        cv::cvtColor(display_img, rgb, cv::COLOR_BGRA2RGB);
    } else {
        cv::cvtColor(display_img, rgb, cv::COLOR_BGR2RGB);
    }

    wxImage wx_img(rgb.cols, rgb.rows, const_cast<unsigned char*>(rgb.data), true);
    bitmap_ = wxBitmap(wx_img);
    has_image_ = true;
    zoom_ = 1.0f;
    offset_x_ = 0;
    offset_y_ = 0;
    Refresh();
}

void ImageCanvas::Clear() {
    image_.release();
    bitmap_ = wxBitmap();
    has_image_ = false;
    Refresh();
}

void ImageCanvas::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    dc.SetBackground(wxBrush(wxColour(20, 20, 20)));
    dc.Clear();

    int width, height;
    GetClientSize(&width, &height);

    dc.SetClippingRegion(0, 0, width, height);

    if (!has_image_ || !bitmap_.IsOk()) {
        dc.SetTextForeground(wxColour(100, 100, 100));
        dc.DrawText("No image", 10, 10);
        dc.DestroyClippingRegion();
        return;
    }

    float img_w = bitmap_.GetWidth() * zoom_;
    float img_h = bitmap_.GetHeight() * zoom_;

    float x = (width - img_w) / 2 + offset_x_;
    float y = (height - img_h) / 2 + offset_y_;

    if (img_w > 0 && img_h > 0) {
        wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
        if (gc) {
            gc->DrawBitmap(bitmap_, x, y, img_w, img_h);
            delete gc;
        } else {
            dc.DrawBitmap(bitmap_, static_cast<int>(x), static_cast<int>(y), true);
        }
    }

    dc.DestroyClippingRegion();
    dc.SetTextForeground(wxColour(255, 255, 255));
    dc.DrawText(wxString::Format("Zoom: %.1fx", zoom_), 10, height - 25);
}

void ImageCanvas::OnSize(wxSizeEvent& event) {
    Refresh();
}

void ImageCanvas::OnMouseWheel(wxMouseEvent& event) {
    if (!has_image_) return;

    int mouse_x = event.GetX();
    int mouse_y = event.GetY();
    int width, height;
    GetClientSize(&width, &height);

    float img_w = bitmap_.GetWidth() * zoom_;
    float img_h = bitmap_.GetHeight() * zoom_;

    float old_x = (width - img_w) / 2 + offset_x_;
    float old_y = (height - img_h) / 2 + offset_y_;

    float img_x = (mouse_x - old_x) / zoom_;
    float img_y = (mouse_y - old_y) / zoom_;

    float factor = event.GetWheelRotation() > 0 ? 1.1f : 0.9f;
    zoom_ *= factor;
    zoom_ = std::max(0.1f, std::min(10.0f, zoom_));

    float new_x = mouse_x - img_x * zoom_;
    float new_y = mouse_y - img_y * zoom_;

    offset_x_ += (new_x - old_x);
    offset_y_ += (new_y - old_y);

    Refresh();
}

void ImageCanvas::OnLeftDown(wxMouseEvent& event) {
    if (!has_image_) return;
    dragging_ = true;
    drag_start_x_ = event.GetX();
    drag_start_y_ = event.GetY();
    CaptureMouse();
}

void ImageCanvas::OnLeftUp(wxMouseEvent& event) {
    if (dragging_) {
        dragging_ = false;
        ReleaseMouse();
    }
}

void ImageCanvas::OnMouseMove(wxMouseEvent& event) {
    if (!dragging_ || !has_image_) return;

    int dx = event.GetX() - drag_start_x_;
    int dy = event.GetY() - drag_start_y_;

    offset_x_ += dx;
    offset_y_ += dy;

    drag_start_x_ = event.GetX();
    drag_start_y_ = event.GetY();

    Refresh();
}

void ImageCanvas::OnSetFocus(wxFocusEvent& event) {
    event.Skip();
}

}
}