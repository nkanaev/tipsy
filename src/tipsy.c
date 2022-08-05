#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include "tigr.h"

#define PI        3.14159
#define WIDTH     320
#define HEIGHT    240
#define FPS       30
#define DISTANCE  5
#define SCALE     0.75

#define VREF(x) (*(Vec*)(x))

// util

static void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

/*
static void debug(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  fprintf(stdout, "\n");
  fflush(stdout);
}
*/

static int readline(char **buf, size_t *buflen, FILE *f) {
  size_t i = 0;
  int ch;
  while ((ch = fgetc(f)) != EOF && ch != '\n') {
    if (ch == '\r') continue;
    if (i + 2 >= *buflen) *buf = realloc(*buf, *buflen = *buflen + 1024);
    (*buf)[i++] = (unsigned char)ch;
  }
  (*buf)[i] = '\0';
  return ch == EOF ? -1 : 0;
}

char *relpath(const char *name, const char *path) {
#ifdef _WIN32
  #define DIR_SEP '\\'
#else
  #define DIR_SEP '/'
#endif
  size_t ln = strlen(name);
  size_t lp = strlen(path);

  while (lp>0 && path[lp] != DIR_SEP) lp--;

  size_t lt = ln + lp + 1;

  char *out = malloc(sizeof(char)*(lt+1));
  strncpy(out, path, lp);
  out[lp] = DIR_SEP;
  strncpy(&out[lp+1], name, ln);
  out[lt] = '\0';

  return out;
}

char *trim(char *s) {
  char c;
  while ((c = s[0]) != '\0' && isspace(c)) s++;
  return s;
}

// list

typedef struct {
  char *p;
  int len, cap, size;
} list;

list *list_new(int size) {
  list *l = calloc(1, sizeof(list));
  l->size = size;
  return l;
}

void list_del(list *l) {
  free(l->p);
  free(l);
}

void list_add(list *l, void *obj) {
  if (l->len >= l->cap) {
    l->p = realloc(l->p, l->size * (l->cap = l->cap*2 ?: 2));
  }
  memcpy(l->p+(l->size*l->len++), obj, l->size);
}

void *list_get(list *l, int idx) {
  if (idx >= l->len) return NULL;
  return l->p+l->size*idx;
}

void list_sort(list *l, int (*comp)(const void *, const void *)) {
  qsort(l->p, l->len, l->size, comp);
}

// math

typedef struct {
  float x, y, z;
} Vec;

float vec_dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float vec_len(Vec a) { return sqrtf(vec_dot(a, a)); }
int vec_eq(Vec a, Vec b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
Vec vec_mul(Vec a, Vec b) { Vec c = {a.x * b.x, a.y * b.y, a.z * b.z}; return c; }
Vec vec_add(Vec a, Vec b) { Vec c = {a.x + b.x, a.y + b.y, a.z + b.z}; return c; }
Vec vec_sub(Vec a, Vec b) { Vec c = {a.x - b.x, a.y - b.y, a.z - b.z}; return c; }
Vec vec_nrm(Vec a) {
  float l = vec_len(a);
  Vec v = {1.0f/l, 1.0f/l, 1.0f/l};
  return vec_mul(a, v);
}
Vec vec_cross(Vec a, Vec b) {
  Vec c = {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
  return c;
}

Vec perspective(Vec v, Vec x, Vec y, Vec z) {
  Vec r = {vec_dot(v, x), vec_dot(v, y), vec_dot(v, z)};
  return r;
}

Vec project(Vec v0, int snap) {
  float ud = DISTANCE;
  float us = ud-1;
  float vs = fminf(HEIGHT, WIDTH)/2 * SCALE;

  Vec v1 = {(v0.x*us)/(v0.z+ud), (v0.y*us)/(v0.z+ud), (v0.z*us)/(v0.z+ud)};
  Vec v2 = {v1.x*vs+WIDTH/2, v1.y*vs+HEIGHT/2, v1.z+DISTANCE};

  if (snap) { v2.x = floorf(v2.x); v2.y = floorf(v2.y); }

  return v2;
}

Vec barycenter(Vec p, Vec v1, Vec v2, Vec v3) {
  float d = (v2.y-v3.y) * (v1.x-v3.x) + (v3.x-v2.x) * (v1.y - v3.y);
  float u = ((v2.y-v3.y) * (p.x-v3.x) + (v3.x-v2.x) * (p.y-v3.y)) / d;
  float v = ((v3.y-v1.y) * (p.x-v3.x) + (v1.x-v3.x) * (p.y-v3.y)) / d;
  float w = 1.0f - u - v;
  Vec ret = {u, v, w}; return ret;
}

// obj/mtl

typedef struct Mtl {
  char *name;
  Tigr *map_Ka, *map_Kd;

  struct Mtl *next;
} Mtl;

typedef struct {
  int v1, v2, v3;
  int vt1, vt2, vt3;
  int vn1, vn2, vn3;
  Mtl *mtl;
} Face;

typedef struct {
  list *v, *vn, *vt, *f;
  Mtl *mtl;
} Obj;

Mtl* mtl_readfile(const char *filepath) {
  FILE *f = fopen(filepath, "r");
  if (f == NULL) error("failed to open mtl file: %s", filepath);

  char *line = NULL, *tline = NULL;
  size_t len = 0;

  Mtl *m = NULL;

  while (readline(&line, &len, f) != -1) {
    tline = trim(line);
    if (strncmp(tline, "newmtl ", 7) == 0) {
      char *mtlname = &line[7];
      Mtl *newm = calloc(1, sizeof(Mtl));
      newm->name = strdup(mtlname);
      newm->next = m;
      m = newm;
    } else if (strncmp(tline, "map_Ka ", 7) == 0 && m != NULL) {
      char *imgname = strdup(tline+7);
      char *imgpath = relpath(imgname, filepath);
      Tigr *img = tigrLoadImage(imgpath);
      if (img == NULL) error("failed to open image: {%s}", imgpath);
      m->map_Ka = img;
      free(imgpath);
      free(imgname);
    } else if (strncmp(tline, "map_Kd ", 7) == 0 && m != NULL) {
      char *imgname = strdup(tline+7);
      char *imgpath = relpath(imgname, filepath);
      Tigr *img = tigrLoadImage(imgpath);
      if (img == NULL) error("failed to open image: {%s}", imgpath);
      m->map_Kd = img;
      free(imgpath);
      free(imgname);
    }
  }
  free(line);
  fclose(f);
  return m;
}

Mtl* mtl_get(Mtl *mtl, const char *name) {
  while (mtl != NULL && strcmp(mtl->name, name) != 0) mtl = mtl->next;
  return mtl;
}

void mtl_del(Mtl *mtl) {
  Mtl *next;
  while (mtl != NULL) {
    free(mtl->name);
    if (mtl->map_Ka != NULL) tigrFree(mtl->map_Ka);
    if (mtl->map_Kd != NULL) tigrFree(mtl->map_Kd);
    next = mtl->next;
    free(mtl);
    mtl = next;
  }
}

Obj* obj_readfile(char *filepath) {
  Obj *o = calloc(1, sizeof(Obj));
  o->v = list_new(sizeof(Vec));
  o->vn = list_new(sizeof(Vec));
  o->vt = list_new(sizeof(Vec));
  o->f = list_new(sizeof(Face));

  FILE *f = fopen(filepath, "r");
  if (f == NULL) error("failed to open obj file: %s", filepath);

  char *line = NULL;
  size_t len = 0;

  Mtl *mtl = NULL;

  while (readline(&line, &len, f) != -1) {
    Vec v = {0, 0, 0};
    if (strncmp(line, "v ", 2) == 0) {
      sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);
      list_add(o->v, &v);
    } else if (strncmp(line, "vn ", 3) == 0) {
      sscanf(line, "vn %f %f %f", &v.x, &v.y, &v.z);
      list_add(o->vn, &v);
    } else if (strncmp(line, "vt ", 3) == 0) {
      sscanf(line, "vt %f %f %f", &v.x, &v.y, &v.z);
      list_add(o->vt, &v);
    } else if (strncmp(line, "f ", 2) == 0) {
      int offset = 2;
      int i = 0, v1, v2, v3, vt1, vt2, vt3, vn1, vn2, vn3;
      while (++i) {
        int v = 0, vt = 0, vn = 0, nread = 0;
        if (!(
          sscanf(line+offset, "%d/%d/%d%n", &v, &vt, &vn, &nread) >= 3 ||
          sscanf(line+offset, "%d/%d%n", &v, &vt, &nread) >= 2 ||
          sscanf(line+offset, "%d//%d%n", &v, &vn, &nread) >= 2 ||
          sscanf(line+offset, "%d%n", &v, &nread) >= 1
        )) break;
        offset += nread;

        if (i == 1) { v1 = v; vt1 = vt; vn1 = vn; continue; }
        v2 = v3; v3 = v;
        vt2 = vt3; vt3 = vt;
        vn2 = vn3; vn3 = vn;

        if (i >= 3) {
          Face f = {
            .v1=v1, .v2=v2, .v3=v3, .vt1=vt1, .vt2=vt2, .vt3=vt3,
            .vn1=vn1, .vn2=vn2, .vn3=vn3, .mtl=mtl
          };
          list_add(o->f, &f);
        }
      }
    } else if (strncmp(line, "mtllib ", 7) == 0) {
      char *mtlname = line+7;
      char *mtlpath = relpath(mtlname, filepath);
      o->mtl = mtl_readfile(mtlpath);
      free(mtlpath);
    } else if (strncmp(line, "usemtl ", 7) == 0) {
      char *name = line+7;
      mtl = mtl_get(o->mtl, name);
      if (mtl == NULL) error("failed to find mtl: %s", name);
    }
  }
  free(line);
  fclose(f);

  return o;
}

void obj_del(Obj *o) {
  mtl_del(o->mtl);
  list_del(o->v);
  list_del(o->vn);
  list_del(o->vt);
  list_del(o->f);
  free(o);
}

void obj_normalize(Obj *obj) {
  Vec min = {FLT_MAX, FLT_MAX, FLT_MAX}, max = {FLT_MIN, FLT_MIN, FLT_MIN};
  for (int i = 0; i < obj->v->len; i++) {
    Vec v = VREF(list_get(obj->v, i));
    min.x = fminf(min.x, v.x); max.x = fmaxf(max.x, v.x);
    min.y = fminf(min.y, v.y); max.y = fmaxf(max.y, v.y);
    min.z = fminf(min.z, v.z); max.z = fmaxf(max.z, v.z);
  }

  Vec size = vec_sub(max, min);
  float W = fmaxf(fmaxf(size.x, size.y), size.z);

  for (int i = 0; i < obj->v->len; i++) {
    Vec *v = (Vec*)list_get(obj->v, i);
    v->x = ((v->x - min.x) / W*2 - size.x/W);
    v->y = -((v->y - min.y) / W*2 - size.y/W);
    v->z = -((v->z - min.z) / W*2 - size.z/W);
  }

  for (int i = 0; i < obj->vn->len; i++) {
    Vec *vn = (Vec*)list_get(obj->vn, i);
    Vec nrm = vec_nrm(*vn);
    vn->x = nrm.x;
    vn->y = -nrm.y;
    vn->z = -nrm.z;
  }
}

void obj_flip(Obj *obj) {
  for (int i = 0; i < obj->v->len; i++) {
    Vec *v = (Vec*)list_get(obj->v, i);
    v->y = -v->y;
    v->z = -v->z;
  }
  for (int i = 0; i < obj->vn->len; i++) {
    Vec *vn = (Vec*)list_get(obj->vn, i);
    vn->y = -vn->y;
    vn->z = -vn->z;
  }
}

// main

typedef struct {
  int draw_wireframe, use_zbuffer, use_pcorrect, inv_bculling, jitter;
  enum { SHADING_NONE = 0, SHADING_FLAT, SHADING_GOURAUD } shading;
  Vec x, y, z;
  float *zbuff;
} State;

typedef struct {
  Vec v1, v2, v3;
  Vec vn1, vn2, vn3;
  Vec nrm;
  int idx;
} Surface;

int surface_cmp(const void *a, const void *b) {
  Surface sa = *(Surface*)a;
  Surface sb = *(Surface*)b;

  float amaxz = fmaxf(fmaxf(sa.v1.z, sa.v2.z), sa.v3.z);
  float bmaxz = fmaxf(fmaxf(sb.v1.z, sb.v2.z), sb.v3.z);

  int apoints = (sa.v1.z < bmaxz) + (sa.v2.z < bmaxz) + (sa.v3.z < bmaxz);
  int bpoints = (sb.v1.z < amaxz) + (sb.v2.z < amaxz) + (sb.v3.z < amaxz);

  return apoints - bpoints;
}

int shade(Vec nrm, Vec bc) {
  static Vec lights = {-1, -1, -1};
  Vec var = {vec_dot(lights, nrm), vec_dot(lights, nrm), vec_dot(lights, nrm)};
  float intensity = vec_dot(bc, var);
  return 0xFF * fminf(fmaxf(0.3, intensity), 1);
}

void draw_wireframe(Tigr *scr, Surface sf, TPixel color) {
  tigrLine(scr, sf.v1.x, sf.v1.y, sf.v2.x, sf.v2.y, color);
  tigrLine(scr, sf.v2.x, sf.v2.y, sf.v3.x, sf.v3.y, color);
  tigrLine(scr, sf.v3.x, sf.v3.y, sf.v1.x, sf.v1.y, color);
}

void draw_surface(Tigr *scr, Obj *obj, Surface sf, State state) {
  Face f = *(Face*)(list_get(obj->f, sf.idx));

  Tigr *texture = NULL;
  if (f.mtl) texture = f.mtl->map_Ka ? f.mtl->map_Ka : f.mtl->map_Kd;
  if (texture == NULL) return;

  int shading = -1, has_normals = 0;
  Vec vn1, vn2, vn3;

  if (state.shading == SHADING_FLAT) {
    Vec bc = {0.333, 0.333, 0.333};
    shading = shade(sf.nrm, bc);
  }
  if (state.shading == SHADING_GOURAUD) {
    has_normals = f.vn1 > 0 && f.vn2 > 0 && f.vn3 > 0;
    if (has_normals) {
      vn1 = vec_nrm(VREF(list_get(obj->vn, f.vn1-1)));
      vn2 = vec_nrm(VREF(list_get(obj->vn, f.vn2-1)));
      vn3 = vec_nrm(VREF(list_get(obj->vn, f.vn3-1)));
      vn1 = perspective(vn1, state.x, state.y, state.z);
      vn2 = perspective(vn2, state.x, state.y, state.z);
      vn3 = perspective(vn3, state.x, state.y, state.z);
    }
  }

  Vec vt1 = VREF(list_get(obj->vt, f.vt1-1)),
      vt2 = VREF(list_get(obj->vt, f.vt2-1)),
      vt3 = VREF(list_get(obj->vt, f.vt3-1));

  int minX = fminf(fminf(sf.v1.x, sf.v2.x), sf.v3.x),
      maxX = fmaxf(fmaxf(sf.v1.x, sf.v2.x), sf.v3.x)+1,
      minY = fminf(fminf(sf.v1.y, sf.v2.y), sf.v3.y),
      maxY = fmaxf(fmaxf(sf.v1.y, sf.v2.y), sf.v3.y)+1;

  for (int y = minY; y < maxY; y++) {
    for (int x = minX; x < maxX; x++) {
      Vec p = {x, y, 0};
      Vec bc = barycenter(p, sf.v1, sf.v2, sf.v3);
      float err = -0.0001;
      if (bc.x >= err && bc.y >= err && bc.z >= err) {
        if (state.use_zbuffer) {
          float z = bc.x*sf.v1.z + bc.y*sf.v2.z + bc.z*sf.v3.z;
          int zbuff_idx = y * WIDTH + x;
          if (z > state.zbuff[zbuff_idx]) continue;
          state.zbuff[zbuff_idx] = z;
        }

        float u, v;

        if (state.use_pcorrect) {
          Vec bcc = bc;
          bcc.x = bc.x/(sf.v1.z);
          bcc.y = bc.y/(sf.v2.z);
          bcc.z = bc.z/(sf.v3.z);
          float bd = bcc.x+bcc.y+bcc.z;
          bcc.x = bcc.x/bd;
          bcc.y = bcc.y/bd;
          bcc.z = bcc.z/bd;

          u = bcc.x*vt1.x + bcc.y*vt2.x + bcc.z*vt3.x;
          v = 1.0-(bcc.x*vt1.y + bcc.y*vt2.y + bcc.z*vt3.y);
        } else {
          u = bc.x*vt1.x + bc.y*vt2.x + bc.z*vt3.x;
          v = 1.0-(bc.x*vt1.y + bc.y*vt2.y + bc.z*vt3.y);
        }

        int tx = texture->w * u;
        int ty = texture->h * v;

        TPixel texel = tigrGet(texture, tx % texture->w, ty % texture->h);

        if (state.shading == SHADING_GOURAUD && has_normals) {
          int s1 = shade(vn1, bc);
          int s2 = shade(vn2, bc);
          int s3 = shade(vn3, bc);
          shading = bc.x*s1 + bc.y*s2 + bc.z*s3;
        }

        if (shading >= 0) {
          texel.r = (texel.r * shading) >> 8;
          texel.g = (texel.g * shading) >> 8;
          texel.b = (texel.b * shading) >> 8;
        }
        tigrPlot(scr, x, y, texel);
      }
    }
  }
}

void draw(Tigr *scr, Obj *obj, State state, list *sfaces) {
  for (int i = 0; i < obj->f->len; i++) {
    Surface *sf = (Surface*)list_get(sfaces, i);
    Face f = *(Face*)list_get(obj->f, sf->idx);
    sf->v1 = perspective(VREF(list_get(obj->v, f.v1-1)), state.x, state.y, state.z);
    sf->v2 = perspective(VREF(list_get(obj->v, f.v2-1)), state.x, state.y, state.z);
    sf->v3 = perspective(VREF(list_get(obj->v, f.v3-1)), state.x, state.y, state.z);
    sf->nrm = vec_nrm(vec_cross(vec_sub(sf->v2, sf->v1), vec_sub(sf->v3, sf->v1)));
    sf->v1 = project(sf->v1, state.jitter);
    sf->v2 = project(sf->v2, state.jitter);
    sf->v3 = project(sf->v3, state.jitter);
  }

  if (!state.use_zbuffer) list_sort(sfaces, surface_cmp);

  for (int i = 0; i < sfaces->len; i++) {
    Surface *sf = (Surface*)list_get(sfaces, i);
    if (state.draw_wireframe) {
      draw_wireframe(scr, *sf, tigrRGB(0xFF, 0xFF, 0xFF));
    } else {
      Vec forward = {0, 0, -1};
      float inv = state.inv_bculling ? -1 : 1;
      if (vec_dot(sf->nrm, forward) * inv > 0) draw_surface(scr, obj, *sf, state);
    }
  }
}

int main(int argc, char **argv) {
  if (argc != 2) error("usage: %s path/to/obj", argv[0]);

  char *filepath = argv[1];
  Obj *obj = obj_readfile(filepath);
  obj_normalize(obj);

  list *sfaces = list_new(sizeof(Surface));
  for (int i = 0; i < obj->f->len; i++) {
    Surface sf;
    sf.idx = i;
    list_add(sfaces, &sf);
  }

  printf("%d vertices, %d faces\n", obj->v->len, obj->f->len);

  Tigr *screen = tigrWindow(WIDTH, HEIGHT, "tipsy", TIGR_FIXED | TIGR_RETINA);
  TPixel colorBlack = tigrRGB(0, 0, 0);

  State state = {
    .draw_wireframe=0, .use_zbuffer=0, .use_pcorrect=0,
    .inv_bculling=0, .jitter=1,
  };
  state.zbuff = malloc(sizeof(float) * (WIDTH * HEIGHT));

  Vec upward = {0, 1, 0};
  float rotX = 0, rotY = 0, sensitivity = 0.05;
  int mouseX, mouseY, mouseBtn, mousePrev = 0, mousePrevX = 0, mousePrevY = 0;

  float elapsed = 1;
  int input = 1;

  while (!tigrClosed(screen) && !tigrKeyDown(screen, TK_ESCAPE)) {
    // FPS cap
    elapsed += tigrTime();
    if (elapsed < 1.0/FPS) { continue; } else { elapsed = 0; }

    if (tigrKeyHeld(screen, TK_LEFT)  && (input = 1)) rotY -= sensitivity;
    if (tigrKeyHeld(screen, TK_RIGHT) && (input = 1)) rotY += sensitivity;
    if (tigrKeyHeld(screen, TK_DOWN)  && (input = 1)) rotX -= sensitivity;
    if (tigrKeyHeld(screen, TK_UP)    && (input = 1)) rotX += sensitivity;
    if (tigrKeyDown(screen, 'W') && (input = 1)) state.draw_wireframe ^= 1;
    if (tigrKeyDown(screen, 'Z') && (input = 1)) state.use_zbuffer ^= 1;
    if (tigrKeyDown(screen, 'P') && (input = 1)) state.use_pcorrect ^= 1;
    if (tigrKeyDown(screen, 'C') && (input = 1)) state.inv_bculling ^= 1;
    if (tigrKeyDown(screen, 'J') && (input = 1)) state.jitter ^= 1;
    if (tigrKeyDown(screen, 'F') && (input = 1)) obj_flip(obj);
    if (tigrKeyDown(screen, 'R') && (input = 1)) rotX = rotY = 0;
    if (tigrKeyDown(screen, '1') && (input = 1)) state.shading = SHADING_NONE;
    if (tigrKeyDown(screen, '2') && (input = 1)) state.shading = SHADING_FLAT;
    if (tigrKeyDown(screen, '3') && (input = 1)) state.shading = SHADING_GOURAUD;

    tigrMouse(screen, &mouseX, &mouseY, &mouseBtn);
    if (mouseBtn & 1) {
      if (mousePrev) {
        rotY -= (mousePrevX-mouseX)*sensitivity;
        rotX += (mousePrevY-mouseY)*sensitivity;
        input = 1;
      }
      mousePrevX = mouseX;
      mousePrevY = mouseY;
      mousePrev = 1;
    } else {
      mousePrev = 0;
    }

    if (!input) { tigrUpdate(screen); continue; } else { input = 0; }

    rotX = fminf(PI/2, fmaxf(rotX, -PI/2));

    Vec z = {cos(rotX)*sin(rotY), -sin(rotX), cos(rotX)*cos(rotY)};
    Vec x = vec_nrm(vec_cross(upward, z));
    Vec y = vec_cross(z, x);

    if (state.use_zbuffer)
      for (int i = 0; i < WIDTH*HEIGHT; state.zbuff[i++] = FLT_MAX);

    state.x = x; state.y = y; state.z = z;
    tigrClear(screen, colorBlack);
    draw(screen, obj, state, sfaces);
    tigrUpdate(screen);
  }

  tigrFree(screen);
  obj_del(obj);
  list_del(sfaces);
  free(state.zbuff);
  return 0;
}
