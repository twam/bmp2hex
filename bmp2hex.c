/***************************************************************************
 *   Copyright (C) 2007 by Tobias Müller                                   *
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

void print_binary(char b) {
	int i;
	for (i=7;i>=0;i--) {
		if (b & (1<<i)) {
			printf("X");
		} else {
			printf(".");
		}
	}
}

int main(int argc, char* argv[]) 
{
	FILE *input;
	FILE *output;
	unsigned char header[54];
	unsigned int width;
	unsigned int height;
	unsigned int rowwidth;
	unsigned char bottomup;
	unsigned short offset;
	char *buf;
	char *img;
	char *tmp;
	unsigned char depth;
	unsigned int row,col;
	unsigned int align;
	unsigned int i;

	if (argc != 4) {
		printf("You need to call: %s <inputfile> <outputfile> <imagename> \n",argv[0]);
		printf("  e.g. %s input.bmp image.h image",argv[0]);
		return 1;
	}

	if ((input = fopen(argv[1],"r")) == NULL) {
		printf("Error while opening inputfile %s\n",argv[1]);
		return 2;
	}

	if ((output = fopen(argv[2],"w")) == NULL) {
		printf("Error while opening outputfile %s\n",argv[2]);
		return 2;
	}
	
	if (fgets((char *)header,54,input) == NULL) {
		printf("Inputfile is not an valid Windows-Bitmap! (Header too short)\n");
		return 3;
	}

	if ((header[0]!=0x42) || (header[1]!=0x4D)) {
		printf("Input file is not an Windows-Bitmap! (Wrong signature: 0x%x%x)\n",header[0],header[1]);
		return 4;
	}

	// offset where image data starts
	offset = (header[11]<<8) | header[10];	

	// read width from header, should be positiv
	width = abs((header[21]<<8) | (header[20]<<16) | (header[19]<<8) | header[18]);

	// read height from header
	height = abs((header[25]<<24) | (header[24]<<16) | (header[23]<<8) | header[22]);

	// Is this a bottum up picture?
	bottomup = ((header[25]<<24) | (header[24]<<16) | (header[23]<<8) | header[22]) >= 0;

	// color depth of image, should be 1 for monochromes
	depth = (header[28]);

	// width of a byte row
	if (width % 8) {
		rowwidth = (width/8)+1;
	} else {
		rowwidth = (width/8) * depth;
	}

	// 4-byte alignment width of a byte row, align >= rowwidth 
	if (rowwidth % 4) {
		align = ((rowwidth / 4)+1)*4;
	} else {
		align = rowwidth;
	}

	if (depth != 1) {
		printf("Input file must be an 1-bit Bitmap!\n");
		return 5;
	}

	printf("%s is a %ix%ix%i bitmap\n",argv[1],width,height,depth);

	fseek(input, offset, SEEK_SET);

	if ((img = (char *)malloc(align*height)) == NULL) {
		printf("Could not aquire memory!\n");
		return 6;
	}

	if ((buf = (char *)malloc(align)) == NULL) {
		printf("Could not aquire memory!\n");
		return 6;
	}
	
	for (row=0;row<height;row++) {
		fseek(input, offset+row*align, SEEK_SET);

/*		if (fgets(buf,align+1,input) == NULL) {
			printf("Input file ended before all pixels could be read!\n");
			return 7;
		}
*/

		for (col =0; col <= align;col++) {
			buf[col] = fgetc(input);
		}

		if (bottomup) {
			memcpy(img+(((height-1)-row)*rowwidth),buf,rowwidth);
		} else {
			memcpy(img+(row*abs(width/8)),buf,rowwidth);
		}

	}

	free(buf);
/*
	int a;
	printf("    ");
	for (a=0;a<abs(width/8);a++) {
		printf("%2i ",a);
	}
	printf("\n");

	for (row=0;row<height;row++) {
		printf("%3i ",row);
		for (col = 0;col<rowwidth;col++) {
			printf("%2X ",(unsigned char)img[row*rowwidth+col]);
		}
		printf("\n");
	}
*/
	printf("\n");

	for (row=0;row<height;row++) {
		printf("%3i ",row);
		for (col = 0;col<rowwidth;col++) {
			print_binary(img[row*rowwidth+col]);
		}
		printf("\n");	
	} 

	tmp = (char*)malloc(strlen(argv[3])+2);

	for (i=0;i<strlen(argv[3]);i++) {
		tmp[i]=toupper(argv[3][i]);
	}	
	tmp[i]='_';
	tmp[i+1]='H';

	buf = (char*)malloc(255);
	sprintf(buf,"#ifndef %s\n#define %s\n\n",tmp,tmp);
	fputs(buf,output);
	sprintf(buf,"uint8_t %s[%i] PROGMEM = {\n",argv[3],rowwidth*height);
	fputs(buf,output);
	for (row=0;row<height;row++) {
		fputs("\t",output);
		for (col=0;col<rowwidth;col++) {
			sprintf(buf,"0x%02X", (unsigned char)img[row*rowwidth+col]);
			fputs(buf,output);
			if (((row+1)<height) || ((col+1)<rowwidth)){
				fputs(", ",output);
			}
		}
		if ((row+1)<height) {
			fputs("\n",output);
		}
	}
	fputs("\n};\n",output);
	sprintf(buf,"prog_uint16_t %swidth = 0x%04X;\n",argv[3],width);
	fputs(buf,output);
	sprintf(buf,"prog_uint16_t %sheight = 0x%04X;\n",argv[3],height);
	fputs(buf,output);
	fputs("#endif\n",output);
	free(buf);
	free(img);
	fclose(input);
	fclose(output);
	
	return 0;
}
