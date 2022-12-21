#include <stdio.h>
#include <windows.h>
#include <io.h>

#define GPC_EOF  -1

struct gpc_buf {
	unsigned char *buf;
	unsigned char *org_buf;
	unsigned char *buf_limit;
	int width, height;
};

int gpc_next_byte(struct gpc_buf* data);
int gpc_next_word(struct gpc_buf* data);
void gpc_seek(struct gpc_buf* data, int offset);

BOOL gpc_load(struct gpc_buf* data, const char * filename);
void gpc_free(struct gpc_buf* data);

int gpc_next_byte(struct gpc_buf* data)
{
	if (data->buf >= data->buf_limit) {
		return GPC_EOF;
	}
	return *data->buf++;
}

int gpc_next_word(struct gpc_buf* data)
{
	if (data->buf >= data->buf_limit) return GPC_EOF;
	int ret = *(unsigned short *)data->buf;
	data->buf += 2;
	return ret;
}

BOOL gpc_load(struct gpc_buf* data, const char * filename)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp) return FALSE;
	fseek(fp, 0, SEEK_END);
	int len = (int)ftell(fp);
	fseek(fp, 0, SEEK_SET);
	data->buf = (unsigned char*)malloc(len);
	data->buf_limit = data->buf + len;
	data->org_buf = data->buf;
	fread(data->buf, len, 1, fp);
	fclose(fp);
	return TRUE;
}

void gpc_free(struct gpc_buf* data)
{
	if (!data->org_buf) return;
	free(data->org_buf);
	data->org_buf = NULL;
}

void gpc_seek(struct gpc_buf* data, int offset)
{
	data->buf = data->org_buf + offset;
}

void gpc2bmp(struct gpc_buf* data, const char* target_file)
{
	gpc_seek(data, 0x10);
	int scans = gpc_next_word(data);
	gpc_seek(data, 0x14);
	int palette_offset = gpc_next_word(data);
	gpc_seek(data, 0x18);
	int image_offset = gpc_next_word(data);

	gpc_seek(data, image_offset);
	int width = gpc_next_word(data);
	int height = gpc_next_word(data);

	int bpl = (width + 7) / 8;
	bpl *= 4;
	data->width = width;
	data->height = height;

	RGBQUAD palette[16];
	gpc_seek(data, palette_offset);
	int colorcount = gpc_next_word(data); // ignore it
	int transcolor = gpc_next_word(data); // ignore it
	for (int i = 0; i < 16; ++i) {
		int paldata = gpc_next_word(data);
		palette[i].rgbGreen = (paldata >> 8) & 0xf;
		palette[i].rgbRed = (paldata >> 4) & 0xf;
		palette[i].rgbBlue = (paldata) & 0xf;
		palette[i].rgbReserved = 0;
		palette[i].rgbRed <<= 4;
		palette[i].rgbGreen <<= 4;
		palette[i].rgbBlue <<= 4;
	}
	unsigned char *bits_buffer = (unsigned char *)malloc(bpl * height);
	gpc_seek(data, image_offset + 0x10);
	unsigned char decode_buf[1024];
	int line_words = (bpl + 1) / 2;
	unsigned char *next_line_pos = decode_buf + line_words * 2 + 1;
	unsigned char *p_target = decode_buf;
	unsigned char *disp_buf = decode_buf + 0x180;
	memset(disp_buf, 0, line_words * 2);
	memset(bits_buffer, 0, bpl * height);

	for (int s = 0; s < scans; ++s) {
		for (int i = s; i < height; ) {

			unsigned char *p = p_target;
			while (p < next_line_pos) {
				int ch = gpc_next_byte(data);
				for (int j = 0; j < 8; ++j, ch <<= 1) {
					if (ch & 0x80) {
						// needs more
						int ch2 = gpc_next_byte(data);
						for (int k = 0; k < 8; ++k, ch2 <<= 1) {
							if (ch2 & 0x80) {
								// load a byte
								*p++ = gpc_next_byte(data);
							}
							else {
								// a zero byte
								*p++ = 0;
							}
						}
					}
					else {
						// 8 bytes of empty!
						for (int k = 0; k < 8; ++k) {
							*p++ = 0;
						}
					}
				}
			}
			p_target = p;
			// post_decode_1
			int ch = decode_buf[0];
			if (ch != 0) {
				unsigned char ch1 = decode_buf[1];
				unsigned char *start = &decode_buf[1];
				unsigned char *p = start + ch;
				for (int j = ch; j > 0; --j) {
					while (p < next_line_pos) {
						ch1 ^= *p;
						*p = ch1;
						p += ch;
					}
					start++;
					p = start;
				}
			}
			// post decode 2
			unsigned char *src = decode_buf + 1;
			unsigned char *dest = disp_buf;
			for (int j = 0; j < line_words * 2; ++j) {
				*dest ^= *src;
				src++;
				dest++;
			};
			// post decode 3
			int size = (int)(p_target - next_line_pos);
			memcpy(decode_buf, next_line_pos, p_target - next_line_pos);
			p_target = decode_buf + (p_target - next_line_pos);
			src = disp_buf;
			// 4 planes -> b4b4
			// a0 a1 a2 a3 a4 a5 a6 a7
			// b0 b1 b2 b3 b4 b5 b6 b7
			// c0 c1 c2 c3 c4 c5 c6 c7
			// d0 d1 d2 d3 d4 d5 d6 d7
			// ==>
			// a0b0c0d0 a1b1c1d1 ; a2b2c2d2 a3b3c3d3 ; a4b4c4d4 a5b5c5d5 ; a6b6c6d6 a7b7c7d7
			unsigned char bits;
			for (int j = 0; j < 4; ++j) {
				dest = bits_buffer + (height - i - 1) * bpl;
				for (int k = 0; k < line_words / 2; ++k) {
					bits = *src++;
					if (bits & 0x80) dest[0] |= 0x10 << j;
					if (bits & 0x40) dest[0] |= 0x01 << j;
					if (bits & 0x20) dest[1] |= 0x10 << j;
					if (bits & 0x10) dest[1] |= 0x01 << j;
					if (bits & 0x08) dest[2] |= 0x10 << j;
					if (bits & 0x04) dest[2] |= 0x01 << j;
					if (bits & 0x02) dest[3] |= 0x10 << j;
					if (bits & 0x01) dest[3] |= 0x01 << j;
					dest += 4;
				}
			}
			i += scans;
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
	bmi.biSizeImage = bpl * height;
	bmi.biWidth = width;
	bmi.biHeight = height;
	bmi.biPlanes = 1;
	bmi.biBitCount = 4;
	bmi.biClrUsed = 16;
	bmi.biClrImportant = 16;
	FILE *fp = fopen(target_file, "wb");
	fwrite(&bfh, 1, sizeof bfh, fp);
	fwrite(&bmi, 1, sizeof bmi, fp);
	fwrite(palette, 4, 16, fp);
	fwrite(bits_buffer, 1, bpl * height, fp);
	fclose(fp);
	free(bits_buffer);
}

int main(int argc, const char* argv[])
{
	const char *input_files = "*.gpc";
	if (argc == 2) {
		input_files = argv[1];
	}
	if (argc > 2) {
		printf("usage: gpc2bmp [gpc files]/n");
		return -1;
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
			if (dot_pos) *dot_pos = '/0';
			strcat(target_file, ".BMP");
			struct gpc_buf data;
			printf("Processing %s ...", t.name);
			gpc_load(&data, t.name);
			gpc2bmp(&data, target_file);
			gpc_free(&data);
			printf("Done./n");
		} while (_findnext(handle, &t) != -1);
	}

	return 0;
}
