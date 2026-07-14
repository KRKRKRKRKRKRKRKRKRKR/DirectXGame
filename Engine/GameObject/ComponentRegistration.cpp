#include "ComponentRegistration.h"
#include "ComponentRegistry.h"
#include "GameObject.h"
#include "Component/Render/Render.h"
#include "Component/Physics/Physics.h"
#include "Component/Lighting/Lighting.h"
#include "Component/Audio/Audio.h"

void RegisterEngineComponents() {
	// 単純パターン：デフォルト構築でき、他コンポーネントや外部リソースに依存しない
	ComponentRegistry::RegisterSimple<GravityComponent>("Gravity");
	ComponentRegistry::RegisterSimple<PlayerControllerComponent>("PlayerController");
	ComponentRegistry::RegisterSimple<SphereColliderComponent>("SphereCollider");
	ComponentRegistry::RegisterSimple<OBBColliderComponent>("OBBCollider");
	ComponentRegistry::RegisterSimple<CubeRenderComponent>("CubeRender");
	ComponentRegistry::RegisterSimple<SphereRenderComponent>("SphereRender");
	ComponentRegistry::RegisterSimple<TriangleRenderComponent>("TriangleRender");
	ComponentRegistry::RegisterSimple<DirectionalLightComponent>("DirectionalLight");
	ComponentRegistry::RegisterSimple<PointLightComponent>("PointLight");
	ComponentRegistry::RegisterSimple<SpotLightComponent>("SpotLight");

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
		});

	// SpriteRenderComponent：is3Dはコンストラクタ引数のため先に読んでから生成する
	ComponentRegistry::Register<SpriteRenderComponent>("SpriteRender",
		[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
			bool is3D = data.value("is3D", true);
			obj.AddComponent<SpriteRenderComponent>(is3D)->FromJson(data);
		});

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
		});

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
		});

	// MirrorComponent：固有データなし。同じGameObjectの兄弟CubeRenderComponentに紐付けるだけ
	ComponentRegistry::Register<MirrorComponent>("Mirror",
		[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json&) {
			obj.AddComponent<MirrorComponent>(obj.GetComponent<CubeRenderComponent>());
		});

	// AudioSourceComponent：コンストラクタ引数一式をJSONから読んで呼び直す
	ComponentRegistry::Register<AudioSourceComponent>("AudioSource",
		[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
			std::string filePath = data.value("filePath", std::string());
			std::string registeredName = data.value("registeredName", std::string());
			SoundType type = static_cast<SoundType>(data.value("soundType", static_cast<int>(SoundType::BGM)));
			bool loop = data.value("loop", true);
			obj.AddComponent<AudioSourceComponent>(filePath, registeredName, type, loop);
		});
}
