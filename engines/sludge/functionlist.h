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
/*
 This is the list of the built in functions

 true or false states if it's a used function
 in the current version of the engine,
 but that value currently isn't used anywhere
 */

namespace Sludge {

#define FUNC(special,name,paramNum)      {#name, builtIn_ ## name, paramNum},
static builtInFunctionData builtInFunctionArray[] = {
	FUNC(true, say, -1)
	FUNC(true, skipSpeech, 0)
	FUNC(true, statusText, 1)
	FUNC(true, pause, 1)
	FUNC(true, onLeftMouse, -1)
	FUNC(true, onRightMouse, -1)
	FUNC(true, setCursor, 1)
	FUNC(true, addOverlay, 3)
	FUNC(true, addCharacter, 4)
	FUNC(true, playSound, 1)
	FUNC(true, getMouseX, 0)
	FUNC(true, getMouseY, 0)
	FUNC(true, addScreenRegion, 8)
	FUNC(true, onMoveMouse, -1)
	FUNC(true, onFocusChange, -1)
	FUNC(true, getOverObject, 0)
	FUNC(true, blankScreen, 0)
	FUNC(true, moveCharacter, -1)
	FUNC(true, onKeyboard, -1)
	FUNC(true, getObjectX, 1)
	FUNC(true, getObjectY, 1)
	FUNC(true, random, 1)
	FUNC(true, spawnSub, 1)
	FUNC(true, blankArea, 4)
	FUNC(true, hideCharacter, 1)
	FUNC(true, showCharacter, 1)
	FUNC(true, callEvent, 2)
	FUNC(true, removeScreenRegion, 1)
	FUNC(true, animate, 2)
	FUNC(true, turnCharacter, 2)
	FUNC(true, removeAllCharacters, 0)
	FUNC(true, removeAllScreenRegions, 0)
	FUNC(true, setScale, 2)
	FUNC(true, newStack, -1)
	FUNC(true, pushToStack, 2)
	FUNC(true, popFromStack, 1)
	FUNC(true, clearStatus, 0)
	FUNC(true, addStatus, 0)
	FUNC(true, removeLastStatus, 0)
	FUNC(true, lightStatus, 1)
	FUNC(true, getStatusText, 0)
	FUNC(true, setStatusColour, 3)
	FUNC(true, deleteFromStack, 2)
	FUNC(true, freeze, 0)
	FUNC(true, unfreeze, 0)
	FUNC(true, pasteImage, 3)
	FUNC(true, copyStack, 1)
	FUNC(true, completeTimers, 0)
	FUNC(true, setCharacterDrawMode, 2)
	FUNC(true, anim, -1)
	FUNC(true, costume, -1)
	FUNC(true, pickOne, -1)
	FUNC(true, setCostume, 2)
	FUNC(true, wait, 2)
	FUNC(true, somethingSpeaking, 0)
	FUNC(true, substring, 3)
	FUNC(true, stringLength, 1)
	FUNC(true, darkBackground, 0)
	FUNC(true, saveGame, 1)
	FUNC(true, loadGame, 1)
	FUNC(true, quitGame, 0)
	FUNC(true, rename, 2)
	FUNC(true, stackSize, 1)
	FUNC(true, pasteString, 3)
	FUNC(true, startMusic, 3)
	FUNC(true, setDefaultMusicVolume, 1)
	FUNC(true, setMusicVolume, 2)
	FUNC(true, stopMusic, 1)
	FUNC(true, stopSound, 1)
	FUNC(true, setFont, 3)
	FUNC(true, alignStatus, 1)
	FUNC(true, showFloor, 0)
	FUNC(true, showBoxes, 0)
	FUNC(true, positionStatus, 2)
	FUNC(true, setFloor, 1)
	FUNC(true, forceCharacter, -1)
	FUNC(true, jumpCharacter, -1)
	FUNC(true, peekStart, 1)
	FUNC(true, peekEnd, 1)
	FUNC(true, enqueue, 2)
	FUNC(true, setZBuffer, 1)
	FUNC(true, getMatchingFiles, 1)
	FUNC(true, inFont, 1)
	FUNC(true, onLeftMouseUp, -1)
	FUNC(true, onRightMouseUp, -1)
	FUNC(true, loopSound, -1)
	FUNC(true, removeCharacter, 1)
	FUNC(true, stopCharacter, 1)
	FUNC(true, launch, 1)
	FUNC(true, howFrozen, 0)
	FUNC(true, setPasteColour, 3)
	FUNC(true, setLitStatusColour, 3)
	FUNC(true, fileExists, 1)
	FUNC(true, floatCharacter, 2)
	FUNC(true, cancelSub, 1)
	FUNC(true, setCharacterWalkSpeed, 2)
	FUNC(true, deleteAllFromStack, 2)
	FUNC(true, setCharacterExtra, 2)
	FUNC(true, mixOverlay, 3)
	FUNC(true, pasteCharacter, 1)
	FUNC(true, setSceneDimensions, 2)
	FUNC(true, aimCamera, 2)
	FUNC(true, getMouseScreenX, 0)
	FUNC(true, getMouseScreenY, 0)
	FUNC(true, setDefaultSoundVolume, 1)
	FUNC(true, setSoundVolume, 2)
	FUNC(true, setSoundLoopPoints, 3)
	FUNC(true, setSpeechMode, 1)
	FUNC(true, setLightMap, -1)
	FUNC(true, think, -1)
	FUNC(true, getCharacterDirection, 1)
	FUNC(true, isCharacter, 1)
	FUNC(true, isScreenRegion, 1)
	FUNC(true, isMoving, 1)
	FUNC(true, deleteFile, 1)
	FUNC(true, renameFile, 2)
	FUNC(true, hardScroll, 1)
	FUNC(true, stringWidth, 1)
	FUNC(true, setSpeechSpeed, 1)
	FUNC(true, normalCharacter, 1)
	FUNC(true, fetchEvent, 2)
	FUNC(true, transitionLevel, 1)
	FUNC(true, spinCharacter, 2)
	FUNC(true, setFontSpacing, 1)
	FUNC(true, burnString, 3)
	FUNC(true, captureAllKeys, 1)
	FUNC(true, cacheSound, 1)
	FUNC(true, setCharacterSpinSpeed, 2)
	FUNC(true, transitionMode, 1)
	FUNC(false, _rem_movieStart, 1)
	FUNC(false, _rem_movieAbort, 0)
	FUNC(false, _rem_moviePlaying, 0)
	FUNC(false, _rem_updateDisplay, 1)
	FUNC(true, getSoundCache, 0)
	FUNC(true, saveCustomData, 2)
	FUNC(true, loadCustomData, 1)
	FUNC(true, setCustomEncoding, 1)
	FUNC(true, freeSound, 1)
	FUNC(true, parallaxAdd, 3)
	FUNC(true, parallaxClear, 0)
	FUNC(true, setBlankColour, 3)
	FUNC(true, setBurnColour, 3)
	FUNC(true, getPixelColour, 2)
	FUNC(true, makeFastArray, 1)
	FUNC(true, getCharacterScale, 1)
	FUNC(true, getLanguageID, 0)
	FUNC(false, _rem_launchWith, 2)
	FUNC(true, getFramesPerSecond, 0)
	FUNC(true, showThumbnail, 3)
	FUNC(true, setThumbnailSize, 2)
	FUNC(true, hasFlag, 2)
	FUNC(true, snapshotGrab, 0)
	FUNC(true, snapshotClear, 0)
	FUNC(true, bodgeFilenames, 1)
	FUNC(false, _rem_registryGetString, 2)
	FUNC(true, quitWithFatalError, 1)
	FUNC(true, _rem_setCharacterAA, 4)
	FUNC(true, _rem_setMaximumAA, 3)
	FUNC(true, setBackgroundEffect, -1)
	FUNC(true, doBackgroundEffect, 0)
	FUNC(true, setCharacterAngleOffset, 2)
	FUNC(true, setCharacterTransparency, 2)
	FUNC(true, setCharacterColourise, 5)
	FUNC(true, zoomCamera, 1)
	FUNC(true, playMovie, 1)
	FUNC(true, stopMovie, 0)
	FUNC(true, pauseMovie, 0)
};
#undef FUNC

const static int NUM_FUNCS = (sizeof (builtInFunctionArray) / sizeof (builtInFunctionArray[0]));

} // End of namespace Sludge
