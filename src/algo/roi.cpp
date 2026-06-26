#include "algo/roi.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/mcc.hpp>

namespace mvtk {

std::vector<ROIInfo> ROIManager::generate24ColorROI(
    int start_x, int start_y, int patch_w, int patch_h, int gap) {
    std::vector<ROIInfo> rois;
    int cols = 4, rows = 6;

    const char* labels[] = {
        "Dark Skin", "Light Skin", "Blue Sky", "Foliage",
        "Blue Flower", "Bluish Green", "Orange", "Purplish Blue",
        "Moderate Red", "Purple", "Yellow Green", "Orange Yellow",
        "Blue", "Green", "Red", "Yellow",
        "Magenta", "Cyan", "White", "Neutral 8",
        "Neutral 6.5", "Neutral 5", "Neutral 3.5", "Black"
    };

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            ROIInfo info;
            info.type = ROIType::COLOR_24;
            info.rect = cv::Rect(
                start_x + c * (patch_w + gap),
                start_y + r * (patch_h + gap),
                patch_w, patch_h);
            info.index = r * cols + c;
            info.label = labels[info.index];
            rois.push_back(info);
        }
    }

    return rois;
}

std::vector<ROIInfo> ROIManager::generate140ColorROI(
    int start_x, int start_y, int patch_w, int patch_h, int gap) {
    std::vector<ROIInfo> rois;
    int cols = 14, rows = 10;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            ROIInfo info;
            info.type = ROIType::COLOR_140;
            info.rect = cv::Rect(
                start_x + c * (patch_w + gap),
                start_y + r * (patch_h + gap),
                patch_w, patch_h);
            info.index = r * cols + c;
            info.label = "Patch " + std::to_string(info.index);
            rois.push_back(info);
        }
    }

    return rois;
}

std::vector<ROIInfo> ROIManager::detectColorChecker(const cv::Mat& image, ROIType type) {
    std::vector<ROIInfo> rois;

    if (image.empty()) {
        return rois;
    }

    cv::Ptr<cv::mcc::CCheckerDetector> detector = cv::mcc::CCheckerDetector::create();

    cv::mcc::TYPECHART chartType;
    int expected_cols, expected_rows, total_patches;
    if (type == ROIType::COLOR_24) {
        chartType = cv::mcc::TYPECHART::MCC24;
        expected_cols = 6;
        expected_rows = 4;
        total_patches = 24;
    } else {
        chartType = cv::mcc::TYPECHART::SG140;
        expected_cols = 14;
        expected_rows = 10;
        total_patches = 140;
    }

    cv::Mat image_bgr;
    if (image.channels() == 1) {
        cv::cvtColor(image, image_bgr, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 3) {
        image_bgr = image.clone();
    }

    if (!detector->process(image_bgr, chartType)) {
        return rois;
    }

    std::vector<cv::Ptr<cv::mcc::CChecker>> checkers = detector->getListColorChecker();
    if (checkers.empty()) {
        return rois;
    }

    cv::Ptr<cv::mcc::CChecker> checker = checkers[0];
    std::vector<cv::Point2f> box = checker->getBox();

    if (box.size() != 4) {
        return rois;
    }

    cv::Point2f tl = box[0];
    cv::Point2f tr = box[1];
    cv::Point2f br = box[2];
    cv::Point2f bl = box[3];

    const char* labels_24[] = {
        "Dark Skin", "Light Skin", "Blue Sky", "Foliage",
        "Blue Flower", "Bluish Green", "Orange", "Purplish Blue",
        "Moderate Red", "Purple", "Yellow Green", "Orange Yellow",
        "Blue", "Green", "Red", "Yellow",
        "Magenta", "Cyan", "White", "Neutral 8",
        "Neutral 6.5", "Neutral 5", "Neutral 3.5", "Black"
    };

    cv::Point2f src_pts[] = {tl, tr, br, bl};
    cv::Point2f dst_pts[] = {
        cv::Point2f(0, 0),
        cv::Point2f(static_cast<float>(expected_cols), 0),
        cv::Point2f(static_cast<float>(expected_cols), static_cast<float>(expected_rows)),
        cv::Point2f(0, static_cast<float>(expected_rows))
    };
    cv::Mat H = cv::getPerspectiveTransform(src_pts, dst_pts);
    cv::Mat H_inv = H.inv();

    float grid_w = cv::norm(tr - tl) / expected_cols;
    float grid_h = cv::norm(bl - tl) / expected_rows;
    float roi_scale = 0.5f;
    float roi_w = grid_w * roi_scale;
    float roi_h = grid_h * roi_scale;

    for (int row = 0; row < expected_rows; ++row) {
        for (int col = 0; col < expected_cols; ++col) {
            cv::Point2f pt_src(col + 0.5f, row + 0.5f);
            std::vector<cv::Point2f> pt_src_vec = {pt_src};
            std::vector<cv::Point2f> pt_dst_vec;
            cv::perspectiveTransform(pt_src_vec, pt_dst_vec, H_inv);

            cv::Point2f center = pt_dst_vec[0];

            int half_w = static_cast<int>(roi_w / 2);
            int half_h = static_cast<int>(roi_h / 2);

            ROIInfo info;
            info.type = type;
            info.rect = cv::Rect(
                static_cast<int>(center.x - half_w),
                static_cast<int>(center.y - half_h),
                static_cast<int>(roi_w),
                static_cast<int>(roi_h));
            info.index = row * expected_cols + col;
            if (type == ROIType::COLOR_24 && info.index < 24) {
                info.label = labels_24[info.index];
            } else {
                info.label = "Patch " + std::to_string(info.index);
            }
            rois.push_back(info);
        }
    }

    return rois;
}

cv::Mat ROIManager::drawROIs(const cv::Mat& image, const std::vector<ROIInfo>& rois, int selected) {
    cv::Mat vis;
    if (image.channels() == 1) {
        cv::cvtColor(image, vis, cv::COLOR_GRAY2BGR);
    } else {
        vis = image.clone();
    }

    for (size_t i = 0; i < rois.size(); ++i) {
        cv::Scalar color = (static_cast<int>(i) == selected)
                               ? cv::Scalar(0, 255, 0)
                               : cv::Scalar(0, 200, 255);

        cv::rectangle(vis, rois[i].rect, color, 2);

        cv::putText(vis, std::to_string(rois[i].index),
                    cv::Point(rois[i].rect.x, rois[i].rect.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
    }

    return vis;
}

int ROIManager::hitTest(const cv::Point& point, const std::vector<ROIInfo>& rois) {
    for (size_t i = 0; i < rois.size(); ++i) {
        if (rois[i].rect.contains(point)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace mvtk