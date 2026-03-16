/**
 * @file main.cpp
 * @brief Zip Code Group Project 2.0 main controller
 *
 * This is a “multi-mode” program. It can run four different tasks depending on
 * the command line arguments.
 *
 * -----------------------------------------------------------------------------
 * MODES (commands you can run):
 *
 * -# Project 1 style CSV analysis (streaming / no vector)
 *    ./zipprog <csv_file>
 * -# Convert CSV → length-indicated data file (.len)
 *    ./zipprog --make-len <input.csv> <output.len>
 * -# Build primary-key index from .len
 *    ./zipprog --build-index <data.len> <index.idx>
 * -# Search ZIP(s) using index (flags like -Z56301)
 *    ./zipprog --search <data.len> <index.idx> -Z56301 -Z99546 -Z99999
 *
 * Notes:
 * - For Project 2 RAM rule during searching:
 *   We only keep:
 *     (1) index in RAM
 *     (2) one record string / one ZipCodeRecord at a time
 * - This file uses the fixed-width ASCII length format:
 *     [10 digits][space][recordText][newline]
 *
 * @author Dristi Barnwal (Project 2 main contributor)
 * @author Teagen Lee (Project 1 main contributor)
 * @author Ethan Jackson (additional revisions)
 * @author Marcus Julius, Natoli Mayu (reviewers)
 * @date March 2026
 */

#include "ZipCodeBuffer.h"
#include "HeaderBuffer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <iomanip>
#include <limits>
#include <cctype>

using namespace std;

/**
 * @struct StateExtremes
 * @brief Keeps the most extreme ZIP codes for one state (east/west/north/south)
 * 
 * We store:
 * - Which ZIP code is easternmost / westernmost / northernmost / southernmost
 * - The coordinate values used to compare
 *
 * Tie-breaking rule:
 * - If two ZIPs tie on coordinate, choose the smaller ZIP so results are stable
 *   even if the input rows are shuffled.
 *
 * @since Project 1
 */
struct StateExtremes {
    int easternmost;
    int westernmost;
    int northernmost;
    int southernmost;

    double minLongitude;
    double maxLongitude;
    double maxLatitude;
    double minLatitude;

    StateExtremes()
        : easternmost(0), westernmost(0), northernmost(0), southernmost(0),
          minLongitude(numeric_limits<double>::max()),
          maxLongitude(numeric_limits<double>::lowest()),
          maxLatitude(numeric_limits<double>::lowest()),
          minLatitude(numeric_limits<double>::max()) {}
};

/**
 * @brief If coordinate ties, choose the smaller ZIP.
 * @param candidate ZIP we are considering
 * @param current ZIP already stored
 * @return true if candidate should replace current
 * @since Project 1
 */
static bool smallerZipWins(int candidate, int current) {
    if (current == 0) return true; // current not set yet
    return candidate < current;
}

/**
 * @brief Update state extremes using one record.
 * @param stateMap Map of state → extremes
 * @param record One ZIP record
 * @return void (stateMap is an in+out parameter)
 */
static void updateStateExtremes(map<string, StateExtremes>& stateMap,
                                const ZipCodeRecord& record) {
    const string& state = record.state;
    StateExtremes& ex = stateMap[state]; // creates entry if missing

    // EASTERNMOST (min longitude)
    if (record.longitude < ex.minLongitude) {
        ex.minLongitude = record.longitude;
        ex.easternmost = record.zipCode;
    } else if (record.longitude == ex.minLongitude &&
               smallerZipWins(record.zipCode, ex.easternmost)) {
        ex.easternmost = record.zipCode;
    }

    // WESTERNMOST (max longitude)
    if (record.longitude > ex.maxLongitude) {
        ex.maxLongitude = record.longitude;
        ex.westernmost = record.zipCode;
    } else if (record.longitude == ex.maxLongitude &&
               smallerZipWins(record.zipCode, ex.westernmost)) {
        ex.westernmost = record.zipCode;
    }

    // NORTHERNMOST (max latitude)
    if (record.latitude > ex.maxLatitude) {
        ex.maxLatitude = record.latitude;
        ex.northernmost = record.zipCode;
    } else if (record.latitude == ex.maxLatitude &&
               smallerZipWins(record.zipCode, ex.northernmost)) {
        ex.northernmost = record.zipCode;
    }

    // SOUTHERNMOST (min latitude)
    if (record.latitude < ex.minLatitude) {
        ex.minLatitude = record.latitude;
        ex.southernmost = record.zipCode;
    } else if (record.latitude == ex.minLatitude &&
               smallerZipWins(record.zipCode, ex.southernmost)) {
        ex.southernmost = record.zipCode;
    }
}

/**
 * @brief Print the state extremes table
 * @param stateMap Map of state → extremes
 * @since Project 1
 */
static void printStateExtremesTable(const map<string,StateExtremes>& stateMap) {
    cout << left;
    cout << setw(8)  << "State"
         << setw(15) << "Easternmost"
         << setw(15) << "Westernmost"
         << setw(15) << "Northernmost"
         << setw(15) << "Southernmost"
         << "\n";
    cout << string(68, '-') << "\n";

    for (const auto& entry : stateMap) {
        const string& state = entry.first;
        const StateExtremes& ex = entry.second;

        cout << setw(8) << state;

        cout << setfill('0') << setw(5) << ex.easternmost
             << setfill(' ') << setw(10) << " ";

        cout << setfill('0') << setw(5) << ex.westernmost
             << setfill(' ') << setw(10) << " ";

        cout << setfill('0') << setw(5) << ex.northernmost
             << setfill(' ') << setw(10) << " ";

        cout << setfill('0') << setw(5) << ex.southernmost
             << setfill(' ') << "\n";
    }

    cout << "\nTotal states/territories: " << stateMap.size() << "\n";
}

/**
 * @brief Write one “length-indicated” record to an output stream.
 *
 * Format:
 *   [digits][space][text][newline]
 *
 * Example:
 *   0000000042 56301,St Cloud,MN,Stearns,45.5579,-94.1632
 *
 * @param out Output stream
 * @param text Record text (still CSV inside)
 * @param width The number of digits to read
 * @return true if write succeeded
 * @bug does not yet support length indicators in a non-ASCII format
 */
static bool writeLenLine(ostream& out, const string& text, int width) {
    if (text.empty()) return false;

    out << setw(width) << setfill('0') << text.size()
        << ' ' << text << '\n';

    return static_cast<bool>(out);
}

/**
 * @brief Read one “length-indicated” record from an input stream.
 *
 * Steps:
 * 1) Read a specified number of chars (must be digits)
 * 2) Read one space
 * 3) Read exactly <length> chars into textOut
 * 4) Consume newline if present
 *
 * @param in Input stream
 * @param textOut Output record text (CSV-like)
 * @param width how many digits to read
 * @return true if record was read, false if EOF or format error
 * @bug does not yet support length indicators in a non-ASCII format
 */
static bool readLenLine(istream& in, string& textOut, int width) {
    textOut.clear();

    char lenBuf[width];
    if (!in.read(lenBuf, width))
        return false; // EOF or error

    for (int i = 0; i < header.sizeOfSizes; i++) {
        if (!isdigit(static_cast<unsigned char>(lenBuf[i])))
            return false;
    }

    char space;
    if (!in.get(space) || space != ' ')
        return false;

    int len = stoi(string(lenBuf, width));
    if (len < 0)
        return false;

    textOut.resize(len);
    if (len > 0 && !in.read(&textOut[0], len))
        return false;

    // Consume newline if present
    if (in.peek() == '\r')
        in.get();
    if (in.peek() == '\n') 
        in.get();

    return true;
}

/**
 * @brief Split a CSV line into fields (basic quote support).
 * @param line CSV line
 * @return vector of fields
 */
static vector<string> splitCsvSimple(const string& line) {
    vector<string> fields;
    string cur;
    bool inQuotes = false;

    for (char c : line) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    fields.push_back(cur);
    return fields;
}

/**
 * @brief Print one record with labels on ONE line (Part II requirement).
 * @param csvLine The record data (CSV text inside LEN)
 * @bug this method does not yet support reordered columns.
 */
static void printLabeledOneLine(const string& csvLine) {
    vector<string> f = splitCsvSimple(csvLine);

    for (auto& field : f) {
        if (!field.empty() && field.back() == '\r')
            field.pop_back();
    }

    // Remove extra empty field caused by a trailing comma
    if (!f.empty() && f.back().empty()) {
        f.pop_back();
    }

    if (f.size() != 6) {
        cout << "Record=" << csvLine << "\n";
        return;
    }

    cout << "ZIP=" << f[0]
         << " | Place=" << f[1]
         << " | State=" << f[2]
         << " | County=" << f[3]
         << " | Lat=" << f[4]
         << " | Long=" << f[5]
         << "\n";
}

/**
 * @brief Analyze state extremes from a CSV file without loading all records.
 * @param csvFile Input CSV filename
 * @return exit status code (0, 2, or 3)
 * @see main()
 */
static int analyzeCsvStreaming(const string& csvFile) {
    ZipCodeBuffer buffer;
    if (!buffer.open(csvFile)) {
        cerr << "Error: Could not open CSV file '" << csvFile << "'\n";
        return 2;
    }

    map<string, StateExtremes> stateMap;

    ZipCodeRecord rec;
    long count = 0;

    while (buffer.readRecord(rec)) {
        updateStateExtremes(stateMap, rec);
        count++;
    }

    buffer.close();

    if (count == 0) {
        cerr << "Error: No valid records found.\n";
        return 3;
    }

    cout << "Reading ZIP code data from: " << csvFile << "\n";
    cout << "Total records read: " << count << "\n\n";
    cout << "Analysis Results:\n=================\n\n";
    printStateExtremesTable(stateMap);

    return 0;
}

/**
 * @brief Convert CSV → LEN file.
 *
 * We skip the CSV header row, then write:
 * 1) a LEN header record (simple placeholder text)
 * 2) each CSV record as a length-indicated record
 *
 * @param csvFile Input CSV
 * @param lenFile Output LEN
 * @return exit status code (2-4 or 0)
 * @see main()
 */
static int makeLenFromCsv(const string& csvFile, const string& lenFile) {
    ifstream in(csvFile);
    if (!in) {
        cerr << "Error: Cannot open CSV file '" << csvFile << "'\n";
        return 2;
    }

    ofstream out(lenFile);
    if (!out) {
        cerr << "Error: Cannot create LEN file '" << lenFile << "'\n";
        return 2;
    }

    // Skip CSV header
    string header;
    if (!getline(in, header)) {
        cerr << "Error: CSV file is empty.\n";
        return 3;
    }

    // Write full header record using HeaderBuffer class
    HeaderBuffer hbuf;
    hbuf.buildDefault(lenFile + ".idx", 0);
    if (!hbuf.write(out)) {
        cerr << "Error: Failed to write LEN header.\n";
        return 4;
    }

    long recCount = 0;
    string line;
    while (getline(in, line)) {
        if (line.empty()) continue;
        if (!writeLenLine(out, line)) {
            cerr << "Warning: skipped a line that could not be written.\n";
            continue;
        }
        recCount++;
    }

    cout << "Created LEN file: " << lenFile << "\n";
    cout << "Records written: " << recCount << "\n";
    return 0;
}

/**
 * @brief Build a primary key index (ZIP → byte offset) from a LEN file.
 *
 * We store the file position BEFORE reading each record.
 * That position is the “start of record” (the 10-digit length field).
 *
 * Index file format (simple):
 *   IDX,1
 *   56301 128
 *   02139 412
 *
 * @param lenFile Input LEN data file
 * @param idxFile Output index file
 * @return exit status code (2-4 or 0)
 * @see main()
 */
static int buildIndexFromLen(const string& lenFile, const string& idxFile) {
    ifstream in(lenFile);
    if (!in) {
        cerr << "Error: Cannot open LEN file '" << lenFile << "'\n";
        return 2;
    }

    ofstream out(idxFile);
    if (!out) {
        cerr << "Error: Cannot create index file '" << idxFile << "'\n";
        return 2;
    }

    string header;
    if (!readLenLine(in, header)) {
        cerr << "Error: LEN file is missing header or is corrupted.\n";
        return 3;
    }

    out << "IDX,1\n";

    long entries = 0;
    while (true) {
        streampos pos = in.tellg();

        string record;
        if (!readLenLine(in, record)) break;

        // ZIP is first field before comma
        size_t comma = record.find(',');
        if (comma == string::npos) continue;

        string zip = record.substr(0, comma); // keep leading zeros if any
        out << zip << " " << static_cast<long>(pos) << "\n";
        entries++;
    }

    cout << "Created index file: " << idxFile << "\n";
    cout << "Index entries: " << entries << "\n";
    return 0;
}

/**
 * @brief Search all -Z flags provided and print results.
 * @param lenFile Data file (.len)
 * @param idxFile Index file (.idx)
 * @param zips List of ZIP strings to search
 * @return exit status code (0, 2 or 3)
 * @see main()
 */
static int searchZips(const string& lenFile, const string& idxFile,
                      const vector<string>& zips) {
    ifstream data(lenFile);
    if (!data) {
        cerr << "Error: Cannot open LEN data file: " << lenFile << "\n";
        return 2;
    }

    
    ifstream in(idxFile);
    if (!in) {
        cerr << "Error: Cannot open IDX data file: " << idxFile << "\n";
        return 2;
    }

    unordered_map<string, streampos> idx;

    string firstLine;
    getline(in, firstLine); // IDX,1

    string zip;
    long pos;
    while (in >> zip >> pos) {
        idx[zip] = static_cast<streampos>(pos);
    }

    if (idx.empty()) {
        cerr << "Error: Index file is empty: " << idxFile << "\n";
        return 3;
    }

    // Read and keep header in RAM (allowed)
    string header;
    if (!readLenLine(data, header)) {
        cerr << "Error: LEN data file header missing or corrupted.\n";
        return 3;
    }

    cout << "Using data file: " << lenFile << "\n";
    cout << "Using index file: " << idxFile << "\n";
    cout << "Header: " << header << "\n\n";

    for (const string& zip : zips) {
        auto it = idx.find(zip);
        if (it == idx.end()) {
            cout << "ZIP " << zip << " not found in file\n";
            continue;
        }

        data.clear();
        data.seekg(it->second);

        string recordLine;
        if (!readLenLine(data, recordLine)) {
            cout << "ZIP " << zip << " found in index but record could not be "
                 << "read (stale index)\n"; //continued from line above
            continue;
        }

        printLabeledOneLine(recordLine);
    }

    return 0;
}

/**
 * @brief Print usage instructions (simple).
 * @param prog program name
 */
static void printUsage(const string& prog) {
    cerr << "USAGE:\n";
    cerr << "  1) Analyze CSV (Project 1 style):\n";
    cerr << "     " << prog << " <file.csv>\n\n";
    cerr << "  2) Make LEN from CSV:\n";
    cerr << "     " << prog << " --make-len <in.csv> <out.len>\n\n";
    cerr << "  3) Build index from LEN:\n";
    cerr << "     " << prog << " --build-index <data.len> <out.idx>\n\n";
    cerr << "  4) Search ZIPs using LEN + IDX:\n";
    cerr << "     " << prog //continued on line below
         << " --search <data.len> <data.idx> -Z<zip code> [-Z<zip code> ...]\n";
}

/**
 * @brief the program's main method
 * @since Project 2
 * 
 * EXIT STATUS CODE INTERPETATION:
 * - 0 = successful exit
 * - 1 = malformed command line arguments
 * - 2 = nonexistent or innaccesible file
 * - 3 = failed to read header data
 * - 4 = failed to write header data
 *
 * @return an exit status code from 0 to 4
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    string cmd = argv[1];

    // MODE 2: --make-len in.csv out.len
    if (cmd == "--make-len") {
        if (argc != 4) {
            printUsage(argv[0]);
            return 1;
        }
        return makeLenFromCsv(argv[2], argv[3]);
    }

    // MODE 3: --build-index data.len out.idx
    if (cmd == "--build-index") {
        if (argc != 4) {
            printUsage(argv[0]);
            return 1;
        }
        return buildIndexFromLen(argv[2], argv[3]);
    }

    // MODE 4: --search data.len data.idx -Zxxxxx ...
    if (cmd == "--search") {
        if (argc < 5) {
            printUsage(argv[0]);
            return 1;
        }

        string lenFile = argv[2];
        string idxFile = argv[3];

        vector<string> zips;
        for (int i = 4; i < argc; i++) {
            string arg = argv[i];
            if (arg.rfind("-Z", 0) == 0 && arg.size() > 2) {
                zips.push_back(arg.substr(2)); // everything after -Z
            }
        }

        if (zips.empty()) {
            cerr << "Error: No -Z flags provided.\n";
            printUsage(argv[0]);
            return 1;
        }

        return searchZips(lenFile, idxFile, zips);
    }

    // MODE 1 (default): treat argv[1] as CSV file and analyze state extremes
    // Example: ./zipprog us_postal_codes.csv
    return analyzeCsvStreaming(argv[1]);
}