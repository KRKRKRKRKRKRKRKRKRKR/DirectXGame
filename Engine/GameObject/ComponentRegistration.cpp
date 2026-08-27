#include "ComponentRegistration.h"
#include "ComponentRegistry.h"
#include "GameObject.h"
#include "Component/Render/Render.h"
#include "Component/Audio/Audio.h"
#include "Component/Physics/Physics.h"
#include "../Utils/Logger.h"

void RegisterEngineComponents() {
	// 単純パターン（デフォルト構築でき、他コンポーネントや外部リソースに依存しないコンポーネント）は
	// ここには登録しない。各コンポーネント自身の.cpp末尾にあるREGISTER_SIMPLE_COMPONENTマクロが
	// プログラム起動時に自己登録する（ComponentRegistry.h参照）。ここを編集しなくても新しい
	// Simpleコンポーネントが自動的にAdd Componentメニュー/プロジェクトパネルに出てくるようにするため

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
			// FromJsonがsubMeshTextureNamesを名前→index解決するのに使うため、先に注入しておく
			c->SetInspectorContext(ctx.renderer, ctx.textures, ctx.ensureTextureRegistered);
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

	// SpriteRenderComponent：is3Dはコンストラクタ引数のため先に読んでから生成する。
	// TransformComponent::is2Dはis3Dと常に逆の関係であるべき（is2D=trueならpx座標系、
	// is3Dはその座標系をどちらのDrawSprite*で解釈するかを決めるため）。過去に、保存済みJSONで
	// この2つが食い違ったまま保存され（Add Componentメニューがis2Dを一方向にしか揃えていなかった、
	// または同一GameObjectにSpriteRenderComponentが2個付いて片方だけが正しく同期された等）、
	// 3D空間にpx単位の座標がそのまま使われてスプライトが描画されない不具合が繰り返し起きたため、
	// ロードのたびにここで強制的に同期し直す（手動編集・過去の壊れたセーブデータへの保険）
	ComponentRegistry::Register<SpriteRenderComponent>("SpriteRender",
		[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
			bool is3D = data.value("is3D", true);
			obj.AddComponent<SpriteRenderComponent>(is3D)->FromJson(data);
			if (TransformComponent* transform = obj.GetComponent<TransformComponent>()) {
				transform->is2D = !is3D;
			}
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
	// （実際の文字列は呼び出し元が毎フレームSetText()で与える必要がある）。
	// SpriteRenderComponentと同じくis3D/TransformComponent::is2Dは常に逆の関係であるべきなので、
	// ロードのたびに強制的に同期し直す（過去にSpriteRenderで起きた食い違いバグの再発防止）
	ComponentRegistry::Register<TextRenderComponent>("TextRender",
		[](GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data) {
			TextRenderComponent* c = obj.AddComponent<TextRenderComponent>();
			c->FromJson(data);
			if (TransformComponent* transform = obj.GetComponent<TransformComponent>()) {
				transform->is2D = !c->is3D;
			}
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

	// ReflexEnemyHealthBarComponent：固有データなし。同じGameObjectの兄弟ReflexEnemyComponentに
	// 紐付けるだけ（MirrorComponentと同じパターン）
	ComponentRegistry::Register<ReflexEnemyHealthBarComponent>("ReflexEnemyHealthBar",
		[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
			ReflexEnemyHealthBarComponent* c = obj.AddComponent<ReflexEnemyHealthBarComponent>(obj.GetComponent<ReflexEnemyComponent>());
			c->FromJson(data);
		},
		[](GameObject& obj) { return obj.RemoveComponent<ReflexEnemyHealthBarComponent>(); },
		"REFLEX敵HPバー");

	// AudioSourceComponent：コンストラクタ引数一式をJSONから読んで呼び直す
	ComponentRegistry::Register<AudioSourceComponent>("AudioSource",
		[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
			std::string filePath = data.value("filePath", std::string());
			std::string registeredName = data.value("registeredName", std::string());
			SoundType type = static_cast<SoundType>(data.value("soundType", static_cast<int>(SoundType::BGM)));
			bool loop = data.value("loop", true);
			bool playOnAwake = data.value("playOnAwake", true);
			float volume = data.value("volume", 1.0f);
			obj.AddComponent<AudioSourceComponent>(filePath, registeredName, type, loop, playOnAwake, volume);
		},
		[](GameObject& obj) { return obj.RemoveComponent<AudioSourceComponent>(); },
		"オーディオソース");

	// HitSoundComponent：ComponentLoadContext.audioClips（名前→現在のindex変換用）が必要。
	// TextureSelectorComponentと同じ「保存時は名前、復元時に現在の一覧から探し直す」パターン
	ComponentRegistry::Register<HitSoundComponent>("HitSound",
		[](GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data) {
			std::string name = data.value("audioName", std::string());
			int index = -1;
			if (ctx.audioClips) {
				for (size_t i = 0; i < ctx.audioClips->size(); i++) {
					if ((*ctx.audioClips)[i].displayName == name) { index = static_cast<int>(i); break; }
				}
			}
			float volume = data.value("volume", 1.0f);
			obj.AddComponent<HitSoundComponent>(ctx.audioClips, index, volume);
		},
		[](GameObject& obj) { return obj.RemoveComponent<HitSoundComponent>(); },
		"ヒットSE");

	// SpawnSoundComponent：HitSoundComponentと完全に同じ登録パターン（敵がスポーンした瞬間に鳴らすSE）
	ComponentRegistry::Register<SpawnSoundComponent>("SpawnSound",
		[](GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data) {
			std::string name = data.value("audioName", std::string());
			int index = -1;
			if (ctx.audioClips) {
				for (size_t i = 0; i < ctx.audioClips->size(); i++) {
					if ((*ctx.audioClips)[i].displayName == name) { index = static_cast<int>(i); break; }
				}
			}
			float volume = data.value("volume", 1.0f);
			obj.AddComponent<SpawnSoundComponent>(ctx.audioClips, index, volume);
		},
		[](GameObject& obj) { return obj.RemoveComponent<SpawnSoundComponent>(); },
		"スポーンSE");

	// PlayButtonComponent：HitSoundComponentと同じ「名前→現在のindex変換」パターンに加え、
	// 色・スケール倍率の保存/復元があるためFromJsonも呼ぶ
	ComponentRegistry::Register<PlayButtonComponent>("PlayButton",
		[](GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data) {
			std::string name = data.value("audioName", std::string());
			int index = -1;
			if (ctx.audioClips) {
				for (size_t i = 0; i < ctx.audioClips->size(); i++) {
					if ((*ctx.audioClips)[i].displayName == name) { index = static_cast<int>(i); break; }
				}
			}
			float volume = data.value("volume", 1.0f);
			auto* button = obj.AddComponent<PlayButtonComponent>(ctx.audioClips, index, volume);
			button->FromJson(data);
		},
		[](GameObject& obj) { return obj.RemoveComponent<PlayButtonComponent>(); },
		"PLAYボタン");
}
