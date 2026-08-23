#include "ztest.h"

int zt_run_count = 0;
int zt_fail_count = 0;

int zt_failures(void) {
    printf("%d checks, %d failures\n", zt_run_count, zt_fail_count);
    return zt_fail_count;
}

void test_smoke(void) { ZT_EQ_INT(1 + 1, 2); }
extern void test_smoke2(void);
extern void test_mock_transport(void);
extern void test_mock_large_fixture(void);
extern void test_http_get_bytes(void);
extern void test_http_post_body_and_port80(void);
extern void test_http_open_failure_propagates(void);
extern void test_http_host_too_long(void);
extern void test_http_body_streams_exact_length(void);
extern void test_http_split_reads(void);
extern void test_http_truncated_body(void);
extern void test_http_chunked_rejected(void);
extern void test_http_bad_status_line(void);
extern void test_http_no_length_reads_to_close(void);
extern void test_http_keepalive_reuses_connection(void);
extern void test_http_server_connection_close_drops_keepalive(void);
extern void test_http_absurd_length(void);
extern void test_http_callback_abort_truncates(void);
extern void test_cfg_basic(void);
extern void test_cfg_default_port_and_lf(void);
extern void test_cfg_missing_token(void);
extern void test_cfg_toolong(void);
extern void test_cfg_template_override_and_unknown_key(void);
extern void test_cfg_load_missing_file(void);
extern void test_cfg_empty_required_value(void);
extern void test_cfg_port_bounds(void);
extern void test_cfg_load_file_and_truncation(void);
extern void test_ha_call_turn_on_bytes(void);
extern void test_ha_call_401_maps_to_auth(void);
extern void test_ha_call_500_maps_to_status(void);
extern void test_ha_call_json_passthrough(void);
extern void test_ha_call_transport_error_passthrough(void);
extern void test_json_escape(void);
extern void test_template_request_and_lines(void);
extern void test_template_lines_split_across_chunks(void);
extern void test_template_400_maps(void);
extern void test_template_callback_abort(void);
extern void test_state_extracts_value(void);
extern void test_state_split_chunks_and_404(void);

int main(void) {
    test_smoke();
    test_smoke2();
    test_mock_transport();
    test_mock_large_fixture();
    test_http_get_bytes();
    test_http_post_body_and_port80();
    test_http_open_failure_propagates();
    test_http_host_too_long();
    test_http_body_streams_exact_length();
    test_http_split_reads();
    test_http_truncated_body();
    test_http_chunked_rejected();
    test_http_bad_status_line();
    test_http_no_length_reads_to_close();
    test_http_keepalive_reuses_connection();
    test_http_server_connection_close_drops_keepalive();
    test_http_absurd_length();
    test_http_callback_abort_truncates();
    test_cfg_basic();
    test_cfg_default_port_and_lf();
    test_cfg_missing_token();
    test_cfg_toolong();
    test_cfg_template_override_and_unknown_key();
    test_cfg_load_missing_file();
    test_cfg_empty_required_value();
    test_cfg_port_bounds();
    test_cfg_load_file_and_truncation();
    test_ha_call_turn_on_bytes();
    test_ha_call_401_maps_to_auth();
    test_ha_call_500_maps_to_status();
    test_ha_call_json_passthrough();
    test_ha_call_transport_error_passthrough();
    test_json_escape();
    test_template_request_and_lines();
    test_template_lines_split_across_chunks();
    test_template_400_maps();
    test_template_callback_abort();
    test_state_extracts_value();
    test_state_split_chunks_and_404();
    return zt_failures() ? 1 : 0;
}
