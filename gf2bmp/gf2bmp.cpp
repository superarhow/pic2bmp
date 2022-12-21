#include <stdio.h>
#include <windows.h>
#include <io.h>

typedef struct _tag_gf {
	unsigned char* buf;
	unsigned char* org_buf;
	int   size;
} gf_rec, *pgf_rec;

void gf_init(pgf_rec rec)
{
	rec->buf = NULL;
	rec->org_buf = NULL;
	rec->size = 0;
}

void gf_free(pgf_rec rec)
{
	if (rec->org_buf) {
		free(rec->org_buf);
	}
	rec->org_buf = NULL;
	rec->buf = NULL;
	rec->size = 0;
}

int gf_load(pgf_rec rec, const char* filename)
{
	FILE* fp = fopen(filename, "rb");
	if (!fp) {
		printf("Cannot open %s for read/n", filename);
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	rec->buf = (unsigned char*)malloc(size);
	fread(rec->buf, 1, size, fp);
	fclose(fp);
	rec->org_buf = rec->buf;
	rec->size = size;
	return 1;
}

void gf_decode(pgf_rec rec, const char* target_file)
{
	unsigned char palette_data[32];

#define READ_WORD(p)  *(unsigned short*)p;

	unsigned char* p = rec->buf;

	p += 16;
	memcpy(palette_data, p, 32);
	p += 32;

	p += 2;
	unsigned short width = READ_WORD(p);
	p += 2;
	unsigned short height = READ_WORD(p);
	p += 2;

	p += 10;

	unsigned char al = 0;
	unsigned char ah = 0;
	unsigned char* target_buf = (unsigned char*)malloc(width*height / 2 + 512 * 4);
	unsigned char* dest = target_buf;
	unsigned char* col_start = dest;

	int dx = 0;
#define STOSB()   *dest++ = al;
#define STOSW()   *dest++ = al; *dest++ = ah;
#define MOVSB()   al = *p++; *dest++ = al;
#define MOVSW()   al = *p++; ah = *p++; *dest++ = al; *dest++ = ah;
#define AX    ((((unsigned short)ah) << 8)|((unsigned short)al))

	unsigned short cnt = 1;
	unsigned short bpl = width / 8;
	for (unsigned short x = 0; x < bpl; ++x) {
		col_start = dest;
		dx = 0;
		while (dx < height) {
			al = *p++;
			if (al & 0x80) {
				/// Ctrl
				if ((al & 0x70) == 0) { /// loc_1953
					al &= 0x7F;
					STOSB();
					MOVSB();
					MOVSW();
					++dx;
				}
				else if ((al & 0x60) == 0) { /// loc_193e
					cnt = (al & 0xF) + 2;
				}
				else if ((al & 0x40) == 0) { /// loc_1947
					unsigned short n = (al & 0x1F) + 1;
					for (unsigned short i = 0; i < n; ++i) {
						MOVSW();
						MOVSW();
						++dx;
					}
				}
				else if ((al & 0x20) != 0) { /// loc_1a8f
					dx += cnt;
					unsigned short i = 0;
					switch (al & 0x1F) {
					case 0: /// 0f48  al=es:[di-4]    stopsb  movsw  al=es:[di-4] stopsb  loop 0f48
						for (i = 0; i < cnt; ++i) {
							al = *(dest - 4); STOSB(); MOVSW(); al = *(dest - 4); STOSB();
						}
						break;
					case 1: /// 0f56  movsb  ax=es:[di-4] stopsw movsb loop 0f56
						for (i = 0; i < cnt; ++i) {
							MOVSB(); al = *(dest - 4); ah = *(dest - 3); STOSW(); MOVSB();
						}
						break;
					case 2: /// 0f60: al=es:[di-4]  stopsb movsb  al=es:[di-4]  stopsb movsb loop 0f60
						for (i = 0; i < cnt; ++i) {
							al = *(dest - 4); STOSB(); MOVSB(); al = *(dest - 4); STOSB(); MOVSB();
						}
						break;
					case 3: /// 0f6f: movsb al=es:[di-4] stopsb movsb al=es:[di-4] stopsb loop 0f6f
						for (i = 0; i < cnt; ++i) {
							MOVSB(); al = *(dest - 4); STOSB(); MOVSB(); al = *(dest - 4); STOSB();
						}
						break;
					case 4: /// 0f7e: ax=es:[di-4] stopsw lodsb  ah=al  stopsw  loop 0f7e
						for (i = 0; i < cnt; ++i) {
							al = *(dest - 4); ah = *(dest - 3); STOSW(); al = *p++; ah = al; STOSW();
						}
						break;
					case 5: /// 0f8a: lodsb ah=al stopsw  ax=es:[di-4] stosw loop 0f8a
						for (i = 0; i < cnt; ++i) {
							al = *p++; ah = al; STOSW(); al = *(dest - 4); ah = *(dest - 3); STOSW();
						}
						break;
					case 6: /// 0f96: al=es:[di-4]  stosb lodsb ah=al stosw al=es:[di-4]  stosb loop 0f96
						for (i = 0; i < cnt; ++i) {
							al = *(dest - 4); STOSB(); al = *p++; ah = al; STOSW(); al = *(dest - 4); STOSB();
						}
						break;
					case 7: /// 0fa7: lodsb stosb bx=ax ax=es:[di-4] stosw ax=bx stosb loop 0fa7
						for (i = 0; i < cnt; ++i) {
							al = *p++; STOSB();
							unsigned char bh = ah; unsigned char bl = al;
							al = *(dest - 4); ah = *(dest - 3); STOSW();
							ah = bh; al = bl; STOSB();
						}
						break;
					case 8: /// 0fb6: al=es:[ei-4] stosb lodsb stosb bx=ax  al=es:[di-4] stosb ax=bx stosb loop 0fb6
						for (i = 0; i < cnt; ++i) {
							al = *(dest - 4); STOSB(); al = *p++; STOSB();
							unsigned char bh = ah; unsigned char bl = al;
							al = *(dest - 4); STOSB(); ah = bh; al = bl; STOSB();
						}
						break;
					case 9: /// 0fca: lodsb stosb bx=ax al=es:[di-4] stosb ax=bx stosb al=es:[di-4] stosb loop 0fca
						for (i = 0; i < cnt; ++i) {
							al = *p++; STOSB();
							unsigned char bh = ah; unsigned char bl = al;
							al = *(dest - 4); STOSB(); ah = bh; al = bl; STOSB(); al = *(dest - 4); STOSB();
						}
						break;
					case 10: /// 0fde
					case 11:
					case 12:
					case 13:
					case 14:
					case 15:
					default:
						break;
					case 16: /// 0ea7: ax=0  stosw stosw
						ah = 0;
						al = 0;
						STOSW(); STOSW();
						break;
					case 17: /// 0eac: ax=0xffff stosw stosw
						ah = 0xFF;
						al = 0xFF;
						STOSW(); STOSW();
						break;
					case 18: /// 0eb2: lodsb ah=al stosw stosw  loop 0eb2
						for (i = 0; i < cnt; ++i) {
							al = *p++; ah = al; STOSW(); STOSW();
						}
						break;
					case 19: /// 0eba: lodsw  bx=ax  ah=al  stosw ax=bx stosw loop 0eba
						for (i = 0; i < cnt; ++i) {
							al = *p++; ah = *p++;
							unsigned char bl = al; unsigned char bh = ah; ah = al;
							STOSW(); al = bl; ah = bh; STOSW();
						}
						break;
					case 20: /// 0ec6: lodsw  bx=ax  ah=al  stosw  al=bh stosw  loop 0ec6
						for (i = 0; i < cnt; ++i) {
							al = *p++; ah = *p++;
							unsigned char bl = al; unsigned char bh = ah;
							ah = al; STOSW(); al = bh; STOSW();
						}
						break;
					case 21: /// 0ed2: lodsw  stosw  ah=al stosw loop 0ed2
						for (i = 0; i < cnt; ++i) {
							al = *p++; ah = *p++; STOSW(); ah = al; STOSW();
						}
						break;
					case 22: /// 0eda: lodsw  xchg ah,al  stosw  al=ah stosw  loop 0eda
						for (i = 0; i < cnt; ++i) {
							ah = *p++; al = *p++; STOSW(); al = ah; STOSW();
						}
						break;
					case 23: /// 0ee4: lodsw  bx=ax  xchg bl,ah  stosw  ax=bx  stosw  loop 0ee4
						for (i = 0; i < cnt; ++i) {
							al = *p++; ah = *p++;
							unsigned char bl = al; unsigned char bh = ah;
							unsigned char t = bl; bl = ah; ah = t;
							STOSW(); ah = bh; al = bl; STOSW();
						}
						break;
					case 24: /// 0ef0: lodsw stosw  xchg ah,al stosw  loop 0ef0
						for (i = 0; i < cnt; ++i) {
							al = *p++; ah = *p++; STOSW();
							unsigned char t = ah; ah = al; al = t; STOSW();
						}
						break;
					case 25: /// 0ef8: lodsw stosw stosw loop 0ef8
						for (i = 0; i < cnt; ++i) {
							al = *p++; ah = *p++; STOSW(); STOSW();
						}
						break;
					case 26: /// 0efe: ax=es:[di-4]  stosw  al=es:[di-4]  stosb movsb  loop 0efe
						for (i = 0; i < cnt; ++i) {
							al = *(dest - 4); ah = *(dest - 3); STOSW(); al = *(dest - 4); STOSB(); MOVSB();
						}
						break;
					case 27: /// 0f0c: ax=es:[di-4]  stosw movsb al=es:[di-4]  stosb loop 0f0c
						for (i = 0; i < cnt; ++i) {
							al = *(dest - 4); ah = *(dest - 3); STOSW(); MOVSB(); al = *(dest - 4); STOSB();
						}
						break;
					case 28: /// 0f1a: al=es:[di-4]  stosb movsb ax=es:[di-4]  stosw loop 0f1a
						for (i = 0; i < cnt; ++i) {
							al = *(dest - 4); STOSB(); MOVSB(); al = *(dest - 4); ah = *(dest - 3); STOSW();
						}
						break;
					case 29: /// 0f28: movsb  al=es:[di-4]  stosb  ax=es:[di-4]  stosw loop 0f28
						for (i = 0; i < cnt; ++i) {
							MOVSB(); al = *(dest - 4); STOSB(); al = *(dest - 4); ah = *(dest - 3); STOSW();
						}
						break;
					case 30: /// 0f36: ax=es:[di-4]  stosw movsw loop 0f36
						for (i = 0; i < cnt; ++i) {
							al = *(dest - 4); ah = *(dest - 3); STOSW(); MOVSW();
						}
						break;
					case 31: /// 0f3f: movsw  ax=es:[di-4]  stosw  loop 0f3f
						for (i = 0; i < cnt; ++i) {
							MOVSW(); al = *(dest - 4); ah = *(dest - 3); STOSW();
						}
						break;
					}
					cnt = 1;
				}
				else if ((al & 0x10) != 0) {
					if ((al & 0x8) != 0) { /// loc_1a68
						unsigned short cx = 7 & al;
						if (cx == 0) { /// loc_1a77
							cx = *p++;
							cx += 8;
						}
						for (unsigned short i = 0; i < cx; ++i) {
							unsigned short n = 1;
							al = (dx >> 8);
							ah = (dx & 0xFF);
							al |= 2;
							unsigned short si = AX;
							si >>= 1;
							si &= 0x3F;
							if (si == 0) {
								/// loc_196e (sub_1ae0)
								unsigned char t = al; al = ah; ah = t;
								si = AX;
								si *= 4;
								unsigned char* src = col_start + si;
								for (unsigned short i = 0; i < n; ++i) {
									*dest++ = *src++;
									*dest++ = *src++;
									*dest++ = *src++;
									*dest++ = *src++;
									++dx;
								}
							}
							else {
								/// loc_1978: copy from left col
								unsigned char* src = target_buf + (x - si)*height * 4;
								unsigned char t = ah; ah = al; al = t;
								ah &= 1;
								src += (AX) * 4;
								for (unsigned short i = 0; i < n; ++i) {
									*dest++ = *src++;
									*dest++ = *src++;
									*dest++ = *src++;
									*dest++ = *src++;
									++dx;
								}
							}
							/// loc_1971
							cnt = 1;
						}
					}
					else { /// 1a50
						unsigned short n = al & 7;
						if (n == 0) {
							n = *p++;
							n += 8;
						}
						al = (dx >> 8);
						ah = (dx & 0xFF);
						al |= 2;
						unsigned short si = AX;
						si >>= 1;
						si &= 0x3F;
						if (si == 0) {
							/// loc_196e (sub_1ae0)
							unsigned char t = al; al = ah; ah = t;
							si = AX;
							si *= 4;
							unsigned char* src = col_start + si;
							for (unsigned short i = 0; i < n; ++i) {
								*dest++ = *src++;
								*dest++ = *src++;
								*dest++ = *src++;
								*dest++ = *src++;
								++dx;
							}
						}
						else {
							/// loc_1978: copy from left col
							unsigned char* src = target_buf + (x - si)*height * 4;
							unsigned char t = ah; ah = al; al = t;
							ah &= 1;
							src += (AX) * 4;
							for (unsigned short i = 0; i < n; ++i) {
								*dest++ = *src++;
								*dest++ = *src++;
								*dest++ = *src++;
								*dest++ = *src++;
								++dx;
							}
						}
						/// loc_1971
						cnt = 1;
					}
				}
				else { /// 1a28
					short bx = -4;
					if ((al & 0x8) != 0) {
						bx *= 2;
					}
					/// loc_1a31
					unsigned short n = al & 7;
					if (n == 0) {
						n = *p++;
						n += 8;
					}
					unsigned char* src = dest + bx;
					for (unsigned short i = 0; i < n; ++i) {
						*dest++ = *src++;
						*dest++ = *src++;
						*dest++ = *src++;
						*dest++ = *src++;
						++dx;
					}
				}
			}
			else {
				/// ! al & 0x80
				--p;
				al = *p++;
				ah = *p++;
				unsigned short n = cnt;
				/// sub_195f
				unsigned short si = AX;
				si >>= 1;
				si &= 0x3F;
				if (si == 0) {
					/// loc_196e (sub_1a06)
					unsigned char t = al; al = ah; ah = t;
					si = AX;
					si &= 0x1FF;
					si *= 4;
					unsigned char* src = col_start + si;
					for (unsigned short i = 0; i < n; ++i) {
						*dest++ = *src++;
						*dest++ = *src++;
						*dest++ = *src++;
						*dest++ = *src++;
						++dx;
					}
				}
				else {
					/// loc_1978: copy from left col
					unsigned char* src = target_buf + (x - si)*height * 4;
					unsigned char t = ah; ah = al; al = t;
					ah &= 1;
					src += (AX) * 4;
					for (unsigned short i = 0; i < n; ++i) {
						*dest++ = *src++;
						*dest++ = *src++;
						*dest++ = *src++;
						*dest++ = *src++;
						++dx;
					}
				}
				/// loc_1971
				cnt = 1;
			}
		}
	}

	unsigned char *bits_buffer = (unsigned char*)malloc(bpl * 4 * height);
	memset(bits_buffer, 0, bpl * 4 * height);
	for (int j = 0; j < height; ++j) {
		dest = bits_buffer + (height - 1 - j)*bpl * 4;
		for (int i = 0; i < bpl; ++i) {
			unsigned char* src0 = target_buf + i * height * 4 + j * 4 + 0;
			unsigned char* src1 = target_buf + i * height * 4 + j * 4 + 1;
			unsigned char* src2 = target_buf + i * height * 4 + j * 4 + 2;
			unsigned char* src3 = target_buf + i * height * 4 + j * 4 + 3;
			/// a0b0c0d0e0f0g0h0 (src0)
			/// a1b1c1d1e1f1g1h1 (src1)
			/// a2b2c2d2e2f2g2h2 (src2)
			/// a3b3c3d3e3f3g3h3 (src3)
			/// => a0123 b0123 | c0123 d0123 | e0123 f0123 | g0123 h0123
			///      dest[0]       dest[1]        dest[2]      dest[3]
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
	for (int i = 0; i < 16; ++i) {
		palette[i * 4 + 0] = ((palette_data[i * 2 + 0] >> 4) << 4) + 0xf;
		palette[i * 4 + 2] = ((palette_data[i * 2 + 0] & 0xF) << 4) + 0xf;
		palette[i * 4 + 1] = ((palette_data[i * 2 + 1] & 0xF) << 4) + 0xf;
		palette[i * 4 + 3] = 0;
	}
	fwrite(palette, 4, 16, fp);
	fwrite(bits_buffer, 1, bpl * height * 4, fp);
	fclose(fp);
	free(bits_buffer);
}

int main(int argc, const char* argv[])
{
	const char *input_files = "*.GF";
	if (argc == 2) {
		input_files = argv[1];
	}
	if (argc > 2) {
		printf("usage: gf [gf files(E.G: *.GF)]/n");
		return -1;
	}
	gf_rec rec;
	struct _finddata_t t;
	long handle;
	if ((handle = (long)_findfirst(input_files, &t)) != -1)
	{
		char target_file[260];
		do
		{
			strcpy(target_file, t.name);
			char *dot_pos = strrchr(target_file, '.');
			if (dot_pos) *dot_pos = '/0';
			strcat(target_file, ".BMP");
			gf_init(&rec);
			printf("Processing %s ...", t.name);
			gf_load(&rec, t.name);
			gf_decode(&rec, target_file);
			gf_free(&rec);
			printf("Done./n");
		} while (_findnext(handle, &t) != -1);
	}
	return 0;
}