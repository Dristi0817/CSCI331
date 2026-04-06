/**
 * @file ZipCodeBuffer.h
 * @brief Record buffer class for reading ZIP code records from CSV files.
 *
 * Carried forward from Project 2 with no functional changes.
 * In Project 3 this class serves as the "record buffer" layer:
 *   BlockBuffer  → unpacks a raw record string from a block
 *   ZipCodeBuffer (record buffer role) → parses that string into a ZipCodeRecord
 *
 * @author Teagen Lee
 * @date Spring 2026
 */

#ifndef ZIPCODEBUFFER_H
#define ZIPCODEBUFFER_H

#include <string>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

// ============================================================================
// ZipCodeRecord
// ============================================================================

/**
 * @struct ZipCodeRecord
 * @brief Stores one complete ZIP code record.
 */
struct ZipCodeRecord {
    int    zipCode;    ///< 5-digit ZIP code (primary key)
    string placeName;  ///< City / place name
    string state;      ///< Two-letter state abbreviation
    string county;     ///< County name
    double latitude;   ///< Latitude coordinate
    double longitude;  ///< Longitude coordinate

    ZipCodeRecord();
    ZipCodeRecord(int zip, const string& place, const string& st,
                  const string& cnty, double lat, double lon);

    /**
     * @brief Serialize this record to a comma-separated string.
     * @return CSV text (no newline), e.g. "56301,St Cloud,MN,Stearns,45.5579,-94.1632"
     */
    string toCSV() const;

    /**
     * @brief Populate this record from a comma-separated string.
     * @param csv CSV text produced by toCSV()
     * @return true on success
     */
    bool fromCSV(const string& csv);
};

// ============================================================================
// ZipCodeBuffer
// ============================================================================

/**
 * @class ZipCodeBuffer
 * @brief Reads ZIP code records one at a time from a CSV file.
 *
 * In Project 3 this is also used as the "record buffer":
 * given a raw CSV string (extracted from a block by BlockBuffer),
 * call fromCSV() on a ZipCodeRecord to unpack its fields.
 */
class ZipCodeBuffer {
public:
    ZipCodeBuffer();
    explicit ZipCodeBuffer(const string& csvFilename);
    ~ZipCodeBuffer();

    bool open(const string& csvFilename);
    void close();
    bool isOpen()   const;
    bool readRecord(ZipCodeRecord& record);
    bool reset();
    long getRecordCount() const;
    string getFilename()  const;

    vector<ZipCodeRecord> gatherAllRecords();

private:
    ifstream fileStream;
    string   filename;
    bool     headerSkipped;
    long     recordCount;

    int colZip, colPlace, colState, colCounty, colLat, colLong;

    bool parseLine(const string& line, ZipCodeRecord& record);
    bool parseHeader(const string& headerLine);
    vector<string> splitCSV(const string& line);
    string trim(const string& str);
};

#endif // ZIPCODEBUFFER_H
