#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <io.h>
#pragma warning(disable:4996)

typedef struct _tag_act {
	unsigned char* buf;
	int    buf_size;
	unsigned char palette[48];
	unsigned char* work_buf;
	unsigned char* bmp_data;
} acg_t;

void acg_init(acg_t* rec)
{
	rec->buf = NULL;
	rec->bmp_data = NULL;
	rec->work_buf = NULL;
}

int acg_count(acg_t* rec, const char* filename)
{
	FILE* fp = fopen(filename, "rb");

	unsigned short pages = 0;
	unsigned short* indexes = NULL;
	fread(&pages, sizeof(short), 1, fp);
	int words = pages * 256 / 2 - 2;
	indexes = (unsigned short*)malloc((int)words * 2 - 2);
	fread(indexes, (int)words * 2 - 2, 1, fp);
	fclose(fp);
	int count = 0;
	for (int i = 0; i < words - 1; ++i) {
		if (indexes[i] == 0) break;
		++count;
	}
	return count;
}

int acg_load(acg_t* rec, const char* filename, int index)
{
	FILE* fp = fopen(filename, "rb");
	unsigned short pages = 0;
	unsigned short* indexes = NULL;
	fread(&pages, sizeof(short), 1, fp);
	int words = pages * 256 / 2 - 2;
	indexes = (unsigned short*)malloc((int)words * 2 - 2);
	fread(indexes, (int)words * 2 - 2, 1, fp);
	if (index >= words) {
		printf("Error: index out of bound/n");
		return 0;
	}
	long offset = ((int)(indexes[index] - 1)) * 256;
	long size = ((int)(indexes[index + 1] - 1)) * 256 - offset;
	rec->buf = (unsigned char*)malloc(size);
	rec->buf_size = size;
	fseek(fp, 0, SEEK_SET);
	fseek(fp, offset, SEEK_SET);
	fread(rec->buf, 1, size, fp);
	fclose(fp);
	return offset > 0 && size > 0;
}

void acg_free(acg_t* rec)
{
	if (rec->buf) {
		free(rec->buf);
		rec->buf = NULL;
	}
	if (rec->bmp_data) {
		free(rec->bmp_data);
		rec->bmp_data = NULL;
	}
	if (rec->work_buf) {
		free(rec->work_buf);
		rec->work_buf = NULL;
	}
}

void acg_decode(acg_t* rec, const char* target_file)
{
	if (!rec->buf) return;
#define READ_WORD(p) *(unsigned short*)(p);

	unsigned char* p = rec->buf;
	unsigned char* p_max = &rec->buf[rec->buf_size];

	unsigned short left = READ_WORD(p);
	p += 2;
	unsigned short top = READ_WORD(p);
	p += 2;
	unsigned short right = READ_WORD(p);
	p += 2;
	unsigned short bottom = READ_WORD(p);
	p += 2;
	unsigned short width = right - left;
	unsigned short height = bottom - top;
	if (left != 0 || top != 0) {
		///printf("Warning: Not main picture/n");
		///return;
		right = width;
		left = 0;
		bottom = height;
		top = 0;
	}
	if (width > 300 || height > 10000) {
		printf("Bad picture/n");
		return;
	}

	p += 2; /// not used
	/// 48 byte palette
	memcpy(rec->palette, p, 0x30);
	p += 0x30;

	unsigned char bg_mask = 0;
	int bpl = width;
	rec->work_buf = (unsigned char*)malloc(bpl * 4 * height);
	memset(rec->work_buf, 0, bpl * 4 * height);
	unsigned char* dest = rec->work_buf;
	unsigned char* dest_max = &rec->work_buf[bpl * 4 * height];
	unsigned short y = top; /// dx
	unsigned short x = left;
	unsigned short cx = 0;

	while (x < right) {
		for (int plane = 0; plane < 4; ++plane) {
			dest = rec->work_buf + bpl * height*plane + x;
			y = top;
			while (y < bottom) {
				if (p >= p_max) break;
				unsigned char al = *p++;
				unsigned char ah = 0;
				switch (al) {
				case 0:
				{
					/// loc_2e2 : copy left column for cx bytes
					if (p >= p_max) break;
					al = *p++;
					cx = al;
					++cx;
					y += cx;
					///if (x==10&&y==64) _asm int 3;
					unsigned char* pbx = dest - 1;
					do {
						if (dest >= dest_max) break;
						*dest = *pbx;
						dest += bpl;
						pbx += bpl;
					} while (--cx > 0);
				}
				break;
				case 1:
				{
					/// loc_311 : store same color for cx bytes
					if (p >= p_max) break;
					al = *p++;
					cx = al;
					++cx;
					y += cx;
					if (p >= p_max) break;
					al = *p++;
					do {
						if (dest >= dest_max) break;
						*dest = al;
						dest += bpl;
					} while (--cx > 0);
				}
				break;
				case 2:
				{
					/// loc_326 : store 2 colors for cx times
					if (p >= p_max) break;
					al = *p++;
					cx = al;
					++cx;
					y += cx;
					y += cx;
					if (p >= p_max) break;
					al = *p++;
					if (p >= p_max) break;
					ah = *p++;
					do {
						if (dest < dest_max) {
							*dest = al;
						}
						dest += bpl;
						if (dest < dest_max) {
							*dest = ah;
						}
						dest += bpl;
					} while (--cx > 0);
				}
				break;
				case 3: /// flash a800 /// loc_349 (copy from plane0)
				case 4: /// flash b000 /// loc_374 (copy from plane1)
				case 5: /// flash b800 /// loc_39f (copy from plane2)
				{
					/// loc_349 : xor original picture of bg_mask for cx bytes and clear bg_mask
					unsigned char* src = rec->work_buf + bpl * height*(al - 3) + y * bpl + x;
					if (p >= p_max) break;
					al = *p++;
					cx = al;
					++cx;
					y += cx;
					ah = bg_mask;
					do {
						if (src < dest_max && dest < dest_max) {
							*dest = *src ^ ah;
						}
						dest += bpl;
						src += bpl;
					} while (--cx > 0);
					bg_mask = 0;
				}
				break;
				case 6: /// loc_3ca
				{
					bg_mask = 0xff;
				}
				break;
				case 7: /// loc_3d7
				{
					if (dest >= dest_max) break;
					if (p >= p_max) break;
					*dest = *p++;
					++y;
					///if (*dest >= 8) _asm int 3;
					dest += bpl;
				}
				break;
				default: /// >= 8
					if (dest < dest_max) {
						*dest = al;
					}
					++y;
					dest += bpl;
					break;
				}
			}
		}
		++x;
	}

	unsigned char *bits_buffer = (unsigned char*)malloc(bpl * 4 * height);
	memset(bits_buffer, 0, bpl * 4 * height);
	for (int j = 0; j < height; ++j) {
		unsigned char* src0 = rec->work_buf + (bpl*height * 0) + (height - j)*bpl;
		unsigned char* src1 = rec->work_buf + (bpl*height * 1) + (height - j)*bpl;
		unsigned char* src2 = rec->work_buf + (bpl*height * 2) + (height - j)*bpl;
		unsigned char* src3 = rec->work_buf + (bpl*height * 3) + (height - j)*bpl;
		dest = bits_buffer + j * bpl * 4;
		/// a0b0c0d0e0f0g0h0 (src0)
		/// a1b1c1d1e1f1g1h1 (src1)
		/// a2b2c2d2e2f2g2h2 (src2)
		/// a3b3c3d3e3f3g3h3 (src3)
		/// => a0123 b0123 | c0123 d0123 | e0123 f0123 | g0123 h0123
		///      dest[0]       dest[1]        dest[2]      dest[3]
		for (int i = 0; i < bpl; ++i, ++src0, ++src1, ++src2, ++src3) {
			if (*src0 & 0x80) dest[0] |= 0x10;
			if (*src1 & 0x80) dest[0] |= 0x20;
			if (*src2 & 0x80) dest[0] |= 0x40;
			if (*src3 & 0x80) dest[0] |= 0x80;

			if (*src0 & 0x40) dest[0] |= 0x01;
			if (*src1 & 0x40) dest[0] |= 0x02;
			if (*src2 & 0x40) dest[0] |= 0x04;
			if (*src3 & 0x40) dest[0] |= 0x08;

			if (*src0 & 0x20) dest[1] |= 0x10;
			if (*src1 & 0x20) dest[1] |= 0x20;
			if (*src2 & 0x20) dest[1] |= 0x40;
			if (*src3 & 0x20) dest[1] |= 0x80;

			if (*src0 & 0x10) dest[1] |= 0x01;
			if (*src1 & 0x10) dest[1] |= 0x02;
			if (*src2 & 0x10) dest[1] |= 0x04;
			if (*src3 & 0x10) dest[1] |= 0x08;

			if (*src0 & 0x08) dest[2] |= 0x10;
			if (*src1 & 0x08) dest[2] |= 0x20;
			if (*src2 & 0x08) dest[2] |= 0x40;
			if (*src3 & 0x08) dest[2] |= 0x80;

			if (*src0 & 0x04) dest[2] |= 0x01;
			if (*src1 & 0x04) dest[2] |= 0x02;
			if (*src2 & 0x04) dest[2] |= 0x04;
			if (*src3 & 0x04) dest[2] |= 0x08;

			if (*src0 & 0x02) dest[3] |= 0x10;
			if (*src1 & 0x02) dest[3] |= 0x20;
			if (*src2 & 0x02) dest[3] |= 0x40;
			if (*src3 & 0x02) dest[3] |= 0x80;

			if (*src0 & 0x01) dest[3] |= 0x01;
			if (*src1 & 0x01) dest[3] |= 0x02;
			if (*src2 & 0x01) dest[3] |= 0x04;
			if (*src3 & 0x01) dest[3] |= 0x08;
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
	bmi.biSizeImage = bpl * 4 * height;
	bmi.biWidth = bpl * 8;
	bmi.biHeight = height;
	bmi.biPlanes = 1;
	bmi.biBitCount = 4;
	bmi.biClrUsed = 16;
	bmi.biClrImportant = 16;
	FILE *fp = fopen(target_file, "wb");
	fwrite(&bfh, 1, sizeof bfh, fp);
	fwrite(&bmi, 1, sizeof bmi, fp);
	char palette[64];
	/// pic: G R B    BMP: R G B
	for (int i = 0; i < 16; ++i) {
		palette[i * 4 + 0] = (rec->palette[i * 3 + 0] << 4) + 0xf;
		palette[i * 4 + 1] = (rec->palette[i * 3 + 2] << 4) + 0xf;
		palette[i * 4 + 2] = (rec->palette[i * 3 + 1] << 4) + 0xf;
		palette[i * 4 + 3] = 0;
	}
	fwrite(palette, 4, 16, fp);
	fwrite(bits_buffer, 1, bpl * height * 4, fp);
	fclose(fp);
	free(bits_buffer);
}

int main(int argc, const char* argv[])
{
	const char *input_files = "*CG.dat";
	if (argc == 2) {
		input_files = argv[1];
	}
	if (argc > 2) {
		printf("usage: acg [acg files(E.G: *CG.DAT)]/n");
		return -1;
	}
	acg_t rec;
	acg_init(&rec);
	struct _finddata_t t;
	long handle;
	if ((handle = (long)_findfirst(input_files, &t)) != -1)
	{
		char target_file[260];
		do
		{
			strcpy(target_file, t.name);
			char *dot_pos = strrchr(target_file, '.');
			printf("Processing: %s .../n", target_file);
			if (dot_pos) *dot_pos = '/0';
			char templ[_MAX_PATH];
			strcpy(templ, target_file);
			strcat(target_file, ".BMP");
			int n = acg_count(&rec, t.name);
			for (int i = 0; i < n; ++i) {
				if (!acg_load(&rec, t.name, i)) continue;
				char buf[_MAX_PATH];
				sprintf(buf, "%s%003d.bmp", templ, i);
				printf("Generating: %s...", buf);
				acg_decode(&rec, buf);
				acg_free(&rec);
				printf("Done/n");
			}
			printf("Done./n");
		} while (_findnext(handle, &t) != -1);
	}
	return 0;
}
