/**
 * @file main.cpp
 * @brief Zip Code Group Project 3.0 — Blocked Sequence Set main controller.
 *
 * This is a multi-mode program. Each mode is selected by the first command line
 * argument.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * MODES
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * 1) Create a blocked sequence set from a sorted CSV:
 *      ./zip3 --create <sorted.csv> <out.bss> <out.sidx> [blockSize]
 *
 * 2) Dump (physical then logical) an existing blocked sequence set:
 *      ./zip3 --dump <data.bss> <data.sidx> [blockSize]
 *
 * 3) Dump just the simple index:
 *      ./zip3 --dump-index <data.bss> <data.sidx> [blockSize]
 *
 * 4) Search for ZIP codes (loads index into RAM, never loads whole data file):
 *      ./zip3 --search <data.bss> <data.sidx> -Z<zip> ... [blockSize]
 *
 * 5) Project-1-style sequential analysis (state extremes):
 *      ./zip3 --analyze <data.bss> <data.sidx> [blockSize]
 *
 * 6) Add records from a file of CSV lines:
 *      ./zip3 --add <data.bss> <data.sidx> <records_to_add.csv> [blockSize]
 *
 * 7) Delete records whose keys are listed in a file (one ZIP per line):
 *      ./zip3 --delete <data.bss> <data.sidx> <keys_to_delete.txt> [blockSize]
 *
 * 8) Print the file header:
 *      ./zip3 --header <data.bss> <data.sidx> [blockSize]
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * NOTES
 * ─────────────────────────────────────────────────────────────────────────────
 *  - The blocked sequence set file (.bss) stores length-indicated,
 *    comma-separated records in fixed-size blocks.
 *  - The simple index file (.sidx) stores {highestKey, RBN} pairs and is
 *    loaded entirely into RAM during search/add/delete.
 *  - Block splits, merges, and redistributions are logged to stdout.
 *  - All program variables that can vary are set by command line or metadata.
 *  - The preceding "--" is optional for all modes arguments.
 *
 * @since Project 3.0
 * @author Teagen Lee (primary contributor)
 * @author Ethan Jackson (formatting, documentation, and functional revisions)
 * @date April 2026
 */

#include "SequenceSet.h"
#include "ZipCodeBuffer.h"
#include "SimpleIndex.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <limits>
#include <algorithm>

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// Project 1 state-extremes analysis (re-used from Project 2)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @struct StateExtremes
 * @brief Keeps the most extreme ZIP codes for one state.
 * @note The corresponding state is not contained in this struct. StateExtremes 
 * is designed for use as the value type in a map that uses state codes as keys.
 */
struct StateExtremes {
    int easternmost,  ///< Zip code of the easternmost location in the state. 
        westernmost,  ///< Zip code of the westernmost location in the state.
        northernmost, ///< Zip code of the northernmost location in the state. 
        southernmost; ///< Zip code of the southernmost location in the state.
    double minLong,   ///< Longitude of the easternmost location.
           maxLong,   ///< Longitude of the westernmost location.
           maxLat,    ///< Latitude of the northernmost location.
           minLat;    ///< Latitude of the southernmost location.

    StateExtremes()
        : easternmost(0), westernmost(0), northernmost(0), southernmost(0),
          minLong(numeric_limits<double>::max()),
          maxLong(numeric_limits<double>::lowest()),
          maxLat(numeric_limits<double>::lowest()),
          minLat(numeric_limits<double>::max()) {}
};

/**
 * @brief Tie-breaker logic for latitude and longitude comparisons.
 * 
 * This method was added to make calculation of extreme longitudes and latitudes
 * not depend on the input file's sort order. When a candidate zip code record
 * has a longitude or latitude exactly equal to one of the current extremes, the
 * smaller zip code is chosen to be stored as the extreme. The one exception to
 * this rule is when the extreme zip is 0. This is a default value and means no
 * zip code records besides the candidate have been read yet, so the candidate's
 * zip is always chosen.
 *
 * @see updateExtremes(map<string, StateExtremes>&, const ZipCodeRecord)
 *
 * @param candidate the new (candidate) zip code
 * @param extreme zip code of the current extreme
 * @return true if the candidate zip is to replace the extreme zip.
 */
static bool smallerZipWins(int candidate, int extreme) {
    return ((candidate < extreme) || (extreme == 0));
}

/**
 * @brief Updates state extremes with one ZipCodeRecord.
 */
static void updateExtremes(map<string, StateExtremes>& stateMap, //...
                           const ZipCodeRecord& r) //continued from line above
{
    StateExtremes& ex = stateMap[r.state];

    if (r.longitude < ex.minLong || (r.longitude == ex.minLong && //...
            smallerZipWins(r.zipCode, ex.easternmost))) { //cont. from above
        ex.minLong = r.longitude;
        ex.easternmost = r.zipCode;
    }
    if (r.longitude > ex.maxLong || (r.longitude == ex.maxLong && //...
            smallerZipWins(r.zipCode, ex.westernmost))) { //cont. from above
        ex.maxLong = r.longitude;
        ex.westernmost = r.zipCode;
    }
    if (r.latitude > ex.maxLat || (r.latitude == ex.maxLat && //...
            smallerZipWins(r.zipCode, ex.northernmost))) { //cont. from above
        ex.maxLat = r.latitude; 
        ex.northernmost = r.zipCode;
    }
    if (r.latitude < ex.minLat || (r.latitude == ex.minLat && //...
            smallerZipWins(r.zipCode, ex.southernmost))) { //cont. from above
        ex.minLat = r.latitude; 
        ex.southernmost = r.zipCode;
    }
}

/**
 * @brief Print the state extremes table.
 */
static void printExtremes(const map<string, StateExtremes>& stateMap) {
    cout << left << setw(8) << "State"
         << setw(15) << "Easternmost"
         << setw(15) << "Westernmost"
         << setw(15) << "Northernmost"
         << setw(15) << "Southernmost" << "\n";
    cout << string(68, '-') << "\n";

    for (const auto& entry : stateMap) {
        const StateExtremes& ex = entry.second;
        cout << setw(8) << entry.first;
        auto printZip = [](int z) {
            cout << setfill('0') << setw(5) << z;
            cout << setfill(' ') << setw(10) << " ";
        };
        printZip(ex.easternmost);
        printZip(ex.westernmost);
        printZip(ex.northernmost);
        cout << setfill('0') << setw(5) << ex.southernmost << "\n";
    }
    cout << "\nTotal states/territories: " << stateMap.size() << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Usage
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Print usage information.
 * @param prog Program name (argv[0]).
 */
static void printUsage(const string& prog) {
    const string dataArgs = " <data.bss> <data.sidx>",
                 lastArg  = " [blockSize]\n  "; //"\r" removes trailing spaces
    cerr << "USAGE:\n  "
         << prog << " --create <sorted.csv> <out.bss> <out.sidx>" << lastArg
         << prog << " --dump" << dataArgs << lastArg
         << prog << " --dump-index" << dataArgs << lastArg
         << prog << " --search" << dataArgs << " -Z<zip> ..." << lastArg
         << prog << " --analyze" << dataArgs << lastArg
         << prog << " --add" << dataArgs << " <add.csv>" << lastArg
         << prog << " --delete << dataArgs << " <keys.txt>" << lastArg
         << prog << " --header" << dataArgs << lastArg << "\r";
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: parse optional block size from argv
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Try to read an integer block size from a command-line argument.
 *
 * @param arg Command-line string.
 * @param result Output integer if parsing succeeds.
 * @return true if arg is a valid positive integer.
 */
static bool parseBlockSize(const string& arg, int& result) {
    try {
        int v = stoi(arg);
        if (v > 0) {
            result = v;
            return true;
        } //else
        cerr << "Error: blockSize must be a positive integer. ";
    } catch (invalid_argument) {
        cerr << "Error: blockSize could not be parsed as an int. ";
    } catch (out_of_range) {
        cerr << "Error: the given blockSize is too large. ";
    }
    cerr << "Trying the default size (" << DEFAULT_BLOCK SIZE << ") instead.\n";
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Mode implementations
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief --create: build a blocked sequence set from a sorted CSV.
 */
static int modeCreate(int argc, char* argv[]) {
    if (argc < 5) {
        cerr << "Error: --create needs <csv> <bss> <sidx>\n";
        return 1;
    }

    string csvFile  = argv[2];
    string bssFile  = argv[3];
    string sidxFile = argv[4];
    int blockSize   = DEFAULT_BLOCK_SIZE;
    
    int blockSize = DEFAULT_BLOCK_SIZE;
    if (argc >= 6) {
        if (argc > 6)
            cerr << "Warning: more arguments given than accepted by " //...
                 << argv[1] << ". Extra arguments will be ignored.\n";
        parseBlockSize(argv[5], blockSize);
    }

    SequenceSet ss(blockSize);
    if (!ss.create(csvFile, bssFile, sidxFile))
        return 2;
    return 0;
}

/**
 * @brief --dump: physical then logical dump.
 */
static int modeDump(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Error: --dump needs <bss> <sidx>\n";
        return 1;
    }

    int blockSize = DEFAULT_BLOCK_SIZE;
    if (argc >= 5) {
        if (argc > 5)
            cerr << "Warning: more arguments given than accepted by " //...
                 << argv[1] << ". Extra arguments will be ignored.\n";
        parseBlockSize(argv[4], blockSize);
    }

    SequenceSet ss(blockSize);
    if (!ss.open(argv[2], argv[3])
        return 2;
    ss.dumpPhysical();
    cout << "\n";
    ss.dumpLogical();
    ss.close();
    return 0;
}

/**
 * @brief --dump-index: print the simple index.
 */
static int modeDumpIndex(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Error: --dump-index needs <bss> <sidx>\n";
        return 1;
    }

    int blockSize = DEFAULT_BLOCK_SIZE;
    if (argc >= 5) {
        if (argc > 5)
            cerr << "Warning: more arguments given than accepted by " //...
                 << argv[1] << ". Extra arguments will be ignored.\n";
        parseBlockSize(argv[4], blockSize);
    }

    SequenceSet ss(blockSize);
    if (!ss.open(argv[2], argv[3]))
        return 2;
    ss.dumpIndex();
    ss.close();
    return 0;
}

/**
 * @brief --search: look up one or more ZIPs using the index.
 *
 * The index is loaded into RAM; the data file is never loaded into RAM.
 * Each requested ZIP requires at most one block read.
 */
static int modeSearch(int argc, char* argv[]) {
    if (argc < 5) {
        cerr << "Error: --search needs <bss> <sidx> -Z<zip>\n";
        return 1;
    }
    string bssFile  = argv[2];
    string sidxFile = argv[3];

    int blockSize = DEFAULT_BLOCK_SIZE;
    vector<string> zips;

    for (int i = 4; i < argc; i++) {
        string arg = argv[i];
        if (arg.rfind("-Z", 0) == 0 && arg.size() > 2) {
            string raw = arg.substr(2);
            // Zero-pad to 5 digits so it matches the stored key format
            if (raw.size() < 5)
                raw = string(5 - raw.size(), '0') + raw;
            zips.push_back(raw);
        } else {
            parseBlockSize(arg, blockSize);
        }
    }

    if (zips.empty()) {
        cerr << "Error: no -Z flags provided.\n";
        return 1;
    }

    SequenceSet ss(blockSize);
    if (!ss.open(bssFile, sidxFile))
        return 2;

    cout << "Using data file : " << bssFile  << "\n";
    cout << "Using index file: " << sidxFile << "\n\n";

    for (const string& zip : zips) {
        ZipCodeRecord result;
        if (ss.search(zip, result)) {
            cout << "ZIP=" << setfill('0') << setw(5) << result.zipCode
                 << " | Place=" << result.placeName
                 << " | State=" << result.state
                 << " | County=" << result.county
                 << " | Lat=" << result.latitude
                 << " | Long=" << result.longitude << "\n";
        } else {
            cout << "ZIP " << zip << " not found in file.\n";
        }
    }

    ss.close();
    return 0;
}

/**
 * @brief --analyze: Project 1 state-extremes analysis via sequential scan.
 */
static int modeAnalyze(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Error: --analyze needs <bss> <sidx>\n";
        return 1;
    }

    int blockSize = DEFAULT_BLOCK_SIZE;
    if (argc >= 5) {
        if (argc > 5)
            cerr << "Warning: more arguments given than accepted by " //...
                 << argv[1] << ". Extra arguments will be ignored.\n";
        parseBlockSize(argv[4], blockSize);
    }

    SequenceSet ss(blockSize);
    if (!ss.open(argv[2], argv[3]))
        return 2;

    map<string, StateExtremes> stateMap;
    long long count = 0;

    ss.scanAll([&](const ZipCodeRecord& r) {
        updateExtremes(stateMap, r);
        count++;
    });

    ss.close();

    cout << "Records scanned: " << count << "\n\n";
    printExtremes(stateMap);
    return 0;
}

/**
 * @brief --add: insert records from a CSV file.
 *
 * File format: one CSV record per line (no header row), same field order as the
 * original ZIP CSV (ZipCode,PlaceName,State,County,Lat,Long).
 *
 * Logs block splits and index changes.
 */
static int modeAdd(int argc, char* argv[]) {
    if (argc < 5) {
        cerr << "Error: --add needs <bss> <sidx> <add.csv>\n";
        return 1;
    }

    int blockSize = DEFAULT_BLOCK_SIZE;
    if (argc >= 6) {
        if (argc > 6)
            cerr << "Warning: more arguments given than accepted by " //...
                 << argv[1] << ". Extra arguments will be ignored.\n";
        parseBlockSize(argv[5], blockSize);
    }

    SequenceSet ss(blockSize);
    if (!ss.open(argv[2], argv[3]))
        return 2;

    ifstream addFile(argv[4]);
    if (!addFile) { 
        cerr << "Error: cannot open add-file '" << argv[4] << "'\n";
        return 3;
    }

    int added = 0, failed = 0;
    string line;
    while (getline(addFile, line)) {
        if (line.empty() || line[0] == '#') // Skip blank lines and comments
            continue;
        if (!line.empty() && line.back() == '\r') // Strip trailing \r
            line.pop_back();

        ZipCodeRecord rec;
        if (!rec.fromCSV(line)) {
            cerr << "Warning: could not parse line: " << line << "\n";
            failed++;
            continue;
        }

        if (ss.insert(rec)) {
            cout << "Added ZIP " << rec.zipCode << "\n";
            added++;
        } else {
            cout << "Skipped ZIP " << rec.zipCode << " (duplicate or error)\n";
            failed++;
        }
    }

    ss.close();
    cout << "\nAdded: " << added << "  Skipped/failed: " << failed << "\n";
    return 0;
}

/**
 * @brief --delete: delete records whose keys are listed in a file.
 *
 * File format: one ZIP code (string) per line.
 *
 * Logs merges, redistributions, and index changes.
 */
static int modeDelete(int argc, char* argv[]) {
    if (argc < 5) {
        cerr << "Error: --delete needs <bss> <sidx> <keys.txt>\n";
        return 1;
    }

    int blockSize = DEFAULT_BLOCK_SIZE;
    if (argc >= 6) {
        if (argc > 6)
            cerr << "Warning: more arguments given than accepted by " //...
                 << argv[1] << ". Extra arguments will be ignored.\n";
        parseBlockSize(argv[5], blockSize);
    }

    SequenceSet ss(blockSize);
    if (!ss.open(argv[2], argv[3]))
        return 2;

    ifstream delFile(argv[4]);
    if (!delFile) {
        cerr << "Error: cannot open delete-file '" << argv[4] << "'\n";
        return 3;
    }

    int deleted = 0, notFound = 0;
    string key;
    while (getline(delFile, key)) {
        if (key.empty() || key[0] == '#') // Skip blank lines and comments
            continue;
        if (!key.empty() && key.back() == '\r') // Strip trailing \r
            key.pop_back();

        // Zero-pad to 5 digits to match stored key format
        if (key.size() < 5)
            key = string(5 - key.size(), '0') + key;

        if (ss.remove(key)) {
            cout << "Deleted ZIP " << key << "\n";
            deleted++;
        } else {
            cout << "ZIP " << key << " not found — skipped\n";
            notFound++;
        }
    }

    ss.close();
    cout << "\nDeleted: " << deleted << "  Not found: " << notFound << "\n";
    return 0;
}

/**
 * @brief --header: print the file header.
 */
static int modeHeader(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Error: --header needs <bss> <sidx>\n";
        return 1;
    }

    int blockSize = DEFAULT_BLOCK_SIZE;
    if (argc >= 5) {
        if (argc > 5)
            cerr << "Warning: more arguments given than accepted by " //...
                 << argv[1] << ". Extra arguments will be ignored.\n";
        parseBlockSize(argv[4], blockSize);
    }

    SequenceSet ss(blockSize);
    if (!ss.open(argv[2], argv[3]))
        return 2;
    ss.printHeader();
    ss.close();
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// main method
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Program entry point. Dispatches to the appropriate mode.
 * @see modeCreate, modeDump, modeIndex, modeDumpIndex, modeSearch, modeAnalyze,
 * modeAdd, modeDelete, & modeHeader
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    switch (argv[1]) {
    case "--create":
    case "create":
        return modeCreate(argc, argv);
    case "--dump":
    case "dump":
        return modeDump(argc, argv);
    case "--dump-index":
    case "dump-index":
        return modeDumpIndex(argc, argv);
    case "--search":
    case "search":
        return modeSearch(argc, argv);
    case "--analyze":
    case "analyze":
        return modeAnalyze(argc, argv);
    case "--add":
    case "add":
        return modeAdd(argc, argv);
    case "--delete":
    case "delete":
        return modeDelete(argc, argv);
    case "--header":
    case "header":
        return modeHeader(argc, argv);
    default:
        cerr << "Unknown command: " << argv[1] << "\n";
        printUsage(argv[0]);
        return 1;
    }
}