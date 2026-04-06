/**
 * @file BlockBuffer.h
 * @brief Block buffer class for the blocked sequence set file.
 *
 * A "blocked sequence set" stores length-indicated, comma-separated ZIP code
 * records packed into fixed-size blocks on disk. Each block is exactly
 * BLOCK_SIZE bytes. Blocks are addressed by their Relative Block Number (RBN),
 * where RBN 0 is the header block (the data file header record) and RBN 1 is
 * the first data/avail block.
 *
 * ── Block layout (active block) ────────────────────────────────────────────
 *  Bytes  0–3   : record count   (4-byte ASCII integer, space-padded, e.g. "   3")
 *  Bytes  4–11  : predecessor RBN (8-byte ASCII integer, e.g. "00000000")
 *  Bytes 12–19  : successor RBN   (8-byte ASCII integer, e.g. "00000002")
 *  Bytes 20–end : packed records, each prefixed with a 4-byte ASCII length
 *
 * ── Block layout (avail block) ─────────────────────────────────────────────
 *  Bytes  0–3   : "   0"  (record count == 0)
 *  Bytes  4–11  : "00000000" (predecessor unused)
 *  Bytes 12–19  : next-avail RBN  (8-byte ASCII, or "00000000" for end-of-list)
 *  Bytes 20–end : blanks
 *
 * ── RBN encoding ────────────────────────────────────────────────────────────
 *  RBN -1  (stored as "FFFFFFFF") means "no link" (NULL)
 *  RBN  0  is reserved for the file header block.
 *  Data/avail blocks start at RBN 1.
 *
 * @author Teagen Lee
 * @date Spring 2026
 */

#ifndef BLOCKBUFFER_H
#define BLOCKBUFFER_H

#include <string>
#include <vector>
#include <fstream>

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

/// Default block size in bytes (assignment default: 512).
static const int DEFAULT_BLOCK_SIZE = 512;

/// Sentinel value meaning "no RBN link" (stored as "FFFFFFFF" in the file).
static const int RBN_NULL = -1;

/// Byte width of the record-count field in a block header.
static const int BLOCK_COUNT_WIDTH = 4;

/// Byte width of each RBN link field in a block header.
static const int BLOCK_RBN_WIDTH = 8;

/// Byte width of the per-record length prefix inside a block.
static const int RECORD_LEN_WIDTH = 4;

/// Total bytes consumed by the block's own metadata header.
static const int BLOCK_META_SIZE = BLOCK_COUNT_WIDTH + 2 * BLOCK_RBN_WIDTH;
// = 4 + 8 + 8 = 20 bytes

// ─────────────────────────────────────────────────────────────────────────────
// BlockBuffer
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class BlockBuffer
 * @brief Reads and writes fixed-size blocks in a blocked sequence set file.
 *
 * Responsibilities:
 *  - Pack a set of CSV record strings + metadata into a fixed-size block buffer.
 *  - Unpack records from a block buffer back into strings.
 *  - Read/write a block from/to disk by Relative Block Number (RBN).
 *  - Manage per-block metadata: predecessor RBN, successor RBN, record count.
 *
 * This class does NOT know about ZIP codes specifically — it works with
 * arbitrary CSV strings. The caller (ZipCodeRecord / main) converts strings
 * to typed objects.
 */
class BlockBuffer {
public:
    // ── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief Construct a BlockBuffer for the given block size.
     * @param blockSize Total bytes per block (default DEFAULT_BLOCK_SIZE).
     */
    explicit BlockBuffer(int blockSize = DEFAULT_BLOCK_SIZE);

    // ── Metadata accessors ───────────────────────────────────────────────────

    int  getBlockSize()  const;  ///< Return the configured block size in bytes.
    int  getRecordCount() const; ///< Return number of records currently in buffer.
    int  getPredRBN()    const;  ///< Predecessor block RBN (RBN_NULL if none).
    int  getSuccRBN()    const;  ///< Successor block RBN   (RBN_NULL if none).
    bool isAvail()       const;  ///< True if this is an avail-list block (count==0).

    void setPredRBN(int rbn);    ///< Set predecessor link.
    void setSuccRBN(int rbn);    ///< Set successor link.

    // ── Record access ────────────────────────────────────────────────────────

    /**
     * @brief Get all record strings currently in the buffer.
     * @return Const reference to the internal record vector.
     */
    const vector<string>& getRecords() const;

    /**
     * @brief Add a record string to the buffer.
     *
     * Returns false (and does not add) if there is not enough remaining
     * capacity for the record + its 4-byte length prefix.
     *
     * @param csvRecord CSV text for one record (no newline).
     * @return true if added, false if block is full.
     */
    bool addRecord(const string& csvRecord);

    /**
     * @brief How many bytes are still free in the block (excluding metadata).
     * @return Free bytes available for additional records.
     */
    int freeBytes() const;

    /**
     * @brief Clear all records from the buffer (keeps metadata/blockSize).
     */
    void clear();

    // ── Serialisation ────────────────────────────────────────────────────────

    /**
     * @brief Pack the buffer into a fixed-size byte array ready to write.
     *
     * Layout:
     *  [4-byte count][8-byte predRBN][8-byte succRBN]
     *  [4-byte len][record][4-byte len][record]...
     *  [padding with spaces to blockSize]
     *
     * @return String of exactly blockSize bytes.
     */
    string pack() const;

    /**
     * @brief Unpack a fixed-size block from a raw byte string.
     *
     * Reads the metadata header and all length-prefixed record strings.
     *
     * @param raw Raw byte string of exactly blockSize bytes.
     * @return true on success, false on format error.
     */
    bool unpack(const string& raw);

    // ── Disk I/O ─────────────────────────────────────────────────────────────

    /**
     * @brief Write this buffer to a specific RBN in the file.
     *
     * The file must be open for read/write (fstream).
     * Seeks to the correct byte offset and writes exactly blockSize bytes.
     *
     * @param fs   Open fstream (binary or text, seekable).
     * @param rbn  Relative Block Number to write (RBN 0 = header block).
     * @return true on success.
     */
    bool writeBlock(fstream& fs, int rbn) const;

    /**
     * @brief Read a specific RBN from the file into this buffer.
     *
     * Seeks to the correct byte offset and reads exactly blockSize bytes,
     * then calls unpack().
     *
     * @param fs   Open fstream.
     * @param rbn  Relative Block Number to read.
     * @return true on success.
     */
    bool readBlock(fstream& fs, int rbn);

    /**
     * @brief Compute the file byte offset for a given RBN.
     *
     * Offset = rbn * blockSize.
     * RBN 0 is reserved for the file header (also exactly one block).
     *
     * @param rbn Relative Block Number.
     * @return Byte offset from start of file.
     */
    long long offsetForRBN(int rbn) const;

    // ── Avail-block helpers ──────────────────────────────────────────────────

    /**
     * @brief Mark this buffer as an avail-list block.
     *
     * Sets record count to 0, predecessor to RBN_NULL, fills payload with spaces.
     * The successor RBN is set to nextAvail (RBN_NULL means end-of-list).
     *
     * @param nextAvail RBN of next avail block, or RBN_NULL.
     */
    void makeAvail(int nextAvail = RBN_NULL);

    // ── Dump helpers ─────────────────────────────────────────────────────────

    /**
     * @brief Return the highest (largest) key string currently in the buffer.
     *
     * Keys are the first CSV field (ZIP code).  Assumes records are sorted.
     * Returns "" if buffer is empty.
     */
    string highestKey() const;

    /**
     * @brief Return the lowest key string currently in the buffer.
     *
     * Returns "" if buffer is empty.
     */
    string lowestKey() const;

private:
    int            blockSize_;   ///< Block size in bytes.
    int            predRBN_;     ///< Predecessor RBN link.
    int            succRBN_;     ///< Successor RBN link.
    vector<string> records_;     ///< Record strings (CSV text, no length prefix).

    /// Encode an integer as a fixed-width zero-padded ASCII string.
    string encodeInt(int value, int width) const;

    /// Decode a fixed-width ASCII integer string.
    /// Returns RBN_NULL if the string equals the NULL sentinel ("FFFFFFFF" etc.).
    int decodeInt(const string& s) const;

    /// Maximum payload bytes = blockSize_ - BLOCK_META_SIZE.
    int maxPayload() const;

    /// Bytes used by records currently in records_ (including length prefixes).
    int usedPayload() const;
};

#endif // BLOCKBUFFER_H
