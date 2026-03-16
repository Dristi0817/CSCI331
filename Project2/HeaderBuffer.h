/**
 * @file HeaderBuffer.h
 * @brief defines the HeaderBuffer class and the FileHeader struct.
 * @author Dristi Barnwal (primary contributor)
 * @author Ethan Jackson (functional revisions and additional comments)
 * @author Marcus Julius, Teagen Lee, Natoli Mayu (reviewers)
 * @date March 2026 
 */
#ifndef HEADERBUFFER_H
#define HEADERBUFFER_H

#include <string>
#include <vector>
#include <fstream>

using namespace std;

/**
 * @struct FileHeader
 * @brief All header record fields required by the assignment
 */
struct FileHeader {
    /**
     * True if the record length field includes its own size.
     */
    bool sizeIncludesItself;
    /**
     * True if the index may be out of date.
     */
    bool staleIndex;
    /**
     * The encoding of the record length indicator ('A' = ASCII, 'b' = binary).
     */
    char sizeFormatType;
    /**
     * The size of the record length indicator field in bytes.
     */
    unsigned char sizeOfSizes;
    /**
     * The version number for this file header structure (currently 3).
     */
    short version;
    /**
     * The number of fields in each record.
     */
    int fieldCount;
    /**
     * Zero-based index of the primary key column.
     */
    int primaryKeyFieldIndex;
    /**
     * The size of this header record in bytes.
     */
    int headerSizeBytes;
    /**
     * The total number of data records.
     */
    long recordCount;
    /**
     * The name of the file type, e.g. "ZipLenFile".
     */
    string fileType;
    /**
     * The name of the associated .idx file.
     */
    string indexFileName;
    /**
     * The names of every column, e.g. "PlaceName"
     */
    vector<string> fieldNames;
    /**
     * The data types of every column, e.g. "int"
     */
    vector<string> fieldTypes;
};

/**
 * @class HeaderBuffer
 * @brief Reads and writes the header record for a length-indicated file.
 */
class HeaderBuffer {
public:
    /// Default constructor
    HeaderBuffer();

    /// Build a default header for the ZIP code file
    void buildDefault(const string& indexFileName, long recordCount);

    /// Write header to an open output stream
    bool write(ofstream& out);

    /// Read header from an open input stream
    bool read(ifstream& in);

    /// Get the loaded header data
    const FileHeader& getHeader() const;

    /// Print header contents to cout
    void print() const;

private:
    FileHeader header_;
    string serialize() const;
    bool deserialize(const string& s);
};

#endif