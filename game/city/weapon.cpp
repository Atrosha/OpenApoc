#include "game/city/weapon.h"
#include "framework/logger.h"
#include "game/city/vehicle.h"
#include "game/tileview/beam_projectile.h"

namespace OpenApoc
{

Weapon::Weapon(const WeaponDef &def, std::shared_ptr<Vehicle> owner, int initialAmmo, State initialState)
	: state(initialState), def(def), owner(owner), ammo(initialAmmo), reloadTime(0)
{

}
std::shared_ptr<Projectile>
Weapon::fire(Vec3<float> target)
{
	if (this->state != State::Ready)
	{
		LogWarning("Trying to fire weapon in state %d", this->state);
		return nullptr;
	}
	if (this->ammo <= 0)
	{
		LogWarning("Trying to fire weapon with no ammo");
		return nullptr;
	}
	this->reloadTime = this->def.firingDelay;
	this->state = State::Reloading;
	this->ammo--;

	if (this->ammo == 0)
	{
		this->state = State::OutOfAmmo;
	}

	switch (this->def.projectileType)
	{
		case WeaponDef::ProjectileType::Beam:
		{
			Vec3<float> velocity = target - this->owner->position;
			velocity = glm::normalize(velocity);
			velocity *= this->def.projectileSpeed;
			//FIXME: Figure out what the stored projectile speed units are.
			//For the time being, treating it as '1/100 of a tile per tick'
			//for testing
			auto &map = this->owner->tileObject->getOwningTile()->map;
			//FIXME: Figure out the stored projectile tail lenght units.
			//Set to 1/10 of a tile for testing
			return std::make_shared<BeamProjectile>(
				map, this->owner, this->owner->position, velocity, def.range,
				def.beamColour, def.projectileTailLength, def.beamWidth);
		}
		default:
			LogWarning("Unknown projectile type");
	}

	return nullptr;
}

void
Weapon::update(int ticks)
{
	if (this->reloadTime != 0)
	{
		if (ticks >= this->reloadTime)
			this->reloadTime = 0;
		else
			this->reloadTime -= ticks;
	}
	switch (this->state)
	{
		case State::Reloading:
			if (this->reloadTime == 0)
				this->state = State::Ready;
			return;
		default:
			return;
	}
}


}; //namespace OpenApoc