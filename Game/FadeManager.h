#pragma once
#include "../Math/MathTypes.h"
#include <vector>

class Renderer;

// 画面全体をモザイク状の四角（セル）で左から右へ順に埋め尽くす/消していく汎用フェード演出。
// GameSessionと同じ「アプリ全体で1つだけ生きているシングルトン」方式（Meyerのシングルトン）を
// 踏襲する。シーンをまたいで状態を持ち回りたい（フェードインで画面を覆ったまま次のシーンへ切り替え、
// 切り替え後にフェードアウトする）ため、GameObject/Component（シーン切替のたびに作り直される）
// ではなくシングルトンに状態を置く。
//
// 描画はGameObjectを介さず、SceneBase::RenderがRenderMainPass()の直後・DrawEditorUiIfVisible()の
// 前でDraw()を直接呼ぶ（DrawGrid等の「シーンが直接Rendererを叩く」既存パターンに倣う）。
class FadeManager {
public:
	// フェードの状態遷移：kIdle（何もしていない、非表示）→ kFadingIn（左から右へ埋めていく途中）→
	// kCovered（全セルが埋まって画面全体が覆われた、静止状態）→ kFadingOut（左から右へ消していく途中）→
	// kIdle に戻る
	enum class State { kIdle, kFadingIn, kCovered, kFadingOut };

	static FadeManager& GetInstance();

	// フェードイン（画面を埋めていく演出）を開始する。既に実行中の場合は最初からやり直す
	void StartFadeIn();

	// フェードアウト（画面を消していく演出）を開始する。kCovered状態でない場合も強制的に開始する
	// （呼び出し側が状態を気にせず呼べるようにするため）
	void StartFadeOut();

	// 毎フレーム呼ぶ。State::kFadingIn/kFadingOut中はelapsed_を進めるだけで、実際のセル配置計算は
	// Draw()側で行う（elapsed_さえ進んでいればDrawを呼ばないフレームがあっても問題ない設計にするため）
	void Update(float deltaTime);

	// State::kIdle以外のとき、現在の進行状況に応じたモザイクセルを描画する。kIdle中は何もしない
	void Draw(Renderer* renderer) const;

	State GetState() const { return state_; }

	// State::kCoveredに入り、さらにcoveredHoldDuration秒経過した後にtrueを返す。SceneManagerが
	// 「画面が完全に覆われたことが視覚的にも確定してからシーンを切り替える」タイミング判定に使う。
	// kCoveredに入った直後（elapsed_がまだcoveredElapsed_を経過していない間）はfalseのままにする
	// ことで、切替タイミングが最後のセルのポップイン完了とギリギリ同フレームになるのを避ける
	bool IsCovered() const { return state_ == State::kCovered && coveredElapsed_ >= coveredHoldDuration; }

	// ---- 演出パラメータ（Inspector的に調整したい場合はここを直接書き換える。現状は
	// GameSessionと同じくコード上の既定値のみで、専用Inspector UIは持たない） ----

	// モザイクの列数・行数
	int columnCount = 12;
	int rowCount = 6;

	// 1列（同じ列内の全行）が完全に埋まりきるまでの所要時間（秒）。列ごとにこの時間だけ
	// ずらして開始することで、全体としては「左から右へ流れる」ように見える
	float columnDuration = 0.05f;

	// 同じ列内でも行ごとに開始タイミングをランダムにずらす最大幅（秒）。0にすると
	// 同じ列の全行が完全に同時に現れる
	float rowJitter = 0.08f;

	// 1セルが0→フルサイズへポップインするのにかかる時間（秒）
	float cellPopDuration = 0.12f;

	// State::kCoveredに入ってから、実際にIsCovered()がtrueを返すまでの追加の待機時間（秒）。
	// 0だと「最後のセルがフルサイズに到達したちょうどそのフレーム」で即座にIsCovered()が
	// trueになり、SceneManagerが同じフレームでChangeSceneを実行してしまう。イージング
	// （kOutBack）が数値上1.0にきっちり収束していても、そのフレームがちょうどPresentされる
	// 前にシーン切替の重い処理（GameObject全破棄・シーンJSON読込）が挟まると、GPU側の描画と
	// CPU側の状態遷移の間でわずかなズレが生じやすい。数フレーム分の余裕を持たせることで、
	// 「完全に覆われた」ことが視覚的にも確定してから切り替えるようにする
	float coveredHoldDuration = 0.5f;

	// フェード色・不透明度
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

private:
	FadeManager() = default;

	// 列インデックスcolumn・行インデックスrowのセルが「開始」する基準時刻（秒、elapsed_と
	// 同じ時間軸）を返す。列の基準時刻 + 行ごとのランダムジッターの合計
	float GetCellStartTime(int column, int row) const;

	State state_ = State::kIdle;
	float elapsed_ = 0.0f;

	// State::kCoveredに入ってからの経過時間。coveredHoldDuration秒に達するまでIsCovered()は
	// falseを返す（詳細はIsCovered()のコメント参照）
	float coveredElapsed_ = 0.0f;

	// 行ごとのジッター値（0〜rowJitterの範囲）。StartFadeIn/StartFadeOutのたびに
	// rowCount個ぶん再抽選する（列をまたいで同じ行は同じジッター値を使う＝毎回作り直すのは
	// 「その行がどれだけ早い/遅いか」という個性を全列で一貫させ、より意図的な揺らぎに見せるため）
	mutable std::vector<float> rowJitterValues_;
	mutable int rowJitterValuesForRowCount_ = -1; // rowJitterValues_を計算した時点のrowCount（変更検知用）

	void EnsureRowJitterValues() const;

	// 全セルが埋まりきる（フェードインなら最後の列が完全にポップイン完了する）までの
	// 所要時間（秒）。columnCount * columnDuration + rowJitterの最大幅 + cellPopDuration
	float GetTotalDuration() const;
};
