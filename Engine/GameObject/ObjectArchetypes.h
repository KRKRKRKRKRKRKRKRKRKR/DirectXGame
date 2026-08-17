#pragma once
#include "../../Externals/Json/json.hpp"
#include "../../Math/MathTypes.h"
#include <string>
#include <vector>

// "Objects"パネルの"Create"から選べるアーキタイプ（コンポーネント構成のひな形）を提供するクラス。
// 既存のJSON復元パイプライン（ComponentRegistry＋GameObject::FromJson）にそのまま流し込める
// 形のJSONを返すだけで、GameObjectそのものの生成には関与しない。
// Model/Sprite/AudioSourceはファイルパス等の追加入力が要るため対象外とし、値だけで完結する種類だけを扱う
class ObjectArchetypes {
public:
	static const std::vector<std::string>& GetNames();
	static nlohmann::json GetJson(const std::string& name);

	// 方向ベクトル→オイラー角(ラジアン)。Directional/Spot Lightの初期回転をデフォルト方向値から
	// 逆算するために使う（PlayScene::Initialize()の初期デモオブジェクトからも共用）
	static Vector3 DirectionToEulerRadians(const Vector3& direction);

private:
	static nlohmann::json MakeCubeArchetype();
	static nlohmann::json MakeSphereArchetype();
	static nlohmann::json MakeTriangleArchetype();
	static nlohmann::json MakeDirectionalLightArchetype();
	static nlohmann::json MakePointLightArchetype();
	static nlohmann::json MakeSpotLightArchetype();
	static nlohmann::json MakeCameraArchetype();

	// REFLEX敵テンプレート一式（見た目＋当たり判定＋ReflexEnemy(isTemplate=true)）を
	// まとめて付与するアーキタイプ。型名は文字列でComponentRegistryへ渡すだけなので、
	// Engine層のこのクラスがGame層の具体的なコンポーネント型を直接知る必要はない
	// （既存のMakeCubeArchetype等と同じ考え方）
	static nlohmann::json MakeEnemyTemplateArchetype();
};
