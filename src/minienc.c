#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include <openssl/evp.h>
#include <string.h> // Untuk strncmp, memcpy
#include <stdio.h>  // Untuk FILE, fopen

// --- KONFIGURASI KRITIS ---
// Server Key Part (16 karakter) HARUS sama di Encoder dan Loader.
#define SERVER_KEY_PART "KunciServerRahasia" 
// Signature file yang menjadi penanda
#define MY_SIGNATURE "MINIENC"
#define SIG_LEN 7

zend_op_array *(*original_compile_file)(zend_file_handle *file_handle, int type);

// Helper untuk membaca file secara aman
size_t read_safe(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fread(ptr, size, nmemb, stream);
}

// Fungsi Dekripsi AES-256-CBC
char* decrypt_data(char *encrypted, int data_len, char *key, char *iv, int *out_len) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    char *plaintext = emalloc(data_len + EVP_MAX_BLOCK_LENGTH);
    int len;
    
    // Inisialisasi proses dekripsi
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (unsigned char*)key, (unsigned char*)iv);
    EVP_DecryptUpdate(ctx, (unsigned char*)plaintext, &len, (unsigned char*)encrypted, data_len);
    *out_len = len;
    
    // Finalisasi proses dekripsi
    EVP_DecryptFinal_ex(ctx, (unsigned char*)plaintext + len, &len);
    *out_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    plaintext[*out_len] = '\0'; // Null terminate string
    return plaintext;
}

// FUNGSI UTAMA: Hook Compiler PHP
zend_op_array *minienc_compile_file(zend_file_handle *file_handle, int type) {
    const char *filename;
    char header[SIG_LEN + 1];

    // Ambil nama file dari handle
    if (file_handle->filename) {
        filename = ZSTR_VAL(file_handle->filename);
    } else {
        return original_compile_file(file_handle, type);
    }

    // Buka file secara biner
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return original_compile_file(file_handle, type);
    }

    // Cek Signature "MINIENC"
    if (read_safe(header, 1, SIG_LEN, fp) != SIG_LEN || strncmp(header, MY_SIGNATURE, SIG_LEN) != 0) {
        // Bukan file terenkripsi kita, kembalikan ke PHP normal
        fclose(fp);
        return original_compile_file(file_handle, type);
    }

    // Baca Key Part (16 Byte) & IV (16 Byte)
    char file_key_part[16];
    char iv[16];
    if (read_safe(file_key_part, 1, 16, fp) != 16 || read_safe(iv, 1, 16, fp) != 16) {
        fclose(fp); return NULL;
    }

    // Hitung dan Baca Payload
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    long payload_len = file_size - (SIG_LEN + 32); // Total Header = 7 + 16 + 16 = 39
    fseek(fp, SIG_LEN + 32, SEEK_SET);

    char *encrypted_payload = emalloc(payload_len);
    if (read_safe(encrypted_payload, 1, payload_len, fp) != payload_len) {
        efree(encrypted_payload); fclose(fp); return NULL;
    }
    fclose(fp);

    // Gabungkan Full Key (16 bytes dari file + 16 bytes dari hardcoded C)
    char full_key[33];
    memcpy(full_key, file_key_part, 16);
    memcpy(full_key + 16, SERVER_KEY_PART, 16);
    full_key[32] = '\0';

    // Dekripsi Data
    int decrypted_len;
    char *clean_code = decrypt_data(encrypted_payload, payload_len, full_key, iv, &decrypted_len);

    // Hapus tag <?php dan <? di awal kode (untuk zend_compile_string)
    char *final_code = clean_code;
    size_t final_len = decrypted_len;

    if (final_len >= 5 && strncmp(final_code, "<?php", 5) == 0) {
        final_code += 5;
        final_len -= 5;
    } else if (final_len >= 2 && strncmp(final_code, "<?", 2) == 0) {
        final_code += 2;
        final_len -= 2;
    }

    // Kompilasi kode bersih ke Zend Engine
    zend_string *code_str = zend_string_init(final_code, final_len, 0);
    zend_op_array *op_array = zend_compile_string(code_str, filename, ZEND_COMPILE_POSITION_AFTER_OPEN_TAG);

    // Bersihkan memori
    zend_string_release(code_str);
    efree(encrypted_payload);
    efree(clean_code);

    return op_array;
}

// Inisialisasi Module (Di sini hook compiler dipasang)
PHP_MINIT_FUNCTION(minienc) {
    original_compile_file = zend_compile_file;
    zend_compile_file = minienc_compile_file;
    return SUCCESS;
}

// Shutdown Module (Kembalikan hook ke compiler asli)
PHP_MSHUTDOWN_FUNCTION(minienc) {
    zend_compile_file = original_compile_file;
    return SUCCESS;
}

// Info Module (Untuk phpinfo())
PHP_MINFO_FUNCTION(minienc) {
    php_info_print_table_start();
    php_info_print_table_row(2, "minienc support", "enabled");
    php_info_print_table_row(2, "Version", "0.1.0 (Hybrid AES-256)");
    php_info_print_table_row(2, "Server Key", SERVER_KEY_PART);
    php_info_print_table_end();
}

// Struktur Module PHP
zend_module_entry minienc_module_entry = {
    STANDARD_MODULE_HEADER,
    "minienc",
    NULL,
    PHP_MINIT(minienc),
    PHP_MSHUTDOWN(minienc),
    NULL,
    NULL,
    PHP_MINFO(minienc),
    "0.1.0",
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MINIENC
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(minienc)
#endif