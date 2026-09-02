/* Key binding functions */
#ifndef FUNCTIONS_H
#define FUNCTIONS_H
/* These prototypes must match the linkage of the definitions that get pulled in
 * later (vanitygaps.c, movestack.c, shiftview.c). Those definitions are
 * non-static, except bstack/bstackhoriz/tile which are static in vanitygaps.c.
 * Keeping them in sync avoids "static declaration follows non-static" errors
 * when a tool (clangd) parses this header after those .c files. */
void defaultgaps(const Arg *arg);
void incrgaps(const Arg *arg);
void incrigaps(const Arg *arg);
void incrogaps(const Arg *arg);
void incrohgaps(const Arg *arg);
void incrovgaps(const Arg *arg);
void incrihgaps(const Arg *arg);
void incrivgaps(const Arg *arg);
void togglegaps(const Arg *arg);
/* Layouts (delete the ones you do not need) */
static void bstack(Monitor *m);
static void bstackhoriz(Monitor *m);
void centeredmaster(Monitor *m);
void centeredfloatingmaster(Monitor *m);
void deck(Monitor *m);
void dwindle(Monitor *m);
void fibonacci(Monitor *m, int s);
void gaplessgrid(Monitor *m);
void grid(Monitor *m);
void horizgrid(Monitor *m);
void nrowgrid(Monitor *m);
void spiral(Monitor *m);
static void tile(Monitor *m);
/* Internals */
void getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv,
             unsigned int *nc);
void getfacts(Monitor *m, int msize, int ssize, float *mf, float *sf,
              int *mr, int *sr);
void setgaps(int oh, int ov, int ih, int iv);

void movestack(const Arg *arg);
void shiftview(const Arg *arg);

#endif /* FUNCTIONS_H */
