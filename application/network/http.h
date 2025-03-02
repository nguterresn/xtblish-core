#pragma once

int http_init(const char* hostname);
int http_get(const char* hostname, const char* query);
