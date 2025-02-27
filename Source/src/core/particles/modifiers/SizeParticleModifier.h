#pragma once

#include "core/particles/ParticleSystem.h"
#include "core/particles/ParticleModifier.h"
#include "ui/widgets/DragFloat.h"

namespace Hachiko
{
    class SizeParticleModifier : public ParticleModifier
    {
    public:
        SizeParticleModifier(const std::string& name);
        ~SizeParticleModifier() override = default;

        void Update(std::vector<Particle>&, float delta_time) override;
        void DrawGui() override;

        void Save(YAML::Node& node) const override;
        void Load(const YAML::Node& node) override;

    private:
        ParticleSystem::VariableTypeProperty size{float2::zero, 0.01f};
        Widgets::DragFloatConfig cfg;
        void UpdateSizeOverTime(Particle& particle) const;
    };
}
