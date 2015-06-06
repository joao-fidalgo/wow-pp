//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software 
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#include "configuration.h"
#include "simple_file_format/sff_write.h"
#include "simple_file_format/sff_read_tree.h"
#include "simple_file_format/sff_load_file.h"
#include "common/constants.h"
#include "log/default_log_levels.h"
#include <fstream>
#include <limits>

namespace wowpp
{
	const UInt32 Configuration::EditorConfigVersion = 0x01;

	Configuration::Configuration()
		: dataPath("")
		, wowGamePath("C:\\Program Files (x86)\\World of Warcraft\\")
		, isLogActive(true)
		, logFileName("wowpp_editor.log")
		, isLogFileBuffering(false)
	{
	}

	bool Configuration::load(const String &fileName)
	{
		typedef String::const_iterator Iterator;
		typedef sff::read::tree::Table<Iterator> Table;

		Table global;
		std::string fileContent;

		std::ifstream file(fileName, std::ios::binary);
		if (!file)
		{
			if (save(fileName))
			{
				ILOG("Saved default settings as " << fileName);
			}
			else
			{
				ELOG("Could not save default settings as " << fileName);
			}

			return false;
		}

		try
		{
			sff::loadTableFromFile(global, fileContent, file);

			// Read config version
			UInt32 fileVersion = 0;
			if (!global.tryGetInteger("version", fileVersion) ||
				fileVersion != EditorConfigVersion)
			{
				file.close();

				if (save(fileName))
				{
					ILOG("Saved updated settings with default values as " << fileName);
				}
				else
				{
					ELOG("Could not save updated default settings as " << fileName);
				}

				return false;
			}

			if (const Table *const log = global.getTable("log"))
			{
				isLogActive = log->getInteger("active", static_cast<unsigned>(isLogActive)) != 0;
				logFileName = log->getString("fileName", logFileName);
				isLogFileBuffering = log->getInteger("buffering", static_cast<unsigned>(isLogFileBuffering)) != 0;
			}

			if (const Table *const game = global.getTable("game"))
			{
				dataPath = game->getString("dataPath", dataPath);
				wowGamePath = game->getString("wowGamePath", wowGamePath);
			}
		}
		catch (const sff::read::ParseException<Iterator> &e)
		{
			const auto line = std::count<Iterator>(fileContent.begin(), e.position.begin, '\n');
			ELOG("Error in config: " << e.what());
			ELOG("Line " << (line + 1) << ": " << e.position.str());
			return false;
		}

		return true;
	}


	bool Configuration::save(const String &fileName)
	{
		std::ofstream file(fileName.c_str());
		if (!file)
		{
			return false;
		}

		typedef char Char;
		sff::write::File<Char> global(file, sff::write::MultiLine);

		// Save file version
		global.addKey("version", EditorConfigVersion);
		global.writer.newLine();

		{
			sff::write::Table<Char> log(global, "log", sff::write::MultiLine);
			log.addKey("active", static_cast<unsigned>(isLogActive));
			log.addKey("fileName", logFileName);
			log.addKey("buffering", isLogFileBuffering);
			log.finish();
		}

		global.writer.newLine();

		{
			sff::write::Table<Char> game(global, "game", sff::write::MultiLine);
			game.addKey("dataPath", dataPath);
			game.addKey("wowGamePath", wowGamePath);
			game.finish();
		}

		return true;
	}
}
