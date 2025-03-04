#pragma once

void http_thread(void* arg1, void* arg2, void* arg3);
int  http_get(const char* hostname, const char* query);
