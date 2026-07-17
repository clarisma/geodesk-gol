// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <clarisma/net/HttpClient.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/io/IOException.h>
#include <clarisma/net/UrlView.h>
#include <clarisma/util/log.h>

namespace clarisma {

HttpClient::HttpClient(std::string_view url) :
    urlView_(url),
    origin_(urlView_.origin()),
    client_(origin_)
{
}

HttpClient::~HttpClient()
{
    /*
    if(useSSL_)
    {
        client_.ssl.~SSLClient();
    }
    else
    {
        client_.http.~Client();
    }
    */
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
    /*  // TODO
    HttpResponse response = get(url);
    int status = response.status();
    if (status != 200)
    {
        throw HttpException("Server response %d", status);
    }
    response.read(data);
    */
}


} // namespace clarisma