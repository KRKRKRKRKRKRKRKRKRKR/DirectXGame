#include <Novice.h>
#include "ScoreSystem.h"
#include <imgui.h>

const char kWindowTitle[] = "コジマ";


enum Flow{
	Score,
	Ranking
};

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	Novice::Initialize(kWindowTitle, 1280, 720);
	char keys[256] = { 0 };
	char preKeys[256] = { 0 };

	ScoreSystem scoreSystem;
	int score = 0;

	Flow flow = Score;
	while (Novice::ProcessMessage() == 0) {
		Novice::BeginFrame();

		memcpy(preKeys, keys, 256);
		Novice::GetHitKeyStateAll(keys);

		
		ImGui::Begin("Ranking System");
		ImGui::SliderInt("Score", &score, 0, 9999);
		if (ImGui::Button("Reset Ranking")) {
			scoreSystem.Reset();
		};
		ImGui::End();


		switch (flow) {
		case Score:
			scoreSystem.InputName(keys,preKeys);

			scoreSystem.DrawScore(score);	
			if (keys[DIK_RETURN] && !preKeys[DIK_RETURN]) {
				scoreSystem.Add(score);
				flow = Ranking;	
			}
			break;

		case Ranking:
			scoreSystem.DrawRanking(keys,preKeys);
			if (keys[DIK_RETURN] && !preKeys[DIK_RETURN]) {
				flow = Score;
			}
			break;
		}






		/// --- 描画処理 ---

		Novice::EndFrame();

		if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) {
			break;
		}
	}

	Novice::Finalize();
	return 0;
}