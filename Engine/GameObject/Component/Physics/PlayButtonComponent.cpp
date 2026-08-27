#include "PlayButtonComponent.h"
#include "../../ComponentRegistry.h"
#include "../../GameObject.h"
#include "../../Systems/ScreenRay.h"
#include "OBBColliderComponent.h"
#include "../../../../Math/Collision.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../Audio/Sound.h"
#include "../../../Audio/OneShotVoice.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include <unordered_map>

namespace {
// ファイルパス→デコード済みPCMデータのキャッシュ。HitSoundComponent/SpawnSoundComponentと
// 同じ理由（同じSEを何度もデコードし直さない）
struct DecodedAudioCache {
	WAVEFORMATEX wfex{};
	std::vector<BYTE> audioData;
};
std::unordered_map<std::string, DecodedAudioCache> g_decodedAudioCache;

const DecodedAudioCache& GetOrDecode(const std::string& path) {
	auto it = g_decodedAudioCache.find(path);
	if (it == g_decodedAudioCache.end()) {
		Sound temp;
		temp.Load(path);
		DecodedAudioCache cache;
		cache.wfex = temp.GetFormat();
		cache.audioData = temp.GetAudioData();
		it = g_decodedAudioCache.emplace(path, std::move(cache)).first;
	}
	return it->second;
}
}

PlayButtonComponent::PlayButtonComponent(const std::vector<ProjectAssetEntry>* audioClips, int initialIndex, float volume)
	: audioClips_(audioClips), audioIndex_(initialIndex), volume_(volume) {
	if (audioIndex_ < 0 || audioIndex_ >= static_cast<int>(audioClips_->size())) audioIndex_ = -1;
}

void PlayButtonComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)deltaTime;

	// Sceneビュー表示中はGizmoControllerが同じ左クリックでオブジェクト選択を行っているため、
	// ReflexPlayerComponentと同じくGameビュー中のみ判定する
	bool leftPressed = ctx.isGameView && ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool clickedThisFrame = leftPressed && !prevMouseLeftPressed_;
	prevMouseLeftPressed_ = leftPressed;

	// enabled==falseの間はprevMouseLeftPressed_の更新だけ行い（再度enabled=trueに戻った瞬間、
	// 押しっぱなしのマウスを誤ってクリックとして拾わないようにするため）、ホバー・クリック判定は
	// 一切行わない
	if (!enabled) {
		isHovering_ = false;
		return;
	}

	bool hovering = false;
	if (ctx.isGameView && ctx.renderer && ctx.sceneObjects && !ImGui::GetIO().WantCaptureMouse) {
		// このコンポーネント自身が付いているGameObject（＝自分のOBBColliderComponentを持つ相手）を
		// ctx.sceneObjectsから、Transformのアドレス一致で逆引きする（IComponent::Updateはtransformしか
		// 受け取らず、兄弟コンポーネントを直接GetComponentできないため。ReflexPlayerComponent::
		// IsPathBlockedがctx.sceneObjectsを同様の目的で使っているのと同じ発想）
		GameObject* self = nullptr;
		for (GameObject* obj : *ctx.sceneObjects) {
			if (obj && &obj->GetTransform() == &transform) { self = obj; break; }
		}

		if (self) {
			if (auto* obbCollider = self->GetComponent<OBBColliderComponent>()) {
				Collision::OBB obb = obbCollider->GetWorldOBB(self->GetWorldTransform());
				Collision::Ray ray = ScreenRay::FromMouse(ctx.renderer, ctx.view, ctx.proj);
				hovering = Collision::OBBRay(obb, ray);
			}
		}
	}

	if (hovering && clickedThisFrame) {
		clicked_ = true;
		if (audioIndex_ >= 0) {
			const DecodedAudioCache& cache = GetOrDecode((*audioClips_)[audioIndex_].path);
			OneShotVoice::Play(cache.wfex, cache.audioData, volume_, /*isBGM=*/false);
		}
	}

	isHovering_ = hovering;
}

void PlayButtonComponent::DrawImGui(const char* namePrefix) {
	std::string comboLabel = std::string(namePrefix) + "クリックSE";
	const char* currentName = (audioIndex_ >= 0) ? (*audioClips_)[audioIndex_].displayName.c_str() : "(未設定)";
	if (ImGui::BeginCombo(comboLabel.c_str(), currentName)) {
		bool noneSelected = (audioIndex_ < 0);
		if (ImGui::Selectable("(未設定)", noneSelected)) audioIndex_ = -1;
		if (noneSelected) ImGui::SetItemDefaultFocus();
		for (int i = 0; i < static_cast<int>(audioClips_->size()); i++) {
			bool selected = (i == audioIndex_);
			if (ImGui::Selectable((*audioClips_)[i].displayName.c_str(), selected)) audioIndex_ = i;
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	std::string volLabel = std::string(namePrefix) + "SE音量";
	ImGui::SliderFloat(volLabel.c_str(), &volume_, 0.0f, 1.0f);

	std::string normalScaleLabel = std::string(namePrefix) + "通常時のサイズ倍率";
	std::string hoverScaleLabel = std::string(namePrefix) + "ホバー時のサイズ倍率";
	ImGui::DragFloat(normalScaleLabel.c_str(), &normalScaleMultiplier, 0.01f, 0.1f, 5.0f);
	ImGui::DragFloat(hoverScaleLabel.c_str(), &hoverScaleMultiplier, 0.01f, 0.1f, 5.0f);

	std::string normalColorLabel = std::string(namePrefix) + "通常時の色";
	std::string hoverColorLabel = std::string(namePrefix) + "ホバー時の色";
	ImGui::ColorEdit4(normalColorLabel.c_str(), &normalColor.x);
	ImGui::ColorEdit4(hoverColorLabel.c_str(), &hoverColor.x);

	std::string statusLabel = std::string(namePrefix) + (isHovering_ ? "状態: ホバー中" : "状態: 通常");
	ImGui::Text("%s", statusLabel.c_str());

	ImGui::TextDisabled("(このGameObjectにOBBColliderComponentも付けてください。当たり判定として使います)");
}

void PlayButtonComponent::ToJson(nlohmann::json& out) const {
	out["audioName"] = (audioIndex_ >= 0 && audioIndex_ < static_cast<int>(audioClips_->size()))
		? (*audioClips_)[audioIndex_].displayName : std::string();
	out["volume"] = volume_;
	out["normalColor"] = Vector4ToJson(normalColor);
	out["hoverColor"] = Vector4ToJson(hoverColor);
	out["normalScaleMultiplier"] = normalScaleMultiplier;
	out["hoverScaleMultiplier"] = hoverScaleMultiplier;
}

void PlayButtonComponent::FromJson(const nlohmann::json& in) {
	volume_ = in.value("volume", volume_);
	if (in.contains("normalColor")) normalColor = Vector4FromJson(in["normalColor"]);
	if (in.contains("hoverColor")) hoverColor = Vector4FromJson(in["hoverColor"]);
	normalScaleMultiplier = in.value("normalScaleMultiplier", normalScaleMultiplier);
	hoverScaleMultiplier = in.value("hoverScaleMultiplier", hoverScaleMultiplier);
	// audioNameはComponentRegistryのcreatorが読み、現在のaudioClips_一覧からindexを引き直す
	// （HitSoundComponentと同じパターン）ためここでは扱わない
}
