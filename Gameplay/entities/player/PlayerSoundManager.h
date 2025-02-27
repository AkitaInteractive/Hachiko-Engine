#pragma once
#include <scripting/Script.h>
#include "entities/player/PlayerController.h"

namespace Hachiko
{

class GameObject;
class ComponentAudioSource;

namespace Scripting
{

class PlayerSoundManager : public Script
{

	SERIALIZATION_METHODS(false)

public:
	PlayerSoundManager(Hachiko::GameObject* game_object);
	~PlayerSoundManager() override = default;

	void OnAwake() override;
	void OnStart() override;
	void OnUpdate() override;

	void SetLevel(const unsigned lvl)
	{
		_level = lvl;
	}
private:
	PlayerController* _player_controller;

	SERIALIZE_FIELD(ComponentAudioSource*, _audio_source);
	SERIALIZE_FIELD(float, _step_frequency);
	SERIALIZE_FIELD(float, _melee_frequency);
	SERIALIZE_FIELD(float, _ranged_frequency);
	SERIALIZE_FIELD(float, _timer);
	SERIALIZE_FIELD(PlayerState, _previous_state);

	unsigned _level = 1;
	float _current_frequency;
	float _damage_timer;
	float _dmg_cool_down;
	PlayerController::WeaponUsed previous_weapon_type;
	void SetGroundEffect();
	void SetWeaponEffect(const PlayerController::WeaponUsed weapon_type);
	void PlayDamagedEffect(const PlayerState state, const PlayerController::DamageType damage_type);
};
} // namespace Scripting
} // namespace Hachiko


