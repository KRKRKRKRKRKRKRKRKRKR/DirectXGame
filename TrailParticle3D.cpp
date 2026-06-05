#include "TrailParticle3D.h"
#include <cmath>

void TrailParticle3D::Init(const TrailParticleParameter& param) {
    param_ = param;
    particles_.resize(param_.maxParticles);

    freeList_.clear();
    freeList_.reserve(param_.maxParticles);
    activeList_.clear();
    activeList_.reserve(param_.maxParticles);

    for (int i = 0; i < param_.maxParticles; i++) {
        particles_[i].isActive = false;
        freeList_.push_back(i);
    }

    ResetFall();
}

void TrailParticle3D::Update(Vector3& outTranslation, Vector3& outRotation) {
    // 方向ベクトルに沿って進む
    outTranslation.x += direction_.x * param_.fallSpeed;
    outTranslation.y += direction_.y * param_.fallSpeed;
    outTranslation.z += direction_.z * param_.fallSpeed;

    Emit(outTranslation, prevPos_, outRotation);
    prevPos_ = outTranslation;

    // パーティクル更新
    for (int i = static_cast<int>(activeList_.size()) - 1; i >= 0; i--) {
        int index = activeList_[i];
        TrailParticleInfo& p = particles_[index];

        p.lifeTimer -= 1.0f;
        float t = 1.0f - (p.lifeTimer / p.lifeTimeMax);
        p.rotation.y += p.rotationSpeed;
        p.scale = param_.sizeStart + (param_.sizeEnd - param_.sizeStart) * t;
        p.color.x = param_.colorStart.x + (param_.colorEnd.x - param_.colorStart.x) * t;
        p.color.y = param_.colorStart.y + (param_.colorEnd.y - param_.colorStart.y) * t;
        p.color.z = param_.colorStart.z + (param_.colorEnd.z - param_.colorStart.z) * t;
        p.color.w = param_.colorStart.w + (param_.colorEnd.w - param_.colorStart.w) * t;

        if (p.lifeTimer <= 0.0f) {
            p.isActive = false;
            freeList_.push_back(index);
            activeList_[i] = activeList_.back();
            activeList_.pop_back();
        }
    }


    if (outTranslation.y <= param_.goalY) {
        ResetFall();
        outTranslation = param_.fixedStartPos;
    }
}

void TrailParticle3D::Emit(const Vector3& current, const Vector3& prev, const Vector3& rotation) {
    float dx = current.x - prev.x;
    float dy = current.y - prev.y;
    float dz = current.z - prev.z;
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);

    int spawnCount = 1 + static_cast<int>(distance / param_.spawnDistance);

    std::uniform_real_distribution<float> distRadius(0.0f, param_.spawnRadius);
    std::uniform_real_distribution<float> distAngle(0.0f, 6.2831853f);

    for (int s = 0; s < spawnCount; s++) {
        if (freeList_.empty()) break;

        int index = freeList_.back();
        freeList_.pop_back();
        TrailParticleInfo& p = particles_[index];

        float ratio = static_cast<float>(s) / static_cast<float>(spawnCount);
        float angle = distAngle(random_);
        float radius = distRadius(random_);

        p.position = Vector3(
            prev.x + dx * ratio + cosf(angle) * radius,
            prev.y + dy * ratio,
            prev.z + dz * ratio + sinf(angle) * radius
        );
        p.rotation = rotation;
        p.scale = param_.sizeStart;
        p.color = param_.colorStart;
        p.lifeTimeMax = param_.lifeTimeMax;
        p.lifeTimer = param_.lifeTimeMax;
        p.rotationSpeed = param_.rotationSpeed;
        p.isActive = true;
        activeList_.push_back(index);
    }
}

void TrailParticle3D::Reset() {
    activeList_.clear();
    freeList_.clear();
    freeList_.reserve(particles_.size());
    for (int i = 0; i < static_cast<int>(particles_.size()); i++) {
        particles_[i].isActive = false;
        freeList_.push_back(i);
    }
}

void TrailParticle3D::ResetFall() {

    goalPos_ = GetRandomGoalPos();

    float dx = goalPos_.x - param_.fixedStartPos.x;
    float dy = goalPos_.y - param_.fixedStartPos.y;
    float dz = goalPos_.z - param_.fixedStartPos.z;
    float len = sqrtf(dx * dx + dy * dy + dz * dz);

    direction_ = Vector3(dx / len, dy / len, dz / len);
    prevPos_ = param_.fixedStartPos;
}

Vector3 TrailParticle3D::GetRandomGoalPos() const {
    std::uniform_real_distribution<float> distAngle(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> distRadius(0.0f, 1.0f);
    float angle = distAngle(random_);
    float radius = sqrtf(distRadius(random_)) * param_.goalAreaRadius;
    return Vector3(cosf(angle) * radius, param_.goalY, sinf(angle) * radius);
}