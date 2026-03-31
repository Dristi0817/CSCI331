/**
 * @file ZipCodeBuffer.cpp
 * @brief Implementation of the ZipCodeBuffer class
 *
 * This version supports CSV files even when the columns are re-ordered.
 * It reads the header row first, remembers the needed column positions,
 * and then parses data rows using those positions.
 */

#include "ZipCodeBuffer.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <stdexcept>

using namespace std;

/* ============================================================================
 * ZipCodeRecord Implementation
 * ============================================================================
 */

/**
 * @brief Default constructor
 */
ZipCodeRecord::ZipCodeRecord()
    : zipCode(0), placeName(""), state(""), county(""),
      latitude(0.0), longitude(0.0) {
}

/**
 * @brief Parameterized constructor
 */
ZipCodeRecord::ZipCodeRecord(int zip, const string& place, const string& st,
                             const string& cnty, double lat, double lon)
    : zipCode(zip), placeName(place), state(st), county(cnty),
      latitude(lat), longitude(lon) {
}

/**
 * @brief Serialize this record to a comma-separated string.
 *
 * Field order matches the original CSV:
 *   ZipCode,PlaceName,State,County,Lat,Long
 *
 * @return CSV text with no trailing newline
 */
string ZipCodeRecord::toCSV() const {
    ostringstream ss;
    // Zero-pad ZIP to 5 digits so string sort == numeric sort
    ss << setw(5) << setfill('0') << zipCode << ","
       << placeName << ","
       << state << ","
       << county << ","
       << latitude << ","
       << longitude;
    return ss.str();
}

/**
 * @brief Populate fields from a comma-separated string.
 *
 * Expected order: ZipCode,PlaceName,State,County,Lat,Long
 *
 * @param csv CSV text (e.g. from toCSV())
 * @return true if all six fields were parsed successfully
 */
bool ZipCodeRecord::fromCSV(const string& csv) {
    // Split on commas (simple — no quoted fields in zip data)
    vector<string> fields;
    string cur;
    bool inQuotes = false;
    for (char c : csv) {
        if (c == '"') { inQuotes = !inQuotes; }
        else if (c == ',' && !inQuotes) { fields.push_back(cur); cur.clear(); }
        else { cur += c; }
    }
    fields.push_back(cur);

    if (fields.size() < 6) return false;

    // Trim helper lambda
    auto trim = [](const string& s) -> string {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    };

    try {
        zipCode   = stoi(trim(fields[0]));
        placeName = trim(fields[1]);
        state     = trim(fields[2]);
        county    = trim(fields[3]);
        latitude  = stod(trim(fields[4]));
        longitude = stod(trim(fields[5]));
        return true;
    } catch (...) {
        return false;
    }
}

/* ============================================================================
 * ZipCodeBuffer Implementation
 * ============================================================================
 */

/**
 * @brief Default constructor
 *
 * Initializes all member variables to safe starting values.
 */
ZipCodeBuffer::ZipCodeBuffer()
    : filename(""), headerSkipped(false), recordCount(0),
      colZip(-1), colPlace(-1), colState(-1),
      colCounty(-1), colLat(-1), colLong(-1) {
}

/**
 * @brief Constructor that opens a file immediately
 */
ZipCodeBuffer::ZipCodeBuffer(const string& csvFilename)
    : filename(""), headerSkipped(false), recordCount(0),
      colZip(-1), colPlace(-1), colState(-1),
      colCounty(-1), colLat(-1), colLong(-1) {
    open(csvFilename);
}

/**
 * @brief Destructor
 */
ZipCodeBuffer::~ZipCodeBuffer() {
    close();
}

/**
 * @brief Opens a CSV file and processes its header row
 *
 * This function:
 * 1) closes any previously open file
 * 2) opens the requested file
 * 3) reads the first line (header row)
 * 4) finds the positions of ZIP, place, state, county, latitude, longitude
 *
 * @param csvFilename CSV file path
 * @return true on success, false on failure
 */
bool ZipCodeBuffer::open(const string& csvFilename) {
    close();

    filename = csvFilename;
    fileStream.open(filename);

    if (!fileStream.is_open()) {
        return false;
    }

    string headerLine;
    if (!getline(fileStream, headerLine)) {
        close();
        return false;
    }

    if (!parseHeader(headerLine)) {
        close();
        return false;
    }

    headerSkipped = true;
    recordCount = 0;
    return true;
}

/**
 * @brief Closes the file and resets internal state
 */
void ZipCodeBuffer::close() {
    if (fileStream.is_open()) {
        fileStream.close();
    }

    filename = "";
    headerSkipped = false;
    recordCount = 0;

    colZip = -1;
    colPlace = -1;
    colState = -1;
    colCounty = -1;
    colLat = -1;
    colLong = -1;
}

/**
 * @brief Checks whether file is open
 */
bool ZipCodeBuffer::isOpen() const {
    return fileStream.is_open();
}

/**
 * @brief Reads one data record from the CSV file
 *
 * @param record Output record
 * @return true if record is read successfully, false on EOF or parse failure
 */
bool ZipCodeBuffer::readRecord(ZipCodeRecord& record) {
    if (!fileStream.is_open()) {
        return false;
    }

    string line;
    if (getline(fileStream, line)) {
        if (line.empty() || trim(line).empty()) {
            return readRecord(record); // skip blank lines
        }

        if (parseLine(line, record)) {
            recordCount++;
            return true;
        } else {
            return false;
        }
    }

    return false;
}

/**
 * @brief Reads all records into a vector
 *
 * This is kept for compatibility, but Project 2 should prefer reading
 * one record at a time.
 */
vector<ZipCodeRecord> ZipCodeBuffer::gatherAllRecords() {
    vector<ZipCodeRecord> records;

    reset();

    ZipCodeRecord record;
    while (readRecord(record)) {
        records.push_back(record);
    }

    reset();
    return records;
}

/**
 * @brief Resets file back to beginning and processes header row again
 *
 * @return true if reset succeeds
 */
bool ZipCodeBuffer::reset() {
    if (!fileStream.is_open()) {
        return false;
    }

    fileStream.clear();
    fileStream.seekg(0, ios::beg);

    string headerLine;
    if (!getline(fileStream, headerLine)) {
        return false;
    }

    if (!parseHeader(headerLine)) {
        return false;
    }

    recordCount = 0;
    headerSkipped = true;
    return true;
}

/**
 * @brief Returns how many records have been read
 */
long ZipCodeBuffer::getRecordCount() const {
    return recordCount;
}

/**
 * @brief Returns current filename
 */
string ZipCodeBuffer::getFilename() const {
    return filename;
}

/**
 * @brief Parses the CSV header row and finds column positions
 *
 * This function makes column re-ordering possible.
 *
 * For example, if the header is:
 * PlaceName,ZipCode,County,State,Long,Lat
 *
 * then this function will correctly discover:
 * - colPlace = 0
 * - colZip = 1
 * - colCounty = 2
 * - colState = 3
 * - colLong = 4
 * - colLat = 5
 *
 * @param headerLine Header row from the CSV file
 * @return true if all needed columns are found
 */
bool ZipCodeBuffer::parseHeader(const string& headerLine) {
    vector<string> fields = splitCSV(headerLine);

    colZip = -1;
    colPlace = -1;
    colState = -1;
    colCounty = -1;
    colLat = -1;
    colLong = -1;

    for (size_t i = 0; i < fields.size(); i++) {
        string name = trim(fields[i]);

        // Make comparison easier by converting to lowercase
        string lowerName = name;
        transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                  [](unsigned char c) { return static_cast<char>(tolower(c)); });

        if (lowerName == "zipcode" || lowerName == "zip" || lowerName == "zip_code") {
            colZip = static_cast<int>(i);
        } else if (lowerName == "placename" || lowerName == "place" || lowerName == "city") {
            colPlace = static_cast<int>(i);
        } else if (lowerName == "state") {
            colState = static_cast<int>(i);
        } else if (lowerName == "county") {
            colCounty = static_cast<int>(i);
        } else if (lowerName == "lat" || lowerName == "latitude") {
            colLat = static_cast<int>(i);
        } else if (lowerName == "long" || lowerName == "longitude" || lowerName == "lon") {
            colLong = static_cast<int>(i);
        }
    }

    return (colZip != -1 &&
            colPlace != -1 &&
            colState != -1 &&
            colCounty != -1 &&
            colLat != -1 &&
            colLong != -1);
}

/**
 * @brief Parses one CSV data line into a ZipCodeRecord
 *
 * This version does NOT assume fixed column order.
 * Instead, it uses the column positions discovered from the header row.
 *
 * @param line One CSV data row
 * @param record Output record
 * @return true if parsing succeeds
 */
bool ZipCodeBuffer::parseLine(const string& line, ZipCodeRecord& record) {
    vector<string> fields = splitCSV(line);

    int maxNeeded = max({colZip, colPlace, colState, colCounty, colLat, colLong});
    if (static_cast<int>(fields.size()) <= maxNeeded) {
        return false;
    }

    try {
        record.zipCode = stoi(trim(fields[colZip]));
        record.placeName = trim(fields[colPlace]);
        record.state = trim(fields[colState]);
        record.county = trim(fields[colCounty]);
        record.latitude = stod(trim(fields[colLat]));
        record.longitude = stod(trim(fields[colLong]));

        return true;
    } catch (const exception&) {
        return false;
    }
}

/**
 * @brief Splits a CSV line into fields
 *
 * Handles:
 * - normal commas
 * - quoted text
 * - commas inside quoted text
 *
 * @param line One CSV line
 * @return Vector of fields
 */
vector<string> ZipCodeBuffer::splitCSV(const string& line) {
    vector<string> fields;
    string currentField;
    bool inQuotes = false;

    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];

        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            fields.push_back(currentField);
            currentField.clear();
        } else {
            currentField += c;
        }
    }

    fields.push_back(currentField);
    return fields;
}

/**
 * @brief Trims whitespace from both ends of a string
 *
 * @param str Input string
 * @return Trimmed string
 */
string ZipCodeBuffer::trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == string::npos) {
        return "";
    }

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}