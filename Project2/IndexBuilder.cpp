/**
 * @file IndexBuilder.cpp
 * @brief Implementation for building ZIP->offset index files.
 * @author Dristi Barnwal (primary contributor)
 * @author Ethan Jackson, Marcus Julius, Teagen Lee, Natoli Mayu (reviewers)
 * @date March 2026
 */
#include "IndexBuilder.h"
#include "LenFileReader.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace std;

/**
 * @brief Builds an index file for the provided .len file.
 *
 * Important detail:
 * - We record the offset BEFORE reading the record.
 * - That offset points to the start of the record length field.
 *
 * @param lenFile Input .len data file
 * @param indexFile Output .idx file
 */
void buildIndex(const string& lenFile, const string& indexFile)
{
    ifstream in(lenFile);
    if (!in) {
        cout << "Error: Cannot open LEN file: " << lenFile << "\n";
        return;
    }

    ofstream out(indexFile);
    if (!out) {
        cout << "Error: Cannot create index file: " << indexFile << "\n";
        return;
    }

    // Read the LEN header record first (we skip it for indexing records).
    string header;
    if (!readLenRecord(in, header)) {
        cout << "Error: LEN file has no header or bad format.\n";
        return;
    }

    // Optional index file header (helps identify it)
    out << "IDX,1\n";

    long long count = 0;

    while (true)
    {
        // Save the start offset of the next record
        streampos pos = in.tellg();

        // Read record text
        string record;
        if (!readLenRecord(in, record))
            break; // reached EOF

        // ZIP is the first field before comma
        size_t comma = record.find(',');
        if (comma == string::npos) {
            // Bad record: skip it
            continue;
        }

        string zip = record.substr(0, comma); // keep leading zeros if any

        // Write: ZIP offset
        out << zip << " " << static_cast<long long>(pos) << "\n";
        count++;
    }

    cout << "Index created: " << indexFile << " (entries=" << count << ")\n";
}