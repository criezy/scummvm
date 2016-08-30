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

#include "common/debug-channels.h"
#include "common/scummsys.h"
#include "common/archive.h"
#include "common/config-manager.h"
#include "common/error.h"
#include "common/fs.h"
#include "common/rect.h"
#include "common/str.h"

#include "engines/util.h"

#include "testbed/events.h"
#include "testbed/fs.h"
#include "testbed/graphics.h"
#include "testbed/midi.h"
#include "testbed/misc.h"
#include "testbed/savegame.h"
#include "testbed/sound.h"
#include "testbed/testbed.h"
#ifdef USE_LIBCURL
#include "testbed/cloud.h"
#endif
#ifdef USE_SDL_NET
#include "testbed/webserver.h"
#endif

namespace Testbed {

void TestbedExitDialog::init() {
	_xOffset = 25;
	_yOffset = 0;
	Common::String text = "Thank you for using ScummVM testbed! Here are yor summarized results:";
	addText(450, 20, text, Graphics::kTextAlignCenter, _xOffset, 15);
	Common::Array<Common::String> strArray;
	GUI::ListWidget::ColorList colors;

	for (Common::Array<Testsuite *>::const_iterator i = _testsuiteList.begin(); i != _testsuiteList.end(); ++i) {
		strArray.push_back(Common::String::format("%s :", (*i)->getDescription()));
		colors.push_back(GUI::ThemeEngine::kFontColorNormal);
		if ((*i)->isEnabled()) {
			strArray.push_back(Common::String::format("Passed: %d  Failed: %d Skipped: %d", (*i)->getNumTestsPassed(), (*i)->getNumTestsFailed(), (*i)->getNumTestsSkipped()));
		} else {
			strArray.push_back("Skipped");
		}
		colors.push_back(GUI::ThemeEngine::kFontColorAlternate);
	}

	addList(0, _yOffset, 500, 200, strArray, &colors);
	text = "More Details can be viewed in the Log file : " + ConfParams.getLogFilename();
	addText(450, 20, text, Graphics::kTextAlignLeft, 0, 0);
	if (ConfParams.getLogDirectory().size()) {
		text = "Directory : " + ConfParams.getLogDirectory();
	} else {
		text = "Directory : .";
	}
	addText(500, 20, text, Graphics::kTextAlignLeft, 0, 0);
	_yOffset += 5;
	addButtonXY(_xOffset + 80, _yOffset, 120, 24, "Rerun test suite", kCmdRerunTestbed);
	addButtonXY(_xOffset + 240, _yOffset, 60, 24, "Close", GUI::kCloseCmd);
}

void TestbedExitDialog::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	switch (cmd) {
	default:
		break;

	case kCmdRerunTestbed :
		ConfParams.setRerunFlag(true);
		cmd = GUI::kCloseCmd;
		break;
	}

	GUI::Dialog::handleCommand(sender, cmd, data);
}

bool TestbedEngine::hasFeature(EngineFeature f) const {
	return (f == kSupportsRTL) ? true : false;
}

TestbedEngine::TestbedEngine(OSystem *syst)
 : Engine(syst) {
	// Put your engine in a sane state, but do nothing big yet;
	// in particular, do not load data from files; rather, if you
	// need to do such things, do them from init().

	// Do not initialize graphics here

	// However this is the place to specify all default directories
	// Put game-data dir in search path
	Common::FSNode gameRoot(ConfMan.get("path"));
	if (gameRoot.exists()) {
		SearchMan.addDirectory(gameRoot.getDisplayName(), gameRoot);
	}

	DebugMan.addDebugChannel(kTestbedLogOutput, "LOG", "Log of test results generated by testbed");
	DebugMan.addDebugChannel(kTestbedEngineDebug, "Debug", "Engine-specific debug statements");
	DebugMan.enableDebugChannel("LOG");

	// Initialize testsuites here
	// GFX
	Testsuite *ts = new GFXTestSuite();
	_testsuiteList.push_back(ts);
	// FS
	ts = new FSTestSuite();
	_testsuiteList.push_back(ts);
	// Savegames
	ts = new SaveGameTestSuite();
	_testsuiteList.push_back(ts);
	// Misc.
	ts = new MiscTestSuite();
	_testsuiteList.push_back(ts);
	// Events
	ts = new EventTestSuite();
	_testsuiteList.push_back(ts);
	// Sound
	ts = new SoundSubsystemTestSuite();
	_testsuiteList.push_back(ts);
	// Midi
	ts = new MidiTestSuite();
	_testsuiteList.push_back(ts);
#ifdef USE_LIBCURL
	// Cloud
	ts = new CloudTestSuite();
	_testsuiteList.push_back(ts);
#endif
#ifdef USE_SDL_NET
	// Webserver
	ts = new WebserverTestSuite();
	_testsuiteList.push_back(ts);
#endif
}

TestbedEngine::~TestbedEngine() {
	ConfParams.deleteWriteStream();
	// Remove all of our debug levels here
	DebugMan.clearAllDebugChannels();

	for (Common::Array<Testsuite *>::const_iterator i = _testsuiteList.begin(); i != _testsuiteList.end(); ++i) {
		delete (*i);
	}
}

void TestbedEngine::invokeTestsuites(TestbedConfigManager &cfMan) {
	Common::Array<Testsuite *>::const_iterator iter;
	uint count = 1;
	Common::Point pt = Testsuite::getDisplayRegionCoordinates();
	int numSuitesEnabled = cfMan.getNumSuitesEnabled();

	if (!numSuitesEnabled)
		return;

	for (iter = _testsuiteList.begin(); iter != _testsuiteList.end(); iter++) {
		if (shouldQuit()) {
			return;
		}
		(*iter)->reset();
		if ((*iter)->isEnabled()) {
			Testsuite::updateStats("Testsuite", (*iter)->getName(), count++, numSuitesEnabled, pt);
			(*iter)->execute();
		}
	}
}

Common::Error TestbedEngine::run() {
	// Initialize graphics using following:
	initGraphics(320, 200, false);

	// As of now we are using GUI::MessageDialog for interaction, Test if it works.
	// interactive mode could also be modified by a config parameter "non-interactive=1"
	// TODO: Implement that

	TestbedConfigManager cfMan(_testsuiteList, "testbed.config");

	// Keep running if rerun requested

	do {
		Testsuite::clearEntireScreen();
		cfMan.selectTestsuites();
		// Init logging
		ConfParams.initLogging(true);
		invokeTestsuites(cfMan);
		// Check if user wanted to exit.
		if (Engine::shouldQuit()) {
			return Common::kNoError;
		}

		TestbedExitDialog tbDialog(_testsuiteList);
		tbDialog.init();
		tbDialog.run();

	} while (ConfParams.isRerunRequired());

	return Common::kNoError;
}

} // End of namespace Testbed
