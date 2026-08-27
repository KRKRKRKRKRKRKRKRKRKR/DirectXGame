#include "FadeManager.h"
#include "../Math/Easing.h"
#include "../Engine/Graphics/Renderer/Renderer.h"
#include <algorithm>
#include <random>

FadeManager& FadeManager::GetInstance() {
	static FadeManager instance;
	return instance;
}

void FadeManager::EnsureRowJitterValues() const {
	if (rowJitterValuesForRowCount_ == rowCount && static_cast<int>(rowJitterValues_.size()) == rowCount) return;

	rowJitterValues_.resize(rowCount);
	static std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist(0.0f, (std::max)(rowJitter, 0.0f));
	for (int i = 0; i < rowCount; i++) {
		rowJitterValues_[i] = dist(rng);
	}
	rowJitterValuesForRowCount_ = rowCount;
}

float FadeManager::GetCellStartTime(int column, int row) const {
	EnsureRowJitterValues();
	float columnStart = static_cast<float>(column) * (std::max)(columnDuration, 0.001f);
	float jitter = (row >= 0 && row < static_cast<int>(rowJitterValues_.size())) ? rowJitterValues_[row] : 0.0f;
	return columnStart + jitter;
}

float FadeManager::GetTotalDuration() const {
	int lastColumn = (std::max)(columnCount - 1, 0);
	float lastColumnStart = static_cast<float>(lastColumn) * (std::max)(columnDuration, 0.001f);
	return lastColumnStart + (std::max)(rowJitter, 0.0f) + (std::max)(cellPopDuration, 0.001f);
}

void FadeManager::StartFadeIn() {
	state_ = State::kFadingIn;
	elapsed_ = 0.0f;
	coveredElapsed_ = 0.0f;
	rowJitterValuesForRowCount_ = -1; // 毎回のフェードで揺らぎのパターンを引き直す
}

void FadeManager::StartFadeOut() {
	state_ = State::kFadingOut;
	elapsed_ = 0.0f;
	coveredElapsed_ = 0.0f;
	rowJitterValuesForRowCount_ = -1;
}

void FadeManager::Update(float deltaTime) {
	if (state_ == State::kIdle) return;

	// シーン切替直後（State::kFadingOutの最初のUpdate）は、ChangeSceneが行うシーンJSON読込・
	// GameObject/アセット生成が重く、そのフレームのdeltaTimeが数百ミリ秒単位に跳ね上がることがある。
	// この巨大なdeltaTimeをそのままelapsed_に加算すると、フェードアウトの全行程（数フレームかけて
	// 左から右へ消えていくはずの演出）が1フレームで一気に完了扱いになり、旧シーンの残像が消えた
	// 直後に新シーンがいきなりフル表示される（＝「一瞬シーンが見える」ように見える）原因になっていた。
	// フェード演出用のdeltaTimeだけ、1フレームあたりの上限をかけて異常なフレームスパイクを吸収する
	constexpr float kMaxDeltaTime = 1.0f / 30.0f; // 30fps相当を下限とする（これより大きい間隔は複数フレームに分割された扱いにする）
	float clampedDeltaTime = (std::min)(deltaTime, kMaxDeltaTime);

	if (state_ == State::kCovered) {
		// 完全に覆われた状態を維持しつつ、IsCovered()がtrueを返すまでの猶予（coveredHoldDuration）を
		// 数える。SceneManagerはこの間ChangeSceneを実行しないため、画面が確実に真っ白な状態を
		// 経由してからシーンが切り替わる
		coveredElapsed_ += clampedDeltaTime;
		return;
	}

	elapsed_ += clampedDeltaTime;
	float totalDuration = GetTotalDuration();
	if (elapsed_ >= totalDuration) {
		// フェードインが完了したら「全面が覆われた」静止状態に入る（SceneManagerがIsCovered()で
		// この状態を検知してシーン切替を行う）。フェードアウトが完了したら通常の非表示状態に戻る
		if (state_ == State::kFadingIn) {
			state_ = State::kCovered;
			elapsed_ = totalDuration; // Draw()側の計算が全セル埋まりきった状態で安定するようクランプ
			coveredElapsed_ = 0.0f;
		} else if (state_ == State::kFadingOut) {
			state_ = State::kIdle;
			elapsed_ = 0.0f;
		}
	}
}

void FadeManager::Draw(Renderer* renderer) const {
	if (!renderer) return;
	if (state_ == State::kIdle) return;

	int columns = (std::max)(columnCount, 1);
	int rows = (std::max)(rowCount, 1);
	float totalDuration = GetTotalDuration();

	float designWidth = Renderer::GetUiDesignWidth();
	float designHeight = Renderer::GetUiDesignHeight();
	float cellWidth = designWidth / static_cast<float>(columns);
	float cellHeight = designHeight / static_cast<float>(rows);
	float popDuration = (std::max)(cellPopDuration, 0.001f);

	for (int column = 0; column < columns; column++) {
		for (int row = 0; row < rows; row++) {
			float cellStart = GetCellStartTime(column, row);

			// フェードインは「開始時刻を過ぎたらポップインし始める」、フェードアウトは
			// 「フェード全体の中で自分の番が来るまでは満杯（scale=1）のまま、来たら縮んで消える」
			// という対称な動きにする（同じセル配置・タイミング計算をイン/アウトで使い回すため）
			// State::kCoveredは「フェードインが完了して静止している状態」であり、fillTの計算式
			// （kFadingIn以外はすべて1.0-tという「フェードアウト用」の式）をそのまま適用すると、
			// t=1.0（ポップイン完了）のときfillT=0.0になってしまい、全セルが描かれない
			// （＝旧シーンがそのまま透けて見える）バグになっていた。kCoveredは常にfillT=1.0固定にする
			float fillT;
			if (state_ == State::kCovered) {
				fillT = 1.0f;
			} else {
				float localElapsed = elapsed_ - cellStart;
				float t = (popDuration > 0.0f) ? std::clamp(localElapsed / popDuration, 0.0f, 1.0f) : 1.0f;
				fillT = (state_ == State::kFadingIn) ? t : (1.0f - t);
			}
			if (fillT <= 0.0f) continue; // まだ/もう何も描かない

			float eased = Easing::Apply(Easing::Type::kOutBack, fillT);
			float scale = std::clamp(eased, 0.0f, 1.2f); // kOutBackは僅かにオーバーシュートするため上限を設ける

			Transform cellTransform;
			cellTransform.translation = {
				(static_cast<float>(column) + 0.5f) * cellWidth,
				(static_cast<float>(row) + 0.5f) * cellHeight,
				0.0f };
			// セル同士の隙間を無くすため、フル状態(scale=1)のとき隣接セルとぴったり接するサイズにする。
			// ポップイン演出中はこのサイズにscaleを掛けて0から広がるように見せる
			cellTransform.scale = { cellWidth * scale, cellHeight * scale, 1.0f };

			renderer->DrawSprite2D(cellTransform, color);
		}
	}
}
