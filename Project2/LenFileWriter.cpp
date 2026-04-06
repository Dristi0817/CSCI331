/**
 * @file LenFileWriter.cpp
 * @brief Implementation of CSV -> .len conversion.
 * @author Dristi Barnwal (primary contributor)
 * @author Ethan Jackson, Marcus Julius, Teagen Lee, Natoli Mayu (reviewers)
 * @date March 2026
 * 
 * This file contains the code that writes the length-indicated format.
 */
#include "LenFileWriter.h"
#include <fstream>
#include <iostream>
#include <iomanip>

using namespace std;

/**
 * @brief Writes one length-indicated line to an output stream.
 *
 * The output format is:
 * [10 digits length][space][text][newline]
 *
 * Example:
 * 0000000005 hello
 *
 * @param out Output stream (file)
 * @param text The text to write (this is the record data)
 * @return true if writing succeeded, false otherwise
 */
static bool writeLenLine(ostream& out, const string& text) {
    // If the record text is empty, we choose not to write it.
    if (text.empty()) return false;

    // Write length as 10 digits with leading zeros.
    // Example: 42 becomes 0000000042
    out << setw(10) << setfill('0') << text.size()
        << ' ' << text << '\n';

    // Return whether the stream is still good.
    return static_cast<bool>(out);
}

/**
 * @brief Converts a CSV file into a length-indicated (.len) file.
 *
 * Simple steps:
 * 1) Open input CSV file for reading
 * 2) Open output LEN file for writing
 * 3) Skip the first CSV line (header)
 * 4) Write a LEN header record (also length-indicated)
 * 5) For every remaining CSV line, write it as a LEN record
 *
 * @param csvFile Input CSV filename
 * @param lenFile Output LEN filename
 */
void makeLenFile(const string& csvFile, const string& lenFile)
{
    ifstream in(csvFile);
    if (!in) {
        cout << "Error: Cannot open CSV file: " << csvFile << "\n";
        return;
    }

    ofstream out(lenFile);
    if (!out) {
        cout << "Error: Cannot create LEN file: " << lenFile << "\n";
        return;
    }

    // Read and ignore the CSV header row (field names line).
    string csvHeader;
    if (!getline(in, csvHeader)) {
        cout << "Error: CSV file is empty: " << csvFile << "\n";
        return;
    }

    // Write a simple LEN header record (you can expand it later).
    // This record itself also uses the LEN format.
    // Example header text: HDR,ZipLenFile,1,SIZEFMT=ASCII,SIZEWIDTH=10
    string lenHeaderText = "HDR,ZipLenFile,1,SIZEFMT=ASCII,SIZEWIDTH=10";
    if (!writeLenLine(out, lenHeaderText)) {
        cout << "Error: Failed writing LEN header record.\n";
        return;
    }

    long long count = 0;

    // Read each CSV record line and write it into LEN file.
    string line;
    while (getline(in, line)) {
        if (line.empty()) continue; // skip empty lines

        if (!writeLenLine(out, line)) {
            cout << "Warning: Skipped a line that could not be written.\n";
            continue;
        }
        count++;
    }

    cout << "LEN file created: " << lenFile << " (records=" << count << ")\n";
}