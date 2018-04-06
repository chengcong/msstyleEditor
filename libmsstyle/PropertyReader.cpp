#include "PropertyReader.h"
#include "StyleProperty.h"
#include "VisualStyleDefinitions.h"

#include <algorithm>
#include <Windows.h>

#undef min

namespace libmsstyle
{
	namespace rw
	{
		PropertyReader::PropertyReader(int numClasses)
			: m_numClasses(numClasses)
		{
		}

		const char* PropertyReader::ReadNextProperty(const char* source, const char* end, StyleProperty* prop)
		{
			const char* cursor = source;
			while (!IsProbablyValidHeader(cursor))
				cursor += 1;

			int diff = cursor - source;
			if (diff > 0)
			{
				char textbuffer[21];
				sprintf(textbuffer, "Skipped %d bytes!\r\n", diff);
				OutputDebugStringA(textbuffer);
			}

			// Copy the property header
			memcpy(&(prop->header), cursor, sizeof(PropertyHeader));
			cursor += sizeof(PropertyHeader);


			// Copy the data
			switch (prop->header.typeID)
			{
			// 32 bytes
			case IDENTIFIER::FILENAME:
			case IDENTIFIER::DISKSTREAM:
			case IDENTIFIER::FONT:
			{
				memcpy(&(prop->data), cursor, 12);
				cursor += 12;
				prop->bytesAfterHeader += 12;
			} break;
			// 40 bytes
			case IDENTIFIER::INT:
			case IDENTIFIER::SIZE:
			case IDENTIFIER::BOOL:
			case IDENTIFIER::COLOR:
			case IDENTIFIER::ENUM:
			case IDENTIFIER::POSITION:
			case IDENTIFIER::UNKNOWN_241:
			{
				memcpy(&(prop->data), cursor, 12);
				cursor += 12;
				prop->bytesAfterHeader += 12;

				// Copy the rest if not a short prop
				if (prop->data.booltype.shortFlag == 0)
				{
					char* dataPlusPSHDR = reinterpret_cast<char*>(&prop->data) + 12;
					memcpy(dataPlusPSHDR, cursor, 8);
					cursor += 8;
					prop->bytesAfterHeader += 8;
				}
			} break;
			// 48 bytes
			case IDENTIFIER::RECT:
			case IDENTIFIER::MARGINS:
			{
				memcpy(&(prop->data), cursor, 12);
				cursor += 12;
				prop->bytesAfterHeader += 12;

				// Copy the rest if not a short prop
				if (prop->data.recttype.shortFlag == 0)
				{
					char* dataPlusPSHDR = reinterpret_cast<char*>(&prop->data) + 12;
					memcpy(dataPlusPSHDR, cursor, 16);
					cursor += 16;
					prop->bytesAfterHeader += 16;
				}
			} break;
			// Arbitrary
			case IDENTIFIER::INTLIST:
			{
				// There was an intlist, without short indicator
				// immediately followed by a property instead of
				// "numints" + list data. It ended exactly on a
				// 8byte boundary.
				if (IsProbablyValidHeader(cursor+12))
				{
					memcpy(&(prop->data), cursor, 12);
					cursor += 12;
					prop->bytesAfterHeader += 12;
					OutputDebugStringA("Bad INTLIST!!!!\r\n");
					break;
				}
				else
				{
					memcpy(&(prop->data), cursor, 16);
					cursor += 16;
					prop->bytesAfterHeader += 16;
				}

				int32_t numValues = prop->data.intlist.numints;

				prop->intlist.reserve(numValues);
				for (int32_t i = 0; i < numValues; ++i)
				{
					const int32_t* valuePtr = reinterpret_cast<const int32_t*>(cursor);
					prop->intlist.push_back(*valuePtr);
					cursor += sizeof(int32_t);
				}

				prop->bytesAfterHeader += prop->data.intlist.numints * sizeof(int32_t);
			} break;
			case IDENTIFIER::STRING:
			{
				memcpy(&(prop->data), cursor, 12);
				cursor += 12;
				prop->bytesAfterHeader += 12;

				int32_t szLen = prop->data.texttype.sizeInBytes / 2;

				prop->text.reserve(szLen);
				for (int32_t i = 0; i < szLen - 1; ++i) // dont need the NULL term.
				{
					const wchar_t* valuePtr = reinterpret_cast<const wchar_t*>(cursor);
					prop->text.push_back(*valuePtr);
					cursor += sizeof(wchar_t);
				}

				prop->bytesAfterHeader += prop->data.texttype.sizeInBytes;
			} break;
			default:
			{
				char textbuffer[64];
				sprintf(textbuffer, "Unknown Type: %d\r\n", prop->header.typeID);
				OutputDebugStringA(textbuffer);
			} break;
			}

			char textbuffer[64];
			if (prop->GetRegularPropertySize() < prop->GetPropertySizeAsFound())
			{
				sprintf(textbuffer, "next header %d bytes after data - bigger\n", prop->bytesAfterHeader);
			}
			if (prop->GetRegularPropertySize() > prop->GetPropertySizeAsFound())
			{
				sprintf(textbuffer, "next header %d bytes after data - smaller\n", prop->bytesAfterHeader);
			}
			if (prop->GetRegularPropertySize() == prop->GetPropertySizeAsFound())
			{
				sprintf(textbuffer, "next header %d bytes after data - exact\n", prop->bytesAfterHeader);
			}
			OutputDebugStringA(textbuffer);

			return cursor;
		}


		bool PropertyReader::IsProbablyValidHeader(const char* source)
		{
			const PropertyHeader* header = reinterpret_cast<const PropertyHeader*>(source);

			if (header->typeID < IDENTIFIER::ENUM || header->typeID >= IDENTIFIER::COLORSCHEMES)
				return false;

			// Some color and font props use an type id as name id.
			// They seem to contain valid data, so ill include them.
			if (header->nameID == IDENTIFIER::COLOR &&
				header->typeID == IDENTIFIER::COLOR)
				return true;
			if (header->nameID == IDENTIFIER::FONT &&
				header->typeID == IDENTIFIER::FONT)
				return true;
			if (header->nameID == IDENTIFIER::DISKSTREAM &&
				header->typeID == IDENTIFIER::DISKSTREAM)
				return true;
			if (header->nameID == IDENTIFIER::STREAM &&
				header->typeID == IDENTIFIER::STREAM)
				return true;

			// Not sure where the line for valid name ids is.
			// Upper bound is ATLASRECT, but im leaving a bit of space
			// for unknown props.
			if (header->nameID < IDENTIFIER::COLORSCHEMES ||
				header->nameID > 10000)
				return false;

			// First attempt was 255, but yielded false-positives.
			// Smaller than 200 eliminates type & prop name ids.
			if (header->partID < 0 ||
				header->partID > 199)
				return false;

			if (header->stateID < 0 ||
				header->stateID > 199)
				return false;

			// Not a known class
			if (header->classID < 0 ||
				header->classID > m_numClasses)
				return false;

			return true;
		}
	}
}