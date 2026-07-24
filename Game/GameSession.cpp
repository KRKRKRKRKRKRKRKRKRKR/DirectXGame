#include "GameSession.h"

GameSession& GameSession::GetInstance() {
	static GameSession instance;
	return instance;
}

void GameSession::Reset() {
	score_ = 0;
	combo_ = 0;
	enteredName_.clear();
}
