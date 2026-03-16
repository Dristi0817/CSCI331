/**
 * @file HeaderBuffer.cpp
 * @brief implementation of the HeaderBuffer class
 * @author Ethan Jackson (refactoring and documentation)
 * @author Teagan Lee (original author)
 * @author Natoli Mayu (documentation)
 * @author Dristi Barnwal, Marcus Julius (reviewers)
 * @date March 2026
 */
#include "HeaderBuffer.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cctype>

using namespace std;

/**
 * @brief The default constructor for HeaderBuffer
 * 
 * Does not initialize any fields.
 *
 * @see buildDefault
 */
HeaderBuffer::HeaderBuffer() {}

/**
 * @brief Sets the fields of this HeaderBuffer to suitable defualt values
 * @param indexFileName the name of the index file
 * @param recordCount the total number of records in the file
 */
void HeaderBuffer::buildDefault(const string& indexFileName, long recordCount) {
    header_.fileType = "ZipLenFile";
    header_.version = 3;
    header_.sizeFormatType = 'A';
    header_.sizeOfSizes = 10; //10 digits in ASCII
    header_.sizeIncludesItself = true; //based on how header size is calculated
    header_.indexFileName = indexFileName;
    header_.recordCount = recordCount;
    header_.primaryKeyFieldIndex = 0;
    header_.staleIndex = false;
    header_.fieldNames = {"ZipCode", "PlaceName", "State", "County", //continued
                          "Longitude", "Latitude"};
    header_.fieldTypes = {"int","string","string","string","double","double"}
    header_.fieldCount = (int)header_.fieldNames.size();
    header_.headerSizeBytes = (int)serialize().size();
}

const FileHeader& HeaderBuffer::getHeader() const {
    return header_;
}

/**
 * @brief Writes this HeaderBuffer to an output file
 *
 * @param out the output file stream
 * @warning binary format is experimental and has not yet been tested.
 */
bool HeaderBuffer::write(ofstream& out) {
    string text = serialize();
    int len = header_.headerSizeBytes;
    if (header_.sizeFormatType == 'A')
        out << setw(header_.sizeOfSizes) << setfill('0') << len;
    else if (header_.sizeFormatType == 'b')
        out.write(reinterpret_cast<char*>(&len), sizeof len);
    out << ' ' << text << '\n'; 
    return out.good();
}

/**
 * @brief Reads header data from an input file
 *
 * @param in the input file stream
 * @return true if the read is successful
 *
 * @warning binary format is experimental and has not yet been tested.
 */
bool HeaderBuffer::read(ifstream& in) {
    string text;
    size_t len;
    char buf[header_.sizeOfSizes];

    if (!in.read(buf, header_.sizeOfSizes))
        return false;

    if (header_.sizeFormatType != 'b') {
        for (int i = 0; i < header_.sizeOfSizes; i++)
            if (!isdigit((unsigned char)buf[i]))
                return false;

        char sp;
        if (!in.get(sp) || sp != ' ')
            return false;

        len = stoui(string(buf, header_.sizeOfSizes));
    } else {
        if (header_.sizeOfSizes > sizeof(len)) 
            return false; //sizeOfSizes has more bytes than len can hold
        if (!in.read(reinterpret_cast<char*>(&len), sizeof len)
            return false;
    }
    text.resize(len);
    if (!in.read(&text[0], len))
        return false;
    if (in.peek() == '\r')
        in.get();
    if (in.peek() == '\n')
        in.get();
    return deserialize(text);
}

void HeaderBuffer::print() const {
    cout << "File type:\t    " << header_.fileType << "\n";
    cout << "Version:\t    " << header_.version << "\n";
    cout << "Header size:\t    " << header_.headerSizeBytes << " bytes\n";
    cout << "Size format:\t    " << header_.sizeFormatType << "\n";
    cout << "Size of size field: " << (int)(header_.sizeOfSizes) << " bytes\n";
    cout<<"Size counts itself: "<<(header_.sizeIncludesItself?"yes":"no")<<"\n";
    cout << "Index file:\t    " << header_.indexFileName << "\n";
    cout << "Record count:\t    " << header_.recordCount << "\n";
    cout << "Field count:\t    " << header_.fieldCount << "\n";
    cout << "Primary key field:  " << header_.primaryKeyFieldIndex << "\n";
    cout << "Stale index:\t    " << (header_.staleIndex ? "yes" : "no") << "\n";
    for (int i = 0; i < (int)header_.fieldCount; i++)
        cout << "Field[" << i << "]:\t    " << header_.fieldNames[i];
             << " (" << header_.fieldTypes[i] << ")\n";
}

/**
 * Helper function for HeaderBuffer::write(). Converts HeaderBuffer into a 
 * comma-delimited line of text.
 */
string HeaderBuffer::serialize() const {
    ostringstream ss;
    ss << "HDR"
       << "," << header_.fileType
       << "," << header_.version
       << "," << header_.sizeFormatType
       << "," << static_cast<int>(header_.sizeOfSizes)
       << "," << (header_.sizeIncludesItself ? 'y' : 'n')
       << "," << header_.indexFileName
       << "," << header_.recordCount
       << "," << header_.fieldCount
       << "," << header_.primaryKeyFieldIndex
       << "," << (header_.staleIndex ? 'y' : 'n');
    for (const string& f : header_.fieldNames)
        ss << "," << f;
    for (const string& f : header_.fieldTypes)
        ss << "," << f;
    return ss.str();
}

/**
 * Helper function for HeaderBuffer::read(). Converts a comma-delimited line of 
 * text produced by the serialize function back into HeaderBuffer data.
 * 
 * @return true if the deserialization was successful
 */
bool HeaderBuffer::deserialize(const string& s) {
    istringstream ss(s);
    string tok;
    vector<string> parts;
    while (getline(ss, tok, ','))
        parts.push_back(tok);
    if (parts.size() < 13 || (parts.size() % 2 == 0) || parts[0] != "HDR")
        return false;
    header_.fileType             = parts[1];
    header_.version              = static_cast<short>(stoi(parts[2]));
    header_.sizeFormatType       = parts[3].at(0);
    header_.sizeOfSizes          = static_cast<unsigned char>(stoi(parts[4]));
    header_.sizeIncludesItself   = (parts[5] == 'y');
    header_.indexFileName        = parts[6];
    header_.recordCount          = stol(parts[7]);
    header_.fieldCount           = static_cast<short>(stoi(parts[8]));
    header_.primaryKeyFieldIndex = stoi(parts[9]);
    header_.staleIndex           = (parts[10] == 'y');
    header_.fieldNames.clear();
    header_.fieldTypes.clear();
    if (header_.fieldCount * 2 + 11 != parts.size())
        return false;
    int typesStart = header_.fieldCount + 11;
    for (int i = 11; i < typesStart; i++)
        header_.fieldNames.push_back(parts[i]);
    for (int i = typesStart; i < (int)parts.size(); i++)
        header_.fieldTypes.push_back(parts[i]);
    return true;
}