/***************************************************************************
 *   Copyright (C) 2011 by Tobias Müller                                   *
 *   Tobias_Mueller@twam.info                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum format_t { xbm, twam  };

struct bitmap_t {
	unsigned int width;
	unsigned int height;
	unsigned int rowwidth;
	unsigned char depth;
	unsigned char *data;	
};

int read_bitmap_from_file(const char* filename, bitmap_t* bitmap) {
	int ret = 0;
	FILE *fd;
	char* buffer;
	char header[54];
	unsigned short offset;
	unsigned char bottomup;
	unsigned int align;

	// open file
	if ((fd = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Error while opening file '%s'.\n", filename);
		ret = -1;
		goto read_bitmap_ret;
	}

	// read header
	if (fgets(header, 54, fd) == NULL) {
		fprintf(stderr, "File '%s' is not a valid Windows Bitmap! (Header too short)\n", filename);
		ret = -2;
		goto read_bitmap_fclose;
	}

	// check signature
	if ((header[0] != 0x42) || (header[1] != 0x4D)) {
		fprintf(stderr, "File '%s' is not a valid Windows Bitmap! (Wrong signature: 0x%X%X)\n", filename, header[0], header[1]);
		ret = -3;
		goto read_bitmap_fclose;
	}
	
	// offset where image data start
	offset = (header[11] << 8) | header[10];

	// read width from header, should be positiv
	bitmap->width = abs((header[21]<<8) | (header[20]<<16) | (header[19]<<8) | header[18]);

	// read height from header
	bitmap->height = abs((header[25]<<24) | (header[24]<<16) | (header[23]<<8) | header[22]);

	// Is this a bottum up picture?
	bottomup = ((header[25]<<24) | (header[24]<<16) | (header[23]<<8) | header[22]) >= 0;

	// color depth of image, should be 1 for monochromes
	bitmap->depth = (header[28]);

	// width of a byte row
	if (bitmap->width % 8) {
		bitmap->rowwidth = (bitmap->width/8)+1;
	} else {
		bitmap->rowwidth = (bitmap->width/8) * bitmap->depth;
	}

	// 4-byte alignment width of a byte row, align >= bitmap->rowwidth 
	if (bitmap->rowwidth % 4) {
		align = ((bitmap->rowwidth / 4)+1)*4;
	} else {
		align = bitmap->rowwidth;
	}

	fprintf(stdout, "File '%s' is a %ix%ix%i bitmap\n", filename, bitmap->width, bitmap->height, bitmap->depth);

	if (bitmap->depth != 1) {
		fprintf(stderr, "File '%s' is not an 1-bit Bitmap!\n", filename);
		ret = -4;
		goto read_bitmap_fclose;
	}

	// jump to offset
	fseek(fd, offset, SEEK_SET);

	if ((bitmap->data = (unsigned char*)malloc(align*bitmap->height)) == NULL) {
		fprintf(stderr, "Could not aquire memory for image data (%u bytes)!\n", align*bitmap->height);
		ret = -5;
		goto read_bitmap_fclose;
	}

	if ((buffer = (char*)malloc(align)) == NULL) {
		fprintf(stderr, "Could not aquire memory for read buffer (%u bytes)!\n", align);
		ret = -6;
		free(bitmap->data);
		goto read_bitmap_fclose;
	}

	for (unsigned int row=0; row<bitmap->height; ++row) {
		fseek(fd, offset+row*align, SEEK_SET);

/**		if (fgets(buffer, align+1, fd) == NULL) {
			printf("Input file ended before all pixels could be read!\n");
			return 7;
		}
**/

		// get char by char
		for (unsigned int col =0; col <= align; ++col) {
			buffer[col] = fgetc(fd);
		}

		if (bottomup) {
			memcpy(bitmap->data+(((bitmap->height-1)-row)*bitmap->rowwidth), buffer, bitmap->rowwidth);
		} else {
			memcpy(bitmap->data+(row*abs(bitmap->width/8)), buffer, bitmap->rowwidth);
		}

	}

	free(buffer);

read_bitmap_fclose:
	fclose(fd);

read_bitmap_ret:
	return ret;	
}

void print_binary(char b, unsigned char length) {
	if (length == 0) length = 8;
	int i;
	for (i=7; i>=8-length; i--) {
		if (b & (1<<i)) {
			printf("X");
		} else {
			printf(".");
		}
	}
}

void print_bitmap(bitmap_t* bitmap) {
	for (unsigned row=0; row<bitmap->height; ++row) {
		printf("%3i ",row);
		for (unsigned int col = 0; col<bitmap->rowwidth; ++col) {
			print_binary(bitmap->data[row*bitmap->rowwidth+col], col == bitmap->rowwidth -1 ? bitmap->width % 8 : 8);
		}
		printf("\n");	
	}
}

// write as standard xbm
int write_bitmap_as_xbm(const char* filename, bitmap_t* bitmap, const char* name) {
	int ret = 0;
	FILE *fd;

	// open file
	if ((fd = fopen(filename, "w")) == NULL) {
		fprintf(stderr, "Error while opening file '%s'.\n", filename);
		ret = -1;
		goto write_bitmap_xbm_ret;
	}

	fprintf(fd, "#define %s_width %u\n", name, bitmap->width);
	fprintf(fd, "#define %s_height %u\n", name, bitmap->height);
	fprintf(fd, "static char %s_bits[] = {\n", name);

	for (unsigned int line = 0; line <= (bitmap->height*bitmap->rowwidth)/12; ++line) {
		fprintf(fd, "  ");

		unsigned int max_pos = (line < (bitmap->height*bitmap->rowwidth)/12) ? 12 : (bitmap->height*bitmap->rowwidth) % 12;

		for (unsigned int pos = 0; pos < max_pos; ++pos) {
			unsigned char data = 0;

			// change LSB<->MSB
			for (unsigned int bit = 0; bit < 8; ++bit) {
				if (bitmap->data[line*12+pos] & (1 << bit)) {
					data |= (1 << (7-bit));
				}
			}
	
			fprintf(fd, "0x%02X, ", data);
		}

		if (line == (bitmap->height*bitmap->rowwidth)/12) {
			fprintf(fd, "};");
		}

		fprintf(fd, "\n");
	}

write_bitmap_xbm_fclose:
	fclose(fd);

write_bitmap_xbm_ret:
	return ret;
}

// write twam's own format
int write_bitmap_as_twam(const char* filename, bitmap_t* bitmap, const char* name) {
	int ret = 0;
	FILE *fd;

	// open file
	if ((fd = fopen(filename, "w")) == NULL) {
		fprintf(stderr, "Error while opening file '%s'.\n", filename);
		ret = -1;
		goto write_bitmap_twam_ret;
	}

	fprintf(fd, "bitmap_t PROGMEM %s = {\n", name);
	fprintf(fd, "  %u,\n", bitmap->width);
	fprintf(fd, "  %u,\n", bitmap->height);
	fprintf(fd, "  {");

	for (unsigned int line = 0; line <= (bitmap->height*bitmap->rowwidth)/12; ++line) {
		if (line != 0) {
			fprintf(fd, "  ");
		}

		unsigned int max_pos = (line < (bitmap->height*bitmap->rowwidth)/12) ? 12 : (bitmap->height*bitmap->rowwidth) % 12;

		for (unsigned int pos = 0; pos < max_pos; ++pos) {
			unsigned char data = 0;

			// change LSB<->MSB
			for (unsigned int bit = 0; bit < 8; ++bit) {
				if (bitmap->data[line*12+pos] & (1 << bit)) {
					data |= (1 << (7-bit));
				}
			}
	
			fprintf(fd, "0x%02X, ", data);
		}

		if (line == (bitmap->height*bitmap->rowwidth)/12) {
			fprintf(fd, "}");
		}

		fprintf(fd, "\n");
	}
	fprintf(fd, "  };\n");

write_bitmap_twam_fclose:
	fclose(fd);

write_bitmap_twam_ret:
	return ret;
}

int main(int argc, char* argv[]) {
	format_t format = twam;
	bitmap_t bitmap;

	if (argc != 4) {
		printf("You need to call: %s <inputfile> <outputfile> <imagename> \n",argv[0]);
		printf("  e.g. %s input.bmp image.h image\n",argv[0]);
		return 1;
	}

	// read bitmap
	if (read_bitmap_from_file(argv[1], &bitmap)<0) {
		fprintf(stderr, "Error while opening file '%s'!\n", argv[1]); 
		exit(EXIT_FAILURE);
	}

	// print bitmap
	print_bitmap(&bitmap);

	switch (format) {
		case xbm:
			write_bitmap_as_xbm(argv[2], &bitmap, argv[3]);
			break;
		case twam:
			write_bitmap_as_twam(argv[2], &bitmap, argv[3]);
			break;
	}

	// clean up bitmap
	free(bitmap.data);

	return 0;
}
