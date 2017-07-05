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

#include "audio/mixer.h"                 // for Audio::Mixer::kSFXSoundType
#include "common/config-manager.h"       // for ConfMan
#include "common/textconsole.h"          // for warning, error
#ifndef USE_RGB_COLOR
#include "common/translation.h"          // for _
#endif
#include "common/util.h"                 // for ARRAYSIZE
#include "common/system.h"               // for g_system
#include "engine.h"                      // for Engine, g_engine
#include "graphics/colormasks.h"         // for createPixelFormat
#include "graphics/palette.h"            // for PaletteManager
#include "graphics/transparent_surface.h" // for TransparentSurface
#include "sci/console.h"                 // for Console
#include "sci/engine/features.h"         // for GameFeatures
#include "sci/engine/state.h"            // for EngineState
#include "sci/engine/vm_types.h"         // for reg_t
#include "sci/event.h"                   // for SciEvent, EventManager, SCI_...
#include "sci/graphics/celobj32.h"       // for CelInfo32, ::kLowResX, ::kLo...
#include "sci/graphics/cursor32.h"       // for GfxCursor32
#include "sci/graphics/frameout.h"       // for GfxFrameout
#include "sci/graphics/helpers.h"        // for Color, Palette
#include "sci/graphics/palette32.h"      // for GfxPalette32
#include "sci/graphics/plane32.h"        // for Plane, PlanePictureCodes::kP...
#include "sci/graphics/screen_item32.h"  // for ScaleInfo, ScreenItem, Scale...
#include "sci/resource.h"                // for ResourceManager, ResourceId,...
#include "sci/sci.h"                     // for SciEngine, g_sci, getSciVersion
#include "sci/sound/audio32.h"           // for Audio32
#include "sci/video/seq_decoder.h"       // for SEQDecoder
#include "video/avi_decoder.h"           // for AVIDecoder
#include "video/coktel_decoder.h"        // for AdvancedVMDDecoder
#include "sci/graphics/video32.h"

namespace Graphics { struct Surface; }

namespace Sci {

static void flushEvents(EventManager *eventMan) {
	// Flushing all the keyboard and mouse events out of the event manager
	// keeps events queued from before the start of playback from accidentally
	// activating a video stop flag
	for (;;) {
		const SciEvent event = eventMan->getSciEvent(SCI_EVENT_ANY & ~SCI_EVENT_QUIT);
		if (event.type == SCI_EVENT_NONE) {
			break;
		}
	}
}

bool VideoPlayer::open(const Common::String &fileName) {
	if (!_decoder->loadFile(fileName)) {
		warning("Failed to load %s", fileName.c_str());
		return false;
	}

#ifndef USE_RGB_COLOR
	// KQ7 2.00b videos are compressed in 24bpp Cinepak, so cannot play on
	// a system with no RGB support
	if (_decoder->getPixelFormat().bytesPerPixel != 1) {
		void showScummVMDialog(const Common::String &message);
		showScummVMDialog(Common::String::format(_("Cannot play back %dbpp video on a system with maximum color depth of 8bpp"), _decoder->getPixelFormat().bpp()));
		_decoder->close();
		return false;
	}
#endif

	return true;
}

bool VideoPlayer::startHQVideo() {
#ifdef USE_RGB_COLOR
	// Optimize rendering performance for unscaled videos, and allow
	// better-than-NN interpolation for videos that are scaled
	if (shouldStartHQVideo()) {
		// TODO: Search for and use the best supported format (which may be
		// lower than 32bpp) once the scaling code in Graphics supports
		// 16bpp/24bpp, and once the SDL backend can correctly communicate
		// supported pixel formats above whatever format is currently used by
		// _hwsurface. Right now, this will either show an error dialog (OpenGL)
		// or just crash entirely (SDL) if the backend does not support this
		// 32bpp pixel format, which sucks since this code really ought to be
		// able to fall back to NN scaling for games with 256-color videos
		// without any error.
		const Graphics::PixelFormat format(4, 8, 8, 8, 8, 24, 16, 8, 0);
		g_sci->_gfxFrameout->setPixelFormat(format);
		_hqVideoMode = (g_system->getScreenFormat() == format);
		return _hqVideoMode;
	} else {
		_hqVideoMode = false;
	}
#endif

	return false;
}

bool VideoPlayer::endHQVideo() {
#ifdef USE_RGB_COLOR
	if (g_system->getScreenFormat().bytesPerPixel != 1) {
		const Graphics::PixelFormat format = Graphics::PixelFormat::createFormatCLUT8();
		g_sci->_gfxFrameout->setPixelFormat(format);
		assert(g_system->getScreenFormat() == format);
		_hqVideoMode = false;
		return true;
	}
#endif

	return false;
}

VideoPlayer::EventFlags VideoPlayer::playUntilEvent(const EventFlags flags, const uint32 maxSleepMs) {
	flushEvents(_eventMan);
	_decoder->start();

	EventFlags stopFlag = kEventFlagNone;
	for (;;) {
		g_sci->sleep(MIN(_decoder->getTimeToNextFrame(), maxSleepMs));
		const Graphics::Surface *nextFrame = nullptr;
		// If a decoder needs more than one update per loop, this means we are
		// running behind and should skip rendering these frames (but must still
		// submit any palettes from skipped frames)
		while (_decoder->needsUpdate()) {
			nextFrame = _decoder->decodeNextFrame();
			if (_decoder->hasDirtyPalette()) {
				submitPalette(_decoder->getPalette());
			}
		}

		// Some frames may contain only audio and/or palette data; this occurs
		// with Duck videos and is not an error
		if (nextFrame) {
			renderFrame(*nextFrame);
		}

		stopFlag = checkForEvent(flags);
		if (stopFlag != kEventFlagNone) {
			break;
		}
	}

	return stopFlag;
}

VideoPlayer::EventFlags VideoPlayer::checkForEvent(const EventFlags flags) {
	if (g_engine->shouldQuit() || _decoder->endOfVideo()) {
		return kEventFlagEnd;
	}

	SciEvent event = _eventMan->getSciEvent(SCI_EVENT_MOUSE_PRESS | SCI_EVENT_PEEK);
	if ((flags & kEventFlagMouseDown) && event.type == SCI_EVENT_MOUSE_PRESS) {
		return kEventFlagMouseDown;
	}

	event = _eventMan->getSciEvent(SCI_EVENT_KEYBOARD | SCI_EVENT_PEEK);
	if ((flags & kEventFlagEscapeKey) && event.type == SCI_EVENT_KEYBOARD) {
		if (getSciVersion() < SCI_VERSION_3) {
			while ((event = _eventMan->getSciEvent(SCI_EVENT_KEYBOARD)),
				   event.type != SCI_EVENT_NONE) {
				if (event.character == SCI_KEY_ESC) {
					return kEventFlagEscapeKey;
				}
			}
		} else if (event.character == SCI_KEY_ESC) {
			return kEventFlagEscapeKey;
		}
	}

	return kEventFlagNone;
}

void VideoPlayer::submitPalette(const uint8 palette[256 * 3]) const {
#ifdef USE_RGB_COLOR
	if (g_system->getScreenFormat().bytesPerPixel != 1) {
		return;
	}
#endif

	assert(palette);
	g_system->getPaletteManager()->setPalette(palette, 0, 256);

	// KQ7 1.x has videos encoded using Microsoft Video 1 where palette 0 is
	// white and 255 is black, which is basically the opposite of DOS/Win
	// SCI palettes. So, when drawing to an 8bpp hwscreen, whenever a new
	// palette is seen, the screen must be re-filled with the new black
	// entry to ensure areas outside the video are always black and not some
	// other color
	for (int color = 0; color < 256; ++color) {
		if (palette[0] == 0 && palette[1] == 0 && palette[2] == 0) {
			g_system->fillScreen(color);
			break;
		}
		palette += 3;
	}
}

void VideoPlayer::renderFrame(const Graphics::Surface &nextFrame) const {
	bool freeConvertedFrame;
	Graphics::Surface *convertedFrame;
	// Avoid creating a duplicate copy of the surface when it is not necessary
	if (_decoder->getPixelFormat() == g_system->getScreenFormat()) {
		freeConvertedFrame = false;
		convertedFrame = const_cast<Graphics::Surface *>(&nextFrame);
	} else {
		freeConvertedFrame = true;
		convertedFrame = nextFrame.convertTo(g_system->getScreenFormat(), _decoder->getPalette());
	}
	assert(convertedFrame);

	if (_decoder->getWidth() != _drawRect.width() || _decoder->getHeight() != _drawRect.height()) {
		Graphics::Surface *const unscaledFrame(convertedFrame);
		// TODO: The only reason TransparentSurface is used here because it is
		// where common scaler code is right now.
		const Graphics::TransparentSurface tsUnscaledFrame(*unscaledFrame);
#ifdef USE_RGB_COLOR
		if (_hqVideoMode) {
			convertedFrame = tsUnscaledFrame.scaleT<Graphics::FILTER_BILINEAR>(_drawRect.width(), _drawRect.height());
		} else {
#elif 1
		{
#else
		}
#endif
			convertedFrame = tsUnscaledFrame.scaleT<Graphics::FILTER_NEAREST>(_drawRect.width(), _drawRect.height());
		}
		assert(convertedFrame);
		if (freeConvertedFrame) {
			unscaledFrame->free();
			delete unscaledFrame;
		}
		freeConvertedFrame = true;
	}

	g_system->copyRectToScreen(convertedFrame->getPixels(), convertedFrame->pitch, _drawRect.left, _drawRect.top, convertedFrame->w, convertedFrame->h);
	g_sci->_gfxFrameout->updateScreen();

	if (freeConvertedFrame) {
		convertedFrame->free();
		delete convertedFrame;
	}
}

template <typename PixelType>
void VideoPlayer::renderLQToSurface(Graphics::Surface &out, const Graphics::Surface &nextFrame, const bool doublePixels, const bool blackLines) const {

	const int lineCount = blackLines ? 2 : 1;
	if (doublePixels) {
		for (int16 y = 0; y < nextFrame.h * 2; y += lineCount) {
			const PixelType *source = (const PixelType *)nextFrame.getBasePtr(0, y >> 1);
			PixelType *target = (PixelType *)out.getBasePtr(0, y);
			for (int16 x = 0; x < nextFrame.w; ++x) {
				*target++ = *source;
				*target++ = *source++;
			}
		}
	} else if (blackLines) {
		for (int16 y = 0; y < nextFrame.h; y += lineCount) {
			const PixelType *source = (const PixelType *)nextFrame.getBasePtr(0, y);
			PixelType *target = (PixelType *)out.getBasePtr(0, y);
			memcpy(target, source, out.w * sizeof(PixelType));
		}
	} else {
		out.copyRectToSurface(nextFrame.getPixels(), nextFrame.pitch, 0, 0, nextFrame.w, nextFrame.h);
	}
}

#pragma mark SEQPlayer

SEQPlayer::SEQPlayer(EventManager *eventMan) :
	VideoPlayer(eventMan) {}

void SEQPlayer::play(const Common::String &fileName, const int16 numTicks, const int16, const int16) {

	_decoder.reset(new SEQDecoder(numTicks));

	if (!VideoPlayer::open(fileName)) {
		_decoder.reset();
		return;
	}

	const int16 scriptWidth = g_sci->_gfxFrameout->getCurrentBuffer().scriptWidth;
	const int16 scriptHeight = g_sci->_gfxFrameout->getCurrentBuffer().scriptHeight;
	const int16 screenWidth = g_sci->_gfxFrameout->getCurrentBuffer().screenWidth;
	const int16 screenHeight = g_sci->_gfxFrameout->getCurrentBuffer().screenHeight;

	const int16 scaledWidth = (_decoder->getWidth() * Ratio(screenWidth, scriptWidth)).toInt();
	const int16 scaledHeight = (_decoder->getHeight() * Ratio(screenHeight, scriptHeight)).toInt();

	// Normally we would use the coordinates passed into the play function
	// to position the video, but since we are scaling the video (which SSCI
	// did not do), the coordinates are not correct. Since videos are always
	// intended to play in the center of the screen, we just recalculate the
	// origin here.
	_drawRect.left = (screenWidth - scaledWidth) / 2;
	_drawRect.top = (screenHeight - scaledHeight) / 2;
	_drawRect.setWidth(scaledWidth);
	_drawRect.setHeight(scaledHeight);

	startHQVideo();
	playUntilEvent(kEventFlagMouseDown | kEventFlagEscapeKey);
	endHQVideo();
	g_system->fillScreen(0);
	_decoder.reset();
}

#pragma mark -
#pragma mark AVIPlayer

AVIPlayer::AVIPlayer(EventManager *eventMan) :
	VideoPlayer(eventMan, new Video::AVIDecoder(Audio::Mixer::kSFXSoundType)),
	_status(kAVINotOpen) {}

AVIPlayer::IOStatus AVIPlayer::open(const Common::String &fileName) {
	if (_status != kAVINotOpen) {
		close();
	}

	if (!VideoPlayer::open(fileName)) {
		return kIOFileNotFound;
	}

	_status = kAVIOpen;
	return kIOSuccess;
}

AVIPlayer::IOStatus AVIPlayer::init(const bool doublePixels) {
	// Calls to initialize the AVI player in SCI can be made in a few ways:
	//
	// * kShowMovie(WinInit, x, y) to render the video at (x,y) using its
	//   original resolution, or
	// * kShowMovie(WinInit, x, y, w, h) to render the video at (x,y) with
	//   rescaling to the given width and height, or
	// * kShowMovie(WinInitDouble, x, y) to render the video at (x,y) with
	//   rescaling to double the original resolution.
	//
	// Unfortunately, the values passed by game scripts are frequently wrong:
	//
	// * KQ7 passes origin coordinates that cause videos to be misaligned on the
	//   Y-axis;
	// * GK1 passes width and height that change the aspect ratio of the videos,
	//   even though they were rendered with square pixels (and in the case of
	//   CREDITS.AVI, cause the video to be badly downscaled);
	// * The GK2 demo does all of these things at the same time.
	//
	// Fortunately, whenever all of these games play an AVI, they are just
	// trying to play a video at the center of the screen. So, we ignore the
	// values that the game sends, and instead calculate the correct dimensions
	// and origin based on the video data, only allowing games to specify
	// whether or not the videos should be scaled up 2x.

	if (_status == kAVINotOpen) {
		return kIOFileNotFound;
	}

	g_sci->_gfxCursor32->hide();

	int16 width = _decoder->getWidth();
	int16 height = _decoder->getHeight();
	if (doublePixels) {
		width *= 2;
		height *= 2;
	}

	const int16 screenWidth = g_sci->_gfxFrameout->getCurrentBuffer().screenWidth;
	const int16 screenHeight = g_sci->_gfxFrameout->getCurrentBuffer().screenHeight;

	// When scaling videos, they must not grow larger than the hardware screen
	// or else the engine will crash. This is particularly important for the GK1
	// CREDITS.AVI since the game sends extra width/height arguments, causing it
	// to be treated as needing upscaling even though it does not.
	width = MIN<int16>(width, screenWidth);
	height = MIN<int16>(height, screenHeight);

	_drawRect.left = (screenWidth - width) / 2;
	_drawRect.top = (screenHeight - height) / 2;
	_drawRect.setWidth(width);
	_drawRect.setHeight(height);

	if (!startHQVideo() && _decoder->getPixelFormat().bytesPerPixel != 1) {
		g_sci->_gfxFrameout->setPixelFormat(_decoder->getPixelFormat());
	}

	return kIOSuccess;
}

AVIPlayer::IOStatus AVIPlayer::play(const int16 from, const int16 to, const int16, const bool async) {
	if (_status == kAVINotOpen) {
		return kIOFileNotFound;
	}

	if (from >= 0 && to > 0 && from <= to) {
		_decoder->seekToFrame(from);
		_decoder->setEndFrame(to);
	}

	if (!async || getSciVersion() == SCI_VERSION_2_1_EARLY) {
		playUntilEvent(kEventFlagNone);
	} else {
		_status = kAVIPlaying;
	}

	return kIOSuccess;
}

AVIPlayer::EventFlags AVIPlayer::playUntilEvent(const EventFlags flags, const uint32 maxSleepMs) {
	// NOTE: In SSCI, whether or not a video could be skipped was controlled by
	// game scripts; here, we always allow skipping video with the mouse or
	// escape key, to improve the user experience
	return VideoPlayer::playUntilEvent(flags | kEventFlagMouseDown | kEventFlagEscapeKey, maxSleepMs);
}

AVIPlayer::IOStatus AVIPlayer::close() {
	if (_status == kAVINotOpen) {
		return kIOSuccess;
	}

	if (!endHQVideo()) {
		// This fixes a single-frame white flash after playback of the KQ7 1.x
		// videos, which replace palette entry 0 with white
		const uint8 black[3] = { 0, 0, 0 };
		g_system->getPaletteManager()->setPalette(black, 0, 1);
	}

	g_system->fillScreen(0);
	g_sci->_gfxCursor32->unhide();
	_decoder->close();
	_status = kAVINotOpen;
	return kIOSuccess;
}

AVIPlayer::IOStatus AVIPlayer::cue(const uint16 frameNo) {
	if (!_decoder->seekToFrame(frameNo)) {
		return kIOSeekFailed;
	}

	_status = kAVIPaused;
	return kIOSuccess;
}

uint16 AVIPlayer::getDuration() const {
	if (_status == kAVINotOpen) {
		return 0;
	}

	return _decoder->getFrameCount();
}

#pragma mark -
#pragma mark VMDPlayer

VMDPlayer::VMDPlayer(EventManager *eventMan, SegManager *segMan) :
	VideoPlayer(eventMan, new Video::AdvancedVMDDecoder(Audio::Mixer::kSFXSoundType)),
	_segMan(segMan),

	_isOpen(false),
	_isInitialized(false),
	_bundledVmd(nullptr),
	_yieldFrame(0),
	_yieldInterval(0),
	_lastYieldedFrameNo(0),

	_plane(nullptr),
	_screenItem(nullptr),
	_planeIsOwned(true),
	_priority(0),
	_doublePixels(false),
	_stretchVertical(false),
	_blackLines(false),
	_leaveScreenBlack(false),
	_leaveLastFrame(false),
	_ignorePalettes(false),

	_blackoutPlane(nullptr),

	_startColor(0),
	_endColor(255),
#ifdef SCI_VMD_BLACK_PALETTE
	_blackPalette(false),
#endif
	_boostPercent(100),
	_boostStartColor(0),
	_boostEndColor(255),

	_showCursor(false) {}

VMDPlayer::~VMDPlayer() {
	close();
}

#pragma mark -
#pragma mark VMDPlayer - Playback

VMDPlayer::IOStatus VMDPlayer::open(const Common::String &fileName, const OpenFlags flags) {
	if (_isOpen) {
		error("Attempted to play %s, but another VMD was loaded", fileName.c_str());
	}

	if (g_sci->_features->VMDOpenStopsAudio()) {
		g_sci->_audio32->stop(kAllChannels);
	}

	Resource *bundledVmd = g_sci->getResMan()->findResource(ResourceId(kResourceTypeVMD, fileName.asUint64()), true);

	if (bundledVmd != nullptr) {
		Common::SeekableReadStream *stream = bundledVmd->makeStream();
		if (_decoder->loadStream(stream)) {
			_bundledVmd = bundledVmd;
			_isOpen = true;
		} else {
			delete stream;
			g_sci->getResMan()->unlockResource(bundledVmd);
		}
	} else if (_decoder->loadFile(fileName)) {
		_isOpen = true;
	}

	if (_isOpen) {
		if (flags & kOpenFlagMute) {
			_decoder->setVolume(0);
		}
		return kIOSuccess;
	}

	return kIOError;
}

void VMDPlayer::init(int16 x, const int16 y, const PlayFlags flags, const int16 boostPercent, const int16 boostStartColor, const int16 boostEndColor) {
	if (getSciVersion() < SCI_VERSION_3) {
		x &= ~1;
	}
	_doublePixels = flags & kPlayFlagDoublePixels;
	_blackLines = ConfMan.getBool("enable_black_lined_video") && (flags & kPlayFlagBlackLines);
	// If ScummVM has been configured to disable black lines on video playback,
	// the boosts need to be ignored too or else the brightness of the video
	// will be too high
	_boostPercent = 100 + (_blackLines && (flags & kPlayFlagBoost) ? boostPercent : 0);
	_boostStartColor = CLIP<int16>(boostStartColor, 0, 255);
	_boostEndColor = CLIP<int16>(boostEndColor, 0, 255);
	_leaveScreenBlack = flags & kPlayFlagLeaveScreenBlack;
	_leaveLastFrame = flags & kPlayFlagLeaveLastFrame;
#ifdef SCI_VMD_BLACK_PALETTE
	_blackPalette = flags & kPlayFlagBlackPalette;
#endif
	_stretchVertical = flags & kPlayFlagStretchVertical;

	_drawRect = Common::Rect(x,
							 y,
							 x + (_decoder->getWidth() << _doublePixels),
							 y + (_decoder->getHeight() << (_doublePixels || _stretchVertical)));
}

VMDPlayer::IOStatus VMDPlayer::close() {
	if (!_isOpen) {
		return kIOSuccess;
	}

	if (_isComposited) {
		closeComposited();
	} else {
		closeOverlay();
	}

	if (_blackoutPlane != nullptr) {
		g_sci->_gfxFrameout->deletePlane(*_blackoutPlane);
		_blackoutPlane = nullptr;
	}

	if (!_leaveLastFrame && !_leaveScreenBlack) {
		// This call *actually* deletes the blackout plane
		g_sci->_gfxFrameout->frameOut(true);
	}

	_decoder->close();

	if (_bundledVmd) {
		g_sci->getResMan()->unlockResource(_bundledVmd);
		_bundledVmd = nullptr;
	}

	if (!_showCursor) {
		g_sci->_gfxCursor32->unhide();
	}

	_isOpen = false;
	_isInitialized = false;
	_ignorePalettes = false;
	_lastYieldedFrameNo = 0;
	_planeIsOwned = true;
	_priority = 0;
	_drawRect = Common::Rect();
	return kIOSuccess;
}

VMDPlayer::VMDStatus VMDPlayer::getStatus() const {
	if (!_isOpen) {
		return kVMDNotOpen;
	}
	if (_decoder->isPaused()) {
		return kVMDPaused;
	}
	if (_decoder->isPlaying()) {
		return kVMDPlaying;
	}
	if (_decoder->endOfVideo()) {
		return kVMDFinished;
	}
	return kVMDOpen;
}

VMDPlayer::EventFlags VMDPlayer::kernelPlayUntilEvent(const EventFlags flags, const int16 lastFrameNo, const int16 yieldInterval) {
	assert(lastFrameNo >= -1);

	const int32 maxFrameNo = _decoder->getFrameCount() - 1;

	if (flags & kEventFlagToFrame) {
		_yieldFrame = MIN<int32>(lastFrameNo, maxFrameNo);
	} else {
		_yieldFrame = maxFrameNo;
	}

	if (flags & kEventFlagYieldToVM) {
		_yieldInterval = 3;
		if (yieldInterval == -1 && !(flags & kEventFlagToFrame)) {
			_yieldInterval = lastFrameNo;
		} else if (yieldInterval != -1) {
			_yieldInterval = MIN<int32>(yieldInterval, maxFrameNo);
		}
	} else {
		_yieldInterval = maxFrameNo;
	}

	return playUntilEvent(flags);
}

VMDPlayer::EventFlags VMDPlayer::playUntilEvent(const EventFlags flags, const uint32) {
	if (flags & kEventFlagReverse) {
		// NOTE: This flag may not work properly since SSCI does not care
		// if a video has audio, but the VMD decoder does.
		warning("VMD reverse playback flag was set. Please report this event to the bug tracker");
		const bool success = _decoder->setReverse(true);
		assert(success);
		_decoder->setVolume(0);
	}

	if (!_isInitialized) {
		_isInitialized = true;

		if (!_showCursor) {
			g_sci->_gfxCursor32->hide();
		}

		if (!_blackoutRect.isEmpty() && _planeIsOwned) {
			_blackoutPlane = new Plane(_blackoutRect);
			g_sci->_gfxFrameout->addPlane(*_blackoutPlane);
		}

		if (shouldUseCompositing()) {
			_isComposited = true;
			initComposited();
		} else {
			_isComposited = false;
			initOverlay();
		}
	}

	// Sleeping any more than 1/60th of a second will make the mouse feel
	// very sluggish during VMD action sequences because the frame rate of
	// VMDs is usually only 15fps
	return VideoPlayer::playUntilEvent(flags, 10);
}

VMDPlayer::EventFlags VMDPlayer::checkForEvent(const EventFlags flags) {
	const int currentFrameNo = _decoder->getCurFrame();

	if (currentFrameNo == _yieldFrame) {
		return kEventFlagEnd;
	}

	if (_yieldInterval > 0 &&
		currentFrameNo != _lastYieldedFrameNo &&
		(currentFrameNo % _yieldInterval) == 0) {

		_lastYieldedFrameNo = currentFrameNo;
		return kEventFlagYieldToVM;
	}

	EventFlags stopFlag = VideoPlayer::checkForEvent(flags);
	if (stopFlag) {
		return stopFlag;
	}

	const SciEvent event = _eventMan->getSciEvent(SCI_EVENT_HOT_RECTANGLE | SCI_EVENT_PEEK);
	if ((flags & kEventFlagHotRectangle) && event.type == SCI_EVENT_HOT_RECTANGLE) {
		return kEventFlagHotRectangle;
	}

	return kEventFlagNone;
}

void VMDPlayer::initOverlay() {
	// Make sure that any pending graphics changes from the game are submitted
	// before starting playback, since if they aren't, and the video player
	// yields back to the VM in the middle of playback, there may be a flash of
	// content that draws over the video. (This happens when subtitles are
	// enabled in Shivers.)
	g_sci->_gfxFrameout->frameOut(true);

#ifdef USE_RGB_COLOR
	// TODO: Allow interpolation for videos where the cursor is drawn, either by
	// writing to an intermediate 4bpp surface and using that surface during
	// cursor drawing, or by promoting the cursor code to use CursorMan, if
	// possible
	if (startHQVideo()) {
		redrawGameScreen();
	}
#endif
}

#ifdef USE_RGB_COLOR
void VMDPlayer::redrawGameScreen() const {
	if (!_hqVideoMode) {
		return;
	}

	Graphics::Surface *game = g_sci->_gfxFrameout->getCurrentBuffer().convertTo(g_system->getScreenFormat(), g_sci->_gfxPalette32->getHardwarePalette());
	assert(game);

	Common::Rect rects[4];
	int splitCount = splitRects(Common::Rect(game->w, game->h), _drawRect, rects);
	if (splitCount != -1) {
		while (splitCount--) {
			const Common::Rect &drawRect = rects[splitCount];
			g_system->copyRectToScreen(game->getBasePtr(drawRect.left, drawRect.top), game->pitch, drawRect.left, drawRect.top, drawRect.width(), drawRect.height());
		}
	}

	game->free();
	delete game;
}
#endif

void VMDPlayer::renderOverlay(const Graphics::Surface &nextFrame) const {
#ifdef USE_RGB_COLOR
	if (_hqVideoMode) {
		VideoPlayer::renderFrame(nextFrame);
		return;
	}
#endif

	Graphics::Surface out = g_sci->_gfxFrameout->getCurrentBuffer().getSubArea(_drawRect);
	renderLQToSurface<uint8>(out, nextFrame, _doublePixels, _blackLines);
	g_sci->_gfxFrameout->directFrameOut(_drawRect);
}

void VMDPlayer::submitPalette(const uint8 rawPalette[256 * 3]) const {
	if (_ignorePalettes) {
		return;
	}

	Palette palette;
	for (uint16 i = 0; i < _startColor; ++i) {
		palette.colors[i].used = false;
	}
	for (uint16 i = _endColor + 1; i < ARRAYSIZE(palette.colors); ++i) {
		palette.colors[i].used = false;
	}
#if SCI_VMD_BLACK_PALETTE
	if (_blackPalette) {
		for (uint16 i = _startColor; i <= _endColor; ++i) {
			palette.colors[i].r = palette.colors[i].g = palette.colors[i].b = 0;
			palette.colors[i].used = true;
		}
	} else
#endif
		fillPalette(rawPalette, palette);

	if (_isComposited) {
		SciBitmap *bitmap = _segMan->lookupBitmap(_bitmapId);
		bitmap->setPalette(palette);
		// NOTE: SSCI calls updateScreenItem and frameOut here, but this should
		// not be necessary in ScummVM since the new palette gets submitted
		// before the next frame is rendered, and the frame rendering call will
		// perform the same operations.
	} else {
		g_sci->_gfxPalette32->submit(palette);
		g_sci->_gfxPalette32->updateForFrame();
		g_sci->_gfxPalette32->updateHardware();
	}

#if SCI_VMD_BLACK_PALETTE
	if (_blackPalette) {
		fillPalette(rawPalette, palette);
		if (_isComposited) {
			SciBitmap *bitmap = _segMan->lookupBitmap(_bitmapId);
			bitmap->setPalette(palette);
		}
		g_sci->_gfxPalette32->submit(palette);
		g_sci->_gfxPalette32->updateForFrame();
		g_sci->_gfxPalette32->updateHardware();
	}
#endif

#ifdef USE_RGB_COLOR
	// Changes to the palette may affect areas outside of the video; when the
	// engine is rendering video in high color, palette changes will only take
	// effect once the entire screen is redrawn to the high color surface
	redrawGameScreen();
#endif
}

void VMDPlayer::closeOverlay() {
#ifdef USE_RGB_COLOR
	if (_hqVideoMode) {
		if (endHQVideo()) {
			g_sci->_gfxFrameout->resetHardware();
		}
		return;
	}
#endif

	g_sci->_gfxFrameout->frameOut(true, _drawRect);
}

void VMDPlayer::initComposited() {
	ScaleInfo vmdScaleInfo;

	if (_doublePixels) {
		vmdScaleInfo.x *= 2;
		vmdScaleInfo.y *= 2;
		vmdScaleInfo.signal = kScaleSignalManual;
	} else if (_stretchVertical) {
		vmdScaleInfo.y *= 2;
		vmdScaleInfo.signal = kScaleSignalManual;
	}

	const uint32 hunkPaletteSize = HunkPalette::calculateHunkPaletteSize(256, false);
	const int16 screenWidth = g_sci->_gfxFrameout->getCurrentBuffer().screenWidth;
	const int16 screenHeight = g_sci->_gfxFrameout->getCurrentBuffer().screenHeight;

	SciBitmap &vmdBitmap = *_segMan->allocateBitmap(&_bitmapId, _drawRect.width(), _drawRect.height(), 255, 0, 0, screenWidth, screenHeight, hunkPaletteSize, false, false);
	vmdBitmap.getBuffer().fillRect(Common::Rect(_drawRect.width(), _drawRect.height()), 0);

	CelInfo32 vmdCelInfo;
	vmdCelInfo.bitmap = _bitmapId;

	Video::AdvancedVMDDecoder *decoder = dynamic_cast<Video::AdvancedVMDDecoder *>(_decoder.get());
	assert(decoder);
	decoder->setSurfaceMemory(vmdBitmap.getPixels(), vmdBitmap.getWidth(), vmdBitmap.getHeight(), 1);

	if (_planeIsOwned) {
		_plane = new Plane(_drawRect, kPlanePicColored);
		if (_priority) {
			_plane->_priority = _priority;
		}
		g_sci->_gfxFrameout->addPlane(*_plane);
		_screenItem = new ScreenItem(_plane->_object, vmdCelInfo, Common::Point(), vmdScaleInfo);
	} else {
		_screenItem = new ScreenItem(_plane->_object, vmdCelInfo, Common::Point(_drawRect.left, _drawRect.top), vmdScaleInfo);
		if (_priority) {
			_screenItem->_priority = _priority;
		}
	}

	if (_blackLines) {
		_screenItem->_drawBlackLines = true;
	}

	// NOTE: There was code for positioning the screen item using insetRect
	// here, but none of the game scripts seem to use this functionality.

	g_sci->_gfxFrameout->addScreenItem(*_screenItem);
}

void VMDPlayer::renderComposited() const {
	g_sci->_gfxFrameout->updateScreenItem(*_screenItem);
	g_sci->_gfxFrameout->frameOut(true);
}

void VMDPlayer::closeComposited() {
	if (_bitmapId != NULL_REG) {
		_segMan->freeBitmap(_bitmapId);
		_bitmapId = NULL_REG;
	}

	if (!_planeIsOwned && _screenItem != nullptr) {
		g_sci->_gfxFrameout->deleteScreenItem(*_screenItem);
		_screenItem = nullptr;
	} else if (_plane != nullptr) {
		g_sci->_gfxFrameout->deletePlane(*_plane);
		_plane = nullptr;
	}

	if (!_leaveLastFrame && _leaveScreenBlack) {
		// This call *actually* deletes the plane/screen item
		g_sci->_gfxFrameout->frameOut(true);
	}
}

#pragma mark -
#pragma mark VMDPlayer - Rendering

void VMDPlayer::renderFrame(const Graphics::Surface &nextFrame) const {
	if (_isComposited) {
		renderComposited();
	} else {
		renderOverlay(nextFrame);
	}
}

void VMDPlayer::fillPalette(const uint8 rawPalette[256 * 3], Palette &outPalette) const {
	const byte *vmdPalette = rawPalette + _startColor * 3;
	for (uint16 i = _startColor; i <= _endColor; ++i) {
		uint8 r = *vmdPalette++;
		uint8 g = *vmdPalette++;
		uint8 b = *vmdPalette++;

		if (_boostPercent != 100 && i >= _boostStartColor && i <= _boostEndColor) {
			r = CLIP(r * _boostPercent / 100, 0, 255);
			g = CLIP(g * _boostPercent / 100, 0, 255);
			b = CLIP(b * _boostPercent / 100, 0, 255);
		}

		outPalette.colors[i].r = r;
		outPalette.colors[i].g = g;
		outPalette.colors[i].b = b;
		outPalette.colors[i].used = true;
	}
}

void VMDPlayer::setPlane(const int16 priority, const reg_t planeId) {
	_priority = priority;
	if (planeId != NULL_REG) {
		_plane = g_sci->_gfxFrameout->getPlanes().findByObject(planeId);
		assert(_plane != nullptr);
		_planeIsOwned = false;
	}
}

#pragma mark -
#pragma mark VMDPlayer - Palette

void VMDPlayer::restrictPalette(const uint8 startColor, const int16 endColor) {
	_startColor = startColor;
	// At least GK2 sends 256 as the end color, which is wrong,
	// but works in the original engine as the storage size is 4 bytes
	// and used values are clamped to 0-255
	_endColor = MIN<int16>(255, endColor);
}

#pragma mark -
#pragma mark DuckPlayer

DuckPlayer::DuckPlayer(SegManager *segMan, EventManager *eventMan) :
	VideoPlayer(eventMan),
	_decoder(new Video::AVIDecoder(Audio::Mixer::kSFXSoundType)),
	_plane(nullptr),
	_status(kDuckClosed),
	_drawRect(),
	_volume(Audio::Mixer::kMaxChannelVolume),
	_doFrameOut(false),
	_pixelDouble(false),
	_scaleBuffer(nullptr) {}

DuckPlayer::~DuckPlayer() {
	close();
	delete _decoder;
}

void DuckPlayer::open(const GuiResourceId resourceId, const int displayMode, const int16 x, const int16 y) {
	if (_status != kDuckClosed) {
		error("Attempted to play %u.duk, but another video was loaded", resourceId);
	}

	const Common::String fileName = Common::String::format("%u.duk", resourceId);
	if (!_decoder->loadFile(fileName)) {
		error("Can't open %s", fileName.c_str());
	}

	_decoder->setVolume(_volume);
	_pixelDouble = displayMode != 0;

	const int16 scale = _pixelDouble ? 2 : 1;
	// SSCI seems to incorrectly calculate the draw rect by scaling the origin
	// in addition to the width/height for the BR point
	_drawRect = Common::Rect(x, y,
							 x + _decoder->getWidth() * scale,
							 y + _decoder->getHeight() * scale);

	g_sci->_gfxCursor32->hide();

	if (_doFrameOut) {
		_plane = new Plane(_drawRect, kPlanePicColored);
		g_sci->_gfxFrameout->addPlane(*_plane);
		g_sci->_gfxFrameout->frameOut(true);
	}

	const Graphics::PixelFormat format = _decoder->getPixelFormat();

	if (_pixelDouble) {
		assert(_scaleBuffer == nullptr);
		_scaleBuffer = new byte[_drawRect.width() * _drawRect.height() * format.bytesPerPixel];
	}

	g_sci->_gfxFrameout->setPixelFormat(format);

	_status = kDuckOpen;
}

void DuckPlayer::play(const int lastFrameNo) {
	flushEvents(_eventMan);

	if (_status != kDuckPlaying) {
		_status = kDuckPlaying;
		_decoder->start();
	}

	while (!g_engine->shouldQuit()) {
		if (_decoder->endOfVideo() || (lastFrameNo != -1 && _decoder->getCurFrame() >= lastFrameNo)) {
			break;
		}

		g_sci->sleep(_decoder->getTimeToNextFrame());
		while (_decoder->needsUpdate()) {
			renderFrame();
		}

		SciEvent event = _eventMan->getSciEvent(SCI_EVENT_MOUSE_PRESS | SCI_EVENT_PEEK);
		if (event.type == SCI_EVENT_MOUSE_PRESS) {
			flushEvents(_eventMan);
			break;
		}

		event = _eventMan->getSciEvent(SCI_EVENT_KEYBOARD | SCI_EVENT_PEEK);
		if (event.type == SCI_EVENT_KEYBOARD) {
			bool stop = false;
			while ((event = _eventMan->getSciEvent(SCI_EVENT_KEYBOARD)),
				   event.type != SCI_EVENT_NONE) {
				if (event.character == SCI_KEY_ESC) {
					stop = true;
					break;
				}
			}

			if (stop) {
				flushEvents(_eventMan);
				break;
			}
		}
	}
}

void DuckPlayer::close() {
	if (_status == kDuckClosed) {
		return;
	}

	_decoder->close();

	const Graphics::PixelFormat format = Graphics::PixelFormat::createFormatCLUT8();
	g_sci->_gfxFrameout->setPixelFormat(format);

	g_sci->_gfxCursor32->unhide();

	if (_doFrameOut) {
		g_sci->_gfxFrameout->deletePlane(*_plane);
		g_sci->_gfxFrameout->frameOut(true);
		_plane = nullptr;
	}

	_pixelDouble = false;
	delete[] _scaleBuffer;
	_scaleBuffer = nullptr;

	_status = kDuckClosed;
}

static inline uint16 interpolate(const Graphics::PixelFormat &format, const uint16 p1, const uint16 p2) {
	uint8 r1, g1, b1, r2, g2, b2;
	format.colorToRGB(p1, r1, g1, b1);
	format.colorToRGB(p2, r2, g2, b2);
	return format.RGBToColor((r1 + r2) >> 1, (g1 + g2) >> 1, (b1 + b2) >> 1);
}

void DuckPlayer::renderFrame() const {
	const Graphics::Surface *surface = _decoder->decodeNextFrame();

	// Audio-only or non-updated frame
	if (surface == nullptr) {
		return;
	}

	assert(surface->format.bytesPerPixel == 2);

	if (_pixelDouble) {
		const uint16 *source = (const uint16 *)surface->getPixels();
		const Graphics::PixelFormat &format = surface->format;
		uint16 *target = (uint16 *)_scaleBuffer;

#ifndef SCI_DUCK_NO_INTERPOLATION
		// divide by 2 gets pixel pitch instead of byte pitch for source
		const uint16 sourcePitch = surface->pitch >> 1;
#endif

		const uint16 targetPitch = surface->pitch;
		const bool blackLined = ConfMan.getBool("enable_black_lined_video");
		for (int y = 0; y < surface->h - 1; ++y) {
			for (int x = 0; x < surface->w - 1; ++x) {
#ifndef SCI_DUCK_NO_INTERPOLATION
				const uint16 a = source[0];
				const uint16 b = source[1];
				const uint16 c = source[sourcePitch];
				const uint16 d = source[sourcePitch + 1];

				target[0] = a;
				target[1] = interpolate(format, a, b);
#else
				const uint16 value = *source;
				target[0] = value;
				target[1] = value;
#endif
				if (!blackLined) {
#ifndef SCI_DUCK_NO_INTERPOLATION
					target[targetPitch] = interpolate(format, a, c);
					target[targetPitch + 1] = interpolate(format, target[1], interpolate(format, c, d));
#else
					target[targetPitch] = value;
					target[targetPitch + 1] = value;
#endif
				}

				target += 2;
				++source;
			}

			const uint16 value = *source++;
			target[0] = value;
			target[1] = value;
			if (!blackLined) {
				target[targetPitch] = value;
				target[targetPitch + 1] = value;
			}
			target += 2;

			if (blackLined) {
				memset(target, 0, targetPitch * format.bytesPerPixel);
			}

			target += targetPitch;
		}

		for (int x = 0; x < surface->w; ++x) {
			const uint16 lastValue = *source++;
			target[0] = lastValue;
			target[1] = lastValue;

			if (!blackLined) {
				target[targetPitch] = lastValue;
				target[targetPitch + 1] = lastValue;
				target += 2;
			}
		}

		if (blackLined) {
			memset(target, 0, targetPitch);
		}

		g_system->copyRectToScreen(_scaleBuffer, surface->pitch * 2, _drawRect.left, _drawRect.top, _drawRect.width(), _drawRect.height());
	} else {
		g_system->copyRectToScreen(surface->getPixels(), surface->pitch, _drawRect.left, _drawRect.top, surface->w, surface->h);
	}

	g_sci->_gfxFrameout->updateScreen();
}

} // End of namespace Sci
