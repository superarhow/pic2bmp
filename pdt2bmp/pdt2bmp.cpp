#include <stdio.h>
#pragma warning(disable: 4996)
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <io.h>

typedef struct __tag_pdt_rec_t {
	unsigned char* src_buf;
	int    src_buf_size;
	unsigned short palette[16];
	int    has_palette;
} pdt_rec_t;

void pdt_init(pdt_rec_t* rec)
{
	rec->src_buf = NULL;
	rec->src_buf_size = 0;
	rec->has_palette = 0;
}

void pdt_free(pdt_rec_t* rec)
{
	if (rec->src_buf) {
		free(rec->src_buf);
		rec->src_buf = NULL;
	}
}

int pdt_load(pdt_rec_t* rec, const char* filename)
{
	FILE* fp = fopen(filename, "rb");
	fseek(fp, 0, SEEK_END);
	rec->src_buf_size = (int)ftell(fp);
	fseek(fp, 0, SEEK_SET);
	rec->src_buf = (unsigned char*)malloc(rec->src_buf_size);
	if (rec->src_buf) {
		fread(rec->src_buf, rec->src_buf_size, 1, fp);
	}
	fclose(fp);
	return rec->src_buf != NULL;
}

#define READ_WORD(p) *(unsigned short*)(p)

int pdt_decode31(pdt_rec_t* rec, const char* target_file)
{
	const unsigned char* p = rec->src_buf;
	unsigned short palette_data[16] = { 0x000, 0x222, 0x444, 0x666, 0x888, 0xAAA, 0xCCC, 0xFFF,
	 0x8B9, 0xADB, 0x64C, 0x89F, 0x79F, 0x9BF, 0xBDF, 0xFFF };
	++p;
	unsigned short left = 0;
	unsigned short top = 0;
	unsigned short right = 0;
	unsigned short bottom = 0;
	left = READ_WORD(p);
	p += 2;
	top = READ_WORD(p);
	p += 2;
	right = READ_WORD(p);
	p += 2;
	bottom = READ_WORD(p);
	p += 2;

	unsigned short width = right - left + 1;
	unsigned short height = bottom - top + 1;
	int bpl = width;
	/// bytes per plane
	int bpp = bpl * height;
	unsigned char* work_buf = (unsigned char*)malloc((int)bpp * 4);
	unsigned char* dest = NULL;
	unsigned char dh;
	unsigned short i, n;
	int plane_order[4] = { 3, 0, 1, 2 };
	for (int plane = 0; plane < 4; ++plane) {
		unsigned short x = 0;
		unsigned short y = 0;
		while (x < width) {
			unsigned char al = *p++;
			unsigned char ah = *p++;
			dest = work_buf + bpp * plane_order[plane] + x + bpl * y;
			if (ah == al) {
				/// Repeat mode
				dh = al;
				al = *p++;
				ah = *p++;
				n = ah;
				++n;
				for (i = 0; i < n; ++i) {
					*dest = dh;
					dest += bpl;
					*dest = al;
					dest += bpl;
					y += 2;
					if (y >= height) {
						y = 0;
						++x;
						dest = work_buf + bpp * plane_order[plane] + x + bpl * y;
					}
				}
			}
			else {
				/// Single mode
				*dest = al;
				dest += bpl;
				*dest = ah;
				dest += bpl;
				y += 2;
				if (y >= height) {
					y = 0;
					++x;
					dest = work_buf + bpp * plane_order[plane] + x + bpl * y;
				}
			}
		}
	}

	unsigned char ch0, ch1, ch2, ch3;
	unsigned char *bits_buffer = (unsigned char*)malloc(bpl * 4 * height);
	memset(bits_buffer, 0, bpl * 4 * height);
	int line_words = bpl / 2;
	unsigned char* src = work_buf;
	dest = bits_buffer;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			ch0 = *(work_buf + bpp * 0 + x + bpl * (height - 1 - y));
			ch1 = *(work_buf + bpp * 1 + x + bpl * (height - 1 - y));
			ch2 = *(work_buf + bpp * 2 + x + bpl * (height - 1 - y));
			ch3 = *(work_buf + bpp * 3 + x + bpl * (height - 1 - y));

			if (ch0 & 0x80) dest[0] |= 0x10;
			if (ch1 & 0x80) dest[0] |= 0x20;
			if (ch2 & 0x80) dest[0] |= 0x40;
			if (ch3 & 0x80) dest[0] |= 0x80;

			if (ch0 & 0x40) dest[0] |= 0x01;
			if (ch1 & 0x40) dest[0] |= 0x02;
			if (ch2 & 0x40) dest[0] |= 0x04;
			if (ch3 & 0x40) dest[0] |= 0x08;

			if (ch0 & 0x20) dest[1] |= 0x10;
			if (ch1 & 0x20) dest[1] |= 0x20;
			if (ch2 & 0x20) dest[1] |= 0x40;
			if (ch3 & 0x20) dest[1] |= 0x80;

			if (ch0 & 0x10) dest[1] |= 0x01;
			if (ch1 & 0x10) dest[1] |= 0x02;
			if (ch2 & 0x10) dest[1] |= 0x04;
			if (ch3 & 0x10) dest[1] |= 0x08;

			if (ch0 & 0x08) dest[2] |= 0x10;
			if (ch1 & 0x08) dest[2] |= 0x20;
			if (ch2 & 0x08) dest[2] |= 0x40;
			if (ch3 & 0x08) dest[2] |= 0x80;

			if (ch0 & 0x04) dest[2] |= 0x01;
			if (ch1 & 0x04) dest[2] |= 0x02;
			if (ch2 & 0x04) dest[2] |= 0x04;
			if (ch3 & 0x04) dest[2] |= 0x08;

			if (ch0 & 0x02) dest[3] |= 0x10;
			if (ch1 & 0x02) dest[3] |= 0x20;
			if (ch2 & 0x02) dest[3] |= 0x40;
			if (ch3 & 0x02) dest[3] |= 0x80;

			if (ch0 & 0x01) dest[3] |= 0x01;
			if (ch1 & 0x01) dest[3] |= 0x02;
			if (ch2 & 0x01) dest[3] |= 0x04;
			if (ch3 & 0x01) dest[3] |= 0x08;

			dest += 4;
		}
	}

	BITMAPFILEHEADER bfh;
	BITMAPINFOHEADER bmi;
	memset(&bfh, 0, sizeof(BITMAPFILEHEADER));
	bfh.bfType = 'MB';
	bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 64;
	memset(&bmi, 0, sizeof(BITMAPINFOHEADER));
	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biSizeImage = bpp * 4;
	bmi.biWidth = width * 8;
	bmi.biHeight = height;
	bmi.biPlanes = 1;
	bmi.biBitCount = 4;
	bmi.biClrUsed = 16;
	bmi.biClrImportant = 16;
	FILE *fp = fopen(target_file, "wb");
	fwrite(&bfh, 1, sizeof bfh, fp);
	fwrite(&bmi, 1, sizeof bmi, fp);
	unsigned char palette[64];
	for (int i = 0; i < 16; ++i) {
		palette[i * 4 + 1] = ((palette_data[i] >> 8) << 4) + 0xF;
		palette[i * 4 + 2] = (((palette_data[i] >> 4) & 0xF) << 4) + 0xF;
		palette[i * 4 + 0] = ((palette_data[i] & 0xF) << 4) + 0xF;
		palette[i * 4 + 3] = 0;
	}
	fwrite(palette, 4, 16, fp);
	fwrite(bits_buffer, 1, bpp * 4, fp);
	fclose(fp);

	free(work_buf);
	free(bits_buffer);

	return 1;
}

int pdt_decodeFF(pdt_rec_t* rec, const char* target_file)
{
	/// TODO
	return 0;
}

int pdt_decode34(pdt_rec_t* rec, const char* target_file)
{
	const unsigned char* p = rec->src_buf;
	unsigned char has_palette = 1;
	if (*p != 0x34) {
		if (*p == 0x31) {
			/// BON.PDT?
			return pdt_decode31(rec, target_file);
		}
		if (*p == 0xFF) {
			return pdt_decodeFF(rec, target_file);
		}
		has_palette = 0;
		///if (*p == 0x33) has_palette = 1;
		/// TODO: 2nd = 3rd == 00....  FF
		//printf("No palette, ignored/n");
		//return 0;
	}
	++p;
	unsigned char const_2nd_byte = *p++;
	unsigned char const_3rd_byte = *p++;

	unsigned short left = 0;
	unsigned short top = 0;
	unsigned short right = 0;
	unsigned short bottom = 0;
	left = READ_WORD(p);
	p += 2;
	top = READ_WORD(p);
	p += 2;
	right = READ_WORD(p);
	p += 2;
	bottom = READ_WORD(p);
	p += 2;

	unsigned short width = right - left + 1;
	unsigned short scan_lines = (bottom - top) / 2 + 1;
	unsigned short height = scan_lines * 2;

	unsigned short palette_data[16] = { 0x000, 0x222, 0x444, 0x666, 0x888, 0xAAA, 0xCCC, 0xFFF,
	 0x8B9, 0xADB, 0x64C, 0x89F, 0x79F, 0x9BF, 0xBDF, 0xFFF };
	if (has_palette) {
		memcpy(palette_data, p, 32);
		p += 32;
	}
	if (rec->has_palette) {
		memcpy(palette_data, rec->palette, 32);
	}
	int bytes_per_line = width << 2;

	unsigned char* work_buf = (unsigned char*)malloc((int)bytes_per_line*height);
	memset(work_buf, 0, (int)bytes_per_line*height);
	int bytes_per_plane = width * height;
	unsigned char* dest = work_buf;

	for (int plane = 0; plane < 4; ++plane) {
		for (int x = 0; x < width; ++x) {
			dest = work_buf + plane * bytes_per_plane + x;
			int y = 0;
			while (y < scan_lines) {
				unsigned char ah = 0, al = 0, cl = 0;
				if (*p == const_2nd_byte) {
					++p;
					al = *p++;
					ah = *p++;
					cl = *p++;
				}
				else if (*p == const_3rd_byte) {
					++p;
					al = *p++;
					ah = al;
					cl = *p++;
				}
				else {
					al = *p++;
					ah = *p++;
					cl = 1;
				}
				y += cl;
				do {
					*dest = al;
					dest += width;
					*dest = ah;
					dest += width;
				} while (--cl != 0);
			}
		}
	}

	// 4 planes -> b4b4
	// a0 a1 a2 a3 a4 a5 a6 a7
	// b0 b1 b2 b3 b4 b5 b6 b7
	// c0 c1 c2 c3 c4 c5 c6 c7
	// d0 d1 d2 d3 d4 d5 d6 d7
	// ==>
	// a0b0c0d0 a1b1c1d1 ; a2b2c2d2 a3b3c3d3 ; a4b4c4d4 a5b5c5d5 ; a6b6c6d6 a7b7c7d7
	int bpl = width;
	unsigned char ch0, ch1, ch2, ch3;
	unsigned char *bits_buffer = (unsigned char*)malloc(bpl * 4 * height);
	memset(bits_buffer, 0, bpl * 4 * height);
	int line_words = bpl / 2;
	int i = 0;
	unsigned char* src = work_buf;
	dest = bits_buffer;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			ch0 = *(work_buf + bytes_per_plane * 0 + x + bpl * (height - 1 - y));
			ch1 = *(work_buf + bytes_per_plane * 1 + x + bpl * (height - 1 - y));
			ch2 = *(work_buf + bytes_per_plane * 2 + x + bpl * (height - 1 - y));
			ch3 = *(work_buf + bytes_per_plane * 3 + x + bpl * (height - 1 - y));

			if (ch0 & 0x80) dest[0] |= 0x10;
			if (ch1 & 0x80) dest[0] |= 0x20;
			if (ch2 & 0x80) dest[0] |= 0x40;
			if (ch3 & 0x80) dest[0] |= 0x80;

			if (ch0 & 0x40) dest[0] |= 0x01;
			if (ch1 & 0x40) dest[0] |= 0x02;
			if (ch2 & 0x40) dest[0] |= 0x04;
			if (ch3 & 0x40) dest[0] |= 0x08;

			if (ch0 & 0x20) dest[1] |= 0x10;
			if (ch1 & 0x20) dest[1] |= 0x20;
			if (ch2 & 0x20) dest[1] |= 0x40;
			if (ch3 & 0x20) dest[1] |= 0x80;

			if (ch0 & 0x10) dest[1] |= 0x01;
			if (ch1 & 0x10) dest[1] |= 0x02;
			if (ch2 & 0x10) dest[1] |= 0x04;
			if (ch3 & 0x10) dest[1] |= 0x08;

			if (ch0 & 0x08) dest[2] |= 0x10;
			if (ch1 & 0x08) dest[2] |= 0x20;
			if (ch2 & 0x08) dest[2] |= 0x40;
			if (ch3 & 0x08) dest[2] |= 0x80;

			if (ch0 & 0x04) dest[2] |= 0x01;
			if (ch1 & 0x04) dest[2] |= 0x02;
			if (ch2 & 0x04) dest[2] |= 0x04;
			if (ch3 & 0x04) dest[2] |= 0x08;

			if (ch0 & 0x02) dest[3] |= 0x10;
			if (ch1 & 0x02) dest[3] |= 0x20;
			if (ch2 & 0x02) dest[3] |= 0x40;
			if (ch3 & 0x02) dest[3] |= 0x80;

			if (ch0 & 0x01) dest[3] |= 0x01;
			if (ch1 & 0x01) dest[3] |= 0x02;
			if (ch2 & 0x01) dest[3] |= 0x04;
			if (ch3 & 0x01) dest[3] |= 0x08;

			dest += 4;
		}
	}

	BITMAPFILEHEADER bfh;
	BITMAPINFOHEADER bmi;
	memset(&bfh, 0, sizeof(BITMAPFILEHEADER));
	bfh.bfType = 'MB';
	bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 64;
	memset(&bmi, 0, sizeof(BITMAPINFOHEADER));
	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biSizeImage = bytes_per_line * height;
	bmi.biWidth = width * 8;
	bmi.biHeight = height;
	bmi.biPlanes = 1;
	bmi.biBitCount = 4;
	bmi.biClrUsed = 16;
	bmi.biClrImportant = 16;
	FILE *fp = fopen(target_file, "wb");
	fwrite(&bfh, 1, sizeof bfh, fp);
	fwrite(&bmi, 1, sizeof bmi, fp);
	unsigned char palette[64];
	for (int i = 0; i < 16; ++i) {
		palette[i * 4 + 1] = ((palette_data[i] >> 8) << 4) + 0xF;
		palette[i * 4 + 2] = (((palette_data[i] >> 4) & 0xF) << 4) + 0xF;
		palette[i * 4 + 0] = ((palette_data[i] & 0xF) << 4) + 0xF;
		palette[i * 4 + 3] = 0;
	}
	fwrite(palette, 4, 16, fp);
	fwrite(bits_buffer, 1, bytes_per_line * height, fp);
	fclose(fp);

	free(work_buf);
	free(bits_buffer);

	return 1;
}

int pdt_decode(pdt_rec_t* rec, const char* target_file)
{
#define READ_WORD(p) *(unsigned short*)(p)

	const unsigned char* p = rec->src_buf;
	if (*p != 0x38) {
		return pdt_decode34(rec, target_file);
		//  printf("Error: unknown format: 0x%x", *p);
		//  throw 1;
	}
	unsigned short left = 0;
	unsigned short top = 0;
	unsigned short right = 0;
	unsigned short bottom = 0;

	p += 3;
	left = READ_WORD(p);
	p += 2;
	top = READ_WORD(p);
	p += 2;
	right = READ_WORD(p);
	p += 2;
	bottom = READ_WORD(p);
	p += 2;
	++bottom;

	unsigned short width = right - left;
	unsigned short height = bottom - top;
	int bpl = width << 4; /// bits per line width:80 640/2=320bytes bpl:80x16=1280 bytes=4 lines
	int bytes_per_line = width << 2;
	unsigned short palette_data[16];
	memcpy(palette_data, p, 32);
	p += 32;

	unsigned char line_buf[1920];
	memset(line_buf, 0, 1920);
	unsigned short offset_table[16];
	memcpy(offset_table, p, 32);
	p += 32;
	unsigned char* addr_limit = line_buf + bytes_per_line + bpl;

	unsigned char left_bits = 0;
	unsigned char left_n = 0;
	unsigned short y = 0;
	unsigned char al = 0;

	unsigned char* bits_buffer = (unsigned char*)malloc((int)bytes_per_line*height);
	memset(bits_buffer, 0, (int)bytes_per_line*height);

	unsigned char* addr = line_buf + bpl;

	for (y = 0; y < height; ++y) {
		///if (y == height-1) _asm int 3;


		while (addr < addr_limit) {
			al = *p++;
			if (left_n == 0) {
				left_n = 8;
				left_bits = al;
				al = *p++;
			}
			if (left_bits & 0x80) {
				/// dict
				unsigned char ch = al;
				unsigned short tbl_offset = offset_table[(ch >> 4) & 0xF];
				unsigned short cnt = (ch & 0xF) + 2;
				for (int i = 0; i < cnt; ++i) {
					if (addr - tbl_offset >= line_buf && addr - tbl_offset < &line_buf[1920]) {
						*addr = *(addr - tbl_offset);
					}
					++addr;
				}
			}
			else {
				/// direct byte
				*addr++ = al;
			}
			left_bits <<= 1;
			--left_n;
		}

		///addr = line_buf + bpl;
		addr -= bytes_per_line;
		unsigned char ch = 0;
		unsigned char src = 0;
		unsigned char* bits_p = bits_buffer + (int)(height - y - 1) * bytes_per_line;
		for (int i = 0; i < bytes_per_line; ++i) {
			ch = 0;
			src = line_buf[i + bpl];
			if (src & 0x80) ch |= 0x10;
			if (src & 0x40) ch |= 0x01;
			if (src & 0x20) ch |= 0x20;
			if (src & 0x10) ch |= 0x02;
			if (src & 0x08) ch |= 0x40;
			if (src & 0x04) ch |= 0x04;
			if (src & 0x02) ch |= 0x80;
			if (src & 0x01) ch |= 0x08;
			*bits_p++ = ch;
		}
		//for ( int i = 0; i < bpl+bytes_per_line; ++i ) {
		// line_buf[i] = line_buf[i+bytes_per_line];
		//}
		memcpy(line_buf, line_buf + bytes_per_line, bpl + bytes_per_line);
	}

	BITMAPFILEHEADER bfh;
	BITMAPINFOHEADER bmi;
	memset(&bfh, 0, sizeof(BITMAPFILEHEADER));
	bfh.bfType = 'MB';
	bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 64;
	memset(&bmi, 0, sizeof(BITMAPINFOHEADER));
	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biSizeImage = bytes_per_line * height;
	bmi.biWidth = width * 8;
	bmi.biHeight = height;
	bmi.biPlanes = 1;
	bmi.biBitCount = 4;
	bmi.biClrUsed = 16;
	bmi.biClrImportant = 16;
	FILE *fp = fopen(target_file, "wb");
	fwrite(&bfh, 1, sizeof bfh, fp);
	fwrite(&bmi, 1, sizeof bmi, fp);
	unsigned char palette[64];
	for (int i = 0; i < 16; ++i) {
		palette[i * 4 + 1] = ((palette_data[i] >> 8) << 4) + 0xF;
		palette[i * 4 + 2] = (((palette_data[i] >> 4) & 0xF) << 4) + 0xF;
		palette[i * 4 + 0] = ((palette_data[i] & 0xF) << 4) + 0xF;
		palette[i * 4 + 3] = 0;
	}
	fwrite(palette, 4, 16, fp);
	fwrite(bits_buffer, 1, bytes_per_line * height, fp);
	fclose(fp);
	free(bits_buffer);
	return 1;
}

int help_text(void)
{
	printf("usage: pdt [pdt files(E.G: *.PDT)] [-p palette_file [index]]/n");
	printf("  E.G: pdt *.pdt -p color.tbl 1/n");
	return -1;
}

int main(int argc, const char* argv[])
{
	pdt_rec_t rec;
	pdt_init(&rec);

	const char *input_files = "*.PDT";
	const char *pal_file = NULL;
	int pal_index = 0;
	int input_file_found = 0;
	for (int i = 1; i < argc; ) {
		if (argv[i][0] == '-' || argv[i][0] == '/') {
			if (argv[i][1] == 'p' || argv[i][1] == 'P') {
				if (i + 1 < argc) {
					pal_file = argv[i + 1];
					if ((i + 1 < argc && input_file_found) || i + 2 < argc) {
						pal_index = atoi(argv[i + 2]);
						i += 3;
					}
					else {
						i += 2;
					}
				}
				else return help_text();
			}
			else if (argv[i][1] == 'h' || argv[i][1] == 'H' || argv[i][1] == '?') {
				return help_text();
			}
			else return help_text();
		}
		else {
			if (!input_file_found) {
				input_file_found = 1;
				input_files = argv[i];
				++i;
			}
			else return help_text();
		}
	}
	if (argc > 5) {
		return help_text();
	}
	if (pal_file) {
		FILE* fp = fopen(pal_file, "rb");
		if (!fp) {
			printf("Error: cannot load palette file: %s/n", pal_file);
		}
		else {
			fseek(fp, pal_index * 32, SEEK_SET);
			fread(rec.palette, 1, 32, fp);
			rec.has_palette = 1;
			fclose(fp);
		}
	}
	struct _finddata_t t;
	long handle;
	if ((handle = (long)_findfirst(input_files, &t)) != -1)
	{
		char target_file[260];
		do
		{
			strcpy(target_file, t.name);
			char *dot_pos = strrchr(target_file, '.');
			printf("Processing: %s ...", target_file);
			if (dot_pos) *dot_pos = '/0';
			char templ[_MAX_PATH];
			strcpy(templ, target_file);
			strcat(target_file, ".BMP");
			pdt_load(&rec, t.name);
			pdt_decode(&rec, target_file);
			pdt_free(&rec);
			printf("Done./n");
		} while (_findnext(handle, &t) != -1);
	}
	return 0;
}
