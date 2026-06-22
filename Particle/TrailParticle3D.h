#pragma once
#include <vector>
#include <random>
#include "../Math/MathTypes.h"

struct TrailParticleParameter {
	// --- 軌道 ---
	Vector3 fixedStartPos = Vector3(0.0f, 30.0f, 0.0f);
	float   goalAreaRadius = 10.0f;
	float   goalY = 0.0f;
	float   fallSpeed = 0.2f;

	// --- パーティクル ---
	int     maxParticles = 100;
	float   lifeTimeMax = 40.0f;
	float   sizeStart = 1.0f;
	float   sizeEnd = 0.0f;
	Vector4 colorStart = Vector4(1.0f, 0.6f, 0.2f, 1.0f);
	Vector4 colorEnd = Vector4(1.0f, 0.2f, 0.0f, 0.0f);
	float   spawnDistance = 0.3f;
	float   spawnRadius = 0.15f;
	float   rotationSpeed = 0.05f; // パーティクルの回転速度
};

struct TrailParticleInfo {
	Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 rotation = Vector3(0.0f, 0.0f, 0.0f);
	float   scale = 1.0f;
	float   lifeTimer = 0.0f;
	float   lifeTimeMax = 1.0f;
	Vector4 color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	bool    isActive = false;
	float   rotationSpeed = 0.05f; // スポーン時に設定
};

class TrailParticle3D {
public:
	void Init(const TrailParticleParameter& param);
	void Update(Vector3& outTranslation, Vector3& outRotation);

	const std::vector<TrailParticleInfo>& GetParticles() const { return particles_; }
	const std::vector<int>& GetActiveList() const { return activeList_; }

	void SetParameter(const TrailParticleParameter& param) { param_ = param; }
private:
	void    Emit(const Vector3& current, const Vector3& prev, const Vector3& rotation);
	void    Reset();
	void    ResetFall();
	Vector3 GetRandomGoalPos() const;

	TrailParticleParameter         param_;
	std::vector<TrailParticleInfo> particles_;
	std::vector<int>               freeList_;
	std::vector<int>               activeList_;

	Vector3 prevPos_ = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 direction_ = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 goalPos_ = Vector3(0.0f, 0.0f, 0.0f);

	mutable std::mt19937 random_{ std::random_device{}() };
};