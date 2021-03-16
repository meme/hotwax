#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <lzma.h>

#define MAX_SIZE 4096
#define g_assert_cmpint(x, y, z)

extern void * range_base;
extern size_t range_size;

void box(char* data) {
  size_t size = strlen(data);

    // if (size > 0 && data[0] == 'H')
    //     if (size > 1 && data[1] == 'I')
    //         if (size > 2 && data[2] == '!')
    //             __builtin_trap();

  lzma_stream stream = LZMA_STREAM_INIT;
  const uint32_t preset = 9 | LZMA_PRESET_EXTREME;
  lzma_ret ret;
  uint8_t * inbuf;
  uint8_t * outbuf;
  size_t inbuf_size;
  size_t outbuf_size;
  const size_t outbuf_size_increment = 1024 * 1024;

  ret = lzma_easy_encoder (&stream, preset, LZMA_CHECK_CRC64);
  g_assert_cmpint (ret, ==, LZMA_OK);

  inbuf_size = range_size < MAX_SIZE ? range_size : MAX_SIZE;
  inbuf = malloc (inbuf_size);

  outbuf_size = outbuf_size_increment;
  outbuf = malloc (outbuf_size);

  memcpy(inbuf, range_base, inbuf_size);
  memcpy(inbuf, data, size);

  stream.next_in = inbuf;
  stream.avail_in = inbuf_size;
  stream.next_out = outbuf;
  stream.avail_out = outbuf_size;

  while (true)
  {
    ret = lzma_code (&stream, LZMA_FINISH);

    if (stream.avail_out == 0)
    {
      size_t compressed_size;

      compressed_size = outbuf_size;

      outbuf_size += outbuf_size_increment;
      outbuf = realloc (outbuf, outbuf_size);

      stream.next_out = outbuf + compressed_size;
      stream.avail_out = outbuf_size - compressed_size;
    }

    if (ret != LZMA_OK)
    {
      g_assert_cmpint (ret, ==, LZMA_STREAM_END);
      break;
    }
  }

  lzma_end (&stream);

  free (outbuf);

  free (inbuf);
}

