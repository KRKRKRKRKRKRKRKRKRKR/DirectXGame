#pragma once
#include "RenderComponentBase.h"

// 大量のCubeを格子状に並べ、天井・床の2面として配置し、各Cubeの高さ（面法線方向）を
// sin波で周期的にずらして「波打つブロックの天井・床がある暗い空間」を表現する背景。
// 1つのGameObjectに付けるだけで、内部ループでRenderer::DrawCubeを直接呼んで全インスタンスを
// 描画する（ParticleComponentのようにセルごとにGameObjectを生成する方式は、9万個規模の
// 過去の負荷テストでSceneBase側の走査コストが支配的だったため採用しない。詳細はDraw内コメント参照）。
// Titleに限らず各シーン共通の背景として、カメラの後方に1個配置する運用を想定する
class GridBackgroundComponent : public RenderComponentBase {
public:
	// 縦横のセル数（片面あたり）。行×列×2面ぶんのCubeを毎フレーム描画する
	int   columns = 50;
	int   rows = 50;
	// セルの中心間隔（ワールド単位）
	float cellSpacing = 1.2f;
	// 天井・床のY座標（中心グリッドからの基準面高さ）。ceilingY > floorYを想定
	float ceilingY = 20.0f;
	float floorY = -20.0f;
	// 各Cubeの一辺の長さ（見た目のブロックサイズ。cellSpacingよりわずかに小さくして
	// タイル間に目地の隙間を作る）
	float cellSize = 1.0f;

	// 波アニメーション。振幅(waveAmplitude)ぶん、各セルのY座標を揺らす。単一のsin波だけだと
	// 全セルが同じ斜め縞状に規則的に動いて単調に見えるため、X軸とZ軸で別々の周波数・速度の
	// sin波を掛け合わせたうえ、さらに高周波の波（waveDetailAmplitude/waveDetailFrequency）を
	// 重ねる2オクターブ構成にして、より不規則な波面に見せる
	float waveAmplitude = 0.6f;
	float waveFrequency = 0.25f;
	float waveSpeed = 0.8f;

	// 波の2つ目のオクターブ（waveFrequencyより高い周波数の細かい波を重ねる）。
	// 0にすると単一のsin波だけの見た目に戻る
	float waveDetailAmplitude = 0.25f;
	float waveDetailFrequency = 0.9f;
	float waveDetailSpeed = -1.3f; // 主要な波と逆方向・別速度で動かし、干渉によるうねりを出す

	// セルごとのランダムなY方向オフセット（sin波の上に重ねる）。1個1個のCubeが
	// バラバラな高さに見えるノイズ状の凹凸を作る。乱数はrandomSeedを種として
	// セル座標から決定的に生成するため、フレームをまたいでも同じセルは同じ値のまま
	// （ImGuiのランダム化ボタンでrandomSeedを引き直すまで固定）
	float randomOffsetAmplitude = 1.5f;
	int   randomSeed = 12345;

	// セルごとのランダムなサイズ倍率を[sizeScaleMin, sizeScaleMax]の範囲から決める
	// （ReflexEnemySpawnerComponent::sizeScaleMin/sizeScaleMaxと同じ「テンプレートに対する
	// 倍率を範囲指定でランダム化する」方式に合わせた）。cellSizeにこの倍率を掛けた値が
	// 実際のCube一辺の長さになる。位置オフセットと同じrandomSeedを使うが、内部のハッシュ関数に
	// 別のsaltを渡して無相関な値を取るため、サイズと高さが連動しない
	// （大きいCubeが必ず高い位置に来る、といったパターンが出ない）
	float sizeScaleMin = 0.5f;
	float sizeScaleMax = 1.5f;

	// 奥行き方向（row=0を手前、row=rows-1を最奥とみなす）に応じてCubeサイズを縮小する係数。
	// 0=奥行きによる縮小なし、1=最奥列でサイズが0まで小さくなる。カメラの実際の位置・向きは
	// 一切参照せず、グリッド上の行インデックスだけで決まる単純な演出（奥へ続いているような
	// 遠近の錯覚を強めるための見た目上のトリックで、実際の遠近投影とは別物）
	float depthShrink = 0.6f;

	// 天井・床それぞれの基本色（RenderComponentBase::colorは使わず、面ごとに個別の色を持つ。
	// 暗い空間の奥にうっすら赤い光が見える、というムードを狙う場合はfloorColorに赤みを足す）
	Vector4 ceilingColor = { 0.55f, 0.6f, 0.62f, 1.0f };
	Vector4 floorColor   = { 0.55f, 0.6f, 0.62f, 1.0f };

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

private:
	// 波アニメーション用に蓄積する経過時間。Update側でだけ加算し、Drawは読むだけ
	// （DrawはRenderMirrorPass等からdeltaTime=0固定で呼ばれることがあるため、
	// Draw自身で時間を進めると鏡越しの見た目が変わってしまう）
	float elapsedTime_ = 0.0f;
};
