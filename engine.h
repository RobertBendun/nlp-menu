#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

struct item {
	char *text;
	struct item *left, *right;
	int out;
};

extern struct item *items;
extern struct item *prev, *curr, *next, *sel;
extern struct item *matches, *matchend;
extern unsigned lines;

void cleanup(void);

void on_input_callback(char const*);
void choose();

#ifdef __cplusplus
}
#endif
