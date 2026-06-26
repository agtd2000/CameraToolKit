/**
 * @file roi_editor.cpp
 * @brief ROI编辑器组件实现
 */

#include "ui/roi_editor.h"
#include <imgui.h>

namespace mvtk {
namespace ui {

ROIEditor::ROIEditor() = default;

void ROIEditor::render(std::vector<mvtk::ROIInfo>& rois) {
    const char* type_names[] = {"24-Color Card", "140-Color Card", "Custom", "Free"};
    int type_idx = static_cast<int>(current_type_);
    ImGui::Combo("ROI Type", &type_idx, type_names, 4);
    current_type_ = static_cast<mvtk::ROIType>(type_idx);

    ImGui::Separator();

    if (ImGui::BeginTable("ROI List", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < rois.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool selected = (static_cast<int>(i) == selected_idx_);
            if (ImGui::Selectable(std::to_string(rois[i].index).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_idx_ = static_cast<int>(i);
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", rois[i].label.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("(%d, %d)", rois[i].rect.x, rois[i].rect.y);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%dx%d", rois[i].rect.width, rois[i].rect.height);
        }
        ImGui::EndTable();
    }

    if (ImGui::Button("Add ROI")) {
        mvtk::ROIInfo info;
        info.type = current_type_;
        info.rect = cv::Rect(0, 0, 30, 30);
        info.index = static_cast<int>(rois.size());
        info.label = "ROI " + std::to_string(info.index);
        rois.push_back(info);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected") && selected_idx_ >= 0 && selected_idx_ < static_cast<int>(rois.size())) {
        rois.erase(rois.begin() + selected_idx_);
        selected_idx_ = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) {
        rois.clear();
        selected_idx_ = -1;
    }

    if (selected_idx_ >= 0 && selected_idx_ < static_cast<int>(rois.size())) {
        ImGui::Separator();
        auto& roi = rois[selected_idx_];
        int x = roi.rect.x, y = roi.rect.y, w = roi.rect.width, h = roi.rect.height;
        ImGui::InputInt("X", &x);
        ImGui::InputInt("Y", &y);
        ImGui::InputInt("Width", &w);
        ImGui::InputInt("Height", &h);
        roi.rect = cv::Rect(x, y, w, h);
    }
}

} // namespace ui
} // namespace mvtk
