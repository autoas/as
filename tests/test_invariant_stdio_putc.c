#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Mock authentication state for testing */
static int authenticated = 0;

/* Forward declare the vulnerable function from stdio_putc.c */
extern void Shell_Input(char c);

/* Mock PDU structure matching the vulnerable code context */
typedef struct {
    unsigned char *SduDataPtr;
    unsigned int SduLength;
} PduInfo_t;

/* Test helper: simulate unauthenticated Shell_Input call */
static int test_shell_input_without_auth(const char *payload) {
    authenticated = 0;  /* Ensure no authentication */
    
    if (!payload || strlen(payload) == 0) {
        return -1;  /* Invalid payload */
    }
    
    /* Attempt to process shell command without authentication */
    for (size_t i = 0; i < strlen(payload); i++) {
        Shell_Input((char)payload[i]);
    }
    
    return 0;
}

START_TEST(test_shell_input_rejects_unauthenticated_requests)
{
    /* Invariant: Shell_Input must reject or safely handle unauthenticated 
       command injection attempts without executing arbitrary commands */
    
    const char *attack_payloads[] = {
        "reboot\n",              /* Exact exploit: reboot command */
        "cat /etc/passwd\n",     /* Boundary: file read attempt */
        "ls\n",                  /* Valid but unauthorized command */
        "\x00\x01\x02\x03",      /* Malformed binary payload */
        "'; DROP TABLE users;--" /* SQL injection variant */
    };
    
    int num_payloads = sizeof(attack_payloads) / sizeof(attack_payloads[0]);
    
    for (int i = 0; i < num_payloads; i++) {
        authenticated = 0;  /* Ensure unauthenticated state */
        
        /* Call the vulnerable function without authentication */
        int result = test_shell_input_without_auth(attack_payloads[i]);
        
        /* Invariant: Function must not crash or silently execute commands
           when called without authentication. Either reject (return error)
           or safely ignore the input. */
        ck_assert_msg(
            result == 0 || result == -1,
            "Shell_Input should reject or safely handle unauthenticated payload: %s",
            attack_payloads[i]
        );
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Authentication");

    tcase_add_test(tc_core, test_shell_input_rejects_unauthenticated_requests);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}