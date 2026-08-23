#include "ztest.h"

int zt_run_count = 0;
int zt_fail_count = 0;

int zt_failures(void) {
    printf("%d checks, %d failures\n", zt_run_count, zt_fail_count);
    return zt_fail_count;
}

extern void test_esp_init_ok(void);
extern void test_esp_init_no_esp(void);
extern void test_esp_init_no_reply_then_reset_recovery(void);
extern void test_esp_init_reset_recovery(void);
extern void test_esp_open_send_read_close(void);
extern void test_esp_open_fails(void);
extern void test_esp_open_already_connect(void);
extern void test_esp_send_fail_and_read_timeout(void);
extern void test_esp_ipd_split_frames(void);
extern void test_esp_ipd_zero_length_skipped(void);
extern void test_esp_ipd_garbled_length_skipped(void);
extern void test_esp_ipd_before_send_ok(void);
extern void test_esp_read_overall_deadline(void);

int main(void) {
    test_esp_init_ok();
    test_esp_init_no_esp();
    test_esp_init_no_reply_then_reset_recovery();
    test_esp_init_reset_recovery();
    test_esp_open_send_read_close();
    test_esp_open_fails();
    test_esp_open_already_connect();
    test_esp_send_fail_and_read_timeout();
    test_esp_ipd_split_frames();
    test_esp_ipd_zero_length_skipped();
    test_esp_ipd_garbled_length_skipped();
    test_esp_ipd_before_send_ok();
    test_esp_read_overall_deadline();
    return zt_failures() ? 1 : 0;
}
