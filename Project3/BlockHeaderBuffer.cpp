/**
 * @file BlockHeaderBuffer.cpp
 * @brief Implementation of BlockHeaderBuffer.
 *
 * @author Teagen Lee
 * @date Spring 2026
 */

#include "BlockHeaderBuffer.h"
#include "BlockBuffer.h"   // for RBN_NULL, RECORD_LEN_WIDTH

#include <sstream>
#include <iomanip>
#include <iostream>
#include <cctype>
#include <stdexcept>

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Construct a BlockHeaderBuffer.
 * @param blockSize Block size in bytes (must match the data file).
 */
BlockHeaderBuffer::BlockHeaderBuffer(int blockSize)
    : blockSize_(blockSize)
{
    // Initialise to safe defaults
    hdr_.fileType            = "";
    hdr_.version             = 0;
    hdr_.headerSizeBytes     = 0;
    hdr_.recordSizeBytes     = RECORD_LEN_WIDTH;
    hdr_.sizeFormatType      = "ASCII";
    hdr_.blockSize           = blockSize;
    hdr_.minBlockCapacityPct = 50;
    hdr_.indexFileName       = "";
    hdr_.indexSchema         = "key:string;rbn:int";  // semicolons avoid CSV parse collision
    hdr_.blockCount          = 0;
    hdr_.fieldCount          = 0;
    hdr_.primaryKeyIndex     = 0;
    hdr_.availHeadRBN        = RBN_NULL;
    hdr_.seqSetHeadRBN       = RBN_NULL;
    hdr_.staleFlag           = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Populate the header with default values for the ZIP code blocked file.
 */
void BlockHeaderBuffer::buildDefault(const string& indexFileName,
                                     int    blockSize,
                                     long long recordCount,
                                     int    blockCount,
                                     int    availHead,
                                     int    seqHead)
{
    hdr_.fileType            = "ZipBlockedSeqSet";
    hdr_.version             = 1;
    hdr_.recordSizeBytes     = RECORD_LEN_WIDTH; // 4 bytes per record length prefix
    hdr_.sizeFormatType      = "ASCII";
    hdr_.blockSize           = blockSize;
    hdr_.minBlockCapacityPct = 50;
    hdr_.indexFileName       = indexFileName;
    hdr_.indexSchema         = "key:string;rbn:int";  // semicolons avoid CSV parse collision
    hdr_.recordCount         = recordCount;
    hdr_.blockCount          = blockCount;
    hdr_.primaryKeyIndex     = 0;
    hdr_.availHeadRBN        = availHead;
    hdr_.seqSetHeadRBN       = seqHead;
    hdr_.staleFlag           = false;

    // Set fields BEFORE calling serialize() so headerSizeBytes is accurate
    hdr_.fields = {
        {"ZipCode",   "int"},
        {"PlaceName", "string"},
        {"State",     "string"},
        {"County",    "string"},
        {"Latitude",  "double"},
        {"Longitude", "double"}
    };
    hdr_.fieldCount = static_cast<int>(hdr_.fields.size());

    // Now measure the serialized size (fields are already set)
    hdr_.headerSizeBytes = static_cast<int>(serialize().size());
}

// ─────────────────────────────────────────────────────────────────────────────
// I/O
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Write the header record to RBN 0 of the open fstream.
 *
 * Format written at byte offset 0:
 *   [10-byte ASCII length][space][CSV text][spaces to blockSize_]
 *
 * @param fs Open fstream (read+write, seekable).
 * @return true on success.
 */
bool BlockHeaderBuffer::write(fstream& fs) const {
    if (!fs) return false;

    // Seek to the beginning of the file (RBN 0)
    fs.seekp(0);

    string csv = serialize();
    int    len = static_cast<int>(csv.size());

    // Build a block-sized buffer filled with spaces
    string block(blockSize_, ' ');

    // Write: [10-byte length][space][csv text]
    ostringstream lenSS;
    lenSS << setw(10) << setfill('0') << len;
    string lenStr = lenSS.str();

    int pos = 0;
    block.replace(pos, 10, lenStr);  pos += 10;
    block[pos] = ' ';                pos += 1;
    block.replace(pos, csv.size(), csv);

    fs.write(block.c_str(), blockSize_);
    return static_cast<bool>(fs);
}

/**
 * @brief Read the header record from RBN 0 of the open fstream.
 *
 * @param fs Open fstream.
 * @return true on success.
 */
bool BlockHeaderBuffer::read(fstream& fs) {
    if (!fs) return false;

    // Seek to start of file
    fs.seekg(0);

    string block(blockSize_, '\0');
    fs.read(&block[0], blockSize_);
    if (!fs && !fs.eof()) return false;

    // Parse the length prefix
    string lenStr = block.substr(0, 10);
    for (char c : lenStr) {
        if (!isdigit(static_cast<unsigned char>(c))) return false;
    }
    int len = stoi(lenStr);

    // Extract the CSV text
    // block[10] == ' '  (space separator)
    if (static_cast<int>(block.size()) < 11 + len) return false;
    string csv = block.substr(11, len);

    return deserialize(csv);
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors / Mutators
// ─────────────────────────────────────────────────────────────────────────────

const BlockFileHeader& BlockHeaderBuffer::getHeader() const { return hdr_; }
BlockFileHeader&       BlockHeaderBuffer::getHeader()       { return hdr_; }

void BlockHeaderBuffer::setRecordCount(long long count) { hdr_.recordCount  = count; }
void BlockHeaderBuffer::setBlockCount(int count)        { hdr_.blockCount   = count; }
void BlockHeaderBuffer::setAvailHead(int rbn)           { hdr_.availHeadRBN = rbn;   }
void BlockHeaderBuffer::setSeqSetHead(int rbn)          { hdr_.seqSetHeadRBN = rbn;  }
void BlockHeaderBuffer::setStale(bool s)                { hdr_.staleFlag    = s;      }

// ─────────────────────────────────────────────────────────────────────────────
// Display
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Print the header contents to cout in a readable format.
 */
void BlockHeaderBuffer::print() const {
    cout << "=== Blocked Sequence Set Header ===\n";
    cout << "  File type           : " << hdr_.fileType          << "\n";
    cout << "  Version             : " << hdr_.version           << "\n";
    cout << "  Header size         : " << hdr_.headerSizeBytes   << " bytes\n";
    cout << "  Record size width   : " << hdr_.recordSizeBytes   << " bytes\n";
    cout << "  Size format         : " << hdr_.sizeFormatType    << "\n";
    cout << "  Block size          : " << hdr_.blockSize         << " bytes\n";
    cout << "  Min block capacity  : " << hdr_.minBlockCapacityPct << "%\n";
    cout << "  Index file          : " << hdr_.indexFileName     << "\n";
    cout << "  Index schema        : " << hdr_.indexSchema       << "\n";
    cout << "  Record count        : " << hdr_.recordCount       << "\n";
    cout << "  Block count         : " << hdr_.blockCount        << "\n";
    cout << "  Field count         : " << hdr_.fieldCount        << "\n";
    cout << "  Primary key field   : " << hdr_.primaryKeyIndex   << "\n";
    cout << "  Avail head RBN      : ";
    if (hdr_.availHeadRBN == RBN_NULL) cout << "(none)\n"; else cout << hdr_.availHeadRBN << "\n";
    cout << "  Seq-set head RBN    : ";
    if (hdr_.seqSetHeadRBN == RBN_NULL) cout << "(none)\n"; else cout << hdr_.seqSetHeadRBN << "\n";
    cout << "  Stale flag          : " << (hdr_.staleFlag ? "yes" : "no") << "\n";
    for (int i = 0; i < static_cast<int>(hdr_.fields.size()); i++) {
        cout << "  Field[" << i << "]            : "
             << hdr_.fields[i].name
             << " (" << hdr_.fields[i].format << ")\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: serialise / deserialise
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Serialise the header to a comma-separated string.
 *
 * Format:
 *   BHDR,fileType,version,headerSize,recSizeBytes,sizeFormat,blockSize,
 *   minCapPct,indexFile,indexSchema,recCount,blockCount,fieldCount,
 *   primaryKey,availHead,seqHead,stale,
 *   field0name:field0format,...
 *
 * @return CSV string (no newline).
 */
string BlockHeaderBuffer::serialize() const {
    ostringstream ss;
    ss << "BHDR"
       << "," << hdr_.fileType
       << "," << hdr_.version
       << "," << hdr_.headerSizeBytes
       << "," << hdr_.recordSizeBytes
       << "," << hdr_.sizeFormatType
       << "," << hdr_.blockSize
       << "," << hdr_.minBlockCapacityPct
       << "," << hdr_.indexFileName
       << "," << hdr_.indexSchema
       << "," << hdr_.recordCount
       << "," << hdr_.blockCount
       << "," << hdr_.fieldCount
       << "," << hdr_.primaryKeyIndex
       << "," << hdr_.availHeadRBN
       << "," << hdr_.seqSetHeadRBN
       << "," << (hdr_.staleFlag ? 1 : 0);

    for (const auto& f : hdr_.fields) {
        ss << "," << f.name << ":" << f.format;
    }
    return ss.str();
}

/**
 * @brief Deserialise a CSV string into hdr_.
 *
 * @param s CSV string produced by serialize().
 * @return true on success, false if format is unrecognised.
 */
bool BlockHeaderBuffer::deserialize(const string& s) {
    // Split on commas, respecting the fixed field ordering
    istringstream ss(s);
    string tok;
    vector<string> parts;
    while (getline(ss, tok, ',')) {
        parts.push_back(tok);
    }

    // Fixed fields: BHDR + 15 values + stale = 17 minimum (field descriptors optional)
    if (parts.size() < 17) return false;
    if (parts[0] != "BHDR") return false;

    try {
        hdr_.fileType            = parts[1];
        hdr_.version             = stoi(parts[2]);
        hdr_.headerSizeBytes     = stoi(parts[3]);
        hdr_.recordSizeBytes     = stoi(parts[4]);
        hdr_.sizeFormatType      = parts[5];
        hdr_.blockSize           = stoi(parts[6]);
        hdr_.minBlockCapacityPct = stoi(parts[7]);
        hdr_.indexFileName       = parts[8];
        hdr_.indexSchema         = parts[9];
        hdr_.recordCount         = stoll(parts[10]);
        hdr_.blockCount          = stoi(parts[11]);
        hdr_.fieldCount          = stoi(parts[12]);
        hdr_.primaryKeyIndex     = stoi(parts[13]);
        hdr_.availHeadRBN        = stoi(parts[14]);
        hdr_.seqSetHeadRBN       = stoi(parts[15]);
        hdr_.staleFlag           = (parts[16] == "1");
    } catch (...) {
        return false;
    }

    // Field descriptors start at index 17 (optional but expected)
    hdr_.fields.clear();
    for (int i = 17; i < static_cast<int>(parts.size()); i++) {
        auto colon = parts[i].find(':');
        if (colon == string::npos) continue;
        BFieldDescriptor fd;
        fd.name   = parts[i].substr(0, colon);
        fd.format = parts[i].substr(colon + 1);
        hdr_.fields.push_back(fd);
    }
    return true;
}
