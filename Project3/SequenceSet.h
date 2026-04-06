/**
 * @file SequenceSet.h
 * @brief High-level manager for the blocked sequence set file.
 *
 * SequenceSet coordinates:
 *   - Creating a blocked sequence set from a sorted CSV file.
 *   - Sequential reading (for Project 1 style analysis).
 *   - Physical and logical dump methods.
 *   - Index-based record search.
 *   - Record insertion (with block splitting).
 *   - Record deletion (with redistribution / merging).
 *   - Avail-list management.
 *
 * It uses:
 *   - BlockBuffer       to pack/unpack individual blocks.
 *   - BlockHeaderBuffer to read/write the file header (RBN 0).
 *   - SimpleIndex       to maintain the in-RAM {highestKey, RBN} index.
 *   - ZipCodeRecord     as the typed record object.
 *
 * @author Teagen Lee
 * @date Spring 2026
 */

#ifndef SEQUENCESET_H
#define SEQUENCESET_H

#include "BlockBuffer.h"
#include "BlockHeaderBuffer.h"
#include "SimpleIndex.h"
#include "ZipCodeBuffer.h"

#include <string>
#include <fstream>
#include <vector>
#include <functional>

using namespace std;

/**
 * @class SequenceSet
 * @brief Manages a blocked sequence set file for ZIP code records.
 */
class SequenceSet {
public:
    // ── Construction / Open / Close ──────────────────────────────────────────

    /**
     * @brief Construct a SequenceSet manager.
     * @param blockSize Block size in bytes (default 512).
     */
    explicit SequenceSet(int blockSize = DEFAULT_BLOCK_SIZE);

    ~SequenceSet();

    /**
     * @brief Create a brand-new blocked sequence set from a sorted CSV file.
     *
     * Steps:
     *   1) Read the CSV one record at a time (streaming).
     *   2) Pack records into blocks until each block hits 100% capacity.
     *   3) Write each completed block to disk.
     *   4) Build the SimpleIndex entry for each block.
     *   5) Write the index file.
     *   6) Write the file header (RBN 0).
     *
     * The CSV must already be sorted ascending by ZIP code.
     *
     * @param csvFile    Path to sorted CSV file.
     * @param dataFile   Path of the blocked sequence set to create.
     * @param indexFile  Path of the simple index file to create.
     * @return true on success.
     */
    bool create(const string& csvFile,
                const string& dataFile,
                const string& indexFile);

    /**
     * @brief Open an existing blocked sequence set (data + index).
     *
     * Reads the header from RBN 0, then loads the index into RAM.
     *
     * @param dataFile   Path to the blocked sequence set file.
     * @param indexFile  Path to the simple index file.
     * @return true on success.
     */
    bool open(const string& dataFile, const string& indexFile);

    /**
     * @brief Close the data file and flush the header.
     */
    void close();

    /**
     * @brief Check whether the file is open.
     */
    bool isOpen() const;

    // ── Sequential read (Project 1 style) ───────────────────────────────────

    /**
     * @brief Read all records in logical order and call visitor for each.
     *
     * Follows the successor RBN chain from seqSetHead.
     * Each block is unpacked; each record string is parsed into a ZipCodeRecord.
     *
     * @param visitor  Callback called with each ZipCodeRecord in key order.
     */
    void scanAll(function<void(const ZipCodeRecord&)> visitor) const;

    // ── Dump methods ─────────────────────────────────────────────────────────

    /**
     * @brief Dump blocks in physical order (by RBN 1, 2, 3, ...).
     *
     * Format:
     *   List Head:  <rbn>
     *   Avail Head: <rbn>
     *   <predRBN>  key1 key2 ... keyN  <succRBN>
     *   <predRBN>  *available*          <succRBN>
     *   ...
     */
    void dumpPhysical() const;

    /**
     * @brief Dump blocks in logical order (following successor chain).
     *
     * Same format as dumpPhysical but blocks appear in linked-list order.
     */
    void dumpLogical() const;

    // ── Index dump ───────────────────────────────────────────────────────────

    /**
     * @brief Print a readable dump of the simple index.
     */
    void dumpIndex() const;

    // ── Search ───────────────────────────────────────────────────────────────

    /**
     * @brief Search for a ZIP code record by key.
     *
     * Uses the index to find the candidate block, reads that block from disk,
     * and performs a linear search within the block.
     *
     * @param zipKey  ZIP code as a string (e.g. "56301").
     * @param result  Output record if found.
     * @return true if found, false if not in the file.
     */
    bool search(const string& zipKey, ZipCodeRecord& result);

    // ── Insert ───────────────────────────────────────────────────────────────

    /**
     * @brief Insert a new ZIP code record into the sequence set.
     *
     * Algorithm:
     *   1) Use the index to find the target block.
     *   2) If the record fits, insert in sorted position.
     *   3) If the block is full, split it:
     *       a) Obtain an avail block (from avail list or append new block).
     *       b) Distribute records evenly between the two blocks.
     *       c) Update predecessor/successor links.
     *       d) Update the index.
     *   Logs block splits to cout.
     *
     * @param rec Record to insert.
     * @return true on success, false if the record already exists.
     */
    bool insert(const ZipCodeRecord& rec);

    // ── Delete ───────────────────────────────────────────────────────────────

    /**
     * @brief Delete a ZIP code record from the sequence set.
     *
     * Algorithm:
     *   1) Use the index to find the target block.
     *   2) Remove the record from the block.
     *   3) If the block is below the minimum fill:
     *       a) Try redistribution with the left or right neighbour.
     *       b) If redistribution is not possible, merge with a neighbour.
     *          The merged block is added to the avail list.
     *   Logs merges / redistributions to cout.
     *
     * @param zipKey ZIP code string to delete.
     * @return true if found and deleted, false if not found.
     */
    bool remove(const string& zipKey);

    // ── Header accessors ─────────────────────────────────────────────────────

    /**
     * @brief Print the file header contents to cout.
     */
    void printHeader() const;

private:
    // ── Member data ──────────────────────────────────────────────────────────

    int               blockSize_;    ///< Block size in bytes.
    mutable fstream   fs_;           ///< Open fstream for the data file.
    string            dataFile_;     ///< Path to the data file.
    string            indexFile_;    ///< Path to the index file.
    BlockHeaderBuffer headerBuf_;    ///< Header buffer (RBN 0).
    SimpleIndex       index_;        ///< In-RAM simple index.
    bool              isOpen_;       ///< True when file is open.

    // ── Private helpers ──────────────────────────────────────────────────────

    /**
     * @brief Allocate a new block at the end of the file.
     * @return RBN of the newly allocated block.
     */
    int appendBlock();

    /**
     * @brief Get an avail block (from avail list, or append a new one).
     *
     * If the avail list is non-empty, pops the head.
     * Otherwise appends a new block and returns its RBN.
     *
     * @return RBN of the obtained block.
     */
    int getAvailBlock();

    /**
     * @brief Release a block back onto the avail list.
     *
     * Overwrites the block with an avail-block structure on disk,
     * then updates the header's avail head pointer.
     *
     * @param rbn RBN of the block to release.
     */
    void releaseBlock(int rbn);

    /**
     * @brief Write the updated file header to disk.
     */
    void flushHeader();

    /**
     * @brief Read a block from disk.
     * @param rbn RBN to read.
     * @param bb  Output BlockBuffer.
     * @return true on success.
     */
    bool readBlock(int rbn, BlockBuffer& bb) const;

    /**
     * @brief Write a block to disk.
     * @param rbn RBN to write.
     * @param bb  BlockBuffer to write.
     * @return true on success.
     */
    bool writeBlock(int rbn, BlockBuffer& bb);

    /**
     * @brief Extract the ZIP key from a raw CSV record string.
     * @param csvRecord CSV string.
     * @return ZIP string (first field before comma).
     */
    static string keyOf(const string& csvRecord);

    /**
     * @brief Minimum number of records a block must hold (50% of capacity).
     *
     * Calculated from blockSize_ and a typical record size.
     * We compute 50% of the block's actual capacity in bytes,
     * then convert that to a minimum count using an average record byte size.
     *
     * For simplicity we use a fixed floor: 1 record minimum.
     *
     * @param bb Reference block (to know actual capacity used).
     * @return Minimum record count.
     */
    int minRecords(const BlockBuffer& bb) const;

    /**
     * @brief Find the RBN of the logically last block (tail of chain).
     * @return RBN of tail, or RBN_NULL if chain is empty.
     */
    int findTailRBN() const;

    /**
     * @brief Insert a sorted CSV string into the records vector.
     * @param records Sorted vector of CSV strings.
     * @param csv     New record to insert (must not already be present).
     */
    static void insertSorted(vector<string>& records, const string& csv);

    /**
     * @brief Remove a CSV string with the given key from a records vector.
     * @param records Sorted vector.
     * @param key     ZIP key to remove.
     * @return true if found and removed.
     */
    static bool removeByKey(vector<string>& records, const string& key);

    /**
     * @brief Print a single block line in the dump format.
     *
     * Format:
     *   <predRBN>  key1 key2 ... keyN  <succRBN>
     * or
     *   <predRBN>  *available*          <succRBN>
     *
     * @param rbn  RBN of the block being printed.
     * @param bb   BlockBuffer for this block.
     */
    static void printBlockLine(int rbn, const BlockBuffer& bb);
};

#endif // SEQUENCESET_H
