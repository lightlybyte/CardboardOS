// drivers/http.h - HTTP Client for File Download
#ifndef _HTTP_H
#define _HTTP_H

#include "net.h"

// HTTP Status Codes
#define HTTP_STATUS_OK             200
#define HTTP_STATUS_MOVED_PERM     301
#define HTTP_STATUS_MOVED_TEMP     302
#define HTTP_STATUS_NOT_MODIFIED   304
#define HTTP_STATUS_BAD_REQUEST    400
#define HTTP_STATUS_UNAUTHORIZED   401
#define HTTP_STATUS_FORBIDDEN      403
#define HTTP_STATUS_NOT_FOUND      404
#define HTTP_STATUS_INTERNAL_ERROR 500

// HTTP Response Structure
typedef struct {
    uint16_t status_code;
    uint32_t content_length;
    uint8_t* body;
    uint32_t body_size;
    uint8_t chunked;
    uint8_t connection_close;
} http_response_t;

// HTTP Functions
int http_get(const char* host, uint16_t port, const char* path, http_response_t* response);
int http_download_file(const char* url, const char* filename);
int http_parse_url(const char* url, char* host, uint16_t* port, char* path);
void http_free_response(http_response_t* response);

// DNS Functions (simplified)
uint32_t dns_resolve(const char* hostname);

#endif // _HTTP_H