// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <clarisma/net/HttpResponse.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/io/IOException.h>
#include <zlib.h>
#include <clarisma/zip/ZipException.h>

namespace clarisma {

int HttpResponse::status() const
{
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);

    // Query the status code from the headers
    if (!WinHttpQueryHeaders(hRequest_,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
        &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
    {
        // TODO: Check for errors other than header not present
        // std::cerr << "Error querying status code: " << GetLastError() << std::endl;
        return 0;
    }
    return static_cast<int>(statusCode);
}

size_t HttpResponse::contentLength() const
{
    wchar_t buf[32];
    DWORD bufSize = sizeof(buf);
    if (!WinHttpQueryHeaders(hRequest_,
        WINHTTP_QUERY_CONTENT_LENGTH,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &buf,
        &bufSize,
        WINHTTP_NO_HEADER_INDEX))
    {
        // TODO: Check for errors other than header not present
        // std::cerr << "Failed to get ContentLength" << std::endl;
        return 0;
    }
    return std::stoull(buf);
}

size_t HttpResponse::read(void* buf, size_t size)
{
    std::byte* p = reinterpret_cast<std::byte*>(buf);
    DWORD bytesRead = 0;
    DWORD totalBytesToRead = static_cast<DWORD>(size);
    DWORD totalBytesRead = 0;

    // Keep reading until all requested bytes are read or an error occurs
    while (totalBytesRead < totalBytesToRead)
    {
        if (!WinHttpReadData(hRequest_, p, totalBytesToRead - totalBytesRead, &bytesRead))
        {
            throw HttpException(GetLastError());
        }

        // If no bytes were read, we've reached the end of the response
        if (bytesRead == 0)
        {
            break;
        }
        totalBytesRead += bytesRead;
        p += bytesRead;
    }
    return totalBytesRead;
}


void HttpResponse::read(std::vector<std::byte>& data)
{
    data.clear();
    size_t size = contentLength();
    if (size)   [[likely]]
    {
        data.resize(size);
        read(data.data(), size);
    }
    else
    {
        DWORD bytesAvailable = 0;
        DWORD bytesRead = 0;
        do
        {
            // Query how much data is available
            if (!WinHttpQueryDataAvailable(hRequest_, &bytesAvailable))
            {
                throw HttpException(GetLastError());
            }

            if (bytesAvailable == 0) break;

            // Allocate buffer for available data
            size_t currentSize = data.size();
            data.resize(currentSize + bytesAvailable);

            // Read data directly into the vector's buffer
            if (!WinHttpReadData(hRequest_, data.data() + currentSize,
                bytesAvailable, &bytesRead))
            {
                throw HttpException(GetLastError());
            }

            if (bytesRead == 0)
            {
                throw HttpException("Unexpected end of data stream");
            }

            // Resize to the actual amount of data read in case of a partial read
            data.resize(currentSize + bytesRead);

        }
        while (bytesAvailable > 0);
    }
}

void HttpResponse::readUnzippedGzip(std::vector<std::byte>& data)
{
    constexpr int BUFFER_SIZE = 16 * 1024;
    unsigned char inBuffer[BUFFER_SIZE];
    unsigned char outBuffer[BUFFER_SIZE];

    // Initialize zlib for gzip decompression.
    // 16 + MAX_WBITS instructs zlib to expect a gzip header.
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    int ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (ret != Z_OK)
    {
        throw ZipException(ret);
    }

    DWORD bytesRead;
    do
    {
        bytesRead = 0;
        if (!WinHttpReadData(hRequest_, inBuffer, BUFFER_SIZE, &bytesRead))
        {
            inflateEnd(&strm);
            throw HttpException(GetLastError());
        }

        strm.avail_in = bytesRead;
        strm.next_in  = inBuffer;

        do
        {
            strm.avail_out = BUFFER_SIZE;
            strm.next_out  = outBuffer;
            ret = inflate(&strm, bytesRead ? Z_NO_FLUSH : Z_FINISH);
            if (ret != Z_OK && ret != Z_STREAM_END)
            {
                inflateEnd(&strm);
                throw ZipException(ret);
            }

            size_t have = BUFFER_SIZE - strm.avail_out;
            auto buf = reinterpret_cast<const std::byte*>(outBuffer);
            data.insert(data.end(), buf, buf + have);
        }
        while (strm.avail_out == 0);
    }
    while (bytesRead);
    ret = inflateEnd(&strm);
    if (ret != Z_OK) throw ZipException(ret);
}

void HttpResponse::close()
{
    if(hRequest_)
    {
        WinHttpCloseHandle(hRequest_);
        hRequest_ = nullptr;
    }
}

} // namespace clarisma