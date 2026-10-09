// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <cstdint>
#include <httplib.h>

namespace clarisma
{

/// HTTP operation errors (cpp-httplib backend).
enum class HttpError : uint32_t
{
    SUCCESS = uint32_t(httplib::Error::Success),
    UNKNOWN = uint32_t(httplib::Error::Unknown),

    // Connection
    CONNECTION = uint32_t(httplib::Error::Connection),
    NAME_RESOLUTION = CONNECTION,
    CONNECTION_LOST = uint32_t(httplib::Error::ConnectionClosed),
    CONNECTION_TIMEOUT =
        uint32_t(httplib::Error::ConnectionTimeout),
    TIMEOUT = uint32_t(httplib::Error::Timeout),
    CANCELED = uint32_t(httplib::Error::Canceled),

    // Data transfer
    READ = uint32_t(httplib::Error::Read),
    WRITE = uint32_t(httplib::Error::Write),
    PAYLOAD_TOO_LARGE =
        uint32_t(httplib::Error::ExceedMaxPayloadSize),
    RESOURCE_EXHAUSTION =
        uint32_t(httplib::Error::ResourceExhaustion),

    // HTTP protocol
    HTTP_PARSING = uint32_t(httplib::Error::HTTPParsing),
    INVALID_HEADERS = uint32_t(httplib::Error::InvalidHeaders),
    INVALID_REQUEST_LINE =
        uint32_t(httplib::Error::InvalidRequestLine),
    INVALID_HTTP_METHOD =
        uint32_t(httplib::Error::InvalidHTTPMethod),
    INVALID_HTTP_VERSION =
        uint32_t(httplib::Error::InvalidHTTPVersion),
    INVALID_RANGE_HEADER =
        uint32_t(httplib::Error::InvalidRangeHeader),

    TOO_MANY_REDIRECTS =
        uint32_t(httplib::Error::ExceedRedirectCount),
    REDIRECT_FAILED = TOO_MANY_REDIRECTS,
    RESEND_REQUEST = 0x10000,

    INVALID_URL = 0x10001,
    UNSUPPORTED_SCHEME = 0x10002,
    URI_TOO_LONG = uint32_t(httplib::Error::ExceedUriMaxLength),

    HEADER_NOT_FOUND = 0x10003,
    HEADER_COUNT_EXCEEDED = 0x10004,
    HEADER_SIZE_EXCEEDED = 0x10005,
    CHUNKED_ENCODING_OVERFLOW = 0x10006,

    // TLS
    SSL_CONNECTION = uint32_t(httplib::Error::SSLConnection),
    SSL_SERVER_VERIFICATION =
        uint32_t(httplib::Error::SSLServerVerification),
    SSL_HOSTNAME_VERIFICATION =
        uint32_t(httplib::Error::SSLServerHostnameVerification),
    SSL_CERT_DATE_INVALID = SSL_SERVER_VERIFICATION,
    SSL_CERT_INVALID_CA = SSL_SERVER_VERIFICATION,
    SSL_CERT_INVALID = SSL_SERVER_VERIFICATION,
    SSL_CERT_REVOKED = SSL_SERVER_VERIFICATION,
    SSL_CERT_REVOCATION_FAILED = SSL_SERVER_VERIFICATION,
    SSL_CERT_WRONG_USAGE = SSL_SERVER_VERIFICATION,
    SSL_CLIENT_CERT_REQUIRED = 0x10007,
    SSL_CLIENT_CERT_NO_PRIVATE_KEY = 0x10008,
    SSL_CLIENT_CERT_PRIVATE_KEY_ACCESS = 0x10009,

    // Proxy
    PROXY_CONNECTION = uint32_t(httplib::Error::ProxyConnection),
    PROXY_AUTO_CONFIG_ERROR = 0x1000A,
    PROXY_AUTODETECTION_FAILED = 0x1000B,
    PROXY_SCRIPT_ERROR = 0x1000C,
    PROXY_SCRIPT_DOWNLOAD = 0x1000D,
    PROXY_SCRIPT_UNSUPPORTED = 0x1000E,
    PROXY_SCRIPT_EXECUTION = 0x1000F,

    // Authentication
    LOGIN_FAILURE = 0x10010,

    // State and configuration
    INTERNAL_ERROR = 0x10011,
    INVALID_OPTION = 0x10012,
    OPTION_NOT_SETTABLE = 0x10013,
    INVALID_HANDLE_TYPE = 0x10014,
    INVALID_HANDLE_STATE = 0x10015,
    CANNOT_CALL_BEFORE_OPEN = 0x10016,
    CANNOT_CALL_BEFORE_SEND = 0x10017,
    CANNOT_CALL_AFTER_SEND = 0x10018,
    CANNOT_CALL_AFTER_OPEN = 0x10019,
    SHUTDOWN = 0x1001A,

    // cpp-httplib-specific errors
    BIND_IP_ADDRESS =
        uint32_t(httplib::Error::BindIPAddress),
    SSL_LOADING_CERTS =
        uint32_t(httplib::Error::SSLLoadingCerts),
    UNSUPPORTED_MULTIPART_BOUNDARY =
        uint32_t(httplib::Error::UnsupportedMultipartBoundaryChars),
    COMPRESSION = uint32_t(httplib::Error::Compression),
    TOO_MANY_FORM_FILES =
        uint32_t(httplib::Error::TooManyFormDataFiles),
    TOO_MANY_SOCKETS =
        uint32_t(httplib::Error::ExceedMaxSocketDescriptorCount),
    MULTIPART_PARSING =
        uint32_t(httplib::Error::MultipartParsing),
    OPEN_FILE = uint32_t(httplib::Error::OpenFile),
    LISTEN = uint32_t(httplib::Error::Listen),
    GET_SOCKET_NAME = uint32_t(httplib::Error::GetSockName),
    UNSUPPORTED_ADDRESS_FAMILY =
        uint32_t(httplib::Error::UnsupportedAddressFamily),
    UNSUPPORTED_CONTENT_ENCODING =
        uint32_t(httplib::Error::UnsupportedContentEncoding),
    WEBSOCKET_HANDSHAKE =
        uint32_t(httplib::Error::WebSocketHandshake),
    USER_CALLBACK_EXCEPTION =
        uint32_t(httplib::Error::UserCallbackException),
};

} // namespace clarisma