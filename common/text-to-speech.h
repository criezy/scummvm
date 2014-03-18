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

#ifndef BACKENDS_TEXT_TO_SPEECH_ABSTRACT_H
#define BACKENDS_TEXT_TO_SPEECH_ABSTRACT_H

#if defined(USE_TTS)

#include "common/str.h"
#include "common/str-array.h"
#include "common/language.h"

namespace Common {
	
/**
 * The TextToSpeechManager allows speech synthesis.
 *
 */
class TextToSpeechManager {
public:
	// This is a simple interface for TTS capabilities, probably too simple.
	// Here are some questions we should look at:
	// - Encoding of the string passed to initSpeech()
	// - The TTS engine can have additional settings, such as the tempo (e.g. "Speaking Rate" on OS X TTS)
	// - Do we need the possibility to list only male or female voices?
	// - Should we make it easy for several sub-systems to use the TTS engine, for example an engine to use TTS
	//   in the game language and when the GMM pops up it can speak the button text using the GUI language and
	//   when close it switch backs to the game language.
	//   If so I can see several ways to handle this. For example:
	//   - use a stack of voice settings. When a setting is pushed it calls initSpecch() with it, when it is popped
	//     it calls again initSpeech() with the previous one (if any).
	//   - use a context. We set the voice settings for a context and then the context should be passed to startSpeech().
	//     If it is different from the previous context it would call initSpeech().
	//   Either way this could be handled by the base class with derive classes only needing to implement initSpeech()
	//   (and maybe clearSpeech()).
	// - This implementation assumes only one Speech Synthesizer engine is available. Do we want the possibility to have
	//   more than one and let the user select which ones he want to use (or a priority order and then it uses the one
	//   with the highest priority that supports the requested language).
	
	TextToSpeechManager() {}
	virtual ~TextToSpeechManager() {}
		
	virtual bool initSpeech(Language, const String &voice = String()) {return false;}
	virtual bool startSpeech(const String&) {return false;}
	virtual void stopSpeech() {}
	virtual bool isSpeaking() const {return false;}

	virtual StringArray listVoices(Language) const {return StringArray();}
};
	
} // End of namespace Common

#endif

#endif // BACKENDS_TEXT_TO_SPEECH_ABSTRACT_H
