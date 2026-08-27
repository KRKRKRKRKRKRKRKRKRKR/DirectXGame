#include "GridBackgroundComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Math/MatrixMath.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace {
// セル座標(col, row, face)とシード・salt（位置用/サイズ用など用途ごとに別の値を取るための
// 追加パラメータ）から[-1, 1]の値を決定的に生成する。std::mt19937等は呼び出しのたびに内部状態が
// 変わるためセルごとに固定した値を求めるのに向かない。整数値をビット拡散させるだけの
// 軽量ハッシュ（PCG系のfinalizerでよく使われる形）で十分にばらけた疑似乱数が得られる。
// saltを変えることで同じセルでも位置用とサイズ用に無相関な値を取れる（大きいCubeが必ず
// 高い位置に来る、といった見た目のパターンが出ないようにするため）
float HashToUnitRange(int col, int row, int face, int seed, int salt) {
	uint32_t h = static_cast<uint32_t>(col) * 374761393u
		+ static_cast<uint32_t>(row) * 668265263u
		+ static_cast<uint32_t>(face) * 2246822519u
		+ static_cast<uint32_t>(seed) * 3266489917u
		+ static_cast<uint32_t>(salt) * 2654435761u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h = h ^ (h >> 16);
	// 上位23bitを[0,1)の float にマッピングしてから[-1,1)へ
	constexpr uint32_t kMantissaMax = 1u << 23;
	float unit = static_cast<float>(h % kMantissaMax) / static_cast<float>(kMantissaMax);
	return unit * 2.0f - 1.0f;
}
}

void GridBackgroundComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)transform;
	(void)ctx;
	elapsedTime_ += deltaTime;
}

void GridBackgroundComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	if (!renderer) return;
	if (columns <= 0 || rows <= 0) return;

	// 回転しない静止グリッドのため、worldInverseTransposeは常に単位行列でよい。
	// Renderer::DrawCubeのworldInverseTranspose指定版を使うことで、通常版が毎回行う
	// Inverse(world)計算を省略できる（過去の大量Cube描画の負荷テストを踏まえた対策）
	static const Matrix4x4 kIdentity = MatrixMath::Identity();

	// グリッド全体をtransform.translationを中心に配置する。列・行はX/Z平面に広げ、
	// 中心が原点に来るよう(columns-1)/2, (rows-1)/2ぶんオフセットする
	float halfWidth  = (static_cast<float>(columns) - 1.0f) * 0.5f * cellSpacing;
	float halfDepth  = (static_cast<float>(rows) - 1.0f) * 0.5f * cellSpacing;

	Transform cube;
	cube.rotation = { 0.0f, 0.0f, 0.0f };

	for (int face = 0; face < 2; face++) {
		bool isCeiling = (face == 0);
		float baseY = isCeiling ? ceilingY : floorY;
		float waveSign = isCeiling ? -1.0f : 1.0f; // 天井は下向き、床は上向きに波が膨らむ見た目にする
		const Vector4& color = isCeiling ? ceilingColor : floorColor;

		for (int row = 0; row < rows; row++) {
			float z = transform.translation.z - halfDepth + static_cast<float>(row) * cellSpacing;
			for (int col = 0; col < columns; col++) {
				float x = transform.translation.x - halfWidth + static_cast<float>(col) * cellSpacing;

				// X軸とZ軸で別々の位相を持つsin波を掛け合わせることで、単純な斜め縞ではなく
				// 格子状に膨らみ・へこみが散らばった波面にする（sin(x)*sin(z)は市松模様的な
				// 凹凸パターンになり、単一のsin(x+z)より不規則に見える）
				float mainPhaseX = x * waveFrequency + elapsedTime_ * waveSpeed;
				float mainPhaseZ = z * waveFrequency * 0.7f + elapsedTime_ * waveSpeed * 0.6f;
				float mainWave = std::sinf(mainPhaseX) * std::sinf(mainPhaseZ);

				// 高周波の波（2オクターブ目）を重ねて、より細かい不規則さを足す
				float detailPhaseX = x * waveDetailFrequency + elapsedTime_ * waveDetailSpeed;
				float detailPhaseZ = z * waveDetailFrequency * 1.3f - elapsedTime_ * waveDetailSpeed * 0.5f;
				float detailWave = std::sinf(detailPhaseX + detailPhaseZ);

				float waveY = waveSign * (mainWave * waveAmplitude + detailWave * waveDetailAmplitude);

				// sin波の上に、セルごとに固定のランダムオフセットを重ねる（1個1個の
				// Cubeがバラバラな高さに見えるノイズ状の凹凸を作る）
				float randomY = HashToUnitRange(col, row, face, randomSeed, 0) * randomOffsetAmplitude;

				float y = transform.translation.y + baseY + waveY + randomY;

				// Cubeサイズも位置とは独立の乱数（salt=1）でセルごとにばらつかせる。
				// ReflexEnemySpawnerComponent::sizeScaleMin/Maxと同じ「範囲から直接倍率を引く」方式
				float sizeUnit = (HashToUnitRange(col, row, face, randomSeed, 1) + 1.0f) * 0.5f; // [0,1)
				float scaleMin = sizeScaleMin;
				float scaleMax = (std::max)(sizeScaleMax, scaleMin); // Enemy側と同じくmax<minの保険
				float sizeScale = scaleMin + sizeUnit * (scaleMax - scaleMin);
				float size = cellSize * sizeScale;

				// 奥行き（row=0が手前、row=rows-1が最奥）に応じてさらに縮小する。
				// カメラの実座標は見ず、グリッド上の行位置だけで決まる単純な減衰
				float depthRatio = (rows > 1) ? static_cast<float>(row) / static_cast<float>(rows - 1) : 0.0f;
				size *= (1.0f - depthRatio * depthShrink);
				if (size < 0.01f) size = 0.01f; // 0以下にはしない（描画自体は残す）
				cube.scale = { size, size, size };

				cube.translation = { x, y, z };
				renderer->DrawCube(cube, kIdentity, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
			}
		}
	}
}

void GridBackgroundComponent::DrawImGui(const char* namePrefix) {
	RenderComponentBase::DrawImGui(namePrefix);

	ImGui::Separator();
	ImGui::Text("グリッド");
	std::string columnsLabel = std::string(namePrefix) + "列数";
	std::string rowsLabel = std::string(namePrefix) + "行数";
	std::string spacingLabel = std::string(namePrefix) + "セル間隔";
	std::string cellSizeLabel = std::string(namePrefix) + "Cubeサイズ";
	ImGui::DragInt(columnsLabel.c_str(), &columns, 1.0f, 1, 200);
	ImGui::DragInt(rowsLabel.c_str(), &rows, 1.0f, 1, 200);
	ImGui::DragFloat(spacingLabel.c_str(), &cellSpacing, 0.05f, 0.1f, 20.0f);
	ImGui::DragFloat(cellSizeLabel.c_str(), &cellSize, 0.05f, 0.05f, 20.0f);

	ImGui::Separator();
	ImGui::Text("天井・床の高さ");
	std::string ceilingYLabel = std::string(namePrefix) + "天井Y座標";
	std::string floorYLabel = std::string(namePrefix) + "床Y座標";
	ImGui::DragFloat(ceilingYLabel.c_str(), &ceilingY, 0.1f, -200.0f, 200.0f);
	ImGui::DragFloat(floorYLabel.c_str(), &floorY, 0.1f, -200.0f, 200.0f);

	ImGui::Separator();
	ImGui::Text("波アニメーション");
	std::string amplitudeLabel = std::string(namePrefix) + "振幅";
	std::string frequencyLabel = std::string(namePrefix) + "周波数";
	std::string speedLabel = std::string(namePrefix) + "速度";
	ImGui::DragFloat(amplitudeLabel.c_str(), &waveAmplitude, 0.05f, 0.0f, 20.0f);
	ImGui::DragFloat(frequencyLabel.c_str(), &waveFrequency, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat(speedLabel.c_str(), &waveSpeed, 0.05f, -10.0f, 10.0f);

	ImGui::Text("波アニメーション（細かい波を重ねる）");
	std::string detailAmplitudeLabel = std::string(namePrefix) + "細波の振幅";
	std::string detailFrequencyLabel = std::string(namePrefix) + "細波の周波数";
	std::string detailSpeedLabel = std::string(namePrefix) + "細波の速度";
	ImGui::DragFloat(detailAmplitudeLabel.c_str(), &waveDetailAmplitude, 0.05f, 0.0f, 20.0f);
	ImGui::DragFloat(detailFrequencyLabel.c_str(), &waveDetailFrequency, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat(detailSpeedLabel.c_str(), &waveDetailSpeed, 0.05f, -10.0f, 10.0f);

	ImGui::Separator();
	ImGui::Text("Cubeごとのランダムなばらつき");
	std::string randomAmplitudeLabel = std::string(namePrefix) + "ランダムオフセット振幅";
	std::string reseedLabel = std::string(namePrefix) + "ばらつきを引き直す";
	ImGui::DragFloat(randomAmplitudeLabel.c_str(), &randomOffsetAmplitude, 0.05f, 0.0f, 20.0f);

	// ReflexEnemySpawnerComponentの「スポーン時のサイズランダム化（テンプレートに対する倍率）」と
	// 同じUI文言・仕組みに揃える
	ImGui::Text("Cubeサイズのランダム化（cellSizeに対する倍率）");
	std::string sizeScaleMinLabel = std::string(namePrefix) + "最小倍率";
	std::string sizeScaleMaxLabel = std::string(namePrefix) + "最大倍率";
	ImGui::DragFloat(sizeScaleMinLabel.c_str(), &sizeScaleMin, 0.02f, 0.1f, 5.0f);
	ImGui::DragFloat(sizeScaleMaxLabel.c_str(), &sizeScaleMax, 0.02f, 0.1f, 5.0f);
	// 最大が最小を下回ると乱数範囲が壊れるため、UI操作直後に矛盾した値になった場合は
	// ここで最小側へ揃えて防ぐ（PlayScene::SpawnEnemyAtのsizeScaleMin/Max抽選と同じガード）
	if (sizeScaleMax < sizeScaleMin) {
		sizeScaleMax = sizeScaleMin;
	}

	std::string depthShrinkLabel = std::string(namePrefix) + "奥行きによる縮小の強さ";
	ImGui::DragFloat(depthShrinkLabel.c_str(), &depthShrink, 0.01f, 0.0f, 1.0f);
	if (ImGui::Button(reseedLabel.c_str())) {
		// 押すたびに違う値になればよいだけなので、フレームカウントに適当な奇数を掛けて種にする
		randomSeed = static_cast<int>(static_cast<uint32_t>(ImGui::GetFrameCount()) * 2654435761u);
	}

	ImGui::Separator();
	ImGui::Text("面ごとの色");
	std::string ceilingColorLabel = std::string(namePrefix) + "天井色";
	std::string floorColorLabel = std::string(namePrefix) + "床色";
	ImGui::ColorEdit4(ceilingColorLabel.c_str(), &ceilingColor.x);
	ImGui::ColorEdit4(floorColorLabel.c_str(), &floorColor.x);
}

void GridBackgroundComponent::ToJson(nlohmann::json& out) const {
	RenderComponentBase::ToJson(out);
	out["columns"] = columns;
	out["rows"] = rows;
	out["cellSpacing"] = cellSpacing;
	out["ceilingY"] = ceilingY;
	out["floorY"] = floorY;
	out["cellSize"] = cellSize;
	out["waveAmplitude"] = waveAmplitude;
	out["waveFrequency"] = waveFrequency;
	out["waveSpeed"] = waveSpeed;
	out["waveDetailAmplitude"] = waveDetailAmplitude;
	out["waveDetailFrequency"] = waveDetailFrequency;
	out["waveDetailSpeed"] = waveDetailSpeed;
	out["randomOffsetAmplitude"] = randomOffsetAmplitude;
	out["randomSeed"] = randomSeed;
	out["sizeScaleMin"] = sizeScaleMin;
	out["sizeScaleMax"] = sizeScaleMax;
	out["depthShrink"] = depthShrink;
	out["ceilingColor"] = Vector4ToJson(ceilingColor);
	out["floorColor"] = Vector4ToJson(floorColor);
}

void GridBackgroundComponent::FromJson(const nlohmann::json& in) {
	RenderComponentBase::FromJson(in);
	columns = in.value("columns", columns);
	rows = in.value("rows", rows);
	cellSpacing = in.value("cellSpacing", cellSpacing);
	ceilingY = in.value("ceilingY", ceilingY);
	floorY = in.value("floorY", floorY);
	cellSize = in.value("cellSize", cellSize);
	waveAmplitude = in.value("waveAmplitude", waveAmplitude);
	waveFrequency = in.value("waveFrequency", waveFrequency);
	waveSpeed = in.value("waveSpeed", waveSpeed);
	waveDetailAmplitude = in.value("waveDetailAmplitude", waveDetailAmplitude);
	waveDetailFrequency = in.value("waveDetailFrequency", waveDetailFrequency);
	waveDetailSpeed = in.value("waveDetailSpeed", waveDetailSpeed);
	randomOffsetAmplitude = in.value("randomOffsetAmplitude", randomOffsetAmplitude);
	randomSeed = in.value("randomSeed", randomSeed);
	sizeScaleMin = in.value("sizeScaleMin", sizeScaleMin);
	sizeScaleMax = in.value("sizeScaleMax", sizeScaleMax);
	depthShrink = in.value("depthShrink", depthShrink);
	if (in.contains("ceilingColor")) ceilingColor = Vector4FromJson(in["ceilingColor"]);
	if (in.contains("floorColor")) floorColor = Vector4FromJson(in["floorColor"]);
}

REGISTER_SIMPLE_COMPONENT(GridBackgroundComponent, "GridBackground", "波打つ天井床グリッド背景", "描画");
