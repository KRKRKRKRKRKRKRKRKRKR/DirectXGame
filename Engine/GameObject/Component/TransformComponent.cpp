#include "TransformComponent.h"
#include "../../../Externals/imgui/imgui.h"
#include <string>

namespace {
constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
}

void TransformComponent::DrawImGui(const char* namePrefix) {
	if (is2D) {
		std::string posLabel      = std::string(namePrefix) + "位置 (px)";
		std::string sizeLabel     = std::string(namePrefix) + "サイズ (px)";
		std::string rotationLabel = std::string(namePrefix) + "回転 (度)";
		ImGui::DragFloat2(posLabel.c_str(), &transform.translation.x, translationSpeed, translationMin, translationMax);
		ImGui::DragFloat2(sizeLabel.c_str(), &transform.scale.x, scaleSpeed, scaleMin, scaleMax);

		// transform.rotationは内部的にラジアンで保持しているため、表示用に度数法へ変換してから
		// DragFloatに渡し、編集があればラジアンへ戻して書き込む
		float rotationDeg = transform.rotation.z * kRadToDeg;
		if (ImGui::DragFloat(rotationLabel.c_str(), &rotationDeg, rotationSpeed, -180.0f, 180.0f)) {
			transform.rotation.z = rotationDeg * kDegToRad;
		}
		return;
	}

	std::string scaleLabel       = std::string(namePrefix) + "スケール";
	std::string rotationLabel    = std::string(namePrefix) + "回転 (度)";
	std::string translationLabel = std::string(namePrefix) + "位置";
	ImGui::DragFloat3(scaleLabel.c_str(), &transform.scale.x, scaleSpeed, scaleMin, scaleMax);

	Vector3 rotationDeg = {
		transform.rotation.x * kRadToDeg,
		transform.rotation.y * kRadToDeg,
		transform.rotation.z * kRadToDeg,
	};
	if (ImGui::DragFloat3(rotationLabel.c_str(), &rotationDeg.x, rotationSpeed, -180.0f, 180.0f)) {
		transform.rotation.x = rotationDeg.x * kDegToRad;
		transform.rotation.y = rotationDeg.y * kDegToRad;
		transform.rotation.z = rotationDeg.z * kDegToRad;
	}
	ImGui::DragFloat3(translationLabel.c_str(), &transform.translation.x, translationSpeed, translationMin, translationMax);
}
