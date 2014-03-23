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
#include <Foundation/NSDictionary.h>
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

	// Look for a voice ID for the given language, and if given, voice name
	Common::String langCode = Common::getLanguageCode(lang);
	Common::String locale = Common::getLanguageLocale(lang);
	NSString *voiceId = 0, *altVoiceId = 0;
	// If no voice is provided, check first the default voice.
	if (voice.empty()) {
		NSString *defaultVoiceId = [NSSpeechSynthesizer defaultVoice];
		NSDictionary *voiceAttr = [NSSpeechSynthesizer attributesForVoice:defaultVoiceId];
		Common::String voiceLocale([[voiceAttr objectForKey:NSVoiceLocaleIdentifier] UTF8String]);
		if (voiceLocale.hasPrefix(locale))
			voiceId = defaultVoiceId;
		else if (voiceLocale.hasPrefix(langCode))
			altVoiceId = defaultVoiceId;
	}
	// Then look for other available voices
	if (voiceId == 0) {
		NSArray *voices =[NSSpeechSynthesizer availableVoices];
		for (NSString *availableVoiceId in voices) {
			NSDictionary *voiceAttr = [NSSpeechSynthesizer attributesForVoice:availableVoiceId];
			// Check the name
			if (!voice.empty()) {
				Common::String voiceName([[voiceAttr objectForKey:NSVoiceName] UTF8String]);
				if (voiceName != voice)
					continue;
			}
			Common::String voiceLocale([[voiceAttr objectForKey:NSVoiceLocaleIdentifier] UTF8String]);
			if (voiceLocale.hasPrefix(locale)) {
				voiceId = availableVoiceId;
				break;
			} else if (altVoiceId == 0 && voiceLocale.hasPrefix(langCode))
				altVoiceId = availableVoiceId;
		}
	}

	if (voiceId == 0) {
		if (altVoiceId == 0)
			return false;
		voiceId = altVoiceId;
	}
	synthesizer = [[NSSpeechSynthesizer alloc] initWithVoice:voiceId];
	return true;
}

bool MacOSXTextToSpeechManager::startSpeech(const Common::String& text, const Common::String& encoding) {
	if (synthesizer == 0)
		return false;
	
	// Get current encoding
	CFStringEncoding stringEncoding = kCFStringEncodingASCII;
	if (!encoding.empty()) {
		CFStringRef encStr = CFStringCreateWithCString(NULL, encoding.c_str(), kCFStringEncodingASCII);
		stringEncoding = CFStringConvertIANACharSetNameToEncoding(encStr);
		CFRelease(encStr);
	}

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
	Common::String langCode = Common::getLanguageCode(lang);
	Common::String locale = Common::getLanguageLocale(lang);

	// First list those with a match on both the language and region code.
	// Then add those with a match on the language code only.
	Common::StringArray voiceList, altVoiceList;
	NSArray *voices =[NSSpeechSynthesizer availableVoices];
	for (NSString *voiceId in voices) {
		NSDictionary *voiceAttr = [NSSpeechSynthesizer attributesForVoice:voiceId];
		Common::String voiceLocale([[voiceAttr objectForKey:NSVoiceLocaleIdentifier] UTF8String]);
		if (voiceLocale.hasPrefix(locale))
			voiceList.push_back(Common::String([[voiceAttr objectForKey:NSVoiceName] UTF8String]));
		else if (voiceLocale.hasPrefix(langCode))
			altVoiceList.push_back(Common::String([[voiceAttr objectForKey:NSVoiceName] UTF8String]));
	}
	voiceList.push_back(altVoiceList);
	return voiceList;
}


#endif
