/* Residual - A 3D game interpreter
 *
 * Residual is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the AUTHORS
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef MYST3_SCENE_H
#define MYST3_SCENE_H

#include "common/rect.h"

namespace Myst3 {

class Myst3Engine;
class SunSpot;

class Scene {
	private:
		Myst3Engine *_vm;

		float _cameraPitch;
		float _cameraHeading;
		Common::Point _mouseOld;

	public:
		Scene(Myst3Engine *vm);

		void updateCamera(Common::Point &mouse);
		Common::Point getMousePos() { return Common::Point(_cameraHeading, _cameraPitch); }

		void lookAt(float pitch, float heading);

		void drawBlackBorders();
		void drawSunspotFlare(const SunSpot &s);

		static const int _originalWidth = 640;
		static const int _originalHeight = 480;
		static const int _topBorderHeight = 30;
		static const int _bottomBorderHeight = 90;
		static const int _frameHeight = 360;
};

} // end of namespace Myst3

#endif
