#include "MicroDarkTemplar.h"

#include "InformationManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroDarkTemplar::MicroDarkTemplar()
{ 
}

void MicroDarkTemplar::executeMicro(const BWAPI::Unitset & targets)
{
    if (!order.isCombatOrder()) return;

    const BWAPI::Unitset & meleeUnits = getUnits();

    // Filter the set for units we may want to attack
	BWAPI::Unitset meleeUnitTargets;
	for (const auto target : targets) 
	{
		if (target->isVisible() &&
			target->isDetected() &&
			!target->isFlying() &&
			target->getPosition().isValid() &&
			target->getType() != BWAPI::UnitTypes::Zerg_Larva && 
			//target->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!target->isStasised() &&
			!target->isUnderDisruptionWeb())             // melee unit can't attack under dweb
		{
			meleeUnitTargets.insert(target);
		}
	}

    // Collect data on enemy detectors
    // We include all known static detectors and visible mobile detectors
    // TODO: Keep track of an enemy detection matrix
    std::vector<std::pair<BWAPI::Position, BWAPI::UnitType>> enemyDetectors;
    for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
        if (!unit->getType().isBuilding() && unit->getType().isDetector())
            enemyDetectors.push_back(std::make_pair(unit->getPosition(), unit->getType()));
    for (auto const & ui : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
        if (ui.second.type.isBuilding() && ui.second.type.isDetector() && !ui.second.goneFromLastPosition && ui.second.completed)
            enemyDetectors.push_back(std::make_pair(ui.second.lastPosition, ui.second.type));

	for (const auto meleeUnit : meleeUnits)
	{
		bool changOrder = false;

        if (unstickStuckUnit(meleeUnit))
        {
            continue;
        }

		if (order.isHarass()) {
			// If in range of a detector, consider fleeing from it
			for (auto const & detector : enemyDetectors) {
				int distance = (detector.second.isBuilding() ? 9 * 32 : 12 * 32);
				if (meleeUnit->getDistance(detector.first) <= distance)
				{
					if (!meleeUnit->isUnderAttack() && !UnitUtil::TypeCanAttackGround(detector.second)) continue;

					//InformationManager::Instance().getLocutusUnit(meleeUnit).fleeFrom(detector.first);

					BWAPI::Position position = cutFleeFrom(meleeUnit, detector.first, distance);
					if (position.isValid() && position.getDistance(detector.first) > distance) {
						InformationManager::Instance().getLocutusUnit(meleeUnit).fleeFrom(detector.first);
					}
					else {
						InformationManager::Instance().getLocutusUnit(meleeUnit).fleeFrom(position);//detector.first
					}

					//order.setPosition(meleeUnit->getPosition());
					changOrder = true;
					break;
					//goto nextUnit; // continue outer loop
				}
			}
		}

		BWAPI::Unit target = getTarget(meleeUnit, meleeUnitTargets);
		if (target)
		{
			if (changOrder) {
				order.setPosition(target->getPosition());
			}
			Micro::AttackUnit(meleeUnit, target);
		}
		else if (meleeUnit->getDistance(order.getPosition()) > meleeUnit->getType().sightRange())
		{
			// There are no targets. Move to the order position if not already close.
            InformationManager::Instance().getLocutusUnit(meleeUnit).moveTo(order.getPosition());
            //Micro::Move(meleeUnit, order.getPosition());
		}

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(meleeUnit->getPosition(), meleeUnit->getTargetPosition(),
				Config::Debug::ColorLineTarget);
		}

    nextUnit:;
	}
}

// Choose a target from the set, or null if we don't want to attack anything
BWAPI::Unit MicroDarkTemplar::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	for (const auto target : targets)
	{
		const int priority = getAttackPriority(meleeUnit, target);		// 0..12
		const int range = meleeUnit->getDistance(target);				// 0..map size in pixels
		int toGoal = target->getDistance(order.getPosition());  // 0..map size in pixels
		const int closerToGoal =										// positive if target is closer than us to the goal
			meleeUnit->getDistance(order.getPosition()) - target->getDistance(order.getPosition());

		// Skip targets that are too far away to worry about.
		//������Щ̫Զ�����õ��ĵ�Ŀ�ꡣ
		if (range >= 12 * 32)
		{
			continue;
		}

		// Let's say that 1 priority step is worth 64 pixels (2 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 20 * 32 * priority - range - toGoal / 2;

		// Adjust for special features.

		// Prefer targets under dark swarm, on the expectation that then we'll be under it too.
		//��ϲ�������µ�Ŀ�꣬ϣ������Ҳ���ڻ����¡�
		if (target->isUnderDarkSwarm())
		{
			score += 4 * 32;
		}

		// A bonus for attacking enemies that are "in front".
		// It helps reduce distractions from moving toward the goal, the order position.
		if (closerToGoal > 0)
		{
			score += 2 * 32;
		}

		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		if (meleeUnit->isInWeaponRange(target))
		{
			if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Ultralisk || meleeUnit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
			{
				score += 12 * 32;   // because they're big and awkward
			}
			else
			{
				score += 6 * 32;
			}
		}
		else if (!target->isMoving())
		{
			if (target->isSieged() ||
				target->getOrder() == BWAPI::Orders::Sieging ||
				target->getOrder() == BWAPI::Orders::Unsieging)
			{
				score += 48;
			}
			else
			{
				score += 32;
			}
		}
		else if (target->isBraking())
		{
			score += 16;
		}
		else if (target->getType().topSpeed() >= meleeUnit->getType().topSpeed())
		{
			score -= 2 * 32;
		}

		if (target->isUnderStorm())
		{
			score -= 4 * 32;
		}

		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() == 0)
		{
			score += 32;
		}
		else if (target->getHitPoints() < target->getType().maxHitPoints())
		{
			score += 24;
		}

		if (target->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::IsDetector, 7 * 32)) {
			score -= 12 * 24;
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	return shouldIgnoreTarget(meleeUnit, bestTarget) ? nullptr : bestTarget;
}

// get the attack priority of a type
int MicroDarkTemplar::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

    if (targetType == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
        !target->isCompleted())
    {
        return 12;
    }

    if (targetType == BWAPI::UnitTypes::Protoss_Observatory ||
        targetType == BWAPI::UnitTypes::Protoss_Robotics_Facility || BWAPI::UnitTypes::Protoss_Forge)
    {
        if (target->isCompleted())
        {
            return 10;
        }

        return 11;
    }

	if (targetType == BWAPI::UnitTypes::Protoss_High_Templar || targetType == BWAPI::UnitTypes::Protoss_Dark_Templar) {
		return 11;
	}

	// Exceptions for dark templar.
	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
	{
		if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
		{
			return 10;
		}
		if ((targetType == BWAPI::UnitTypes::Terran_Missile_Turret || targetType == BWAPI::UnitTypes::Terran_Comsat_Station) &&
			(BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0))
		{
			return 9;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			return 8;
		}
		if (targetType.isWorker())
		{
			return 11;
		}
	}

	// if the target is building something near our base something is fishy
	if (InformationManager::Instance().getEnemyMainBaseLocation()) {
		BWAPI::Position enemyBasePosition = InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();
		if (target->getDistance(enemyBasePosition) < 10 * 32) {
			if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()))
			{
				return 12;
			}

			if (target->getType().isBuilding())
			{
				// This includes proxy buildings, which deserve high priority.
				// But when bases are close together, it can include innocent buildings.
				// We also don't want to disrupt priorities in case of proxy buildings
				// supported by units; we may want to target the units first.
				if (UnitUtil::CanAttackGround(target) || UnitUtil::CanAttackAir(target))
				{
					return 10;
				}

				return 8;
			}
		}
	}

	// Short circuit: Enemy unit which is far enough outside its range is lower priority than a worker.
	//��·:���˵ĵ�λ�������������㹻Զ�����ȼ���һ�����˵͡�
	int enemyRange = UnitUtil::GetAttackRange(target, attacker);
	if (enemyRange &&
		!targetType.isWorker() &&
		attacker->getDistance(target) > 32 + enemyRange)
	{
		return 8;
	}
	// Short circuit: Units before bunkers!
	if (targetType == BWAPI::UnitTypes::Terran_Bunker)
	{
		return 10;
	}

	// Medics and ordinary combat units. Include workers that are doing stuff.
	if (targetType == BWAPI::UnitTypes::Terran_Medic ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Dark_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 12;
	}

	if (targetType.groundWeapon() != BWAPI::WeaponTypes::None && !targetType.isWorker())
	{
		return 11;
	}

	if (targetType.isWorker() && (target->isRepairing() || target->isConstructing() || unitNearNarrowChokepoint(target)))
	{
		return 12;
	}
	else {
		return 11;
	}

	// next priority is bored workers and turrets
	if (targetType == BWAPI::UnitTypes::Terran_Missile_Turret)
	{
		return 6;
	}

	// next priority is bored workers and turrets
	if (targetType == BWAPI::UnitTypes::Terran_Missile_Turret && !target->isCompleted())
	{
		return 11;
	}

	// Buildings come under attack during free time, so they can be split into more levels.
	// Nydus canal is critical.
	if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 10;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spire)
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool ||
		targetType.isResourceDepot() ||
		targetType == BWAPI::UnitTypes::Protoss_Templar_Archives ||
		targetType.isSpellcaster())
	{
		return 5;
	}
	// Short circuit: Addons other than a completed comsat are worth almost nothing.
	// TODO should also check that it is attached
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
	}
	// anything with a cost
	if (targetType.gasPrice() > 0)
	{
		return 3;
	}

	// then everything else
	return 1;
}
