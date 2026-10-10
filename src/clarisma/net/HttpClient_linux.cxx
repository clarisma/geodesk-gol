// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <clarisma/net/HttpClient.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/io/IOException.h>
#include <clarisma/net/UrlView.h>
#include <clarisma/util/log.h>

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



} // namespace clarisma