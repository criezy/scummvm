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

// Disable symbol overrides so that we can use system headers.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "backends/text-to-speech/macosx/macosx-text-to-speech.h"

#if defined(MACOSX) && defined(USE_OSX_TTS)
#include "common/translation.h"
#include <AppKit/NSSpeechSynthesizer.h>
#include <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>

NSSpeechSynthesizer* synthesizer;

MacOSXTextToSpeechManager::MacOSXTextToSpeechManager() {
	synthesizer = 0;
}

MacOSXTextToSpeechManager::~MacOSXTextToSpeechManager() {
	[synthesizer release];
}

bool MacOSXTextToSpeechManager::initSpeech(Common::Language lang, const Common::String &voice) {
	if (synthesizer != 0) {
		[synthesizer stopSpeaking];
		[synthesizer release];
		synthesizer = 0;
	}
	
	// Only English is supported
	switch(lang) {
	case Common::EN_ANY:
	case Common::EN_GRB:
	case Common::EN_USA:
			break;
	default:
		return false;
	}

	if (synthesizer != 0)
		[synthesizer release];
	synthesizer = [NSSpeechSynthesizer alloc];
	if (voice.empty())
		[synthesizer init];
	else {
		// Get current encoding
#ifdef USE_TRANSLATION
		CFStringRef encStr = CFStringCreateWithCString(NULL, TransMan.getCurrentCharset().c_str(), kCFStringEncodingASCII);
		CFStringEncoding stringEncoding = CFStringConvertIANACharSetNameToEncoding(encStr);
		CFRelease(encStr);
#else
		CFStringEncoding stringEncoding = kCFStringEncodingASCII;
#endif
		CFStringRef voiceNSString = CFStringCreateWithCString(NULL, voice.c_str(), stringEncoding);
		[synthesizer initWithVoice:(NSString *)voiceNSString];
		CFRelease(voiceNSString);
	}
	return true;
}

bool MacOSXTextToSpeechManager::startSpeech(const Common::String& text) {
	if (synthesizer == 0)
		return false;
	
	// Get current encoding
	// Assume translations manager encoding for now
#ifdef USE_TRANSLATION
	CFStringRef encStr = CFStringCreateWithCString(NULL, TransMan.getCurrentCharset().c_str(), kCFStringEncodingASCII);
	CFStringEncoding stringEncoding = CFStringConvertIANACharSetNameToEncoding(encStr);
	CFRelease(encStr);
#else
	CFStringEncoding stringEncoding = kCFStringEncodingASCII;
#endif

	CFStringRef textNSString = CFStringCreateWithCString(NULL, text.c_str(), stringEncoding);
	bool status = [synthesizer startSpeakingString:(NSString *)textNSString];
	CFRelease(textNSString);
	return status;
}

void MacOSXTextToSpeechManager::stopSpeech() {
	if (synthesizer != 0)
		[synthesizer stopSpeaking];
}

bool MacOSXTextToSpeechManager::isSpeaking() const {
	if (synthesizer == 0)
		return false;
	return [synthesizer isSpeaking];
}

Common::StringArray MacOSXTextToSpeechManager::listVoices(Common::Language lang) const {
	// Only English is supported
	switch(lang) {
	case Common::EN_ANY:
	case Common::EN_GRB:
	case Common::EN_USA:
			break;
	default:
		return Common::StringArray();
	}

	// Get current encoding
#ifdef USE_TRANSLATION
	CFStringRef encStr = CFStringCreateWithCString(NULL, TransMan.getCurrentCharset().c_str(), kCFStringEncodingASCII);
	CFStringEncoding stringEncoding = CFStringConvertIANACharSetNameToEncoding(encStr);
	CFRelease(encStr);
#else
	CFStringEncoding stringEncoding = kCFStringEncodingASCII;
#endif

	Common::StringArray voiceList;
	NSArray* voices =[NSSpeechSynthesizer availableVoices];
	for (NSString* string in voices) {
		voiceList.push_back(Common::String([string cStringUsingEncoding:stringEncoding]));
	}
	return voiceList;
}


#endif
