#include <stdio.h>
#include <windows.h>
#include <io.h>

struct gra_data {
	unsigned char *buf;
	unsigned char *org_buf;
	int    count;
	unsigned char byte_left;
	unsigned char left_bit_count;
};


int load_gra_data(char* filename, struct gra_data* pdata)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp) return 0;
	fseek(fp, 0, SEEK_END);
	int len = (int)ftell(fp);
	fseek(fp, 0, SEEK_SET);
	pdata->buf = (unsigned char*)malloc(len);
	pdata->count = len;
	pdata->left_bit_count = 0;
	pdata->byte_left = 0;
	pdata->org_buf = pdata->buf;
	fread(pdata->buf, len, 1, fp);
	fclose(fp);
	return 0;
}

void free_gra_data(struct gra_data* pdata)
{
	if (!pdata->org_buf) return;
	free(pdata->org_buf);
	pdata->org_buf = NULL;
}

#define GRA_EOF  -1

int gra_next_bit(struct gra_data *pdata)
{
	if (pdata->left_bit_count == 0) {
		if (pdata->count <= 0) return GRA_EOF;
		pdata->left_bit_count = 8;
		pdata->byte_left = *pdata->buf++;
		pdata->count--;
	}
	int ret;
	if (pdata->byte_left & 0x80) ret = 1; else ret = 0;
	pdata->left_bit_count--;
	pdata->byte_left <<= 1;
	return ret;
}

unsigned char gra_next_byte(struct gra_data *pdata)
{
	unsigned char ch = *pdata->buf++;
	pdata->count--;
	return ch;
}

void gra_ignore_bytes(struct gra_data *pdata, int n)
{
	pdata->buf += n;
	pdata->count -= n;
}

BOOL gra_eof(struct gra_data *pdata)
{
	return pdata->count <= 0;
}

void init_dict(unsigned char *dict)
{
	unsigned char ch;
	int j;
	for (j = 0, ch = 0; j < 16; j++, ch += 16) {
		for (int i = 0; i < 16; ++i, ch -= 16) {
			*dict++ = ch;
		}
	}
}

unsigned char swap_dict(unsigned char *dict, int lastindex)
{
	if (lastindex == 0) return dict[0];
	if (lastindex == 1) {
		unsigned char ch = dict[1];
		dict[1] = dict[0];
		dict[0] = ch;
		return ch;
	}
	unsigned char ch = dict[lastindex];
	for (int i = lastindex; i > 0; --i) {
		dict[i] = dict[i - 1];
	}
	dict[0] = ch;
	return ch;
}

int decode_code(struct gra_data* pdata, unsigned char *dict, unsigned char dict_offset)
{
	char buf[40];
	sprintf(buf, "%02x/n", dict_offset);
	//OutputDebugString( buf );
	int bit = gra_next_bit(pdata);
	bit = (bit << 1) | gra_next_bit(pdata);
	int ch1, ch2, cnt;
	switch (bit)
	{
	case 3: // 11
		ch1 = swap_dict(dict + dict_offset, 1);
		break;
	case 2: // 10
		ch1 = dict[dict_offset];
		break;
	case 0: // 00
		cnt = 2 + gra_next_bit(pdata);
		ch1 = swap_dict(dict + dict_offset, cnt);
		break;
	default: // 01
		if (gra_next_bit(pdata) == 0) {
			// 010
			cnt = 2 + gra_next_bit(pdata);
			cnt = (cnt << 1) + gra_next_bit(pdata);
		}
		else {
			// 011
			cnt = 2 + gra_next_bit(pdata);
			cnt = (cnt << 1) + gra_next_bit(pdata);
			cnt = (cnt << 1) + gra_next_bit(pdata);
		}
		ch1 = swap_dict(dict + dict_offset, cnt);
		break;
	}
	dict_offset = ch1;
	bit = gra_next_bit(pdata);
	bit = (bit << 1) | gra_next_bit(pdata);
	switch (bit)
	{
	case 3: // 11
		ch2 = swap_dict(dict + dict_offset, 1);
		break;
	case 2: // 10
		ch2 = dict[dict_offset];
		break;
	case 0: // 00
		cnt = 2 + gra_next_bit(pdata);
		ch2 = swap_dict(dict + dict_offset, cnt);
		break;
	default: // 01
		if (gra_next_bit(pdata) == 0) {
			// 010
			cnt = 2 + gra_next_bit(pdata);
			cnt = (cnt << 1) + gra_next_bit(pdata);
		}
		else {
			// 011
			cnt = 2 + gra_next_bit(pdata);
			cnt = (cnt << 1) + gra_next_bit(pdata);
			cnt = (cnt << 1) + gra_next_bit(pdata);
		}
		ch2 = swap_dict(dict + dict_offset, cnt);
		break;
	}
	return (ch2 << 8) + ch1;
}

unsigned char *decode_until_bit0(struct gra_data* pdata,
	unsigned char *dest,
	unsigned char *dict,
	int* old_offset)
{
	int code;
	int dict_offset = *(dest - 1);
	for (;; ) {
		code = decode_code(pdata, dict, dict_offset);
		*(unsigned short *)dest = (short)code;
		dict_offset = code >> 8;
		dest += 2;
		if (gra_next_bit(pdata) != 1) break;
	}
	*old_offset = 0;
	return dest;
}

unsigned char *get_n_bytes_and_copy(struct gra_data* pdata,
	unsigned char *dest,
	unsigned char *dict,
	int src_offset,
	int *old_offset)
{
	if (*old_offset == src_offset) {
		return decode_until_bit0(pdata, dest, dict, old_offset);
	}
	*old_offset = src_offset;
	int bit_cnt = 0, mov_cnt = 0;
	if (gra_next_bit(pdata) == 0) bit_cnt = 0;
	else {
		do {
			++bit_cnt;
		} while (gra_next_bit(pdata) == 1);
	}
	if (bit_cnt == 0) mov_cnt = 1;
	else {
		mov_cnt = 1;
		for (int i = 0; i < bit_cnt; ++i) mov_cnt = (mov_cnt << 1) | gra_next_bit(pdata);
	}
	for (int i = 0; i < mov_cnt; ++i) {
		// mov_sw
		*(unsigned short *)dest = *(unsigned short *)(dest + src_offset);
		dest += 2;
	}
	return dest;
}


void gra2bmp(struct gra_data* pdata, char* target_file)
{
	unsigned char ch;
	do {
		ch = gra_next_byte(pdata);
		if (gra_eof(pdata)) return;
	} while (ch != 0x1A);
	gra_next_byte(pdata); // load byte 0
	ch = gra_next_byte(pdata); // load byte ignore
	unsigned char flags = ch;
	gra_next_byte(pdata); // load word(hi)
	gra_next_byte(pdata); // load word(lo)
	// the word above is related with ROM, just ignore
	ch = gra_next_byte(pdata); // 4
	if (ch != 4) return;
	gra_ignore_bytes(pdata, 4);
	int size = gra_next_byte(pdata);
	size = (size << 8) | gra_next_byte(pdata);
	gra_ignore_bytes(pdata, size);
	int width_bytes;
	size = gra_next_byte(pdata);
	size = (size << 8) | gra_next_byte(pdata);
	width_bytes = size;

	size = gra_next_byte(pdata);
	size = (size << 8) | gra_next_byte(pdata);
	int height = size;
	unsigned char palette[48];
	if (flags & 0x80) {
		// don't know how to fill the palette  
	}
	else {
		// has palette
		for (int i = 0; i < 48; ++i) {
			palette[i] = gra_next_byte(pdata);
		}
	}
	unsigned char dict[256];
	init_dict(dict);
	unsigned char *buf = NULL;
	buf = (unsigned char*)malloc(width_bytes * height * 2 /* test */);
	unsigned char *p = buf;
	int code = decode_code(pdata, dict, 0);
	// fill background
	for (int i = 0; i < width_bytes; ++i) {
		*(unsigned short *)p = (short)code;
		p += 2;
	}
	int src_offset = 0;
	int old_offset = 0;
	for (;; ) {
		if (gra_eof(pdata)) break;
		if (gra_next_bit(pdata) == 1) {
			// 9030
			if (gra_next_bit(pdata) == 0) {
				src_offset = -width_bytes * 2;
				p = get_n_bytes_and_copy(pdata, p, dict, src_offset, &old_offset);
			}
			else {
				src_offset = -width_bytes + 1;
				if (gra_next_bit(pdata) == 1) src_offset -= 2;
				p = get_n_bytes_and_copy(pdata, p, dict, src_offset, &old_offset);
			}
		}
		else {
			// 90a0
			if (gra_next_bit(pdata) == 1) {
				src_offset = -width_bytes;
				p = get_n_bytes_and_copy(pdata, p, dict, src_offset, &old_offset);
			}
			else {
				// 90a7
				src_offset = -4;
				if (*(p - 1) == *(p - 2)) {
					// 90fc
					if (src_offset == old_offset) {
						p = decode_until_bit0(pdata, p, dict, &old_offset);
					}
					else {
						old_offset = src_offset;

						int bit_cnt = 0, mov_cnt = 0;
						src_offset = -2;

						if (gra_next_bit(pdata) == 0) bit_cnt = 0;
						else {
							do {
								++bit_cnt;
							} while (gra_next_bit(pdata) == 1);
						}
						if (bit_cnt == 0) mov_cnt = 1;
						else {
							mov_cnt = 1;
							for (int i = 0; i < bit_cnt; ++i) mov_cnt = (mov_cnt << 1) | gra_next_bit(pdata);
						}
						for (int i = 0; i < mov_cnt; ++i) {
							// mov_sw
							*(unsigned short *)p = *(unsigned short *)(p + src_offset);
							p += 2;
						}
					}
				}
				else {
					p = get_n_bytes_and_copy(pdata, p, dict, src_offset, &old_offset);
				}
			}
		}
	}
	unsigned char *bmp_buffer = NULL;
	// 1 byte for 1 pixel, convert to compressed form
	int bpl = width_bytes;
	// make dword aligned
	bpl = width_bytes * 4 + 31;
	bpl /= 32; bpl *= 4;

	bmp_buffer = (unsigned char *)malloc(width_bytes * height);
	for (int j = 0; j < height; ++j) {
		for (int i = 0; i < width_bytes / 2; ++i) {
			bmp_buffer[(height - j - 1) * bpl + i] = (buf[j * width_bytes + i * 2]) | (buf[j * width_bytes + i * 2 + 1] >> 4);
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
	bmi.biSizeImage = width_bytes * height;
	bmi.biWidth = width_bytes;
	bmi.biHeight = height;
	bmi.biPlanes = 1;
	bmi.biBitCount = 4;
	bmi.biClrUsed = 16;
	bmi.biClrImportant = 16;
	FILE *fp = fopen(target_file, "wb");
	fwrite(&bfh, 1, sizeof bfh, fp);
	fwrite(&bmi, 1, sizeof bmi, fp);
	RGBQUAD bmp_pal[16];
	for (int i = 0; i < 16; ++i) {
		bmp_pal[i].rgbRed = palette[i * 3];
		bmp_pal[i].rgbGreen = palette[i * 3 + 1];
		bmp_pal[i].rgbBlue = palette[i * 3 + 2];
		bmp_pal[i].rgbReserved = 0;
	}
	fwrite(bmp_pal, 4, 16, fp);
	fwrite(bmp_buffer, 1, bpl * height, fp);
	fclose(fp);
	free(bmp_buffer);
	free(buf);
}

int main(int argc, const char* argv[])
{
	const char *input_files = "*.gra";
	if (argc == 2) {
		input_files = argv[1];
	}
	if (argc > 2) {
		printf("usage: gra2bmp [gra files]/n");
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
			struct gra_data data;
			printf("Processing %s ...", t.name);
			load_gra_data(t.name, &data);
			gra2bmp(&data, target_file);
			free_gra_data(&data);
			printf("Done./n");
		} while (_findnext(handle, &t) != -1);
	}

	return 0;
}
