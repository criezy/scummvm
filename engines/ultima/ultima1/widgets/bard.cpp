/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "ultima/ultima1/widgets/bard.h"
#include "ultima/ultima1/map/map_city_castle.h"
#include "ultima/ultima1/core/resources.h"

namespace Ultima {
namespace Ultima1 {
namespace Widgets {

void Bard::movement() {
	if (areGuardsHostile())
		return;

	Point playerPos = _map->_playerWidget->_position;
	bool stolen = false;

	// Choose a new random position to  move to
	Point newPos = _position + getRandomMoveDelta();

	if (canMoveTo(newPos)) {
		// Move to the new position
		_position = newPos;
		_game->playFX(4);
	} else if (newPos == playerPos) {
		// Moved into player, which causes them to steal a weapon from the player
		stolen = stealWeapon();
	}

	if (!stolen && _game->getRandomNumber(1, 255) < 15)
		talk();
}

bool Bard::stealWeapon() {
	Shared::Character *c = _game->_party._currentCharacter;
	for (uint idx = 1; idx < c->_weapons.size(); ++idx) {
		if (c->_weapons[idx]._quantity > 0 && (int)idx != c->_equippedWeapon) {
			c->_weapons[idx]._quantity--;

			if (_game->getRandomNumber(1, 255) < (c->_agility + 128)) {

			}

			return true;
		}
	}

	return false;
}

void Bard::talk() {
	if (dynamic_cast<Map::MapCity *>(_map)) {
		addInfoMsg(_game->_res->BARD_SPEECH1);
		addInfoMsg(_game->_res->BARD_SPEECH2);
	} else {
		addInfoMsg(_game->_res->JESTER_SPEECH1);
		addInfoMsg(_game->_res->JESTER_SPEECH2);
	}
}

} // End of namespace Widgets
} // End of namespace Ultima1
} // End of namespace Ultima
