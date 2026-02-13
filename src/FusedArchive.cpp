#include "FusedArchive.h"
#include "misc.h"

#include "miniz/miniz.h"

#include <cstdint>

bool FusedArchive::fused_ = false;
std::unordered_map<std::string, std::string> FusedArchive::files_;

std::string FusedArchive::normalizePath(const std::string& p)
{
    std::string r = p;
    for (char& c : r)
        if (c == '\\') c = '/';
    while (r.size() >= 2 && r[0] == '.' && r[1] == '/')
        r = r.substr(2);
    return r;
}

// Searches backwards from the end of the buffer for the zip End of Central
// Directory record, then uses it to compute where the appended zip begins.
// Returns the byte offset of the archive start, or npos on failure.
static size_t findZipStartOffset(const std::string& data)
{
    const size_t dataSize = data.size();
    if (dataSize < 22) // Minimum EOCD size
        return std::string::npos;

    const unsigned char* buf = reinterpret_cast<const unsigned char*>(data.data());

    // EOCD can sit at most (22 + 65535) bytes from the end (fixed header + max comment)
    size_t maxSearch = 22 + 65535;
    size_t searchFrom = (dataSize > maxSearch) ? (dataSize - maxSearch) : 0;

    size_t i = dataSize - 22;
    while (true)
    {
        // EOCD signature: PK\x05\x06
        if (buf[i] == 0x50 && buf[i + 1] == 0x4B &&
            buf[i + 2] == 0x05 && buf[i + 3] == 0x06)
        {
            // The comment length field must account for exactly the remaining bytes
            uint16_t commentLen = (uint16_t)buf[i + 20] | ((uint16_t)buf[i + 21] << 8);
            if (i + 22 + commentLen == dataSize)
            {
                // EOCD+12: central directory size (4 bytes LE)
                uint32_t cdSize = (uint32_t)buf[i + 12] | ((uint32_t)buf[i + 13] << 8) |
                    ((uint32_t)buf[i + 14] << 16) | ((uint32_t)buf[i + 15] << 24);
                // EOCD+16: central directory offset from start of *archive* (4 bytes LE)
                uint32_t cdOffset = (uint32_t)buf[i + 16] | ((uint32_t)buf[i + 17] << 8) |
                    ((uint32_t)buf[i + 18] << 16) | ((uint32_t)buf[i + 19] << 24);

                // In the buffer the central directory actually sits at (eocdPos - cdSize).
                // In the original zip it sat at cdOffset from the zip's own byte 0.
                //   archiveStart + cdOffset == eocdPos - cdSize
                //   archiveStart == (eocdPos - cdSize) - cdOffset
                if (i < (size_t)cdSize)
                    return std::string::npos;
                size_t actualCdPos = i - (size_t)cdSize;
                if (actualCdPos < (size_t)cdOffset)
                    return std::string::npos;

                return actualCdPos - (size_t)cdOffset;
            }
        }

        if (i == searchFrom)
            break;
        --i;
    }

    return std::string::npos;
}

bool FusedArchive::init(const fs::path& exePath)
{
    fused_ = false;
    files_.clear();

    std::string exeData;
    if (!readWholeFile(exePath, exeData))
        return false;

    // Locate the boundary between the EXE and the appended zip
    size_t archiveOffset = findZipStartOffset(exeData);
    if (archiveOffset == std::string::npos || archiveOffset >= exeData.size())
        return false;

    // Pass only the zip slice to miniz
    const char* zipData = exeData.data() + archiveOffset;
    size_t zipSize = exeData.size() - archiveOffset;

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, zipData, zipSize, 0))
        return false;

    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);

    for (mz_uint i = 0; i < numFiles; i++)
    {
        if (mz_zip_reader_is_file_a_directory(&zip, i))
            continue;

        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat))
            continue;

        size_t uncompSize = 0;
        void* fileData = mz_zip_reader_extract_to_heap(&zip, i, &uncompSize, 0);
        if (!fileData)
            continue;

        std::string filename = normalizePath(stat.m_filename);
        files_[filename] = std::string(static_cast<const char*>(fileData), uncompSize);
        mz_free(fileData);
    }

    mz_zip_reader_end(&zip);

    if (files_.empty())
        return false;

    fused_ = true;
    return true;
}

bool FusedArchive::isFused()
{
    return fused_;
}

bool FusedArchive::hasFile(const std::string& name)
{
    if (!fused_) return false;
    return files_.count(normalizePath(name)) > 0;
}

bool FusedArchive::readFile(const std::string& name, std::string& out)
{
    if (!fused_) return false;
    auto it = files_.find(normalizePath(name));
    if (it == files_.end()) return false;
    out = it->second;
    return true;
}

std::vector<std::string> FusedArchive::listFiles()
{
    std::vector<std::string> result;
    result.reserve(files_.size());
    for (const auto& [name, _] : files_)
        result.push_back(name);
    return result;
}

void FusedArchive::shutdown()
{
    files_.clear();
    fused_ = false;
}