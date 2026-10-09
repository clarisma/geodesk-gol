// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <cstdint>
#include <httplib.h>

namespace clarisma
{

/// HTTP operation errors.
enum class HttpError : uint32_t
{
    SUCCESS                 = uint32_t(httplib::Error::Success),
    UNKNOWN                 = uint32_t(httplib::Error::Unknown),
    CONNECTION              = uint32_t(httplib::Error::Connection),
    BIND_IP_ADDRESS         = uint32_t(httplib::Error::BindIPAddress),
    READ                    = uint32_t(httplib::Error::Read),
    WRITE                   = uint32_t(httplib::Error::Write),
    TOO_MANY_REDIRECTS      = uint32_t(httplib::Error::ExceedRedirectCount),
    CANCELED                = uint32_t(httplib::Error::Canceled),
    SSL_CONNECTION          = uint32_t(httplib::Error::SSLConnection),
    SSL_LOADING_CERTS       = uint32_t(httplib::Error::SSLLoadingCerts),
    SSL_SERVER_VERIFICATION = uint32_t(httplib::Error::SSLServerVerification),
    SSL_HOSTNAME_VERIFICATION = uint32_t(httplib::Error::SSLServerHostnameVerification),
    UNSUPPORTED_MULTIPART_BOUNDARY = uint32_t(httplib::Error::UnsupportedMultipartBoundaryChars),
    COMPRESSION             = uint32_t(httplib::Error::Compression),
    CONNECTION_TIMEOUT      = uint32_t(httplib::Error::ConnectionTimeout),
    PROXY_CONNECTION        = uint32_t(httplib::Error::ProxyConnection),
    CONNECTION_LOST         = uint32_t(httplib::Error::ConnectionClosed),
    TIMEOUT                 = uint32_t(httplib::Error::Timeout),
    RESOURCE_EXHAUSTION     = uint32_t(httplib::Error::ResourceExhaustion),
    TOO_MANY_FORM_FILES     = uint32_t(httplib::Error::TooManyFormDataFiles),
    PAYLOAD_TOO_LARGE       = uint32_t(httplib::Error::ExceedMaxPayloadSize),
    URI_TOO_LONG            = uint32_t(httplib::Error::ExceedUriMaxLength),
    TOO_MANY_SOCKETS        = uint32_t(httplib::Error::ExceedMaxSocketDescriptorCount),
    INVALID_REQUEST_LINE    = uint32_t(httplib::Error::InvalidRequestLine),
    INVALID_HTTP_METHOD     = uint32_t(httplib::Error::InvalidHTTPMethod),
    INVALID_HTTP_VERSION    = uint32_t(httplib::Error::InvalidHTTPVersion),
    INVALID_HEADERS         = uint32_t(httplib::Error::InvalidHeaders),
    MULTIPART_PARSING       = uint32_t(httplib::Error::MultipartParsing),
    OPEN_FILE               = uint32_t(httplib::Error::OpenFile),
    LISTEN                  = uint32_t(httplib::Error::Listen),
    GET_SOCKET_NAME         = uint32_t(httplib::Error::GetSockName),
    UNSUPPORTED_ADDRESS_FAMILY = uint32_t(httplib::Error::UnsupportedAddressFamily),
    HTTP_PARSING            = uint32_t(httplib::Error::HTTPParsing),
    INVALID_RANGE_HEADER    = uint32_t(httplib::Error::InvalidRangeHeader),
    UNSUPPORTED_CONTENT_ENCODING = uint32_t(httplib::Error::UnsupportedContentEncoding),
    WEBSOCKET_HANDSHAKE     = uint32_t(httplib::Error::WebSocketHandshake),
    USER_CALLBACK_EXCEPTION = uint32_t(httplib::Error::UserCallbackException)
};

} // namespace clarisma