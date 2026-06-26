#include "wxui/worker_thread.h"

namespace mvtk {
namespace wxui {

wxDEFINE_EVENT(wxEVT_WORKER_PROGRESS, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_WORKER_COMPLETE, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_WORKER_ERROR, wxCommandEvent);

WorkerThread::WorkerThread(wxEvtHandler* parent, const cv::Mat& input, TaskFunction task)
    : wxThread(wxTHREAD_JOINABLE), parent_(parent), input_(input.clone()), task_(task), progress_(0.0) {}

WorkerThread::~WorkerThread() {
    if (IsRunning()) {
        Wait();
    }
}

void* WorkerThread::Entry() {
    result_ = input_.clone();
    int progress_int = 0;
    
    try {
        task_(result_, progress_int);
        progress_ = progress_int / 100.0;
        
        if (!TestDestroy()) {
            wxCommandEvent event(wxEVT_WORKER_COMPLETE, GetId());
            parent_->QueueEvent(event.Clone());
        }
    } catch (const std::exception& e) {
        error_ = e.what();
        wxCommandEvent event(wxEVT_WORKER_ERROR, GetId());
        event.SetString(error_);
        parent_->QueueEvent(event.Clone());
    } catch (...) {
        error_ = "Unknown error";
        wxCommandEvent event(wxEVT_WORKER_ERROR, GetId());
        event.SetString(error_);
        parent_->QueueEvent(event.Clone());
    }
    
    return nullptr;
}

CalibrationWorker::CalibrationWorker(wxEvtHandler* parent, const cv::Mat& input,
                                     std::function<void(cv::Mat&, int&, std::string&)> task)
    : wxThread(wxTHREAD_JOINABLE), parent_(parent), input_(input.clone()), task_(task) {}

CalibrationWorker::~CalibrationWorker() {
    if (IsRunning()) {
        Wait();
    }
}

void* CalibrationWorker::Entry() {
    result_ = input_.clone();
    int progress = 0;
    
    try {
        task_(result_, progress, output_data_);
        
        if (!TestDestroy()) {
            wxCommandEvent event(wxEVT_WORKER_COMPLETE, GetId());
            parent_->QueueEvent(event.Clone());
        }
    } catch (const std::exception& e) {
        error_ = e.what();
        wxCommandEvent event(wxEVT_WORKER_ERROR, GetId());
        event.SetString(error_);
        parent_->QueueEvent(event.Clone());
    } catch (...) {
        error_ = "Unknown error";
        wxCommandEvent event(wxEVT_WORKER_ERROR, GetId());
        event.SetString(error_);
        parent_->QueueEvent(event.Clone());
    }
    
    return nullptr;
}

}
}