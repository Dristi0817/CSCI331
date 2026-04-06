/**
 * @file LenFileReader.cpp
 * @brief Implementation for reading length-indicated records.
 * @author Dristi Barnwal (primary contributor)
 * @author Ethan Jackson, Marcus Julius, Teagen Lee, Natoli Mayu (reviewers)
 * @date March 2026
 */
#include "LenFileReader.h"
#include <cctype>    // for std::isdigit

using namespace std;

/**
 * @brief Reads one length-indicated record from the input stream.
 *
 * This is the key function that makes the data file "seekable".
 * Once we also build an index (ZIP -> offset), we can do:
 * - seekg(offset)
 * - readLenRecord(...)
 * and get exactly one ZIP record.
 *
 * @param in Input stream
 * @param recordLine Output record data (CSV-style text)
 * @return true if successful, false if EOF or bad format
 */
bool readLenRecord(ifstream& in, string& recordLine)
{
    recordLine.clear();

    // 1) Read 10 bytes for length
    char lenBuf[10];
    if (!in.read(lenBuf, 10)) {
        // likely EOF or stream error
        return false;
    }

    // 2) Validate they are digits (simple safety check)
    for (int i = 0; i < 10; i++) {
        if (!isdigit(static_cast<unsigned char>(lenBuf[i]))) {
            return false; // bad format
        }
    }

    // 3) Read the space after length
    char space;
    if (!in.get(space) || space != ' ') {
        return false; // bad format
    }

    // Convert length buffer into integer
    int length = stoi(string(lenBuf, 10));
    if (length < 0) return false;

    // 4) Read exactly "length" characters of record data
    recordLine.resize(length);
    if (!in.read(&recordLine[0], length)) {
        return false; // truncated file
    }

    // 5) Consume newline if present (supports \n or \r\n)
    if (in.peek() == '\n') {
        in.get();
    } else if (in.peek() == '\r') {
        in.get();
        if (in.peek() == '\n') in.get();
    }

    return true;
}