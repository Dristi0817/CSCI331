/**
 * @file LenFileReader.h
 * @brief Reads one record at a time from a length-indicated file (.len).
 * @author Dristi Barnwal (primary contributor)
 * @author Ethan Jackson, Marcus Julius, Teagen Lee, Natoli Mayu
 *
 * The LEN file format for each record is:
 * [10 ASCII digits length][space][record text][newline]
 *
 * Example:
 * 0000000042 56301,St Cloud,MN,Stearns,45.5579,-94.1632
 *
 * This reader helps the program follow the RAM rule:
 * - Only ONE record is read at a time.
 * - The entire data file is not loaded into memory.
 */
#ifndef LENFILEREADER_H
#define LENFILEREADER_H

#include <string>
#include <fstream>

/**
 * @brief Reads one length-indicated record from a .len file.
 *
 * Steps:
 * 1) Read 10 characters for the length
 * 2) Convert them to an integer
 * 3) Read 1 space character
 * 4) Read exactly "length" characters into recordLine
 * 5) Optionally consume newline at the end
 *
 * @param in Input file stream (must already be open)
 * @param recordLine Output string that receives the record data text
 * @return true if a record was read successfully, false if EOF or format error
 */
bool readLenRecord(std::ifstream& in, std::string& recordLine);

#endif