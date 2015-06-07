
#include "adt_page.h"
#include "log/default_log_levels.h"
#include <cassert>

namespace wowpp
{
	namespace adt
	{
#define MAKE_CHUNK_HEADER(first, second, third, fourth) \
	((UInt32)fourth) | (((UInt32)third) << 8) | (((UInt32)second) << 16) | (((UInt32)first) << 24)
		// MAIN Chunks
		static const UInt32 MVERChunk = MAKE_CHUNK_HEADER('M', 'V', 'E', 'R');		// 4 bytes, usually 0x12 format version
		static const UInt32 MHDRChunk = MAKE_CHUNK_HEADER('M', 'H', 'D', 'R');		// some offsets for faster access
		static const UInt32 MCINChunk = MAKE_CHUNK_HEADER('M', 'C', 'I', 'N');		// 
		static const UInt32 MTEXChunk = MAKE_CHUNK_HEADER('M', 'T', 'E', 'X');		// list of texture file names used by this page
		static const UInt32 MWIDChunk = MAKE_CHUNK_HEADER('M', 'W', 'I', 'D');		// list of wmo file names used by this page
		static const UInt32 MWMOChunk = MAKE_CHUNK_HEADER('M', 'W', 'M', 'O');		// 
		static const UInt32 MMIDChunk = MAKE_CHUNK_HEADER('M', 'M', 'I', 'D');		// 
		static const UInt32 MMDXChunk = MAKE_CHUNK_HEADER('M', 'M', 'D', 'X');		// 
		static const UInt32 MDDFChunk = MAKE_CHUNK_HEADER('M', 'D', 'D', 'F');		// 
		static const UInt32 MODFChunk = MAKE_CHUNK_HEADER('M', 'O', 'D', 'F');		// 
		static const UInt32 MCNKChunk = MAKE_CHUNK_HEADER('M', 'C', 'N', 'K');		// 
		static const UInt32 MH2OChunk = MAKE_CHUNK_HEADER('M', 'H', '2', 'O');		// 
		static const UInt32 MFBOChunk = MAKE_CHUNK_HEADER('M', 'F', 'B', 'O');		// 
		static const UInt32 MTXPChunk = MAKE_CHUNK_HEADER('M', 'T', 'X', 'P');		// 
		static const UInt32 MTXFChunk = MAKE_CHUNK_HEADER('M', 'T', 'X', 'F');		// 
		// SUB Chunks of MCNK chunk
		static const UInt32 MCVTSubChunk = MAKE_CHUNK_HEADER('M', 'C', 'V', 'T');	// 
		static const UInt32 MCNRSubChunk = MAKE_CHUNK_HEADER('M', 'C', 'N', 'R');	// 
#undef MAKE_CHUNK_HEADER

		namespace read
		{
			static bool readMVERChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				UInt32 formatVersion;
				if (!ptr->read(&formatVersion, sizeof(UInt32)))
				{
					ELOG("Could not read MVER chunk!");
					return false;
				}

				// Log file format version
				if (formatVersion != 0x12)
				{
					ILOG("Expected ADT file format 0x12, but found 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
						<< formatVersion << " instead. Please make sure that you use World of Warcraft Version 2.4.3!");
					return false;
				}

				return true;
			}

			static bool readMHDRChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				ptr->skip(chunkSize);
				return true;
			}

			struct MCNKHeader
			{
				/*0000*/	UInt32 flags;
				/*0004*/	UInt32 IndexX;
				/*0008*/	UInt32 IndexY;
				/*0012*/	UInt32 nLayers;				// maximum 4
				/*0016*/	UInt32 nDoodadRefs;
				/*0020*/	UInt32 ofsHeight;
				/*0024*/	UInt32 ofsNormal;
				/*0028*/	UInt32 ofsLayer;
				/*0032*/	UInt32 ofsRefs;
				/*0036*/	UInt32 ofsAlpha;
				/*0040*/	UInt32 sizeAlpha;
				/*0044*/	UInt32 ofsShadow;				// only with flags&0x1
				/*0048*/	UInt32 sizeShadow;
				/*0052*/	UInt32 areaid;
				/*0056*/	UInt32 nMapObjRefs;
				/*0060*/	UInt32 holes;
				/*0064*/	UInt32 ReallyLowQualityTextureingMap[4];	// It is used to determine which detail doodads to show. Values are an array of two bit unsigned integers, naming the layer.
				/*0080*/	UInt32 predTex;				// 03-29-2005 By ObscuR; TODO: Investigate
				/*0084*/	UInt32 noEffectDoodad;				// 03-29-2005 By ObscuR; TODO: Investigate
				/*0088*/	UInt32 ofsSndEmitters;
				/*0092*/	UInt32 nSndEmitters;				//will be set to 0 in the client if ofsSndEmitters doesn't point to MCSE!
				/*0096*/	UInt32 ofsLiquid;
				/*0100*/	UInt32 sizeLiquid;  			// 8 when not used; only read if >8.
				/*0104*/	float x, y, z;
				/*0116*/	UInt32 ofsMCCV; 				// only with flags&0x40, had UINT32 textureId; in ObscuR's structure.
				/*0120*/	UInt32 ofsMCLV; 				// 
				/*0124*/	UInt32 unused; 			        // currently unused
			};

			static bool readMCVTSubChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize, const MCNKHeader &header)
			{
				assert(header.IndexX < constants::TilesPerPage);
				assert(header.IndexY < constants::TilesPerPage);
				assert(chunkSize == sizeof(float) * constants::VertsPerTile);

				// Calculate tile index
				UInt32 tileIndex = header.IndexY + header.IndexX * constants::TilesPerPage;
				auto &tileHeight = page.terrain.heights[tileIndex];
				if (!ptr->read(&tileHeight[0], sizeof(float) * tileHeight.size()))
				{
					ELOG("Could not read MCVT subchunk (eof reached?)");
					return false;
				}

				// Add tile height
				for (auto &height : tileHeight)
				{
					height += header.z;
				}

				return true;
			}

			static bool readMCNKChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				// Read the chunk header
				MCNKHeader header;
				if (!ptr->read(&header, sizeof(MCNKHeader)))
				{
					ELOG("Could not read MCNK chunk header!");
					return false;
				}

				// From here on, we will read subchunks
				const size_t subStart = ptr->tell();
				while (ptr->tell() < subStart + chunkSize - 5)
				{
					// Read header of sub chunk
					UInt32 subHeader = 0, subSize = 0;
					bool result = (
						ptr->read(&subHeader, sizeof(UInt32)) &&
						ptr->read(&subSize, sizeof(UInt32))
						);
					if (!result || subHeader == 0)
					{
						ELOG("Could not read MCNK subchunk header!");
						return false;
					}

					// This is a dirty hack... something about the code in here isn't right.
					if (subHeader == MCNKChunk)
					{
						ptr->skip(-8);
						return true;
					}

					switch (subHeader)
					{
						case MCVTSubChunk:
						{
							result = readMCVTSubChunk(page, ptr, subSize, header);
							break;
						}

						case MCNRSubChunk:
						{
							ptr->skip(sizeof(UInt8) * 3 * 145 + 13);	// subSize does not match this! Fucking inconsistency...
							break;
						}

						default:
						{
							WLOG("Unknown subchunk found: " << subHeader);
							ptr->skip(subSize);
							break;
						}
					}

					if (!result)
					{
						WLOG("Error reading subchunk: " << subHeader);
						return false;
					}
				}

				return true;
			}
		}

		void load(const Ogre::DataStreamPtr &file, adt::Page &out_page)
		{
			// Chunk data
			UInt32 chunkHeader = 0, chunkSize = 0;

			// Read all chunks until end of file reached
			while (!(file->eof()))
			{
				// Reset chunk data
				chunkHeader = 0;
				chunkSize = 0;

				// Read chunk header
				if (!(file->read(&chunkHeader, sizeof(UInt32))) ||
					chunkHeader == 0)
				{
					break;
				}

				// Read chunk size
				file->read(&chunkSize, sizeof(UInt32));

				// Check chunk header
				bool result = true;
				switch (chunkHeader)
				{
					case MVERChunk:
					{
						result = read::readMVERChunk(out_page, file, chunkSize);
						break;
					}

					case MHDRChunk:
					{
						result = read::readMHDRChunk(out_page, file, chunkSize);
						break;
					}

					case MCNKChunk:
					{
						result = read::readMCNKChunk(out_page, file, chunkSize);
						break;
					}

					case MCINChunk:
					case MTEXChunk:
					case MWMOChunk:
					case MMIDChunk:
					case MMDXChunk:
					case MDDFChunk:
					case MODFChunk:
					case MH2OChunk:
					case MFBOChunk:
					case MTXPChunk:
					case MTXFChunk:
					case MWIDChunk:
					{
						// We don't want to handle these, but we don't want warnings either
						file->skip(chunkSize);
						break;
					}

					default:
					{
						// Skip chunk data
						//WLOG("Undefined map chunk: " << chunkHeader);
						file->skip(chunkSize);
						break;
					}
				}

				// Something failed
				if (!result)
				{
					ELOG("Could not load ADT file");
					file->close();

					break;
				}
			}
		}
	}
}