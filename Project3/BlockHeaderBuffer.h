/**
 * @file BlockHeaderBuffer.h
 * @brief Header record class for the blocked sequence set data file.
 *
 * Extends the Project 2 header concept to include all metadata required
 * by the assignment for a blocked sequence set file:
 *
 *   - file structure type
 *   - version
 *   - header record size
 *   - bytes used for each record-size integer
 *   - size format type (ASCII)
 *   - block size
 *   - minimum block capacity percentage
 *   - index file name
 *   - index file schema
 *   - record count
 *   - block count
 *   - field count + per-field name/type
 *   - primary key field index
 *   - avail-list head RBN
 *   - sequence-set list head RBN
 *   - stale flag
 *
 * The header is stored in RBN 0 (the first block of the file) as a
 * length-indicated, comma-separated record padded with spaces to blockSize.
 *
 * @author Teagen Lee (primary contributor)
 * @author Ethan Jackson (miscellaneous small revisions)
 * @date April 2026
 */

#ifndef BLOCKHEADERBUFFER_H
#define BLOCKHEADERBUFFER_H

#include <string>
#include <vector>
#include <fstream>

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// Supporting structures
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @struct BFieldDescriptor
 * @brief Stores the name and type format for one field in the blocked file.
 */
struct BFieldDescriptor {
    string name;   ///< The field's name, e.g. "ZipCode"
    string format; ///< The field's data type, e.g. "int", "string", "double"
};

/**
 * @struct BlockFileHeader
 * @brief All header fields for a blocked sequence set data file.
 */
struct BlockFileHeader {
    bool staleFlag;      ///< True if index may be out of date.
    int version;         ///< File structure version (start at 1).
    int headerSizeBytes; ///< Byte length of the serialized header record.
    int recordSizeBytes; ///< Width of per-record length prefix (4 bytes).
    int blockSize;       ///< Bytes per block (default 512).
    int minBlockCapacityPct; ///< Minimum fill percentage (default 50).
    int blockCount;      ///< Total blocks allocated (including header block).
    int fieldCount;      ///< Number of fields per record.
    int primaryKeyIndex; ///< 0-based field index of the primary key.
    int availHeadRBN;    ///< Head of avail-list chain (RBN_NULL if empty).
    int seqSetHeadRBN; ///< Head of active SequenceSet chain (1st data block).
    long long recordCount; ///< Total ZIP records stored.
    string fileType;       ///< "ZipBlockedSeqSet".
    string sizeFormatType; ///< "ASCII".
    string indexFileName;  ///< Name of the simple index file (.sidx).
    string indexSchema; ///< Describes index format, e.g. "key:string;rbn:int".
    vector<BFieldDescriptor> fields; ///< One entry per field.
};

// ─────────────────────────────────────────────────────────────────────────────
// BlockHeaderBuffer
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class BlockHeaderBuffer
 * @brief Reads and writes the header record for a blocked sequence set file.
 *
 * The header occupies exactly one block (RBN 0).  It is stored as a
 * length-indicated CSV record padded with spaces to blockSize bytes.
 */
class BlockHeaderBuffer {
public:
    // ── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief Construct with a given block size.
     * @param blockSize Block size in bytes (must match the data file).
     */
    explicit BlockHeaderBuffer(int blockSize = 512);

    // ── Build ────────────────────────────────────────────────────────────────

    /**
     * @brief Populate the header with default values for the ZIP code file.
     *
     * Call this before writing a brand-new blocked sequence set file.
     *
     * @param indexFileName Name of the simple primary key index file.
     * @param blockSize Block size in bytes.
     * @param recordCount Total ZIP records to be stored (may be 0 initially).
     * @param blockCount Total blocks allocated (including RBN 0).
     * @param availHead RBN of first avail block (RBN_NULL if none).
     * @param seqHead RBN of first active data block.
     */
    void buildDefault(const string& indexFileName,
                      int    blockSize,
                      long long recordCount,
                      int    blockCount,
                      int    availHead,
                      int    seqHead);

    // ── I/O ──────────────────────────────────────────────────────────────────

    /**
     * @brief Write the header to RBN 0 of an open fstream.
     *
     * The header record is written as:
     *   [10-byte ASCII length][space][CSV text][padding to blockSize]
     *
     * @param fs Open fstream positioned at any location (seeked internally).
     * @return true on success.
     */
    bool write(fstream& fs) const;

    /**
     * @brief Read the header from RBN 0 of an open fstream.
     *
     * @param fs Open fstream.
     * @return true on success.
     */
    bool read(fstream& fs);

    // ── Accessors/Mutators ───────────────────────────────────────────────────

    const BlockFileHeader& getHeader() const;
    BlockFileHeader&       getHeader();

    void setRecordCount(long long count);
    void setBlockCount(int count);
    void setAvailHead(int rbn);
    void setSeqSetHead(int rbn);
    void setStale(bool s);

    // ── Display ──────────────────────────────────────────────────────────────

    /**
     * @brief Print the header contents to cout.
     */
    void print() const;

private:
    BlockFileHeader hdr_; ///< The header data.
    int blockSize_; ///< Block size for padding.

    /// Serialize hdr_ to a CSV string.
    string serialize() const;

    /// Deserialize a CSV string into hdr_.
    bool deserialize(const string& s);
};

#endif // BLOCKHEADERBUFFER_H
