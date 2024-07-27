#include "stdio.h"
#include "openssl/crypto.h"
#include "openssl/evp.h"
#include "openssl/rsa.h"
#include "openssl/err.h"
#include "openssl/core_names.h"

#define KEY_SIZE (512 * 8 * 4)

int main(void) {
  EVP_PKEY_CTX* ctx = { 0 };
  EVP_PKEY* key = { 0 };
  unsigned char pub_key[KEY_SIZE/8] = { 0 };
  size_t key_size = 0;
  unsigned int primes = 2;
  BIGNUM *n = NULL, *e = NULL, *d = NULL, *p = NULL, *q = NULL;

  if (NULL == (ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL))) { printf("%d\n", __LINE__); goto error; }
  if (0 >= EVP_PKEY_keygen_init(ctx))                          { printf("%d\n", __LINE__); goto error; }
  if (0 >= EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, KEY_SIZE))    { printf("%d\n", __LINE__); goto error; }
  if (0 >= EVP_PKEY_CTX_set_rsa_keygen_primes(ctx, primes))    { printf("%d\n", __LINE__); goto error; }
  //if (0 >= EVP_PKEY_CTX_set_rsa_keygen_primes(ctx, 1361))    { printf("%d\n", __LINE__); goto error; }
  if (0 >= EVP_PKEY_generate(ctx, &key))                       { printf("%d\n", __LINE__); goto error; }
  return 0;
  if (0 >= EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_N, &n)) { printf("%d\n", __LINE__); goto error; }
  if (0 >= EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_E, &e)) { printf("%d\n", __LINE__); goto error; }
  if (0 >= EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_D, &d)) { printf("%d\n", __LINE__); goto error; }

  BN_print_fp(stdout, n);
  printf("\n");
  BN_print_fp(stdout, e);
  printf("\n");
  BN_print_fp(stdout, d);
  printf("\n");
  BN_bn2bin(n, pub_key);
  for (int i = 0 ; i < (KEY_SIZE / 8); ++i) printf("%02x ", pub_key[i]);
  printf("\n");
  return 0;

error:;
  unsigned long err = ERR_get_error();
  printf("erro %lu: %s\n", err, ERR_reason_error_string(err));
  printf("key size: %zu\n", key_size);
  return 0;
}
