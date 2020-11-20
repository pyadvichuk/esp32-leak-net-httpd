#ifndef _HTTPD_H
#define _HTTPD_H

typedef struct httpd_handle_wrapper_t
{
  atomic_uint_least16_t refs;
  void* handler;
} httpd_handle_wrapper_t;

void start_web_server(httpd_handle_wrapper_t* pWrapper);
void stop_web_server(httpd_handle_wrapper_t* pWrapper);

#endif
