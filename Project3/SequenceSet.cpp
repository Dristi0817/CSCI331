/**
 * @file SequenceSet.cpp
 * @brief Implementation of the SequenceSet class.
 *
 * @author Teagen Lee
 * @date Spring 2026
 */

#include "SequenceSet.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <functional>

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Construct a SequenceSet manager with the given block size.
 */
SequenceSet::SequenceSet(int blockSize)
    : blockSize_(blockSize),
      headerBuf_(blockSize),
      isOpen_(false)
{
}

/**
 * @brief Destructor — close the file if it is still open.
 */
SequenceSet::~SequenceSet() {
    if (isOpen_) close();
}

// ─────────────────────────────────────────────────────────────────────────────
// Create
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Create a brand-new blocked sequence set from a sorted CSV file.
 *
 * The CSV must already be sorted ascending by ZIP code (first field).
 * Each block is filled to capacity before a new block is started.
 *
 * @param csvFile   Sorted CSV input.
 * @param dataFile  Output blocked sequence set file.
 * @param indexFile Output simple index file.
 * @return true on success.
 */
bool SequenceSet::create(const string& csvFile,
                         const string& dataFile,
                         const string& indexFile)
{
    dataFile_  = dataFile;
    indexFile_ = indexFile;

    // ── Open the output data file ────────────────────────────────────────────
    fs_.open(dataFile, ios::in | ios::out | ios::binary | ios::trunc);
    if (!fs_) {
        cerr << "SequenceSet::create — cannot create '" << dataFile << "'\n";
        return false;
    }
    isOpen_ = true;

    // RBN 0 is reserved for the header. Write a placeholder block of spaces.
    {
        string placeholder(blockSize_, ' ');
        fs_.seekp(0);
        fs_.write(placeholder.c_str(), blockSize_);
    }

    // ── Open the CSV ─────────────────────────────────────────────────────────
    ZipCodeBuffer csvBuf;
    if (!csvBuf.open(csvFile)) {
        cerr << "SequenceSet::create — cannot open CSV '" << csvFile << "'\n";
        return false;
    }

    // ── Pack records into blocks ─────────────────────────────────────────────
    index_.removeByRBN(-999); // clear index (noop, just reset)
    // Actually clear by rebuilding:
    SimpleIndex freshIndex;
    index_ = freshIndex;

    int nextRBN = 1;  // first data block is RBN 1

    BlockBuffer current(blockSize_);
    current.setPredRBN(RBN_NULL);
    current.setSuccRBN(RBN_NULL);

    long long totalRecords = 0;

    ZipCodeRecord rec;
    while (csvBuf.readRecord(rec)) {
        string csv = rec.toCSV();

        if (!current.addRecord(csv)) {
            // Block is full — write it and start a new one
            int thisRBN = nextRBN++;

            // Link: current block's successor = next block
            current.setSuccRBN(nextRBN);

            // Write current block to disk
            if (!writeBlock(thisRBN, current)) {
                cerr << "SequenceSet::create — write failed at RBN " << thisRBN << "\n";
                return false;
            }

            // Add index entry: highest key of this block → thisRBN
            index_.upsert(current.highestKey(), thisRBN);

            // Start new block
            current.clear();
            current.setPredRBN(thisRBN);
            current.setSuccRBN(RBN_NULL);

            // Now add the record that didn't fit
            current.addRecord(csv);
        }
        totalRecords++;
    }
    csvBuf.close();

    // ── Write the final (possibly partial) block ─────────────────────────────
    if (current.getRecordCount() > 0) {
        int thisRBN = nextRBN++;
        current.setSuccRBN(RBN_NULL); // last block: no successor
        if (!writeBlock(thisRBN, current)) {
            cerr << "SequenceSet::create — write failed at final block\n";
            return false;
        }
        index_.upsert(current.highestKey(), thisRBN);
    }

    int totalBlocks = nextRBN; // includes RBN 0 (header)

    // ── Write the index file ─────────────────────────────────────────────────
    if (!index_.write(indexFile)) {
        cerr << "SequenceSet::create — cannot write index '" << indexFile << "'\n";
        return false;
    }

    // ── Write the file header ────────────────────────────────────────────────
    headerBuf_.buildDefault(indexFile, blockSize_, totalRecords,
                            totalBlocks, RBN_NULL, 1);
    if (!headerBuf_.write(fs_)) {
        cerr << "SequenceSet::create — cannot write file header\n";
        return false;
    }

    cout << "Created blocked sequence set: " << dataFile << "\n";
    cout << "  Records : " << totalRecords << "\n";
    cout << "  Blocks  : " << (totalBlocks - 1) << " data blocks (+ 1 header)\n";
    cout << "  Index   : " << indexFile << " (" << index_.size() << " entries)\n";

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Open / Close
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Open an existing blocked sequence set.
 *
 * Reads the header (RBN 0) and loads the index into RAM.
 *
 * @param dataFile   Path to the blocked sequence set file.
 * @param indexFile  Path to the simple index file.
 * @return true on success.
 */
bool SequenceSet::open(const string& dataFile, const string& indexFile) {
    dataFile_  = dataFile;
    indexFile_ = indexFile;

    fs_.open(dataFile, ios::in | ios::out | ios::binary);
    if (!fs_) {
        cerr << "SequenceSet::open — cannot open '" << dataFile << "'\n";
        return false;
    }
    isOpen_ = true;

    // Read header from RBN 0
    if (!headerBuf_.read(fs_)) {
        cerr << "SequenceSet::open — cannot read file header\n";
        return false;
    }

    // Load index into RAM
    if (!index_.read(indexFile)) {
        cerr << "SequenceSet::open — cannot read index '" << indexFile << "'\n";
        return false;
    }

    return true;
}

/**
 * @brief Close the data file, flushing the header first.
 */
void SequenceSet::close() {
    if (!isOpen_) return;
    flushHeader();
    fs_.close();
    isOpen_ = false;
}

/**
 * @brief Return whether the file is currently open.
 */
bool SequenceSet::isOpen() const {
    return isOpen_;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sequential Scan
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Visit every record in logical (key) order.
 *
 * Follows the successor chain starting from seqSetHeadRBN.
 * For each active block, reads its records and calls visitor.
 *
 * @param visitor Callback receiving each ZipCodeRecord.
 */
void SequenceSet::scanAll(function<void(const ZipCodeRecord&)> visitor) const {
    if (!isOpen_) return;

    int rbn = headerBuf_.getHeader().seqSetHeadRBN;
    while (rbn != RBN_NULL) {
        BlockBuffer bb(blockSize_);
        if (!readBlock(rbn, bb)) break;

        if (!bb.isAvail()) {
            for (const string& csv : bb.getRecords()) {
                ZipCodeRecord rec;
                if (rec.fromCSV(csv)) {
                    visitor(rec);
                }
            }
        }
        rbn = bb.getSuccRBN();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Dump methods
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Print each block line in the format required by the assignment.
 *
 * "List Head:  RBN"
 * "Avail Head: RBN"
 * "RBN  keya keyb ... keyi  RBN"
 * "RBN  *available*          RBN"
 *
 * @param rbn RBN of the block.
 * @param bb  BlockBuffer for the block.
 */
void SequenceSet::printBlockLine(int rbn, const BlockBuffer& bb) {
    string predStr = (bb.getPredRBN() == RBN_NULL) ? "NULL" : to_string(bb.getPredRBN());
    string succStr = (bb.getSuccRBN() == RBN_NULL) ? "NULL" : to_string(bb.getSuccRBN());

    cout << setw(4) << predStr << "  ";

    if (bb.isAvail()) {
        cout << left << setw(40) << "*available*" << right;
    } else {
        for (const string& csv : bb.getRecords()) {
            string key = keyOf(csv);
            cout << key << " ";
        }
        // Pad so the successor column lines up reasonably
        cout << " ";
    }

    cout << setw(4) << succStr << "  (RBN " << rbn << ")\n";
}

/**
 * @brief Dump all blocks in physical (RBN) order.
 */
void SequenceSet::dumpPhysical() const {
    if (!isOpen_) { cout << "(file not open)\n"; return; }

    const BlockFileHeader& h = headerBuf_.getHeader();
    cout << "=== Physical Dump ===\n";
    cout << "List Head:  " << h.seqSetHeadRBN << "\n";
    cout << "Avail Head: ";
    if (h.availHeadRBN == RBN_NULL) cout << "(none)\n"; else cout << h.availHeadRBN << "\n";

    // Iterate RBN 1 through blockCount-1
    for (int rbn = 1; rbn < h.blockCount; rbn++) {
        BlockBuffer bb(blockSize_);
        if (!readBlock(rbn, bb)) {
            cout << "  RBN " << rbn << ": (read error)\n";
            continue;
        }
        printBlockLine(rbn, bb);
    }
}

/**
 * @brief Dump blocks in logical (successor chain) order.
 */
void SequenceSet::dumpLogical() const {
    if (!isOpen_) { cout << "(file not open)\n"; return; }

    const BlockFileHeader& h = headerBuf_.getHeader();
    cout << "=== Logical Dump ===\n";
    cout << "List Head:  " << h.seqSetHeadRBN << "\n";
    cout << "Avail Head: ";
    if (h.availHeadRBN == RBN_NULL) cout << "(none)\n"; else cout << h.availHeadRBN << "\n";

    // Follow the successor chain
    int rbn = h.seqSetHeadRBN;
    while (rbn != RBN_NULL) {
        BlockBuffer bb(blockSize_);
        if (!readBlock(rbn, bb)) {
            cout << "  RBN " << rbn << ": (read error)\n";
            break;
        }
        printBlockLine(rbn, bb);
        rbn = bb.getSuccRBN();
    }

    // Also dump avail list
    rbn = h.availHeadRBN;
    if (rbn != RBN_NULL) {
        cout << "--- Avail list ---\n";
        while (rbn != RBN_NULL) {
            BlockBuffer bb(blockSize_);
            if (!readBlock(rbn, bb)) break;
            printBlockLine(rbn, bb);
            rbn = bb.getSuccRBN(); // repurposed as next-avail link
        }
    }
}

/**
 * @brief Print a readable dump of the simple index to cout.
 */
void SequenceSet::dumpIndex() const {
    index_.dump();
}

// ─────────────────────────────────────────────────────────────────────────────
// Search
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Search for a record by ZIP key.
 *
 * Uses the simple index to locate the candidate block, then searches
 * that block linearly.
 *
 * @param zipKey ZIP code string (e.g. "56301").
 * @param result Output ZipCodeRecord if found.
 * @return true if found.
 */
bool SequenceSet::search(const string& zipKey, ZipCodeRecord& result) {
    if (!isOpen_) return false;

    int rbn = index_.findRBN(zipKey);
    if (rbn == -1) {
        // Key is beyond all blocks — not found
        return false;
    }

    BlockBuffer bb(blockSize_);
    if (!readBlock(rbn, bb)) return false;

    // Linear search within the block
    for (const string& csv : bb.getRecords()) {
        if (keyOf(csv) == zipKey) {
            return result.fromCSV(csv);
        }
    }
    return false; // not in this block
}

// ─────────────────────────────────────────────────────────────────────────────
// Insert
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Insert a new record into the sequence set.
 *
 * Finds the target block using the index, inserts the record in sorted order,
 * and splits the block if it is full.
 *
 * @param rec Record to insert.
 * @return true on success, false if the key already exists.
 */
bool SequenceSet::insert(const ZipCodeRecord& rec) {
    if (!isOpen_) return false;

    string csv = rec.toCSV();

    // Zero-pad to 5 digits so string sort matches numeric sort
    ostringstream ks;
    ks << setw(5) << setfill('0') << rec.zipCode;
    string key = ks.str();

    // ── Find the target block ────────────────────────────────────────────────
    int rbn = index_.findRBN(key);

    if (rbn == -1) {
        // Key is beyond all current blocks — insert into the last block
        rbn = findTailRBN();
        if (rbn == RBN_NULL) {
            // Empty file — allocate the first data block
            rbn = appendBlock();
            headerBuf_.getHeader().seqSetHeadRBN = rbn;
        }
    }

    BlockBuffer bb(blockSize_);
    if (!readBlock(rbn, bb)) return false;

    // ── Check for duplicate ──────────────────────────────────────────────────
    for (const string& existing : bb.getRecords()) {
        if (keyOf(existing) == key) {
            cerr << "insert: ZIP " << key << " already exists\n";
            return false;
        }
    }

    // ── Try to insert directly ───────────────────────────────────────────────
    vector<string> recs = bb.getRecords();
    insertSorted(recs, csv);

    // Rebuild block with the new record list
    BlockBuffer trial(blockSize_);
    trial.setPredRBN(bb.getPredRBN());
    trial.setSuccRBN(bb.getSuccRBN());
    bool fits = true;
    for (const string& r : recs) {
        if (!trial.addRecord(r)) { fits = false; break; }
    }

    if (fits) {
        // Simple insert — write updated block and update index
        writeBlock(rbn, trial);
        index_.upsert(trial.highestKey(), rbn);
        flushHeader();
        return true;
    }

    // ── Block is full: split ─────────────────────────────────────────────────
    cout << "LOG: Block split at RBN " << rbn
         << " (inserting ZIP " << key << ")\n";

    int newRBN = getAvailBlock();

    // Split records evenly
    size_t half = recs.size() / 2;
    vector<string> leftRecs(recs.begin(), recs.begin() + half);
    vector<string> rightRecs(recs.begin() + half, recs.end());

    // Left block keeps the original RBN
    BlockBuffer leftBB(blockSize_);
    leftBB.setPredRBN(bb.getPredRBN());
    leftBB.setSuccRBN(newRBN);
    for (const string& r : leftRecs) leftBB.addRecord(r);

    // Right block gets the new RBN
    BlockBuffer rightBB(blockSize_);
    rightBB.setPredRBN(rbn);
    rightBB.setSuccRBN(bb.getSuccRBN());
    for (const string& r : rightRecs) rightBB.addRecord(r);

    // Update the successor block's predecessor link
    if (bb.getSuccRBN() != RBN_NULL) {
        BlockBuffer succBB(blockSize_);
        if (readBlock(bb.getSuccRBN(), succBB)) {
            succBB.setPredRBN(newRBN);
            writeBlock(bb.getSuccRBN(), succBB);
        }
    }

    writeBlock(rbn,    leftBB);
    writeBlock(newRBN, rightBB);

    // Update index: old key removed, left and right high keys added
    index_.removeByRBN(rbn);
    index_.upsert(leftBB.highestKey(),  rbn);
    index_.upsert(rightBB.highestKey(), newRBN);

    cout << "LOG: Index updated after split — left high=" << leftBB.highestKey()
         << " (RBN " << rbn << "), right high=" << rightBB.highestKey()
         << " (RBN " << newRBN << ")\n";

    // Persist index
    index_.write(indexFile_);
    headerBuf_.getHeader().recordCount++;
    flushHeader();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Delete
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Delete a record by ZIP key.
 *
 * After deletion, if the block falls below minimum fill, attempts redistribution
 * with a neighbour; if that is not possible, merges with a neighbour.
 *
 * @param zipKey ZIP code string to delete.
 * @return true if found and deleted.
 */
bool SequenceSet::remove(const string& zipKey) {
    if (!isOpen_) return false;

    int rbn = index_.findRBN(zipKey);
    if (rbn == -1) return false;

    BlockBuffer bb(blockSize_);
    if (!readBlock(rbn, bb)) return false;

    // ── Remove the record ────────────────────────────────────────────────────
    vector<string> recs = bb.getRecords();
    if (!removeByKey(recs, zipKey)) {
        return false; // record not in this block
    }

    // If block is now empty and it is the only block, just write it back empty
    // (we'll still keep the block allocated; merge logic below handles cleanup)

    // ── Rebuild the block with the updated record list ───────────────────────
    BlockBuffer updated(blockSize_);
    updated.setPredRBN(bb.getPredRBN());
    updated.setSuccRBN(bb.getSuccRBN());
    for (const string& r : recs) updated.addRecord(r);

    // ── Check minimum fill ───────────────────────────────────────────────────
    int minRec = minRecords(bb);

    if (static_cast<int>(recs.size()) >= minRec || recs.empty() == false) {
        // Still above minimum (or has records) — check threshold properly
        if (static_cast<int>(recs.size()) >= minRec) {
            writeBlock(rbn, updated);
            index_.upsert(updated.highestKey().empty() ? "" : updated.highestKey(), rbn);
            if (!recs.empty()) index_.upsert(updated.highestKey(), rbn);
            else index_.removeByRBN(rbn);
            index_.write(indexFile_);
            headerBuf_.getHeader().recordCount--;
            flushHeader();
            return true;
        }
    }

    // ── Below minimum fill — try redistribution ──────────────────────────────
    int predRBN = bb.getPredRBN();
    int succRBN = bb.getSuccRBN();

    // Try left neighbour first
    if (predRBN != RBN_NULL) {
        BlockBuffer leftBB(blockSize_);
        if (readBlock(predRBN, leftBB) && !leftBB.isAvail()) {
            vector<string> leftRecs = leftBB.getRecords();
            // Can we redistribute? Left must have more than min after giving one away
            if (static_cast<int>(leftRecs.size()) - 1 >= minRec) {
                cout << "LOG: Redistribution between RBN " << predRBN
                     << " (left) and RBN " << rbn << " (right)\n";

                // Move the last record of left into the front of recs
                recs.insert(recs.begin(), leftRecs.back());
                leftRecs.pop_back();

                // Rebuild both blocks
                BlockBuffer newLeft(blockSize_);
                newLeft.setPredRBN(leftBB.getPredRBN());
                newLeft.setSuccRBN(rbn);
                for (const string& r : leftRecs) newLeft.addRecord(r);

                BlockBuffer newRight(blockSize_);
                newRight.setPredRBN(predRBN);
                newRight.setSuccRBN(succRBN);
                for (const string& r : recs) newRight.addRecord(r);

                writeBlock(predRBN, newLeft);
                writeBlock(rbn,     newRight);

                index_.upsert(newLeft.highestKey(),  predRBN);
                index_.upsert(newRight.highestKey(), rbn);

                cout << "LOG: Index updated after redistribution\n";
                index_.write(indexFile_);
                headerBuf_.getHeader().recordCount--;
                flushHeader();
                return true;
            }
        }
    }

    // Try right neighbour
    if (succRBN != RBN_NULL) {
        BlockBuffer rightBB(blockSize_);
        if (readBlock(succRBN, rightBB) && !rightBB.isAvail()) {
            vector<string> rightRecs = rightBB.getRecords();
            if (static_cast<int>(rightRecs.size()) - 1 >= minRec) {
                cout << "LOG: Redistribution between RBN " << rbn
                     << " (left) and RBN " << succRBN << " (right)\n";

                // Move the first record of right into the back of recs
                recs.push_back(rightRecs.front());
                rightRecs.erase(rightRecs.begin());

                BlockBuffer newLeft(blockSize_);
                newLeft.setPredRBN(predRBN);
                newLeft.setSuccRBN(succRBN);
                for (const string& r : recs) newLeft.addRecord(r);

                BlockBuffer newRight(blockSize_);
                newRight.setPredRBN(rbn);
                newRight.setSuccRBN(rightBB.getSuccRBN());
                for (const string& r : rightRecs) newRight.addRecord(r);

                writeBlock(rbn,     newLeft);
                writeBlock(succRBN, newRight);

                index_.upsert(newLeft.highestKey(),  rbn);
                index_.upsert(newRight.highestKey(), succRBN);

                cout << "LOG: Index updated after redistribution\n";
                index_.write(indexFile_);
                headerBuf_.getHeader().recordCount--;
                flushHeader();
                return true;
            }
        }
    }

    // ── Redistribution impossible — merge ────────────────────────────────────
    // Prefer merging with the right neighbour; fall back to left.
    if (succRBN != RBN_NULL) {
        BlockBuffer rightBB(blockSize_);
        if (readBlock(succRBN, rightBB) && !rightBB.isAvail()) {
            cout << "LOG: Merge RBN " << rbn << " (left) + RBN " << succRBN
                 << " (right) → RBN " << rbn << " survives\n";

            vector<string> rightRecs = rightBB.getRecords();
            vector<string> merged    = recs;
            merged.insert(merged.end(), rightRecs.begin(), rightRecs.end());

            BlockBuffer mergedBB(blockSize_);
            mergedBB.setPredRBN(predRBN);
            mergedBB.setSuccRBN(rightBB.getSuccRBN());
            for (const string& r : merged) mergedBB.addRecord(r);

            // Update the block after the merged-away right block
            if (rightBB.getSuccRBN() != RBN_NULL) {
                BlockBuffer afterRight(blockSize_);
                if (readBlock(rightBB.getSuccRBN(), afterRight)) {
                    afterRight.setPredRBN(rbn);
                    writeBlock(rightBB.getSuccRBN(), afterRight);
                }
            }

            writeBlock(rbn, mergedBB);
            releaseBlock(succRBN);

            index_.removeByRBN(rbn);
            index_.removeByRBN(succRBN);
            index_.upsert(mergedBB.highestKey(), rbn);

            cout << "LOG: Index updated after merge; RBN " << succRBN
                 << " added to avail list\n";
            index_.write(indexFile_);
            headerBuf_.getHeader().recordCount--;
            flushHeader();
            return true;
        }
    } else if (predRBN != RBN_NULL) {
        BlockBuffer leftBB(blockSize_);
        if (readBlock(predRBN, leftBB) && !leftBB.isAvail()) {
            cout << "LOG: Merge RBN " << predRBN << " (left) + RBN " << rbn
                 << " (right) → RBN " << predRBN << " survives\n";

            vector<string> leftRecs = leftBB.getRecords();
            vector<string> merged   = leftRecs;
            merged.insert(merged.end(), recs.begin(), recs.end());

            BlockBuffer mergedBB(blockSize_);
            mergedBB.setPredRBN(leftBB.getPredRBN());
            mergedBB.setSuccRBN(succRBN);
            for (const string& r : merged) mergedBB.addRecord(r);

            if (succRBN != RBN_NULL) {
                BlockBuffer afterRight(blockSize_);
                if (readBlock(succRBN, afterRight)) {
                    afterRight.setPredRBN(predRBN);
                    writeBlock(succRBN, afterRight);
                }
            }

            writeBlock(predRBN, mergedBB);
            releaseBlock(rbn);

            index_.removeByRBN(predRBN);
            index_.removeByRBN(rbn);
            index_.upsert(mergedBB.highestKey(), predRBN);

            cout << "LOG: Index updated after merge; RBN " << rbn
                 << " added to avail list\n";
            index_.write(indexFile_);
            headerBuf_.getHeader().recordCount--;
            flushHeader();
            return true;
        }
    }

    // ── Last resort: block has too few records but no neighbours to merge with
    // (single-block file after deletion) — just write it back as-is.
    writeBlock(rbn, updated);
    if (!recs.empty()) index_.upsert(updated.highestKey(), rbn);
    else               index_.removeByRBN(rbn);
    index_.write(indexFile_);
    headerBuf_.getHeader().recordCount--;
    flushHeader();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Header display
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Print the file header to cout.
 */
void SequenceSet::printHeader() const {
    headerBuf_.print();
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Append a new empty block at the end of the file.
 *
 * Writes a blank block and increments the block count in the header.
 *
 * @return RBN of the new block.
 */
int SequenceSet::appendBlock() {
    BlockFileHeader& h = headerBuf_.getHeader();
    int newRBN = h.blockCount; // next available RBN

    // Write a blank (avail-style) block at this position
    BlockBuffer blank(blockSize_);
    blank.makeAvail(RBN_NULL);

    fs_.seekp(static_cast<long long>(newRBN) * blockSize_);
    string packed = blank.pack();
    fs_.write(packed.c_str(), blockSize_);

    h.blockCount++;
    return newRBN;
}

/**
 * @brief Obtain an avail block for reuse.
 *
 * Pops the head of the avail list if non-empty; otherwise appends a new block.
 *
 * @return RBN of the obtained block.
 */
int SequenceSet::getAvailBlock() {
    BlockFileHeader& h = headerBuf_.getHeader();

    if (h.availHeadRBN != RBN_NULL) {
        // Pop the head of the avail list
        int rbn = h.availHeadRBN;
        BlockBuffer avail(blockSize_);
        if (readBlock(rbn, avail)) {
            // The avail block's succRBN field is the next avail
            h.availHeadRBN = avail.getSuccRBN();
        } else {
            h.availHeadRBN = RBN_NULL;
        }
        return rbn;
    }

    // No avail blocks — allocate a new one
    return appendBlock();
}

/**
 * @brief Release a block back to the avail list.
 *
 * Overwrites the block with an avail structure on disk and links it
 * to the front of the avail list.
 *
 * @param rbn RBN of the block to release.
 */
void SequenceSet::releaseBlock(int rbn) {
    BlockFileHeader& h = headerBuf_.getHeader();

    BlockBuffer avail(blockSize_);
    avail.makeAvail(h.availHeadRBN); // chain to old avail head

    // Overwrite on disk
    writeBlock(rbn, avail);

    // Update avail head
    h.availHeadRBN = rbn;
}

/**
 * @brief Write the file header back to RBN 0.
 */
void SequenceSet::flushHeader() {
    headerBuf_.write(fs_);
}

/**
 * @brief Read a block from disk into a BlockBuffer.
 *
 * @param rbn RBN to read.
 * @param bb  Output BlockBuffer.
 * @return true on success.
 */
bool SequenceSet::readBlock(int rbn, BlockBuffer& bb) const {
    fs_.seekg(static_cast<long long>(rbn) * blockSize_);
    string raw(blockSize_, '\0');
    fs_.read(&raw[0], blockSize_);
    if (!fs_ && !fs_.eof()) return false;
    return bb.unpack(raw);
}

/**
 * @brief Write a BlockBuffer to disk at the given RBN.
 *
 * @param rbn RBN to write.
 * @param bb  BlockBuffer to write.
 * @return true on success.
 */
bool SequenceSet::writeBlock(int rbn, BlockBuffer& bb) {
    fs_.seekp(static_cast<long long>(rbn) * blockSize_);
    string packed = bb.pack();
    fs_.write(packed.c_str(), blockSize_);
    return static_cast<bool>(fs_);
}

/**
 * @brief Extract the ZIP key (first CSV field) from a raw CSV record string.
 *
 * @param csvRecord CSV text.
 * @return ZIP key string.
 */
string SequenceSet::keyOf(const string& csvRecord) {
    size_t comma = csvRecord.find(',');
    return (comma == string::npos) ? csvRecord : csvRecord.substr(0, comma);
}

/**
 * @brief Compute the minimum number of records required in a block (50% fill).
 *
 * We estimate capacity by how many records the block can hold at the current
 * average record size.  Minimum is always at least 1.
 *
 * @param bb Reference block (used to compute average record size).
 * @return Minimum record count.
 */
int SequenceSet::minRecords(const BlockBuffer& bb) const {
    // Payload available for records
    int payload = blockSize_ - BLOCK_META_SIZE;

    // Estimate average record byte size from existing records
    const vector<string>& recs = bb.getRecords();
    if (recs.empty()) return 1;

    int totalBytes = 0;
    for (const string& r : recs) totalBytes += RECORD_LEN_WIDTH + static_cast<int>(r.size());
    double avgSize = static_cast<double>(totalBytes) / recs.size();

    // Capacity = how many average-sized records fit
    int capacity = static_cast<int>(payload / avgSize);

    // 50% minimum, at least 1
    int minCount = max(1, capacity / 2);
    return minCount;
}

/**
 * @brief Find the RBN of the last block in the logical chain.
 *
 * @return RBN of the tail block, or RBN_NULL if the chain is empty.
 */
int SequenceSet::findTailRBN() const {
    int rbn = headerBuf_.getHeader().seqSetHeadRBN;
    if (rbn == RBN_NULL) return RBN_NULL;

    while (true) {
        BlockBuffer bb(blockSize_);
        if (!readBlock(rbn, bb)) break;
        if (bb.getSuccRBN() == RBN_NULL) return rbn;
        rbn = bb.getSuccRBN();
    }
    return RBN_NULL;
}

/**
 * @brief Insert a CSV string into a sorted vector in ascending key order.
 *
 * @param records Sorted vector of CSV strings.
 * @param csv     New CSV record string.
 */
void SequenceSet::insertSorted(vector<string>& records, const string& csv) {
    string newKey = keyOf(csv);
    auto pos = lower_bound(records.begin(), records.end(), csv,
        [](const string& a, const string& b) {
            return keyOf(a) < keyOf(b);
        });
    records.insert(pos, csv);
}

/**
 * @brief Remove the record with the given key from a sorted vector.
 *
 * @param records Sorted vector.
 * @param key     ZIP key to remove.
 * @return true if found and removed.
 */
bool SequenceSet::removeByKey(vector<string>& records, const string& key) {
    for (auto it = records.begin(); it != records.end(); ++it) {
        if (keyOf(*it) == key) {
            records.erase(it);
            return true;
        }
    }
    return false;
}
