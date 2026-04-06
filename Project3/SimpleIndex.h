/**
 * @file SimpleIndex.h
 * @brief Simple primary key index for the blocked sequence set.
 *
 * This index stores ordered pairs {highest key in block, RBN} as described
 * in Folk Figure 10.3.  The index is small enough to load entirely into RAM.
 *
 * On disk the index file has this format:
 *   Line 0: SIDX,1                   (file type tag + version)
 *   Line N: <key> <rbn>              (one entry per active block)
 *
 * Example:
 *   SIDX,1
 *   01005 1
 *   02139 2
 *   56301 3
 *
 * @author Teagen Lee
 * @date Spring 2026
 */

#ifndef SIMPLEINDEX_H
#define SIMPLEINDEX_H

#include <string>
#include <map>
#include <vector>
#include <fstream>

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// SimpleIndex
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class SimpleIndex
 * @brief In-RAM representation of the simple primary key index.
 *
 * The index maps each active block's highest key to its RBN.
 * It uses a std::map so the entries are always sorted by key.
 *
 * Usage pattern:
 *   1) Build the index during file creation (insert entries for each block).
 *   2) Write to disk.
 *   3) At search time, load from disk into RAM.
 *   4) To find a ZIP code, call findRBN(); it returns the RBN of the block
 *      whose highest key >= the target key (the block that could contain it).
 */
class SimpleIndex {
public:
    SimpleIndex();

    // ── Build / Modify ───────────────────────────────────────────────────────

    /**
     * @brief Insert or update an entry {highestKey → rbn}.
     *
     * If an entry for this rbn already exists under a different key,
     * the old entry is removed first.
     *
     * @param highestKey Highest ZIP key in the block (e.g. "56310").
     * @param rbn        Relative Block Number of that block.
     */
    void upsert(const string& highestKey, int rbn);

    /**
     * @brief Remove the entry whose value equals rbn.
     *
     * @param rbn RBN of the block to remove from the index.
     */
    void removeByRBN(int rbn);

    /**
     * @brief Remove the entry whose key equals highestKey.
     *
     * @param highestKey The exact highest-key string to remove.
     */
    void removeByKey(const string& highestKey);

    // ── Search ───────────────────────────────────────────────────────────────

    /**
     * @brief Find the RBN of the block that could contain targetKey.
     *
     * Looks up the first block whose highest key >= targetKey.
     * If no such block exists, returns -1 (the key is beyond the last block).
     *
     * @param targetKey ZIP code string to search for (e.g. "56301").
     * @return RBN of the candidate block, or -1 if not found.
     */
    int findRBN(const string& targetKey) const;

    // ── I/O ──────────────────────────────────────────────────────────────────

    /**
     * @brief Write the index to a text file.
     *
     * @param filename Output file path.
     * @return true on success.
     */
    bool write(const string& filename) const;

    /**
     * @brief Load the index from a text file into RAM.
     *
     * @param filename Input file path.
     * @return true on success.
     */
    bool read(const string& filename);

    // ── Accessors ────────────────────────────────────────────────────────────

    /**
     * @brief Return the number of entries in the index.
     */
    int size() const;

    /**
     * @brief Return all entries as {key, rbn} pairs in key order.
     */
    vector<pair<string, int>> entries() const;

    // ── Display ──────────────────────────────────────────────────────────────

    /**
     * @brief Print a readable dump of the index to cout.
     */
    void dump() const;

private:
    /// Sorted map: highest key string → RBN.
    map<string, int> index_;
};

#endif // SIMPLEINDEX_H
