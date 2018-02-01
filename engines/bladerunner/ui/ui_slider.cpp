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

#include "bladerunner/ui/ui_slider.h"

#include "bladerunner/audio_player.h"
#include "bladerunner/bladerunner.h"
#include "bladerunner/game_info.h"

namespace BladeRunner {

const uint16 UISlider::kColors[] = { 0x0000, 0x0821, 0x1061, 0x1C82, 0x24C2, 0x2CE3, 0x3524, 0x4145, 0x4586, 0x4DC7, 0x5609, 0x5E4B, 0x668C, 0x6EEE, 0x7730, 0x7B92 };

UISlider::UISlider(BladeRunnerEngine *vm, UIComponentCallback *valueChangedCallback, void *callbackData, Common::Rect rect, int maxValue, int value)
	: UIComponent(vm) {

	_valueChangedCallback = valueChangedCallback;
	_callbackData = callbackData;

	_maxValue = MAX(0, maxValue);
	_value = CLIP(value, 0, _maxValue - 1);

	_rect = rect;
	_isEnabled = true;
	_currentFrameColor = 0;
	_hasFocus = false;
	_pressedStatus = 0;
	_mouseX = 0;
}

void UISlider::draw(Graphics::Surface &surface) {
	if (_rect.isEmpty()) {
		return;
	}

	int frameColor;
	if (_pressedStatus == 1) {
		frameColor = 10;
	} else if (_hasFocus && _pressedStatus != 2 && _isEnabled) {
		frameColor = 5;
	} else {
		frameColor = 0;
	}

	if (_currentFrameColor < frameColor) {
		++_currentFrameColor;
	}

	if (_currentFrameColor > frameColor) {
		--_currentFrameColor;
	}

	surface.frameRect(_rect, kColors[_currentFrameColor]);

	int sliderX = 0;
	if (_maxValue <= 1) {
		sliderX = _rect.left;
	} else {
		sliderX = _rect.left + ((_value * _rect.width()) / (_maxValue - 1));
	}

	if (_pressedStatus == 1) {
		int sliderValue = ((_maxValue - 1) * (_mouseX - _rect.left)) / _rect.width();
		sliderX = _rect.left + ((sliderValue * _rect.width()) / (_maxValue - 1));
		sliderX = CLIP(sliderX, (int)_rect.left, (int)_rect.right);
	}

	if (_rect.left + 1 < _rect.right - 1) {
		int striding = _rect.left + sliderX;
		for (int x = _rect.left + 1; x < _rect.right - 1; x++) {
			int colorIndex =  15 - (abs(sliderX - x) >> 2);

			if (!_isEnabled) {
				colorIndex /= 2;
			}

			if (colorIndex < 3) {
				colorIndex = 3;
			}

			uint16 color = kColors[colorIndex];
			if ((striding + x) & 1 || x == sliderX) {
				color = 0;
			}

			surface.vLine(x, _rect.top + 1, _rect.bottom - 2, color);
		}
	}
}

void UISlider::handleMouseMove(int mouseX, int mouseY) {
	_mouseX = mouseX;
	if (_rect.contains(mouseX, mouseY)) {
		if (!_hasFocus && _isEnabled && _pressedStatus == 0) {
			_vm->_audioPlayer->playAud(_vm->_gameInfo->getSfxTrack(508), 100, 0, 0, 50, 0);
		}
		_hasFocus = true;
	} else {
		_hasFocus = false;
	}
}

void UISlider::handleMouseDown(bool alternateButton) {
	if (_isEnabled && !alternateButton) {
		if (_hasFocus) {
			_pressedStatus = 1;
		} else {
			_pressedStatus = 2;
		}
	}
}

void UISlider::handleMouseUp(bool alternateButton) {
	if (!alternateButton) {
		if (_pressedStatus == 1) {
			if (_rect.width() == 0) {
				_value = 0;
			} else {
				_value = ((_maxValue - 1) * (_mouseX - _rect.left)) / _rect.width();
			}
			_value = CLIP(_value, 0, _maxValue - 1);

			if (_valueChangedCallback) {
				_valueChangedCallback(_callbackData, this);
			}
		}
		_pressedStatus = 0;
	}
}

void UISlider::setValue(int value) {
	_value = CLIP(value, 0, _maxValue - 1);
}

} // End of namespace BladeRunner
