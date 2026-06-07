/*
	This file is part of nestylize.
	
	nestylize is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	nestylize is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with nestylize.  If not, see <https://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>

static const char *program_name = NULL;

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define CLAMP(i, a, b) (i = (MAX(MIN(i, b), a)))
#define PRINT_ERROR(...) fprintf(stderr, "%s: " __VA_ARGS__, program_name)

typedef struct color {
	uint8_t r, g, b, a;
} color_t;

enum DitherType {
	DITHER_TYPE_NONE = 0,
	DITHER_TYPE_BAYER,
}; 

#include "palette.h"

void usage(void);
unsigned char * resize_image(unsigned char *img, int w, int h, int channels, int new_w, int new_h);
color_t find_closest_color_on_palette(color_t other);
unsigned char * convert_to_palette(unsigned char *img, int w, int h, int channels);
void apply_dithering_bayer(unsigned char *img, int w, int h, int channels, int bayer_level);

int main(int argc, char **argv) {
	int w, h, channels;
	unsigned int scale = 4;
	int new_w, new_h;
	int dither_type = 0;
	unsigned int bayer_level = 1;
	uint8_t *img, *img_scaled, *img_quant;
	const char *next_arg;
	const char *input, *output;

	argc--;
	program_name = *(argv++);

	if(argc < 2) {
		usage();
		exit(1);
	}

	while((next_arg = *(argv++))[0] == '-') {
		argc--;

		if(argc == 2) {
			PRINT_ERROR("Not enough arguments.\n");
			exit(1);
		} else if(!strcmp(next_arg, "--bayer")) {
			argc++;
			dither_type = DITHER_TYPE_BAYER;

			if(sscanf(*(argv++), "%u", &bayer_level) != 1) {
				PRINT_ERROR("Failed to read bayer level.\n");
				exit(1);
			}
		} else if(!strcmp(next_arg, "--scale")) {
			argc++;

			if(sscanf(*(argv++), "%u", &scale) != 1) {
				PRINT_ERROR("Failed to read scale.\n");
				exit(1);
			}

			if(scale == 0) {
				PRINT_ERROR("Scale can not be zero.\n");
				exit(1);
			}
		} else {
			PRINT_ERROR("Unknown argument.\n");
			exit(1);
		}
	}

	input = next_arg;
	output = *argv;

	img = stbi_load(input, &w, &h, &channels, 4);
	channels = 4;

	new_w = w / scale;
	new_h = h / scale;

	img_scaled = resize_image(img, w, h, channels, new_w, new_h);

	switch(dither_type) {
		case DITHER_TYPE_BAYER:
			apply_dithering_bayer(img_scaled, new_w, new_h, channels, bayer_level);
			break;
	}

	img_quant = convert_to_palette(img_scaled, new_w, new_h, channels);
	stbi_write_png(output, new_w, new_h, channels, img_quant, new_w * channels);

	free(img);
	free(img_scaled);
	free(img_quant);

	return 0;
}

void usage(void) {
	printf(
			"%s: Not enough arguments.\n"
			"Usage: %s --bayer-level <level> --scale <level> <input> <output>\n",
			program_name, program_name
			);
}

unsigned char * resize_image(unsigned char *img, int w, int h, int channels, int new_w, int new_h) {
	uint32_t *new_img = malloc(new_w * new_h * channels);
	uint32_t *conv = (uint32_t *) img;

	for(int j = 0; j < new_h; j++) {
		for(int i = 0; i < new_w; i++) {
			int sample_i = i * w / new_w;
			int sample_j = j * h / new_h;

			CLAMP(sample_i, 0, w - 1);
			CLAMP(sample_j, 0, h - 1);

			new_img[i + j * new_w] = conv[sample_i + sample_j * w];
		}
	}

	return (unsigned char *) new_img;
}

color_t find_closest_color_on_palette(color_t other) {
	int32_t closest_distance = 0;
	color_t found_color;
	int32_t dist_r, dist_g, dist_b;
	int32_t full_distance;

	for(int i = 0; i < NUM_COLORS_PALETTE; i++) {
		dist_r = (int32_t) other.r - (int32_t) palette[i].r;
		dist_g = (int32_t) other.g - (int32_t) palette[i].g;
		dist_b = (int32_t) other.b - (int32_t) palette[i].b;

		full_distance = dist_r * dist_r + dist_g * dist_g + dist_b * dist_b;

		if(full_distance < closest_distance || i == 0) {
			closest_distance = full_distance;
			found_color = palette[i];
		}
	}

	return found_color;
}

unsigned char * convert_to_palette(unsigned char *img, int w, int h, int channels) {
	color_t *quantized = (color_t *) malloc(w * h * channels);
	color_t *color_img = (color_t *) img;

	for(int j = 0; j < h; j++) {
		for(int i = 0; i < w; i++) {
			quantized[i + j * w] =
				find_closest_color_on_palette(color_img[i + j * w]);
		}
	}

	return (unsigned char *) quantized;
}

void apply_dithering_bayer(unsigned char *img, int w, int h, int channels, int bayer_level) {
	static const int bayer4x4[4][4] = {
		{ 0, 8,  2, 10},
		{12, 4, 14,  6},
		{ 3, 11, 1,  9},
		{15, 7, 13,  5}
	};

	for(int j = 0; j < h; j++) {
		for(int i = 0; i < w; i++) {
			int idx = (j * w + i) * channels;
			int bayer_value = bayer4x4[j % 4][i % 4];

			int offset = (bayer_value - 8) * bayer_level;

			for(int c = 0; c < 3; c++) {
				int old = img[idx + c];
				int dithered = old + offset;
				CLAMP(dithered, 0, 255);
				img[idx + c] = dithered;
			}
		}
	}
}
