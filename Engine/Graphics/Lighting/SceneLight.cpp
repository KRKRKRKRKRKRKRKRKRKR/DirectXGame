#include "SceneLight.h"
#include "../../../Externals/imgui/imgui.h"
#include <cassert>

void SceneLight::Initialize(ID3D12Device* device) {
    resource_ = ResourceFactory::CreateBufferResource(device, sizeof(LightData));
    assert(resource_);
    HRESULT hr = resource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped_));
    assert(SUCCEEDED(hr) && mapped_);

    resourceOff_ = ResourceFactory::CreateBufferResource(device, sizeof(LightData));
    assert(resourceOff_);
    hr = resourceOff_->Map(0, nullptr, reinterpret_cast<void**>(&mappedOff_));
    assert(SUCCEEDED(hr) && mappedOff_);
    // enableLighting=0 固定（他フィールドは不使用なので初期値のまま）
    mappedOff_->enableLighting = 0;

    Upload();
}

void SceneLight::Upload() {
    if (mapped_) {
        *mapped_ = data_;
        // enableLighting は常に 1
        mapped_->enableLighting = 1;
    }
}

void SceneLight::DrawImGui() {
    ImGui::Begin("Lighting");

    ImGui::Text("Toon Shading");
    bool enableToon = data_.enableToon != 0;
    if (ImGui::Checkbox("Enable Toon", &enableToon)) {
        SetEnableToon(enableToon);
    }
    if (ImGui::SliderFloat("Toon Threshold", &data_.toonThreshold, 0.0f, 1.0f)) {
        SetToonThreshold(data_.toonThreshold);
    }

    ImGui::Separator();
    ImGui::Text("Specular (Blinn-Phong)");
    bool enableSpecular = data_.enableSpecular != 0;
    if (ImGui::Checkbox("Enable Specular", &enableSpecular)) {
        SetEnableSpecular(enableSpecular);
    }
    if (ImGui::ColorEdit3("Specular Color", &data_.specularColor.x)) {
        SetSpecularColor(data_.specularColor);
    }
    if (ImGui::SliderFloat("Shininess", &data_.shininess, 1.0f, 200.0f)) {
        SetShininess(data_.shininess);
    }

    ImGui::Separator();
    ImGui::Text("Rim Light");
    bool enableRim = data_.enableRim != 0;
    if (ImGui::Checkbox("Enable Rim", &enableRim)) {
        SetEnableRim(enableRim);
    }
    if (ImGui::ColorEdit3("Rim Color", &data_.rimColor.x)) {
        SetRimColor(data_.rimColor);
    }
    if (ImGui::SliderFloat("Rim Power", &data_.rimPower, 0.1f, 8.0f)) {
        SetRimPower(data_.rimPower);
    }
    if (ImGui::SliderFloat("Rim Strength", &data_.rimStrength, 0.0f, 4.0f)) {
        SetRimStrength(data_.rimStrength);
    }

    ImGui::End();
}
