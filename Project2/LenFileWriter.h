/**
 * @file LenFileWriter.h
 * @brief Converts a CSV file into a length-indicated file (.len).
 * @author Dristi Barnwal (primary contributor)
 * @author Ethan Jackson, Marcus Julius, Teagen Lee, Natoli Mayu (reviewers) 
 * @date March 2026
 *
 * A "length-indicated" file means:
 * - Each record (line) is saved with its size (length) written first.
 * - Then the actual CSV record is written.
 *
 * Example of one record in a .len file:
 * 0000000042 56301,St Cloud,MN,Stearns,45.5579,-94.1632
 *
 * Here:
 * - "0000000042" is 10 digits (fixed width).
 * - It tells how many characters are in the CSV text after the space.
 * - The CSV text is still comma-separated.
 *
 * Why we do this:
 * - Later, we will build an index that stores ZIP -> file position (offset).
 * - Then we can jump directly to a ZIP record without reading the whole file.
 */
#ifndef LENFILEWRITER_H
#define LENFILEWRITER_H

#include <string>

/**
 * @brief Creates a .len file from a CSV file.
 *
 * This function:
 * 1) Opens the input CSV file
 * 2) Skips the first line (CSV header row)
 * 3) Writes a header record into the .len output file
 * 4) For each CSV record line:
 *    - writes 10-digit ASCII length
 *    - writes one space
 *    - writes the CSV record text
 *    - writes newline
 *
 * @param csvFile Path to the input CSV file (e.g. "us_postal_codes.csv")
 * @param lenFile Path to the output length-indicated file (e.g. "sorted.len")
 */
void makeLenFile(const std::string& csvFile,
                 const std::string& lenFile);

#endif