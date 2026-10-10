// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <clarisma/io/IOException.h>
#include <clarisma/net/HttpClient.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/net/UrlView.h>
#include <clarisma/util/log.h>
#include <clarisma/zip/ZipException.h>

namespace clarisma {

HttpClient::HttpClient(std::string_view url, std::string_view userAgent) :
    urlView_(url),
    origin_(urlView_.origin()),
    client_(origin_)
{
    client_.set_keep_alive(true);
    client_.set_default_headers({{ "User-Agent", std::string(userAgent) }});
}

HttpClient::~HttpClient()
{
}


void HttpClient::open()
{
    // no-op on Linux, opened lazily on demand
}

/*
HttpResponse HttpClient::get(const char* url)
{
    std::shared_ptr<httplib::Response> response;
    if(useSSL_)
    {
    	response = client_.ssl.Get(url);
    }
    else
    {
    	response = client_.http.Get(url);
    }
    if(!response)
    {
    	// TODO: error
    }
    return HttpResponse(response);
}
 */

void HttpClient::close()
{
    // do nothing
}


void HttpClient::get(const char* url, std::vector<std::byte>& data)
{
    data.clear();

    int status = 0;
    auto result = client_.Get(url,
        [&](const httplib::Response& response)
        {
            status = response.status;
            return status == httplib::StatusCode::OK_200;
        },
        [&](const char* bytes, size_t size)
        {
            const auto* begin =
                reinterpret_cast<const std::byte*>(bytes);
            data.insert(data.end(), begin, begin + size);
            return true;
        });

    if (status != 0 && status != httplib::StatusCode::OK_200)
    {
        throw HttpException("Server response %d", status);
    }
    if (!result)
    {
        throw HttpException(
            "HTTP request failed: %s",
            httplib::to_string(result.error()).c_str());
    }
}


void HttpClient::getUnzippedGzip(
    const char* path, std::vector<std::byte>& data)
{
    constexpr size_t BUFFER_SIZE = 16 * 1024;

    data.clear();

    unsigned char outBuffer[BUFFER_SIZE];

    z_stream strm{};
    int ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (ret != Z_OK) throw ZipException(ret);

    int status = 0;
    bool done = false;

    try
    {
        auto result = client_.Get(path,
            [&](const httplib::Response& response)
            {
                status = response.status;
                return status == httplib::StatusCode::OK_200;
            },
            [&](const char* bytes, size_t size)
            {
                if (done) return true;

                strm.next_in = reinterpret_cast<Bytef*>(
                    const_cast<char*>(bytes));
                strm.avail_in = static_cast<uInt>(size);

                while (strm.avail_in != 0)
                {
                    strm.next_out = outBuffer;
                    strm.avail_out = sizeof(outBuffer);

                    ret = inflate(&strm, Z_NO_FLUSH);

                    const size_t have =
                        sizeof(outBuffer) - strm.avail_out;
                    const auto* buffer =
                        reinterpret_cast<const std::byte*>(outBuffer);

                    data.insert(data.end(), buffer, buffer + have);

                    if (ret == Z_STREAM_END)
                    {
                        done = true;
                        break;
                    }
                    if (ret != Z_OK) throw ZipException(ret);
                }

                return true;
            });

        if (status != 0 &&
            status != httplib::StatusCode::OK_200)
        {
            throw HttpException("Server response %d", status);
        }

        if (!result)
        {
            throw HttpException(
                "HTTP request failed: %s",
                httplib::to_string(result.error()).c_str());
        }

        if (!done) throw ZipException(Z_DATA_ERROR);
    }
    catch (...)
    {
        inflateEnd(&strm);
        throw;
    }

    ret = inflateEnd(&strm);
    if (ret != Z_OK) throw ZipException(ret);
}

} // namespace clarisma