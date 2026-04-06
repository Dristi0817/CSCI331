/**
 * @file SimpleIndex.cpp
 * @brief Implementation of SimpleIndex.
 *
 * @author Teagen Lee
 * @date Spring 2026
 */

#include "SimpleIndex.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Construct an empty SimpleIndex.
 */
SimpleIndex::SimpleIndex() {}

// ─────────────────────────────────────────────────────────────────────────────
// Build / Modify
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Insert or update the entry {highestKey → rbn}.
 *
 * If the rbn already has an entry under a different key, the old entry is
 * removed first (a block can only have one highest key at a time).
 *
 * @param highestKey Highest ZIP key string in the block.
 * @param rbn        Relative Block Number.
 */
void SimpleIndex::upsert(const string& highestKey, int rbn) {
    // Remove any existing entry that points to this same RBN
    removeByRBN(rbn);
    // Insert the new (or updated) entry
    index_[highestKey] = rbn;
}

/**
 * @brief Remove the entry whose value (RBN) equals rbn.
 *
 * Scans all entries (the index is small enough that linear scan is fine).
 *
 * @param rbn RBN to remove.
 */
void SimpleIndex::removeByRBN(int rbn) {
    for (auto it = index_.begin(); it != index_.end(); ++it) {
        if (it->second == rbn) {
            index_.erase(it);
            return; // each RBN appears at most once
        }
    }
}

/**
 * @brief Remove the entry whose key equals highestKey.
 *
 * @param highestKey Exact key string to remove.
 */
void SimpleIndex::removeByKey(const string& highestKey) {
    index_.erase(highestKey);
}

// ─────────────────────────────────────────────────────────────────────────────
// Search
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Find the RBN of the block that could contain targetKey.
 *
 * Uses lower_bound to find the first entry whose key >= targetKey.
 * That block is the only one that could contain the record.
 *
 * Example:
 *   Index: 01005→1, 10199→2, 56310→3
 *   Search for "05678": lower_bound("05678") → key "10199" → RBN 2
 *   Block 2's range is (01005, 10199], so 05678 could be in block 2.
 *
 * @param targetKey ZIP code string.
 * @return RBN, or -1 if targetKey is beyond all blocks.
 */
int SimpleIndex::findRBN(const string& targetKey) const {
    auto it = index_.lower_bound(targetKey);
    if (it == index_.end()) {
        return -1; // key is beyond the last block's highest key
    }
    return it->second;
}

// ─────────────────────────────────────────────────────────────────────────────
// I/O
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Write the index to a text file.
 *
 * File format:
 *   SIDX,1
 *   <key> <rbn>
 *   ...
 *
 * @param filename Path to write.
 * @return true on success.
 */
bool SimpleIndex::write(const string& filename) const {
    ofstream out(filename);
    if (!out) {
        cerr << "SimpleIndex::write — cannot open '" << filename << "'\n";
        return false;
    }

    out << "SIDX,1\n";
    for (const auto& entry : index_) {
        out << entry.first << " " << entry.second << "\n";
    }
    return static_cast<bool>(out);
}

/**
 * @brief Load the index from a text file.
 *
 * Expects the format written by write().
 *
 * @param filename Path to read.
 * @return true on success.
 */
bool SimpleIndex::read(const string& filename) {
    ifstream in(filename);
    if (!in) {
        cerr << "SimpleIndex::read — cannot open '" << filename << "'\n";
        return false;
    }

    index_.clear();

    // First line: tag
    string tag;
    if (!getline(in, tag)) return false;
    if (tag.substr(0, 4) != "SIDX") return false;

    string key;
    int rbn;
    while (in >> key >> rbn) {
        index_[key] = rbn;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return the number of index entries.
 */
int SimpleIndex::size() const {
    return static_cast<int>(index_.size());
}

/**
 * @brief Return all entries as a sorted vector of {key, rbn} pairs.
 */
vector<pair<string, int>> SimpleIndex::entries() const {
    vector<pair<string, int>> v(index_.begin(), index_.end());
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Display
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Print a readable dump of the index to cout.
 */
void SimpleIndex::dump() const {
    cout << "=== Simple Primary Key Index ===\n";
    cout << left << setw(12) << "HighestKey" << "  RBN\n";
    cout << string(20, '-') << "\n";
    for (const auto& entry : index_) {
        cout << setw(12) << entry.first << "  " << entry.second << "\n";
    }
    cout << "Total entries: " << index_.size() << "\n";
}
