/* cell_establish_intro.c -- generated by Trunnel v1.4.5.
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
#include <stdlib.h>
#include "trunnel-impl.h"

#include "cell_establish_intro.h"

#define TRUNNEL_SET_ERROR_CODE(obj) \
  do {                              \
    (obj)->trunnel_error_code_ = 1; \
  } while (0)

#if defined(__COVERITY__) || defined(__clang_analyzer__)
/* If we're runnning a static analysis tool, we don't want it to complain
 * that some of our remaining-bytes checks are dead-code. */
int cellestablishintro_deadcode_dummy__ = 0;
#define OR_DEADCODE_DUMMY || cellestablishintro_deadcode_dummy__
#else
#define OR_DEADCODE_DUMMY
#endif

#define CHECK_REMAINING(nbytes, label)                           \
  do {                                                           \
    if (remaining < (nbytes) OR_DEADCODE_DUMMY) {                \
      goto label;                                                \
    }                                                            \
  } while (0)

typedef struct cell_extension_st cell_extension_t;
cell_extension_t *cell_extension_new(void);
void cell_extension_free(cell_extension_t *victim);
ssize_t cell_extension_parse(cell_extension_t **output, const uint8_t *input, const size_t len_in);
ssize_t cell_extension_encoded_len(const cell_extension_t *obj);
ssize_t cell_extension_encode(uint8_t *output, size_t avail, const cell_extension_t *input);
const char *cell_extension_check(const cell_extension_t *obj);
int cell_extension_clear_errors(cell_extension_t *obj);
hs_cell_establish_intro_t *
hs_cell_establish_intro_new(void)
{
  hs_cell_establish_intro_t *val = trunnel_calloc(1, sizeof(hs_cell_establish_intro_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
hs_cell_establish_intro_clear(hs_cell_establish_intro_t *obj)
{
  (void) obj;
  TRUNNEL_DYNARRAY_WIPE(&obj->auth_key);
  TRUNNEL_DYNARRAY_CLEAR(&obj->auth_key);
  cell_extension_free(obj->extensions);
  obj->extensions = NULL;
  TRUNNEL_DYNARRAY_WIPE(&obj->sig);
  TRUNNEL_DYNARRAY_CLEAR(&obj->sig);
}

void
hs_cell_establish_intro_free(hs_cell_establish_intro_t *obj)
{
  if (obj == NULL)
    return;
  hs_cell_establish_intro_clear(obj);
  trunnel_memwipe(obj, sizeof(hs_cell_establish_intro_t));
  trunnel_free_(obj);
}

const uint8_t *
hs_cell_establish_intro_get_start_mac_data(const hs_cell_establish_intro_t *inp)
{
  return inp->start_mac_data;
}
uint8_t
hs_cell_establish_intro_get_auth_key_type(hs_cell_establish_intro_t *inp)
{
  return inp->auth_key_type;
}
int
hs_cell_establish_intro_set_auth_key_type(hs_cell_establish_intro_t *inp, uint8_t val)
{
  if (! ((val == 0 || val == 1 || val == 2))) {
     TRUNNEL_SET_ERROR_CODE(inp);
     return -1;
  }
  inp->auth_key_type = val;
  return 0;
}
uint16_t
hs_cell_establish_intro_get_auth_key_len(hs_cell_establish_intro_t *inp)
{
  return inp->auth_key_len;
}
int
hs_cell_establish_intro_set_auth_key_len(hs_cell_establish_intro_t *inp, uint16_t val)
{
  inp->auth_key_len = val;
  return 0;
}
size_t
hs_cell_establish_intro_getlen_auth_key(const hs_cell_establish_intro_t *inp)
{
  return TRUNNEL_DYNARRAY_LEN(&inp->auth_key);
}

uint8_t
hs_cell_establish_intro_get_auth_key(hs_cell_establish_intro_t *inp, size_t idx)
{
  return TRUNNEL_DYNARRAY_GET(&inp->auth_key, idx);
}

int
hs_cell_establish_intro_set_auth_key(hs_cell_establish_intro_t *inp, size_t idx, uint8_t elt)
{
  TRUNNEL_DYNARRAY_SET(&inp->auth_key, idx, elt);
  return 0;
}
int
hs_cell_establish_intro_add_auth_key(hs_cell_establish_intro_t *inp, uint8_t elt)
{
#if SIZE_MAX >= UINT16_MAX
  if (inp->auth_key.n_ == UINT16_MAX)
    goto trunnel_alloc_failed;
#endif
  TRUNNEL_DYNARRAY_ADD(uint8_t, &inp->auth_key, elt, {});
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}

uint8_t *
hs_cell_establish_intro_getarray_auth_key(hs_cell_establish_intro_t *inp)
{
  return inp->auth_key.elts_;
}
int
hs_cell_establish_intro_setlen_auth_key(hs_cell_establish_intro_t *inp, size_t newlen)
{
  uint8_t *newptr;
#if UINT16_MAX < SIZE_MAX
  if (newlen > UINT16_MAX)
    goto trunnel_alloc_failed;
#endif
  newptr = trunnel_dynarray_setlen(&inp->auth_key.allocated_,
                 &inp->auth_key.n_, inp->auth_key.elts_, newlen,
                 sizeof(inp->auth_key.elts_[0]), (trunnel_free_fn_t) NULL,
                 &inp->trunnel_error_code_);
  if (newlen != 0 && newptr == NULL)
    goto trunnel_alloc_failed;
  inp->auth_key.elts_ = newptr;
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}
struct cell_extension_st *
hs_cell_establish_intro_get_extensions(hs_cell_establish_intro_t *inp)
{
  return inp->extensions;
}
int
hs_cell_establish_intro_set_extensions(hs_cell_establish_intro_t *inp, struct cell_extension_st *val)
{
  if (inp->extensions && inp->extensions != val)
    cell_extension_free(inp->extensions);
  return hs_cell_establish_intro_set0_extensions(inp, val);
}
int
hs_cell_establish_intro_set0_extensions(hs_cell_establish_intro_t *inp, struct cell_extension_st *val)
{
  inp->extensions = val;
  return 0;
}
const uint8_t *
hs_cell_establish_intro_get_end_mac_data(const hs_cell_establish_intro_t *inp)
{
  return inp->end_mac_data;
}
size_t
hs_cell_establish_intro_getlen_handshake_mac(const hs_cell_establish_intro_t *inp)
{
  (void)inp;  return TRUNNEL_SHA3_256_LEN;
}

uint8_t
hs_cell_establish_intro_get_handshake_mac(const hs_cell_establish_intro_t *inp, size_t idx)
{
  trunnel_assert(idx < TRUNNEL_SHA3_256_LEN);
  return inp->handshake_mac[idx];
}

int
hs_cell_establish_intro_set_handshake_mac(hs_cell_establish_intro_t *inp, size_t idx, uint8_t elt)
{
  trunnel_assert(idx < TRUNNEL_SHA3_256_LEN);
  inp->handshake_mac[idx] = elt;
  return 0;
}

uint8_t *
hs_cell_establish_intro_getarray_handshake_mac(hs_cell_establish_intro_t *inp)
{
  return inp->handshake_mac;
}
uint16_t
hs_cell_establish_intro_get_sig_len(hs_cell_establish_intro_t *inp)
{
  return inp->sig_len;
}
int
hs_cell_establish_intro_set_sig_len(hs_cell_establish_intro_t *inp, uint16_t val)
{
  inp->sig_len = val;
  return 0;
}
const uint8_t *
hs_cell_establish_intro_get_end_sig_fields(const hs_cell_establish_intro_t *inp)
{
  return inp->end_sig_fields;
}
size_t
hs_cell_establish_intro_getlen_sig(const hs_cell_establish_intro_t *inp)
{
  return TRUNNEL_DYNARRAY_LEN(&inp->sig);
}

uint8_t
hs_cell_establish_intro_get_sig(hs_cell_establish_intro_t *inp, size_t idx)
{
  return TRUNNEL_DYNARRAY_GET(&inp->sig, idx);
}

int
hs_cell_establish_intro_set_sig(hs_cell_establish_intro_t *inp, size_t idx, uint8_t elt)
{
  TRUNNEL_DYNARRAY_SET(&inp->sig, idx, elt);
  return 0;
}
int
hs_cell_establish_intro_add_sig(hs_cell_establish_intro_t *inp, uint8_t elt)
{
#if SIZE_MAX >= UINT16_MAX
  if (inp->sig.n_ == UINT16_MAX)
    goto trunnel_alloc_failed;
#endif
  TRUNNEL_DYNARRAY_ADD(uint8_t, &inp->sig, elt, {});
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}

uint8_t *
hs_cell_establish_intro_getarray_sig(hs_cell_establish_intro_t *inp)
{
  return inp->sig.elts_;
}
int
hs_cell_establish_intro_setlen_sig(hs_cell_establish_intro_t *inp, size_t newlen)
{
  uint8_t *newptr;
#if UINT16_MAX < SIZE_MAX
  if (newlen > UINT16_MAX)
    goto trunnel_alloc_failed;
#endif
  newptr = trunnel_dynarray_setlen(&inp->sig.allocated_,
                 &inp->sig.n_, inp->sig.elts_, newlen,
                 sizeof(inp->sig.elts_[0]), (trunnel_free_fn_t) NULL,
                 &inp->trunnel_error_code_);
  if (newlen != 0 && newptr == NULL)
    goto trunnel_alloc_failed;
  inp->sig.elts_ = newptr;
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}
const char *
hs_cell_establish_intro_check(const hs_cell_establish_intro_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->auth_key_type == 0 || obj->auth_key_type == 1 || obj->auth_key_type == 2))
    return "Integer out of bounds";
  if (TRUNNEL_DYNARRAY_LEN(&obj->auth_key) != obj->auth_key_len)
    return "Length mismatch for auth_key";
  {
    const char *msg;
    if (NULL != (msg = cell_extension_check(obj->extensions)))
      return msg;
  }
  if (TRUNNEL_DYNARRAY_LEN(&obj->sig) != obj->sig_len)
    return "Length mismatch for sig";
  return NULL;
}

ssize_t
hs_cell_establish_intro_encoded_len(const hs_cell_establish_intro_t *obj)
{
  ssize_t result = 0;

  if (NULL != hs_cell_establish_intro_check(obj))
     return -1;


  /* Length of u8 auth_key_type IN [0, 1, 2] */
  result += 1;

  /* Length of u16 auth_key_len */
  result += 2;

  /* Length of u8 auth_key[auth_key_len] */
  result += TRUNNEL_DYNARRAY_LEN(&obj->auth_key);

  /* Length of struct cell_extension extensions */
  result += cell_extension_encoded_len(obj->extensions);

  /* Length of u8 handshake_mac[TRUNNEL_SHA3_256_LEN] */
  result += TRUNNEL_SHA3_256_LEN;

  /* Length of u16 sig_len */
  result += 2;

  /* Length of u8 sig[sig_len] */
  result += TRUNNEL_DYNARRAY_LEN(&obj->sig);
  return result;
}
int
hs_cell_establish_intro_clear_errors(hs_cell_establish_intro_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
hs_cell_establish_intro_encode(uint8_t *output, const size_t avail, const hs_cell_establish_intro_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = hs_cell_establish_intro_encoded_len(obj);
#endif

  if (NULL != (msg = hs_cell_establish_intro_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u8 auth_key_type IN [0, 1, 2] */
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->auth_key_type));
  written += 1; ptr += 1;

  /* Encode u16 auth_key_len */
  trunnel_assert(written <= avail);
  if (avail - written < 2)
    goto truncated;
  trunnel_set_uint16(ptr, trunnel_htons(obj->auth_key_len));
  written += 2; ptr += 2;

  /* Encode u8 auth_key[auth_key_len] */
  {
    size_t elt_len = TRUNNEL_DYNARRAY_LEN(&obj->auth_key);
    trunnel_assert(obj->auth_key_len == elt_len);
    trunnel_assert(written <= avail);
    if (avail - written < elt_len)
      goto truncated;
    if (elt_len)
      memcpy(ptr, obj->auth_key.elts_, elt_len);
    written += elt_len; ptr += elt_len;
  }

  /* Encode struct cell_extension extensions */
  trunnel_assert(written <= avail);
  result = cell_extension_encode(ptr, avail - written, obj->extensions);
  if (result < 0)
    goto fail; /* XXXXXXX !*/
  written += result; ptr += result;

  /* Encode u8 handshake_mac[TRUNNEL_SHA3_256_LEN] */
  trunnel_assert(written <= avail);
  if (avail - written < TRUNNEL_SHA3_256_LEN)
    goto truncated;
  memcpy(ptr, obj->handshake_mac, TRUNNEL_SHA3_256_LEN);
  written += TRUNNEL_SHA3_256_LEN; ptr += TRUNNEL_SHA3_256_LEN;

  /* Encode u16 sig_len */
  trunnel_assert(written <= avail);
  if (avail - written < 2)
    goto truncated;
  trunnel_set_uint16(ptr, trunnel_htons(obj->sig_len));
  written += 2; ptr += 2;

  /* Encode u8 sig[sig_len] */
  {
    size_t elt_len = TRUNNEL_DYNARRAY_LEN(&obj->sig);
    trunnel_assert(obj->sig_len == elt_len);
    trunnel_assert(written <= avail);
    if (avail - written < elt_len)
      goto truncated;
    if (elt_len)
      memcpy(ptr, obj->sig.elts_, elt_len);
    written += elt_len; ptr += elt_len;
  }


  trunnel_assert(ptr == output + written);
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  {
    trunnel_assert(encoded_len >= 0);
    trunnel_assert((size_t)encoded_len == written);
  }

#endif

  return written;

 truncated:
  result = -2;
  goto fail;
 check_failed:
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As hs_cell_establish_intro_parse(), but do not allocate the output
 * object.
 */
static ssize_t
hs_cell_establish_intro_parse_into(hs_cell_establish_intro_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;
  obj->start_mac_data = ptr;

  /* Parse u8 auth_key_type IN [0, 1, 2] */
  CHECK_REMAINING(1, truncated);
  obj->auth_key_type = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  if (! (obj->auth_key_type == 0 || obj->auth_key_type == 1 || obj->auth_key_type == 2))
    goto fail;

  /* Parse u16 auth_key_len */
  CHECK_REMAINING(2, truncated);
  obj->auth_key_len = trunnel_ntohs(trunnel_get_uint16(ptr));
  remaining -= 2; ptr += 2;

  /* Parse u8 auth_key[auth_key_len] */
  CHECK_REMAINING(obj->auth_key_len, truncated);
  TRUNNEL_DYNARRAY_EXPAND(uint8_t, &obj->auth_key, obj->auth_key_len, {});
  obj->auth_key.n_ = obj->auth_key_len;
  if (obj->auth_key_len)
    memcpy(obj->auth_key.elts_, ptr, obj->auth_key_len);
  ptr += obj->auth_key_len; remaining -= obj->auth_key_len;

  /* Parse struct cell_extension extensions */
  result = cell_extension_parse(&obj->extensions, ptr, remaining);
  if (result < 0)
    goto relay_fail;
  trunnel_assert((size_t)result <= remaining);
  remaining -= result; ptr += result;
  obj->end_mac_data = ptr;

  /* Parse u8 handshake_mac[TRUNNEL_SHA3_256_LEN] */
  CHECK_REMAINING(TRUNNEL_SHA3_256_LEN, truncated);
  memcpy(obj->handshake_mac, ptr, TRUNNEL_SHA3_256_LEN);
  remaining -= TRUNNEL_SHA3_256_LEN; ptr += TRUNNEL_SHA3_256_LEN;

  /* Parse u16 sig_len */
  CHECK_REMAINING(2, truncated);
  obj->sig_len = trunnel_ntohs(trunnel_get_uint16(ptr));
  remaining -= 2; ptr += 2;
  obj->end_sig_fields = ptr;

  /* Parse u8 sig[sig_len] */
  CHECK_REMAINING(obj->sig_len, truncated);
  TRUNNEL_DYNARRAY_EXPAND(uint8_t, &obj->sig, obj->sig_len, {});
  obj->sig.n_ = obj->sig_len;
  if (obj->sig_len)
    memcpy(obj->sig.elts_, ptr, obj->sig_len);
  ptr += obj->sig_len; remaining -= obj->sig_len;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 relay_fail:
  trunnel_assert(result < 0);
  return result;
 trunnel_alloc_failed:
  return -1;
 fail:
  result = -1;
  return result;
}

ssize_t
hs_cell_establish_intro_parse(hs_cell_establish_intro_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = hs_cell_establish_intro_new();
  if (NULL == *output)
    return -1;
  result = hs_cell_establish_intro_parse_into(*output, input, len_in);
  if (result < 0) {
    hs_cell_establish_intro_free(*output);
    *output = NULL;
  }
  return result;
}
hs_cell_intro_established_t *
hs_cell_intro_established_new(void)
{
  hs_cell_intro_established_t *val = trunnel_calloc(1, sizeof(hs_cell_intro_established_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
hs_cell_intro_established_clear(hs_cell_intro_established_t *obj)
{
  (void) obj;
  cell_extension_free(obj->extensions);
  obj->extensions = NULL;
}

void
hs_cell_intro_established_free(hs_cell_intro_established_t *obj)
{
  if (obj == NULL)
    return;
  hs_cell_intro_established_clear(obj);
  trunnel_memwipe(obj, sizeof(hs_cell_intro_established_t));
  trunnel_free_(obj);
}

struct cell_extension_st *
hs_cell_intro_established_get_extensions(hs_cell_intro_established_t *inp)
{
  return inp->extensions;
}
int
hs_cell_intro_established_set_extensions(hs_cell_intro_established_t *inp, struct cell_extension_st *val)
{
  if (inp->extensions && inp->extensions != val)
    cell_extension_free(inp->extensions);
  return hs_cell_intro_established_set0_extensions(inp, val);
}
int
hs_cell_intro_established_set0_extensions(hs_cell_intro_established_t *inp, struct cell_extension_st *val)
{
  inp->extensions = val;
  return 0;
}
const char *
hs_cell_intro_established_check(const hs_cell_intro_established_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  {
    const char *msg;
    if (NULL != (msg = cell_extension_check(obj->extensions)))
      return msg;
  }
  return NULL;
}

ssize_t
hs_cell_intro_established_encoded_len(const hs_cell_intro_established_t *obj)
{
  ssize_t result = 0;

  if (NULL != hs_cell_intro_established_check(obj))
     return -1;


  /* Length of struct cell_extension extensions */
  result += cell_extension_encoded_len(obj->extensions);
  return result;
}
int
hs_cell_intro_established_clear_errors(hs_cell_intro_established_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
hs_cell_intro_established_encode(uint8_t *output, const size_t avail, const hs_cell_intro_established_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = hs_cell_intro_established_encoded_len(obj);
#endif

  if (NULL != (msg = hs_cell_intro_established_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode struct cell_extension extensions */
  trunnel_assert(written <= avail);
  result = cell_extension_encode(ptr, avail - written, obj->extensions);
  if (result < 0)
    goto fail; /* XXXXXXX !*/
  written += result; ptr += result;


  trunnel_assert(ptr == output + written);
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  {
    trunnel_assert(encoded_len >= 0);
    trunnel_assert((size_t)encoded_len == written);
  }

#endif

  return written;

 check_failed:
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As hs_cell_intro_established_parse(), but do not allocate the
 * output object.
 */
static ssize_t
hs_cell_intro_established_parse_into(hs_cell_intro_established_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse struct cell_extension extensions */
  result = cell_extension_parse(&obj->extensions, ptr, remaining);
  if (result < 0)
    goto relay_fail;
  trunnel_assert((size_t)result <= remaining);
  remaining -= result; ptr += result;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 relay_fail:
  trunnel_assert(result < 0);
  return result;
}

ssize_t
hs_cell_intro_established_parse(hs_cell_intro_established_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = hs_cell_intro_established_new();
  if (NULL == *output)
    return -1;
  result = hs_cell_intro_established_parse_into(*output, input, len_in);
  if (result < 0) {
    hs_cell_intro_established_free(*output);
    *output = NULL;
  }
  return result;
}
