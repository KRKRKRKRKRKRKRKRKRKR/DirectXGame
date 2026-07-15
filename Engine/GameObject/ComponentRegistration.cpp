#include "ComponentRegistration.h"
#include "ComponentRegistry.h"
#include "GameObject.h"
#include "Component/Render/Render.h"
#include "Component/Physics/Physics.h"
#include "Component/Lighting/Lighting.h"
#include "Component/Audio/Audio.h"
#include "Component/Camera/Camera.h"
#include "../Utils/Logger.h"

void RegisterEngineComponents() {
	// 単純パターン：デフォルト構築でき、他コンポーネントや外部リソースに依存しない
	// （第2引数はInspector/Add Componentメニューでの表示名。第1引数のtypeName自体はJSON保存の
	// キーとして使われるため変更しない）
	ComponentRegistry::RegisterSimple<GravityComponent>("Gravity", "重力");
	ComponentRegistry::RegisterSimple<PlayerControllerComponent>("PlayerController", "プレイヤー操作");
	ComponentRegistry::RegisterSimple<SphereColliderComponent>("SphereCollider", "球コライダー");
	ComponentRegistry::RegisterSimple<OBBColliderComponent>("OBBCollider", "直方体コライダー");
	ComponentRegistry::RegisterSimple<CubeRenderComponent>("CubeRender", "キューブ描画");
	ComponentRegistry::RegisterSimple<SphereRenderComponent>("SphereRender", "球描画");
	ComponentRegistry::RegisterSimple<TriangleRenderComponent>("TriangleRender", "三角形描画");
	ComponentRegistry::RegisterSimple<DirectionalLightComponent>("DirectionalLight", "平行光源");
	ComponentRegistry::RegisterSimple<PointLightComponent>("PointLight", "点光源");
	ComponentRegistry::RegisterSimple<SpotLightComponent>("SpotLight", "スポットライト");
	ComponentRegistry::RegisterSimple<CameraComponent>("Camera", "カメラ");

	// カスタムパターン：コンストラクタ引数が要る、または兄弟コンポーネント/外部リソースに依存する

	// ModelRenderComponent：directoryPath/filenameからRenderer::LoadModelを呼び直してhandleを得る
	ComponentRegistry::Register<ModelRenderComponent>("ModelRender",
		[](GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data) {
			std::string dir  = data.value("directoryPath", std::string());
			std::string file = data.value("filename", std::string());
			bool hasAnimation = data.value("hasAnimation", false);
			ModelRenderComponent* c = obj.AddComponent<ModelRenderComponent>(
				ctx.renderer->LoadModel(dir, file), hasAnimation);
			c->directoryPath = dir;
			c->filename = file;
			c->FromJson(data); // RenderComponentBase共通フィールドを反映
		},
		[](GameObject& obj) {
			// TextureSelectorComponentがこのModelRenderComponentへの生ポインタを持っている場合、
			// 先にTextureSelector側を消してもらわないとダングリングポインタになるため拒否する
			if (obj.GetComponent<TextureSelectorComponent>()) {
				Logger::Log("ComponentRegistry: ModelRenderを消す前にTexture Selectorを先に削除してください\n");
				return false;
			}
			return obj.RemoveComponent<ModelRenderComponent>();
		},
		"モデル描画");

	// SpriteRenderComponent：is3Dはコンストラクタ引数のため先に読んでから生成する
	ComponentRegistry::Register<SpriteRenderComponent>("SpriteRender",
		[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
			bool is3D = data.value("is3D", true);
			obj.AddComponent<SpriteRenderComponent>(is3D)->FromJson(data);
		},
		[](GameObject& obj) {
			// TextureSelectorComponentがこのSpriteRenderComponentへの生ポインタを持っている場合、
			// 先にTextureSelector側を消してもらわないとダングリングポインタになるため拒否する
			if (obj.GetComponent<TextureSelectorComponent>()) {
				Logger::Log("ComponentRegistry: SpriteRenderを消す前にTexture Selectorを先に削除してください\n");
				return false;
			}
			return obj.RemoveComponent<SpriteRenderComponent>();
		},
		"スプライト描画");

	// TextRenderComponent：txtFilePath/fontFilePath/fontSize/lineSpacingを読んでからLoad()し直す。
	// dynamicText（Camera座標表示等）の場合はtxtFilePathを使わないためLoadDynamic()を呼ぶ
	// （実際の文字列は呼び出し元が毎フレームSetText()で与える必要がある）
	ComponentRegistry::Register<TextRenderComponent>("TextRender",
		[](GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data) {
			TextRenderComponent* c = obj.AddComponent<TextRenderComponent>();
			c->FromJson(data);
			if (c->dynamicText) {
				c->LoadDynamic(ctx.renderer);
			} else {
				c->Load(ctx.renderer);
			}
		},
		[](GameObject& obj) { return obj.RemoveComponent<TextRenderComponent>(); },
		"テキスト描画");

	// TextureSelectorComponent：同じGameObjectに既に復元済みのRenderComponentBaseと、
	// ComponentLoadContext.textures（名前→現在のindex変換用）が必要
	ComponentRegistry::Register<TextureSelectorComponent>("TextureSelector",
		[](GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data) {
			std::string name = data.value("textureName", std::string());
			int index = 0;
			if (ctx.textures) {
				for (size_t i = 0; i < ctx.textures->size(); i++) {
					if ((*ctx.textures)[i].name == name) { index = static_cast<int>(i); break; }
				}
			}
			obj.AddComponent<TextureSelectorComponent>(obj.GetComponent<RenderComponentBase>(), ctx.textures, index);
		},
		[](GameObject& obj) { return obj.RemoveComponent<TextureSelectorComponent>(); },
		"テクスチャ選択");

	// MirrorComponent：固有データなし。同じGameObjectの兄弟CubeRenderComponentに紐付けるだけ
	ComponentRegistry::Register<MirrorComponent>("Mirror",
		[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json&) {
			obj.AddComponent<MirrorComponent>(obj.GetComponent<CubeRenderComponent>());
		},
		[](GameObject& obj) { return obj.RemoveComponent<MirrorComponent>(); },
		"鏡");

	// AudioSourceComponent：コンストラクタ引数一式をJSONから読んで呼び直す
	ComponentRegistry::Register<AudioSourceComponent>("AudioSource",
		[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
			std::string filePath = data.value("filePath", std::string());
			std::string registeredName = data.value("registeredName", std::string());
			SoundType type = static_cast<SoundType>(data.value("soundType", static_cast<int>(SoundType::BGM)));
			bool loop = data.value("loop", true);
			obj.AddComponent<AudioSourceComponent>(filePath, registeredName, type, loop);
		},
		[](GameObject& obj) { return obj.RemoveComponent<AudioSourceComponent>(); },
		"オーディオソース");
}
