#pragma once

#include <wx/wx.h>
#include <wx/thread.h>
#include <opencv2/core/mat.hpp>
#include <functional>
#include <memory>

namespace mvtk {
namespace wxui {

wxDECLARE_EVENT(wxEVT_WORKER_PROGRESS, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_WORKER_COMPLETE, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_WORKER_ERROR, wxCommandEvent);

class WorkerThread : public wxThread {
public:
    using TaskFunction = std::function<void(cv::Mat&, int&)>;
    
    WorkerThread(wxEvtHandler* parent, const cv::Mat& input, TaskFunction task);
    ~WorkerThread() override;
    
    void* Entry() override;
    
    double GetProgress() const { return progress_; }
    const cv::Mat& GetResult() const { return result_; }
    const std::string& GetError() const { return error_; }
    
private:
    wxEvtHandler* parent_;
    cv::Mat input_;
    cv::Mat result_;
    TaskFunction task_;
    double progress_;
    std::string error_;
};

class CalibrationWorker : public wxThread {
public:
    CalibrationWorker(wxEvtHandler* parent, const cv::Mat& input,
                      std::function<void(cv::Mat&, int&, std::string&)> task);
    ~CalibrationWorker() override;
    
    void* Entry() override;
    
    const cv::Mat& GetResult() const { return result_; }
    const std::string& GetOutputData() const { return output_data_; }
    const std::string& GetError() const { return error_; }
    
private:
    wxEvtHandler* parent_;
    cv::Mat input_;
    cv::Mat result_;
    std::function<void(cv::Mat&, int&, std::string&)> task_;
    std::string output_data_;
    std::string error_;
};

}
}