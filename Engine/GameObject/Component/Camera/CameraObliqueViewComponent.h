#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

// 箱全体を映す固定の斜め見下ろしカメラ用コンポーネント。GraviTwistのカメラ回転演出(Twist)は
// 過去にEuler分解由来の反転バグで廃止した経緯があるため、これはターゲットをlookAtで毎フレーム
// 追いかける方式ではなく、pitchDegrees/distanceという2つの数値だけからカメラの位置・向きを
// 一意に計算する「固定角度」方式にしてある（プレイヤーが重力反転でどの壁にいても、カメラの
// 見た目は常に一定になる）。
//
// 毎フレーム再計算しているのは、一度きりの初期化ではなくInspectorで数値をドラッグしながら
// エディタ上でリアルタイムに画角を確認できるようにするため（CameraComponentのpositionOffset/
// rotationOffsetと同じ設計思想）。
//
// このコンポーネント自身は位置・回転を決めるだけで、実際のView/Projection行列生成は
// 同じGameObjectに付けたCameraComponentが担当する（役割分担はCameraFollowComponentと同じ）。
class CameraObliqueViewComponent : public IComponent {
public:
	bool enabled = true;

	// 見下ろし角（度）。0で真横から水平に見る、90で真上から見下ろす
	float pitchDegrees = 50.0f;

	// 箱の中心（boxCenter）からカメラまでの距離
	float distance = 15.0f;

	// 箱の中心のワールド座標。カメラはここを向く
	Vector3 boxCenter = { 0.0f, 0.0f, 0.0f };

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
