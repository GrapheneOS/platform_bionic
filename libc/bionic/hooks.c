#include <stdlib.h>

// android_getaddrinfofornetcontext()
// SSL_set_tlsext_host_name()
const char* hook_translate_hostname(const char* hostname) {
    if (hostname == NULL) {
        return NULL;
    }

    switch (get_prog_id()) {
        default:
            return hostname;
    }
}

// X509_verify_cert()
const char* const* hook_get_trusted_ssl_certificates() {
    // return NULL or NULL-terminated array of base64-encoded SHA-256 sums of public keys of
    // trusted certificates
    switch (get_prog_id()) {
        default:
            return NULL;
    }
}

// SSL_write()
void* hook_override_ssl_write(const void* orig_buf, int orig_len, int* out_len) {
    if (orig_len <= 0) {
        return NULL;
    }

    switch (get_prog_id()) {
        default:
            return NULL;
    }
}
