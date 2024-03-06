#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/stat.h>
typedef struct meta_data
{
	char *file_name;
	int size, ele_size;
	FILE *fpt;
	void *data;
}m_data;
typedef struct dictionary_lzw
{
	char **arr;
	int *str_len;
	int size;
	int top;
}dict_lzw;
int get_file_size(char *filename)
{
	struct stat file_status;
	if (stat(filename, &file_status) < 0) return -1;
	return file_status.st_size;
}
void read_data(m_data *meta_d)
{
	meta_d->data = (void*)calloc(meta_d->size, meta_d->ele_size);
	meta_d->fpt = fopen(meta_d->file_name, "r");
	fread(meta_d->data, meta_d->ele_size, meta_d->size/meta_d->ele_size, meta_d->fpt);
	fclose(meta_d->fpt);

}
void write_data(m_data *meta_d)
{
	meta_d->fpt = fopen(meta_d->file_name, "w");
	fwrite(meta_d->data, meta_d->ele_size, meta_d->size, meta_d->fpt);
	fclose(meta_d->fpt);
}
void init_dict(dict_lzw *dict)
{
	dict->size = 1<<16;
	dict->top = 0;
	dict->arr = (char**)calloc(dict->size, sizeof(char*));
	dict->str_len = (int*)calloc(dict->size, sizeof(int));
	char c = 0;
	int i = 0;
	for(i=0;i<256;i++,c++) {dict->arr[i] = strndup(&c, 1);dict->str_len[i] = 1;}
	dict->top = i;
}
void clear_dict(dict_lzw *dict)
{
	for(int i=0;i<dict->top;i++) free(dict->arr[i]);
	free(dict->arr);
	free(dict->str_len);
}
void add_to_dict(dict_lzw *dict, const unsigned char *pattern, const int len)
{
	if(dict->top == dict->size){printf("LZW dict is full (%d/%d)\n", dict->top, dict->size);exit(-1);}
	dict->arr[dict->top] = strndup((const char*)pattern, len);
	dict->str_len[dict->top++] = len;
}
int str_n_cmp(const unsigned char *p, const unsigned char *s, int n)
{
	int i=0;
	while(i<n && p[i] == s[i]) i++;
	return i==n?0:p[i]-s[i];;
}
int scan_dict(dict_lzw *dict, const unsigned char *pattern, int len)
{
	for(int i=0;i<dict->top;i++) if(dict->str_len[i] == len && !str_n_cmp((unsigned char*)dict->arr[i], pattern, len)) return i;
	return -1;
}
int cpy_idx_data(dict_lzw *dict, unsigned char *data, int idx)
{
	strncpy((char*)data, dict->arr[idx], dict->str_len[idx]);
	return dict->str_len[idx];
}
void lzw_decompress(m_data *in_data, m_data *out_data)
{
	in_data->ele_size = sizeof(unsigned short);
	read_data(in_data);
	dict_lzw dict;
	init_dict(&dict);
	const unsigned short *p = in_data->data, *c = p+1;
	unsigned char *data = (unsigned char *)calloc(1<<20, sizeof(unsigned char));
	unsigned char *ptrn = (unsigned char *)calloc(1<<15, sizeof(unsigned char));
	unsigned char const *ori = data;
	int i=1, n = in_data->size/in_data->ele_size;
	data += cpy_idx_data(&dict, data, *p);
	while(i++ < n)
	{
		if(*c < dict.top)
		{
			data += cpy_idx_data(&dict, data, *c);
			strncpy((char*)ptrn, dict.arr[*p], dict.str_len[*p]);
			*(ptrn+dict.str_len[*p]) = dict.arr[*c][0];
			add_to_dict(&dict, ptrn, dict.str_len[*p]+1);
		}
		else
		{
			strncpy((char*)ptrn, dict.arr[*p], dict.str_len[*p]);
			*(ptrn+dict.str_len[*p]) = dict.arr[*p][0];
			add_to_dict(&dict, ptrn, dict.str_len[*p]+1);
			data += cpy_idx_data(&dict, data, dict.top-1);
		}
		p = c++;
	}
	out_data->data = (void *)ori;
	out_data->size = data - ori;
	out_data->ele_size = sizeof(unsigned char);
	write_data(out_data);
	clear_dict(&dict);
	free((void *)ori);
	free(ptrn);
}
void lzw_compress(m_data *in_data, m_data *out_data)
{
	in_data->ele_size = sizeof(unsigned char);
	read_data(in_data);
	dict_lzw dict;
	init_dict(&dict);
	const unsigned char *c = in_data->data, *p = in_data->data;
	unsigned short *data = (unsigned short *)calloc(in_data->size, sizeof(unsigned short));
	int i = 0, j=0, code_id, p_code_id;
	while(i<in_data->size)
	{
		code_id = scan_dict(&dict, p, c-p+1);
		if(code_id!=-1){c++;i++;}
		else
		{
			add_to_dict(&dict, p, c-p+1);
			data[j++] = (unsigned short)p_code_id;
			p = c;
		}
		p_code_id = code_id;
	}
	data[j++] = code_id;
	out_data->size = j;
	out_data->data = data;
	out_data->ele_size = sizeof(unsigned short);
	write_data(out_data);
	clear_dict(&dict);
	free(data);
}
int main(int argc, char **argv)
{
	m_data in_data, out_data;
	int comp = 1;
	if(argc != 4)
	{
		printf("usage: %s input_file_name, compressed_file_name, 1(compress) or 0(decompress)\n", argv[0]);
		exit(0);
	}
	in_data.file_name = strdup(argv[1]);
	out_data.file_name = strdup(argv[2]);
	comp = argc == 4?atoi(argv[3]):1;
	in_data.size = get_file_size(in_data.file_name);
	if(comp) lzw_compress(&in_data, &out_data);
	else lzw_decompress(&in_data, &out_data);
	printf("in data size: %d bytes\n", in_data.size);
	printf("out data size: %d bytes\n", out_data.size*out_data.ele_size);
	return 0;
}