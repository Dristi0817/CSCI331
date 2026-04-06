/**
 * @file BlockBuffer.cpp
 * @brief Implementation of the BlockBuffer class.
 *
 * See BlockBuffer.h for the block layout specification.
 *
 * @author Teagen Lee
 * @date Spring 2026
 */

#include "BlockBuffer.h"

#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <iostream>

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Construct a BlockBuffer with the given block size.
 *
 * All links default to RBN_NULL; the record list is empty.
 *
 * @param blockSize Bytes per block.
 */
BlockBuffer::BlockBuffer(int blockSize)
    : blockSize_(blockSize),
      predRBN_(RBN_NULL),
      succRBN_(RBN_NULL)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Metadata accessors
// ─────────────────────────────────────────────────────────────────────────────

int BlockBuffer::getBlockSize()   const { return blockSize_; }
int BlockBuffer::getRecordCount() const { return static_cast<int>(records_.size()); }
int BlockBuffer::getPredRBN()     const { return predRBN_; }
int BlockBuffer::getSuccRBN()     const { return succRBN_; }
bool BlockBuffer::isAvail()       const { return records_.empty(); }

void BlockBuffer::setPredRBN(int rbn) { predRBN_ = rbn; }
void BlockBuffer::setSuccRBN(int rbn) { succRBN_ = rbn; }

// ─────────────────────────────────────────────────────────────────────────────
// Record access
// ─────────────────────────────────────────────────────────────────────────────

const vector<string>& BlockBuffer::getRecords() const {
    return records_;
}

/**
 * @brief Attempt to add a CSV record string to this buffer.
 *
 * Each record occupies RECORD_LEN_WIDTH bytes for its length prefix
 * plus the length of the record text itself.
 *
 * @param csvRecord CSV text (no newline).
 * @return true if the record fit, false if the block is full.
 */
bool BlockBuffer::addRecord(const string& csvRecord) {
    // Space needed: 4-byte length prefix + content
    int needed = RECORD_LEN_WIDTH + static_cast<int>(csvRecord.size());

    if (usedPayload() + needed > maxPayload()) {
        return false; // not enough room
    }

    records_.push_back(csvRecord);
    return true;
}

/**
 * @brief Return the number of free bytes in the block payload.
 */
int BlockBuffer::freeBytes() const {
    return maxPayload() - usedPayload();
}

/**
 * @brief Clear all records from the buffer.
 */
void BlockBuffer::clear() {
    records_.clear();
    predRBN_ = RBN_NULL;
    succRBN_ = RBN_NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialisation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Pack the buffer into a fixed-size string of blockSize_ bytes.
 *
 * Layout:
 *   [4-byte count][8-byte predRBN][8-byte succRBN]
 *   [4-byte len][record text][4-byte len][record text]...
 *   [padding spaces]
 *
 * RBN_NULL is stored as "FFFFFFFF".
 *
 * @return String of exactly blockSize_ bytes.
 */
string BlockBuffer::pack() const {
    string buf(blockSize_, ' '); // initialise entire block to spaces
    int pos = 0;

    // ── Block header ────────────────────────────────────────────────────────
    // Record count (4 bytes, space-padded, right-justified)
    string countStr = encodeInt(static_cast<int>(records_.size()), BLOCK_COUNT_WIDTH);
    buf.replace(pos, BLOCK_COUNT_WIDTH, countStr);
    pos += BLOCK_COUNT_WIDTH;

    // Predecessor RBN (8 bytes)
    string predStr = encodeInt(predRBN_, BLOCK_RBN_WIDTH);
    buf.replace(pos, BLOCK_RBN_WIDTH, predStr);
    pos += BLOCK_RBN_WIDTH;

    // Successor RBN (8 bytes)
    string succStr = encodeInt(succRBN_, BLOCK_RBN_WIDTH);
    buf.replace(pos, BLOCK_RBN_WIDTH, succStr);
    pos += BLOCK_RBN_WIDTH;

    // ── Records ─────────────────────────────────────────────────────────────
    for (const string& rec : records_) {
        // 4-byte length of this record
        string lenStr = encodeInt(static_cast<int>(rec.size()), RECORD_LEN_WIDTH);
        buf.replace(pos, RECORD_LEN_WIDTH, lenStr);
        pos += RECORD_LEN_WIDTH;

        // Record text
        buf.replace(pos, rec.size(), rec);
        pos += static_cast<int>(rec.size());
    }

    // Remaining bytes are already spaces (from initialisation above).
    return buf;
}

/**
 * @brief Unpack a raw block byte string into this buffer.
 *
 * Reads the metadata header and all length-prefixed records.
 *
 * @param raw Raw byte string of exactly blockSize_ bytes.
 * @return true on success, false on format/length error.
 */
bool BlockBuffer::unpack(const string& raw) {
    if (static_cast<int>(raw.size()) != blockSize_) {
        return false; // wrong block size
    }

    records_.clear();
    int pos = 0;

    // ── Block header ────────────────────────────────────────────────────────
    int count = decodeInt(raw.substr(pos, BLOCK_COUNT_WIDTH));
    pos += BLOCK_COUNT_WIDTH;

    predRBN_ = decodeInt(raw.substr(pos, BLOCK_RBN_WIDTH));
    pos += BLOCK_RBN_WIDTH;

    succRBN_ = decodeInt(raw.substr(pos, BLOCK_RBN_WIDTH));
    pos += BLOCK_RBN_WIDTH;

    // ── Records ─────────────────────────────────────────────────────────────
    // If count == 0 this is an avail block — no records to read.
    for (int i = 0; i < count; i++) {
        if (pos + RECORD_LEN_WIDTH > blockSize_) {
            return false; // truncated
        }

        int recLen = decodeInt(raw.substr(pos, RECORD_LEN_WIDTH));
        pos += RECORD_LEN_WIDTH;

        if (recLen < 0 || pos + recLen > blockSize_) {
            return false; // corrupt length
        }

        records_.push_back(raw.substr(pos, recLen));
        pos += recLen;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Disk I/O
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Write this buffer to disk at the position for rbn.
 *
 * @param fs  Open fstream (must be seekable, read+write).
 * @param rbn Relative Block Number.
 * @return true on success.
 */
bool BlockBuffer::writeBlock(fstream& fs, int rbn) const {
    if (!fs) return false;

    long long offset = offsetForRBN(rbn);
    fs.seekp(offset);
    if (!fs) return false;

    string packed = pack();
    fs.write(packed.c_str(), blockSize_);
    return static_cast<bool>(fs);
}

/**
 * @brief Read a block from disk by RBN into this buffer.
 *
 * @param fs  Open fstream.
 * @param rbn Relative Block Number.
 * @return true on success.
 */
bool BlockBuffer::readBlock(fstream& fs, int rbn) {
    if (!fs) return false;

    long long offset = offsetForRBN(rbn);
    fs.seekg(offset);
    if (!fs) return false;

    string raw(blockSize_, '\0');
    fs.read(&raw[0], blockSize_);
    if (!fs && !fs.eof()) return false;

    return unpack(raw);
}

/**
 * @brief Compute the file byte offset for a given RBN.
 *
 * @param rbn Relative Block Number.
 * @return Byte offset from the start of the file.
 */
long long BlockBuffer::offsetForRBN(int rbn) const {
    return static_cast<long long>(rbn) * blockSize_;
}

// ─────────────────────────────────────────────────────────────────────────────
// Avail-block helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Configure this buffer as an avail-list block.
 *
 * An avail block has record count 0 and payload filled with spaces.
 * The successor RBN links to the next avail block (or RBN_NULL).
 *
 * @param nextAvail RBN of the next avail block, or RBN_NULL for end-of-list.
 */
void BlockBuffer::makeAvail(int nextAvail) {
    records_.clear();
    predRBN_ = RBN_NULL;  // predecessor unused for avail blocks
    succRBN_ = nextAvail; // "successor" field repurposed as next-avail link
}

// ─────────────────────────────────────────────────────────────────────────────
// Dump helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return the highest (last, largest) key in the buffer.
 *
 * The key is the ZIP code — the first field of the CSV record.
 * Assumes records_ is sorted ascending by key.
 *
 * @return Highest key string, or "" if the buffer is empty.
 */
string BlockBuffer::highestKey() const {
    if (records_.empty()) return "";
    const string& last = records_.back();
    size_t comma = last.find(',');
    return (comma == string::npos) ? last : last.substr(0, comma);
}

/**
 * @brief Return the lowest (first, smallest) key in the buffer.
 *
 * @return Lowest key string, or "" if the buffer is empty.
 */
string BlockBuffer::lowestKey() const {
    if (records_.empty()) return "";
    const string& first = records_.front();
    size_t comma = first.find(',');
    return (comma == string::npos) ? first : first.substr(0, comma);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Encode an integer as a fixed-width zero-padded ASCII string.
 *
 * RBN_NULL (-1) is encoded as all 'F' characters (e.g. "FFFFFFFF" for width 8).
 *
 * @param value Integer to encode.
 * @param width Desired string width.
 * @return Fixed-width string.
 */
string BlockBuffer::encodeInt(int value, int width) const {
    if (value == RBN_NULL) {
        // Sentinel: all F's
        return string(width, 'F');
    }
    ostringstream ss;
    ss << setw(width) << setfill('0') << value;
    return ss.str();
}

/**
 * @brief Decode a fixed-width ASCII integer string.
 *
 * If the string is all 'F' (NULL sentinel) or cannot be parsed, returns RBN_NULL.
 *
 * @param s Fixed-width ASCII integer string.
 * @return Decoded integer, or RBN_NULL.
 */
int BlockBuffer::decodeInt(const string& s) const {
    // Check for NULL sentinel (all F's)
    bool allF = true;
    for (char c : s) {
        if (c != 'F') { allF = false; break; }
    }
    if (allF && !s.empty()) return RBN_NULL;

    try {
        // stoi handles space-padded strings fine
        return stoi(s);
    } catch (...) {
        return RBN_NULL;
    }
}

/**
 * @brief Maximum payload bytes available for records.
 *
 * @return blockSize_ - BLOCK_META_SIZE
 */
int BlockBuffer::maxPayload() const {
    return blockSize_ - BLOCK_META_SIZE;
}

/**
 * @brief Bytes consumed by records currently in records_.
 *
 * Each record uses RECORD_LEN_WIDTH (4 bytes) + its text length.
 *
 * @return Total used payload bytes.
 */
int BlockBuffer::usedPayload() const {
    int total = 0;
    for (const string& r : records_) {
        total += RECORD_LEN_WIDTH + static_cast<int>(r.size());
    }
    return total;
}
