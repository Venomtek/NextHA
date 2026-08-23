#ifndef TEST_MOCK_TRANSPORT_H
#define TEST_MOCK_TRANSPORT_H

void mock_tp_reset(void);
void mock_tp_script_response(const char *bytes, unsigned len);  /* appended to the read stream */
void mock_tp_set_read_chunk(unsigned max_per_read);              /* 0 = unlimited */
void mock_tp_set_open_result(int rc);
const char *mock_tp_written(unsigned *len);                      /* everything passed to tp_write, NUL-terminated */
extern int mock_tp_open_calls, mock_tp_close_calls, mock_tp_write_calls;
extern char mock_tp_last_host[64]; extern unsigned mock_tp_last_port;

#endif
