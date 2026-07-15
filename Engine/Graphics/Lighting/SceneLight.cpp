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
    ImGui::Begin("ライティング##Lighting");

    ImGui::Text("トゥーンシェーディング");
    bool enableToon = data_.enableToon != 0;
    if (ImGui::Checkbox("トゥーンを有効化", &enableToon)) {
        SetEnableToon(enableToon);
    }
    if (ImGui::SliderFloat("トゥーンのしきい値", &data_.toonThreshold, 0.0f, 1.0f)) {
        SetToonThreshold(data_.toonThreshold);
    }

    ImGui::Separator();
    ImGui::Text("スペキュラー（Blinn-Phong）");
    bool enableSpecular = data_.enableSpecular != 0;
    if (ImGui::Checkbox("スペキュラーを有効化", &enableSpecular)) {
        SetEnableSpecular(enableSpecular);
    }
    if (ImGui::ColorEdit3("スペキュラーの色", &data_.specularColor.x)) {
        SetSpecularColor(data_.specularColor);
    }
    if (ImGui::SliderFloat("光沢度", &data_.shininess, 1.0f, 200.0f)) {
        SetShininess(data_.shininess);
    }

    ImGui::Separator();
    ImGui::Text("リムライト");
    bool enableRim = data_.enableRim != 0;
    if (ImGui::Checkbox("リムライトを有効化", &enableRim)) {
        SetEnableRim(enableRim);
    }
    if (ImGui::ColorEdit3("リムライトの色", &data_.rimColor.x)) {
        SetRimColor(data_.rimColor);
    }
    if (ImGui::SliderFloat("リムの強さ（指数）", &data_.rimPower, 0.1f, 8.0f)) {
        SetRimPower(data_.rimPower);
    }
    if (ImGui::SliderFloat("リムの強さ（係数）", &data_.rimStrength, 0.0f, 4.0f)) {
        SetRimStrength(data_.rimStrength);
    }

    ImGui::End();
}
