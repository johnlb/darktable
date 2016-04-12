#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "common/darktable.h"

float *read_pfm(const char *filename, int *wd, int *ht)
{
  FILE *f = fopen(filename, "rb");

  if(!f)
  {
    fprintf(stderr, "can't open input file\n");
    return NULL;
  }

  char magic[2];
  char scale_factor_string[64] = { 0 };
  int width, height, cols;
  // using fscanf to read floats only really works with LANG=C :(
  fscanf(f, "%c%c %d %d ", &magic[0], &magic[1], &width, &height);
  fscanf(f, " %63s ", scale_factor_string);
  float scale_factor = g_ascii_strtod(scale_factor_string, NULL);
  if(magic[0] != 'P')
  {
    fprintf(stderr, "wrong input file format\n");
    fclose(f);
    return NULL;
  }
  if(magic[1] == 'F')
    cols = 3;
  else if(magic[1] == 'f')
    cols = 1;
  else
  {
    fprintf(stderr, "wrong input file format\n");
    fclose(f);
    return NULL;
  }

  int swap_byte_order = (scale_factor >= 0.0) ^ (G_BYTE_ORDER == G_BIG_ENDIAN);

  float *image = (float *)dt_alloc_align(16, sizeof(float) * width * height * 3);
  if(!image)
  {
    fprintf(stderr, "error allocating memory\n");
    fclose(f);
    return NULL;
  }

  if(cols == 3)
  {
    int ret = fread(image, 3 * sizeof(float), (size_t)width * height, f);
    if(ret != width * height)
    {
      fprintf(stderr, "error reading PFM\n");
      free(image);
      fclose(f);
      return NULL;
    }
    if(swap_byte_order)
    {
      for(size_t i = (size_t)width * height; i > 0; i--)
        for(int c = 0; c < 3; c++)
        {
          union {
            float f;
            guint32 i;
          } v;
          v.f = image[3 * (i - 1) + c];
          v.i = GUINT32_SWAP_LE_BE(v.i);
          image[3 * (i - 1) + c] = v.f;
        }
    }
  }
  else
    for(size_t j = 0; j < height; j++)
      for(size_t i = 0; i < width; i++)
      {
        union {
          float f;
          guint32 i;
        } v;
        int ret = fread(&v.f, sizeof(float), 1, f);
        if(ret != 1)
        {
          fprintf(stderr, "error reading PFM\n");
          free(image);
          fclose(f);
          return NULL;
        }
        if(swap_byte_order) v.i = GUINT32_SWAP_LE_BE(v.i);
        image[3 * (width * j + i) + 2] = image[3 * (width * j + i) + 1] = image[3 * (width * j + i) + 0] = v.f;
      }
  float *line = (float *)calloc(3 * width, sizeof(float));
  for(size_t j = 0; j < height / 2; j++)
  {
    memcpy(line, image + width * j * 3, 3 * sizeof(float) * width);
    memcpy(image + width * j * 3, image + width * (height - 1 - j) * 3, 3 * sizeof(float) * width);
    memcpy(image + width * (height - 1 - j) * 3, line, 3 * sizeof(float) * width);
  }
  free(line);
  fclose(f);

  if(wd) *wd = width;
  if(ht) *ht = height;
  return image;
}

void write_pfm(const char *filename, int width, int height, float *data)
{
  FILE *f = fopen(filename, "wb");
  if(f)
  {
    // INFO: per-line fwrite call seems to perform best. LebedevRI, 18.04.2014
    (void)fprintf(f, "PF\n%d %d\n-1.0\n", width, height);
    void *buf_line = dt_alloc_align(16, 3 * sizeof(float) * width);
    for(int j = 0; j < height; j++)
    {
      // NOTE: pfm has rows in reverse order
      const int row_in = height - 1 - j;
      const float *in = data + 3 * (size_t)width * row_in;
      float *out = (float *)buf_line;
      for(int i = 0; i < width; i++, in += 3, out += 3)
      {
        memcpy(out, in, 3 * sizeof(float));
      }
      int cnt = fwrite(buf_line, 3 * sizeof(float), width, f);
      if(cnt != width) break;
    }
    dt_free_align(buf_line);
    buf_line = NULL;
    fclose(f);
  }
}

// int main(int argc, char *argv[])
// {
//   if(argc != 3)
//   {
//     fprintf(stderr, "usage: %s <input PFM> <output PFM>\n", argv[0]);
//     exit(1);
//   }
//
//   int width, height;
//   float *image = read_pfm(argv[1], &width, &height);
//   if(!image) exit(1);
//
//   // do something with your image. as a (stupid) example set green (or a) and blue (or b) channel to 0:
//   float *iter = image;
//   for(int i = 0; i < width * height; i++)
//   {
//     iter[1] = iter[2] = 0.0;
//     iter += 3;
//   }
//
//   write_pfm(argv[2], width, height, image);
//
//   free(image);
//
//   return 0;
// }
