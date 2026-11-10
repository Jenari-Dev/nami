/* Nami - PS5 homebrew shell UI (M3b: real names + launch)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Installed titles now show their real names (from app.db's
 * tbl_conceptmetadata) and launch via sceSystemServiceLaunchApp. Payloads
 * launch via the bootstrap request. Icons/pictures are M3c. Theme is still
 * placeholder.
 */
#include <SDL.h>
#include <SDL_test_font.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <sqlite3.h>

#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#define SCREEN_W 1920
#define SCREEN_H 1080
#define WEB_PORT 8080

#define FOCUS_X   260
#define CAT_GAP   340
#define BAR_Y     330
#define ITEM_Y0   430
#define ITEM_DY   60
#define VIS_ITEMS 9
#define MAX_ITEMS 128
#define REQUEST_PATH "/data/ps5_homebrew_launcher_request.txt"
#define APP_DB "/system_data/priv/mms/app.db"

/* Installed-title launch. LncAppParam layout + the launch sequence are from
 * LightningMods/Itemzflow (GPL-3, same licence as Nami). Passing NULL for
 * param yields 0x80940005; it must carry its size, the foreground user id,
 * and the skip-update flag. */
typedef struct {
    uint32_t sz;
    uint32_t user_id;
    uint32_t app_opt;
    uint64_t crash_report;
    uint32_t check_flag;
} LncAppParam;
#define LNC_SKIP_SYSTEM_UPDATE_CHECK 2u

int sceUserServiceGetForegroundUser(uint32_t *userId);
int sceSystemServiceLaunchApp(const char *titleId, const char *const *argv,
                              LncAppParam *param);
int sceLncUtilLaunchApp(const char *titleId, const char *const *argv,
                        LncAppParam *param);

/* Returns 0 on success, else the (negative/0x80..) error code. */
static int
launch_title(const char *title_id) {
    uint32_t user_id = 0;
    sceUserServiceGetForegroundUser(&user_id);

    LncAppParam param;
    param.sz = sizeof(param);
    param.user_id = user_id;
    param.app_opt = 0;
    param.crash_report = 0;
    param.check_flag = LNC_SKIP_SYSTEM_UPDATE_CHECK;

    unsigned rc = (unsigned)sceLncUtilLaunchApp(title_id, NULL, &param);
    if (rc & 0x80000000u) {
        rc = (unsigned)sceSystemServiceLaunchApp(title_id, NULL, &param);
    }
    return (rc & 0x80000000u) ? (int)rc : 0;
}

enum { KIND_INFO, KIND_PAYLOAD, KIND_TITLE, KIND_DIR, KIND_DESKTOP };

typedef struct {
    char label[128];
    char path[512];   /* ELF path (payload) or titleId (title) */
    char icon[256];   /* icon0.png path for a title, else empty */
    int  kind;
} Item;

typedef struct {
    const char *name;
    Uint8 r, g, b;
    Item items[MAX_ITEMS];
    int  count;
} Category;

enum { CAT_GAMES, CAT_PAYLOADS, CAT_FILES, CAT_NETWORK, CAT_DISC,
       CAT_SETTINGS, CAT_POWER, N_CATS };

static Category g_cats[N_CATS] = {
    [CAT_GAMES]    = {"GAMES",    0x7f, 0xd0, 0xff, {{{0}}}, 0},
    [CAT_PAYLOADS] = {"PAYLOADS", 0xbd, 0xa6, 0xff, {{{0}}}, 0},
    [CAT_FILES]    = {"FILES",    0x8a, 0xe6, 0xb4, {{{0}}}, 0},
    [CAT_NETWORK]  = {"NETWORK",  0x6f, 0xd0, 0xe8, {{{0}}}, 0},
    [CAT_DISC]     = {"DISC",     0xff, 0xcf, 0x7a, {{{0}}}, 0},
    [CAT_SETTINGS] = {"SETTINGS", 0xc0, 0xc8, 0xd8, {{{0}}}, 0},
    [CAT_POWER]    = {"POWER",    0xff, 0x8a, 0x8a, {{{0}}}, 0},
};

/* titleId -> display name map, loaded once from app.db. */
#define MAX_NAMES 256
typedef struct { char tid[16]; char name[96]; char icon[256]; } TitleName;
static TitleName g_names[MAX_NAMES];
static int g_name_count;

/* Pull a <4 letters><5 digits> title id out of a path if present. */
static int
tid_from_path(const char *s, char *out, size_t n) {
    for (const char *p = s; *p; p++) {
        int ok = 1;
        for (int i = 0; i < 4; i++) {
            if (!isupper((unsigned char)p[i])) { ok = 0; break; }
        }
        if (!ok) continue;
        for (int i = 4; i < 9; i++) {
            if (!isdigit((unsigned char)p[i])) { ok = 0; break; }
        }
        if (ok) {
            snprintf(out, n, "%.9s", p);
            return 1;
        }
    }
    return 0;
}

/* Map one Unicode codepoint to a close ASCII letter (macrons, accents) so
 * the 8x8 debug font can show real names until the TTF theme pass. */
static char
cp_to_ascii(unsigned cp) {
    switch (cp) {   /* Latin Extended-A (macrons used in JP romanization) */
        case 0x100: case 0x102: case 0x104: return 'A';
        case 0x101: case 0x103: case 0x105: return 'a';
        case 0x112: case 0x114: case 0x116: case 0x118: case 0x11A: return 'E';
        case 0x113: case 0x115: case 0x117: case 0x119: case 0x11B: return 'e';
        case 0x128: case 0x12A: case 0x12C: case 0x12E: return 'I';
        case 0x129: case 0x12B: case 0x12D: case 0x12F: return 'i';
        case 0x14C: case 0x14E: case 0x150: return 'O';
        case 0x14D: case 0x14F: case 0x151: return 'o';
        case 0x168: case 0x16A: case 0x16C: case 0x16E: case 0x170: return 'U';
        case 0x169: case 0x16B: case 0x16D: case 0x16F: case 0x171: return 'u';
        default: break;
    }
    if (cp >= 0xC0 && cp <= 0xFF) {   /* Latin-1 supplement */
        static const char lo[] = "aaaaaaaceeeeiiiidnooooo.ouuuuy.y";
        if (cp == 0xDF) return 's';
        int idx = (cp >= 0xE0) ? (int)(cp - 0xE0) : (int)(cp - 0xC0);
        char c = lo[idx];
        if (c == '.') return 0;
        return (cp < 0xE0) ? (char)toupper((unsigned char)c) : c;
    }
    return 0;   /* unknown -> drop */
}

static void
sanitize_ascii(char *dst, size_t n, const char *src) {
    size_t o = 0;
    const unsigned char *p = (const unsigned char *)src;
    while (*p && o + 1 < n) {
        unsigned c = *p;
        if (c < 0x80) { dst[o++] = (char)c; p++; continue; }
        int len; unsigned cp;
        if ((c & 0xE0) == 0xC0)      { cp = c & 0x1F; len = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 3; }
        else { p++; continue; }
        p++;
        for (int i = 0; i < len && (*p & 0xC0) == 0x80; i++) {
            cp = (cp << 6) | (*p & 0x3F);
            p++;
        }
        char a = cp_to_ascii(cp);
        if (a) dst[o++] = a;
    }
    dst[o] = 0;
}

static void
add_name(const char *tid, const char *name, const char *icon) {
    if (!tid || !tid[0] || !name || g_name_count >= MAX_NAMES) {
        return;
    }
    for (int i = 0; i < g_name_count; i++) {
        if (strcmp(g_names[i].tid, tid) == 0) return;   /* first wins */
    }
    TitleName *t = &g_names[g_name_count++];
    snprintf(t->tid, sizeof(t->tid), "%s", tid);
    sanitize_ascii(t->name, sizeof(t->name), name);
    t->icon[0] = 0;
    if (icon && icon[0]) {
        snprintf(t->icon, sizeof(t->icon), "%s", icon);
        char *q = strchr(t->icon, '?');   /* drop the ?ts= cache-buster */
        if (q) *q = 0;
    }
}

static const char *
name_for(const char *tid) {
    for (int i = 0; i < g_name_count; i++) {
        if (strcmp(g_names[i].tid, tid) == 0) return g_names[i].name;
    }
    return NULL;
}

static const char *
icon_for(const char *tid) {
    for (int i = 0; i < g_name_count; i++) {
        if (strcmp(g_names[i].tid, tid) == 0) return g_names[i].icon;
    }
    return NULL;
}

static void
load_title_names(void) {
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(APP_DB, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return;
    }
    sqlite3_stmt *st = NULL;
    const char *q = "SELECT localConceptId, conceptName, icon0Info "
                    "FROM tbl_conceptmetadata";
    if (sqlite3_prepare_v2(db, q, -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *lci = (const char *)sqlite3_column_text(st, 0);
            const char *nm  = (const char *)sqlite3_column_text(st, 1);
            const char *ic  = (const char *)sqlite3_column_text(st, 2);
            char tid[16] = "";
            if (lci && strncmp(lci, "cid:local:", 10) == 0) {
                snprintf(tid, sizeof(tid), "%s", lci + 10);
            } else if (ic) {
                tid_from_path(ic, tid, sizeof(tid));
            }
            if (tid[0] && nm) {
                add_name(tid, nm, ic);
            }
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
}

static void
add_item(Category *c, const char *label, const char *path, int kind) {
    if (c->count >= MAX_ITEMS) return;
    Item *it = &c->items[c->count++];
    snprintf(it->label, sizeof(it->label), "%s", label);
    snprintf(it->path, sizeof(it->path), "%s", path ? path : "");
    it->kind = kind;
}

static int
has_suffix(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

static void
scan_elfs(Category *c, const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' || !has_suffix(e->d_name, ".elf")) continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        add_item(c, e->d_name, full, KIND_PAYLOAD);
    }
    closedir(d);
}

static void
scan_titles(Category *c, const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "CUSA", 4) != 0 &&
            strncmp(e->d_name, "PPSA", 4) != 0) {
            continue;
        }
        const char *nm = name_for(e->d_name);
        char label[128];
        snprintf(label, sizeof(label), "%s", nm ? nm : e->d_name);
        add_item(c, label, e->d_name, KIND_TITLE);

        /* Attach the title's own icon0.png: from the DB, else probe the two
         * standard on-disk locations. */
        Item *it = &c->items[c->count - 1];
        const char *ic = icon_for(e->d_name);
        if (ic && ic[0] && access(ic, F_OK) == 0) {
            snprintf(it->icon, sizeof(it->icon), "%s", ic);
        } else {
            char g[256];
            snprintf(g, sizeof(g), "/system_ex/app/%s/sce_sys/icon0.png", e->d_name);
            if (access(g, F_OK) == 0) {
                snprintf(it->icon, sizeof(it->icon), "%s", g);
            } else {
                snprintf(g, sizeof(g), "/user/appmeta/%s/icon0.png", e->d_name);
                if (access(g, F_OK) == 0) {
                    snprintf(it->icon, sizeof(it->icon), "%s", g);
                }
            }
        }
    }
    closedir(d);
}

static void
build_content(void) {
    load_title_names();

    scan_titles(&g_cats[CAT_GAMES], "/system_ex/app");
    if (g_cats[CAT_GAMES].count == 0) {
        add_item(&g_cats[CAT_GAMES], "(no installed titles found)", "", KIND_INFO);
    }

    scan_elfs(&g_cats[CAT_PAYLOADS], "/data/elf-arsenal/payloadmgr");
    scan_elfs(&g_cats[CAT_PAYLOADS], "/data");
    if (g_cats[CAT_PAYLOADS].count == 0) {
        add_item(&g_cats[CAT_PAYLOADS], "(no payloads found)", "", KIND_INFO);
    }

    add_item(&g_cats[CAT_FILES], "Open Desktop (windowed)", "", KIND_DESKTOP);
    add_item(&g_cats[CAT_FILES], "Browse / (root)", "/", KIND_DIR);
    add_item(&g_cats[CAT_FILES], "/data", "/data", KIND_DIR);
    add_item(&g_cats[CAT_FILES], "/user", "/user", KIND_DIR);
    add_item(&g_cats[CAT_FILES], "/mnt/usb0", "/mnt/usb0", KIND_DIR);

    add_item(&g_cats[CAT_NETWORK], "FTP server (running :2121)", "", KIND_INFO);
    add_item(&g_cats[CAT_NETWORK], "Web UI :8080 - M9", "", KIND_INFO);
    add_item(&g_cats[CAT_NETWORK], "IP info", "", KIND_INFO);

    add_item(&g_cats[CAT_DISC], "Drive status", "", KIND_INFO);
    add_item(&g_cats[CAT_DISC], "Activate drive (research)", "", KIND_INFO);

    add_item(&g_cats[CAT_SETTINGS], "Video", "", KIND_INFO);
    add_item(&g_cats[CAT_SETTINGS], "Audio", "", KIND_INFO);
    add_item(&g_cats[CAT_SETTINGS], "System", "", KIND_INFO);
    add_item(&g_cats[CAT_SETTINGS], "About Nami", "", KIND_INFO);

    add_item(&g_cats[CAT_POWER], "Exit Nami", "", KIND_INFO);
}

static void
request_launch(const char *elf_path) {
    char cwd[512];
    snprintf(cwd, sizeof(cwd), "%s", elf_path);
    char *slash = strrchr(cwd, '/');
    if (slash) *slash = 0; else snprintf(cwd, sizeof(cwd), "/data");
    FILE *fp = fopen(REQUEST_PATH, "w");
    if (!fp) return;
    fprintf(fp, "%s\n%s\n", elf_path, cwd);
    fclose(fp);
}

static void
local_ipv4(char *out, size_t n) {
    struct ifaddrs *ifs = NULL, *p;
    snprintf(out, n, "offline");
    if (getifaddrs(&ifs) != 0) return;
    for (p = ifs; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        struct sockaddr_in *sin = (struct sockaddr_in *)p->ifa_addr;
        const char *ip = inet_ntoa(sin->sin_addr);
        if (ip && strcmp(ip, "127.0.0.1") != 0) {
            snprintf(out, n, "%s", ip);
            break;
        }
    }
    freeifaddrs(ifs);
}

static void
fill(SDL_Renderer *r, int x, int y, int w, int h,
     Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_RenderFillRect(r, &rect);
}

/* Real typography via SDL2_ttf (DejaVuSans, bundled), cached per
 * string+size+colour; falls back to the 8x8 debug font if the TTF is absent
 * so the UI never breaks. */
#define FONT_PATH "/data/nami/nami.ttf"
static TTF_Font *g_font[5];     /* indexed by scale 1..4 */

typedef struct {
    char s[96];
    int scale;
    Uint32 rgb;
    SDL_Texture *tex;
    int w, h;
    unsigned used;
} TxCache;
#define TXC 384
static TxCache g_txc[TXC];
static unsigned g_txtick;

static void
text(SDL_Renderer *r, int x, int y, int scale,
     Uint8 cr, Uint8 cg, Uint8 cb, const char *s) {
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    if (!s) s = "";
    TTF_Font *f = g_font[scale];
    if (!f || !s[0]) {
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
        SDL_RenderSetScale(r, (float)scale, (float)scale);
        SDLTest_DrawString(r, (float)x / scale, (float)y / scale, s);
        SDL_RenderSetScale(r, 1.0f, 1.0f);
        return;
    }
    Uint32 rgb = ((Uint32)cr << 16) | ((Uint32)cg << 8) | cb;
    g_txtick++;

    TxCache *slot = NULL;
    int freeidx = -1, oldidx = 0;
    unsigned oldest = 0xffffffffu;
    for (int i = 0; i < TXC; i++) {
        TxCache *c = &g_txc[i];
        if (!c->tex) { if (freeidx < 0) freeidx = i; continue; }
        if (c->scale == scale && c->rgb == rgb &&
            strncmp(c->s, s, sizeof(c->s) - 1) == 0) { slot = c; break; }
        if (c->used < oldest) { oldest = c->used; oldidx = i; }
    }
    if (!slot) {
        int idx = (freeidx >= 0) ? freeidx : oldidx;
        TxCache *c = &g_txc[idx];
        if (c->tex) { SDL_DestroyTexture(c->tex); c->tex = NULL; }
        SDL_Color col = {cr, cg, cb, 255};
        SDL_Surface *surf = TTF_RenderUTF8_Blended(f, s, col);
        if (!surf) return;
        c->tex = SDL_CreateTextureFromSurface(r, surf);
        c->w = surf->w; c->h = surf->h;
        c->scale = scale; c->rgb = rgb;
        snprintf(c->s, sizeof(c->s), "%s", s);
        SDL_FreeSurface(surf);
        slot = c;
    }
    slot->used = g_txtick;
    if (slot->tex) {
        SDL_Rect dst = {x, y, slot->w, slot->h};
        SDL_RenderCopy(r, slot->tex, NULL, &dst);
    }
}

static void
fonts_load(void) {
    if (TTF_Init() != 0) return;
    static const int sizes[5] = {0, 15, 21, 30, 40};  /* scale 1..4 */
    for (int sc = 1; sc <= 4; sc++) {
        g_font[sc] = TTF_OpenFont(FONT_PATH, sizes[sc]);
    }
}

static void
draw_glyph(SDL_Renderer *r, int cx, int cy, int size, int active,
           Uint8 cr, Uint8 cg, Uint8 cb) {
    int s = active ? size : size - 10;
    Uint8 a = active ? 255 : 150;
    fill(r, cx - s, cy - s, s * 2, s * 2, cr, cg, cb, (Uint8)(a / 4));
    fill(r, cx - s + 6, cy - s + 6, s * 2 - 12, s * 2 - 12, cr, cg, cb, a);
    if (active) {
        fill(r, cx - s, cy - s, s * 2, 3, 0xff, 0xff, 0xff, 200);
    }
}

/* ---- file browser (FILES category) ---- */

#define BR_MAX 1024
typedef struct { char name[256]; int is_dir; long long size; } FEntry;
static FEntry g_ents[BR_MAX];

static void
fmt_size(long long b, char *out, size_t n) {
    static const char *u[] = {"B", "KB", "MB", "GB", "TB"};
    double v = (double)b;
    int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; i++; }
    if (i == 0) snprintf(out, n, "%lld B", b);
    else        snprintf(out, n, "%.1f %s", v, u[i]);
}

static int
fentry_cmp(const void *a, const void *b) {
    const FEntry *x = a, *y = b;
    if (strcmp(x->name, "..") == 0) return -1;
    if (strcmp(y->name, "..") == 0) return 1;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;  /* dirs first */
    return strcasecmp(x->name, y->name);
}

static int
load_dir(const char *path, FEntry *out, int max) {
    DIR *d = opendir(path);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        if (strcmp(e->d_name, ".") == 0) continue;
        FEntry *f = &out[n];
        snprintf(f->name, sizeof(f->name), "%s", e->d_name);
        char full[1280];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        struct stat st;
        if (stat(full, &st) == 0) {
            f->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
            f->size = (long long)st.st_size;
        } else {
            f->is_dir = 0;
            f->size = 0;
        }
        n++;
    }
    closedir(d);
    qsort(out, n, sizeof(FEntry), fentry_cmp);
    return n;
}

static void
path_up(char *path) {
    char *s = strrchr(path, '/');
    if (s && s != path) *s = 0;
    else                strcpy(path, "/");
}

static void
browse(SDL_Renderer *ren, SDL_GameController *pad, const char *start) {
    (void)pad;
    char path[1024];
    snprintf(path, sizeof(path), "%s", start);
    int n = load_dir(path, g_ents, BR_MAX);
    int sel = 0, top = 0, running = 1;
    const int VIS = 15;
    char info[320] = "";

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            int up = 0, down = 0, enter = 0, back = 0;
            if (e.type == SDL_QUIT) {
                running = 0;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = 0; break;
                    case SDLK_UP:     up = 1; break;
                    case SDLK_DOWN:   down = 1; break;
                    case SDLK_RETURN: enter = 1; break;
                    case SDLK_BACKSPACE: back = 1; break;
                    default: break;
                }
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:   up = 1; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: down = 1; break;
                    case SDL_CONTROLLER_BUTTON_A:         enter = 1; break;
                    case SDL_CONTROLLER_BUTTON_B:         back = 1; break;
                    case SDL_CONTROLLER_BUTTON_START:     running = 0; break;
                    default: break;
                }
            }

            if (up && sel > 0) sel--;
            if (down && sel < n - 1) sel++;
            if (sel < top) top = sel;
            if (sel >= top + VIS) top = sel - VIS + 1;

            if (back) {
                path_up(path);
                n = load_dir(path, g_ents, BR_MAX);
                sel = 0; top = 0; info[0] = 0;
            }
            if (enter && n > 0) {
                FEntry *f = &g_ents[sel];
                if (strcmp(f->name, "..") == 0) {
                    path_up(path);
                    n = load_dir(path, g_ents, BR_MAX);
                    sel = 0; top = 0; info[0] = 0;
                } else if (f->is_dir) {
                    char np[1024];
                    snprintf(np, sizeof(np), "%s%s%s",
                             path, (strcmp(path, "/") == 0) ? "" : "/", f->name);
                    snprintf(path, sizeof(path), "%s", np);
                    n = load_dir(path, g_ents, BR_MAX);
                    sel = 0; top = 0; info[0] = 0;
                } else {
                    char sz[32];
                    fmt_size(f->size, sz, sizeof(sz));
                    snprintf(info, sizeof(info), "%s   %s", f->name, sz);
                }
            }
        }

        for (int band = 0; band < SCREEN_H; band += 6) {
            float t = (float)band / SCREEN_H;
            Uint8 rr = (Uint8)(0x0a + (0x17 - 0x0a) * (1.0f - t));
            Uint8 gg = (Uint8)(0x13 + (0x36 - 0x13) * (1.0f - t));
            Uint8 bb = (Uint8)(0x22 + (0x5c - 0x22) * (1.0f - t));
            fill(ren, 0, band, SCREEN_W, 6, rr, gg, bb, 255);
        }

        text(ren, 60, 50, 3, 0x8a, 0xe6, 0xb4, "FILES");
        text(ren, 60, 112, 2, 0xc8, 0xe4, 0xff, path);
        fill(ren, 60, 150, SCREEN_W - 120, 2, 0x8a, 0xe6, 0xb4, 60);

        int y0 = 180, dy = 46;
        int shown = n - top; if (shown > VIS) shown = VIS;
        for (int k = 0; k < shown; k++) {
            int j = top + k;
            int iy = y0 + k * dy;
            FEntry *f = &g_ents[j];
            int on = (j == sel);
            if (on) {
                fill(ren, 54, iy - 4, SCREEN_W - 108, dy - 8, 0x8a, 0xe6, 0xb4, 34);
                fill(ren, 54, iy - 4, 5, dy - 8, 0x8a, 0xe6, 0xb4, 255);
            }
            Uint8 cr = f->is_dir ? 0x9f : 0xc8;
            Uint8 cg = f->is_dir ? 0xe0 : 0xc8;
            Uint8 cb = f->is_dir ? 0xff : 0xd0;
            char line[300];
            snprintf(line, sizeof(line), "%s%s", f->is_dir ? "[ ] " : "    ", f->name);
            text(ren, 80, iy, 2, cr, cg, cb, line);
            if (!f->is_dir) {
                char sz[32];
                fmt_size(f->size, sz, sizeof(sz));
                int sw = (int)strlen(sz) * 8 * 2;
                text(ren, SCREEN_W - 120 - sw, iy, 2, 0x72, 0x80, 0x98, sz);
            }
        }

        char foot[96];
        snprintf(foot, sizeof(foot), "%d items   -   Cross enter   Circle up   Options back",
                 n);
        text(ren, 60, SCREEN_H - 120, 2, 0x7a, 0x8a, 0xb0, foot);
        if (info[0]) {
            text(ren, 60, SCREEN_H - 80, 2, 0x9f, 0xf0, 0xc2, info);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }
}

/* ---- file operations (copy / move / delete, recursive) ---- */

static int
copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (out < 0) { close(in); return -1; }
    char buf[65536];
    ssize_t r;
    int rc = 0;
    while ((r = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, (size_t)r) != r) { rc = -1; break; }
    }
    if (r < 0) rc = -1;
    close(in);
    close(out);
    return rc;
}

static int
copy_path(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        mkdir(dst, 0777);
        DIR *d = opendir(src);
        if (!d) return -1;
        struct dirent *e;
        int rc = 0;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char s[1024], t[1024];
            snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
            snprintf(t, sizeof(t), "%s/%s", dst, e->d_name);
            if (copy_path(s, t) != 0) rc = -1;
        }
        closedir(d);
        return rc;
    }
    return copy_file(src, dst);
}

static int
remove_path(const char *p) {
    struct stat st;
    if (stat(p, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(p);
        if (!d) return -1;
        struct dirent *e;
        int rc = 0;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char s[1024];
            snprintf(s, sizeof(s), "%s/%s", p, e->d_name);
            if (remove_path(s) != 0) rc = -1;
        }
        closedir(d);
        if (rmdir(p) != 0) rc = -1;
        return rc;
    }
    return unlink(p);
}

static int
move_path(const char *src, const char *dst) {
    if (rename(src, dst) == 0) return 0;      /* same filesystem */
    if (copy_path(src, dst) == 0) return remove_path(src);
    return -1;
}

/* ---- windowed desktop (multiMAN-style) ---- */

#define MAX_WIN   8
#define WIN_ENTS  256
#define TBAR_H    44     /* window title bar height */
#define TOOLBAR_H 34     /* per-window action toolbar */
#define TASK_H    56     /* taskbar height          */

typedef struct {
    int open, z, minimized, maximized;
    char path[1024];
    SDL_Rect r;                 /* live window rect */
    SDL_Rect saved;             /* pre-maximize rect */
    FEntry ents[WIN_ENTS];
    int count, sel, top;
} Win;

typedef struct { char label[48]; char path[1024]; int x, y; } Shortcut;

static int
pt_in(SDL_Rect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void
win_load(Win *w) {
    w->count = load_dir(w->path, w->ents, WIN_ENTS);
    w->sel = -1;
    w->top = 0;
}

static Win *
win_open(Win *wins, int *zmax, const char *path, int n_open) {
    for (int i = 0; i < MAX_WIN; i++) {
        if (!wins[i].open) {
            Win *w = &wins[i];
            w->open = 1;
            w->minimized = 0;
            w->maximized = 0;
            w->z = ++(*zmax);
            snprintf(w->path, sizeof(w->path), "%s", path);
            w->r.x = 120 + (n_open % 5) * 48;
            w->r.y = 90 + (n_open % 5) * 44;
            w->r.w = 760;
            w->r.h = 560;
            win_load(w);
            return w;
        }
    }
    return NULL;
}

/* Linux-style pointer: black arrow with a white outline. */
static void
draw_cursor(SDL_Renderer *ren, int x, int y) {
    for (int i = 0; i < 23; i++) {              /* white outline */
        int w = 3 + (i * 12) / 23;
        fill(ren, x - 1, y - 1 + i, w + 2, 3, 0xff, 0xff, 0xff, 255);
    }
    for (int i = 0; i < 20; i++) {              /* black fill */
        int w = 2 + (i * 10) / 20;
        fill(ren, x + 1, y + 1 + i, w, 2, 0x0a, 0x0a, 0x0a, 255);
    }
}

/* Original Nami desktop backdrop: deep gradient, top glow, flowing wave
 * ribbons (the 波 motif). All drawn from primitives. */
static void
draw_nami_bg(SDL_Renderer *r) {
    for (int band = 0; band < SCREEN_H; band += 4) {
        float t = (float)band / SCREEN_H;
        Uint8 rr = (Uint8)(0x07 + (0x14 - 0x07) * (1.0f - t));
        Uint8 gg = (Uint8)(0x0d + (0x22 - 0x0d) * (1.0f - t));
        Uint8 bb = (Uint8)(0x1a + (0x3e - 0x1a) * (1.0f - t));
        fill(r, 0, band, SCREEN_W, 4, rr, gg, bb, 255);
    }
    for (int i = 0; i < 220; i++)               /* top glow */
        fill(r, 0, i, SCREEN_W, 1, 0x2a, 0x56, 0x96, (Uint8)(36 * (1.0f - i / 220.0f)));
    for (int layer = 0; layer < 3; layer++) {   /* wave ribbons */
        int baseY = SCREEN_H / 2 + layer * 150 - 80;
        Uint8 a = (Uint8)(34 - layer * 8);
        for (int x = 0; x < SCREEN_W; x += 2) {
            int yy = baseY + (int)(sinf(x * 0.0038f + layer * 1.4f) * 72);
            fill(r, x, yy, 2, 48, (Uint8)(0x2a + layer * 8),
                 (Uint8)(0x5a + layer * 6), 0x9a, a);
        }
    }
}

#define TRASH_DIR "/data/nami/trash"

static void
ensure_trash(void) {
    mkdir("/data/nami", 0777);
    mkdir(TRASH_DIR, 0777);
}

static int
empty_trash(void) {
    ensure_trash();
    DIR *d = opendir(TRASH_DIR);
    if (!d) return -1;
    struct dirent *e;
    int rc = 0;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[1024];
        snprintf(p, sizeof(p), "%s/%s", TRASH_DIR, e->d_name);
        if (remove_path(p) != 0) rc = -1;
    }
    closedir(d);
    return rc;
}

/* On-screen keyboard (cursor-clickable; also accepts a USB keyboard).
 * Returns 1 and fills out[] if confirmed, 0 if cancelled. */
static int
text_input(SDL_Renderer *ren, SDL_GameController *pad, const char *title,
           const char *initial, char *out, size_t outsz) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", initial ? initial : "");
    static const char *rows[4] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
    const int kw = 74, kh = 66, gap = 8;
    int px = (SCREEN_W - (10 * (kw + gap) - gap)) / 2;
    int py = 360;
    int caps = 0, running = 1, ok = 0;
    float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;

    struct { const char *lbl; int w; } sp[] = {
        {"CAPS", 120}, {"SPACE", 240}, {"_", 74}, {"-", 74},
        {".", 74}, {"BACK", 120}, {"OK", 130}, {"CANCEL", 170}
    };
    int nsp = 8;

    SDL_StartTextInput();
    while (running) {
        int click = 0;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; }
            else if (e.type == SDL_MOUSEMOTION) { cx = e.motion.x; cy = e.motion.y; }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) click = 1;
            else if (e.type == SDL_TEXTINPUT) {
                size_t l = strlen(buf);
                if (l + strlen(e.text.text) < sizeof(buf)) strcat(buf, e.text.text);
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE) { size_t l = strlen(buf); if (l) buf[l-1] = 0; }
                else if (e.key.keysym.sym == SDLK_RETURN) { ok = 1; running = 0; }
                else if (e.key.keysym.sym == SDLK_ESCAPE) { running = 0; }
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_A) click = 1;
                else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_B) { size_t l = strlen(buf); if (l) buf[l-1] = 0; }
                else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_START) { ok = 1; running = 0; }
            }
        }
        if (pad) {
            int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
            int ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
            if (ax > 8000 || ax < -8000) cx += (ax / 32768.0f) * 16.0f;
            if (ay > 8000 || ay < -8000) cy += (ay / 32768.0f) * 16.0f;
            if (cx < 0) cx = 0; if (cx > SCREEN_W - 2) cx = SCREEN_W - 2;
            if (cy < 0) cy = 0; if (cy > SCREEN_H - 2) cy = SCREEN_H - 2;
        }
        int mx = (int)cx, my = (int)cy;

        if (click) {
            int hit = 0;
            for (int r = 0; r < 4 && !hit; r++) {
                int n = (int)strlen(rows[r]);
                for (int i = 0; i < n; i++) {
                    SDL_Rect kr = {px + i * (kw + gap), py + r * (kh + gap), kw, kh};
                    if (pt_in(kr, mx, my)) {
                        char c = rows[r][i];
                        if (caps && c >= 'a' && c <= 'z') c -= 32;
                        size_t l = strlen(buf);
                        if (l + 1 < sizeof(buf)) { buf[l] = c; buf[l+1] = 0; }
                        hit = 1; break;
                    }
                }
            }
            int sy = py + 4 * (kh + gap), sx = px;
            for (int s = 0; s < nsp && !hit; s++) {
                SDL_Rect sr = {sx, sy, sp[s].w, kh};
                if (pt_in(sr, mx, my)) {
                    const char *L = sp[s].lbl;
                    if (!strcmp(L, "CAPS")) caps = !caps;
                    else if (!strcmp(L, "SPACE")) { size_t l = strlen(buf); if (l+1 < sizeof(buf)) { buf[l] = ' '; buf[l+1] = 0; } }
                    else if (!strcmp(L, "BACK")) { size_t l = strlen(buf); if (l) buf[l-1] = 0; }
                    else if (!strcmp(L, "OK")) { ok = 1; running = 0; }
                    else if (!strcmp(L, "CANCEL")) { running = 0; }
                    else { size_t l = strlen(buf); if (l+1 < sizeof(buf)) { buf[l] = L[0]; buf[l+1] = 0; } }
                    hit = 1;
                }
                sx += sp[s].w + gap;
            }
        }

        for (int band = 0; band < SCREEN_H; band += 6) {
            float t = (float)band / SCREEN_H;
            Uint8 rr = (Uint8)(0x0a + (0x17 - 0x0a) * (1.0f - t));
            Uint8 gg = (Uint8)(0x13 + (0x36 - 0x13) * (1.0f - t));
            Uint8 bb = (Uint8)(0x22 + (0x5c - 0x22) * (1.0f - t));
            fill(ren, 0, band, SCREEN_W, 6, rr, gg, bb, 255);
        }
        text(ren, px, 150, 3, 0x9f, 0xd0, 0xff, title);
        fill(ren, px, 220, 10 * (kw + gap) - gap, 70, 0x0c, 0x16, 0x26, 255);
        fill(ren, px, 220, 10 * (kw + gap) - gap, 3, 0x7f, 0xd0, 0xff, 255);
        {
            char show[300];
            snprintf(show, sizeof(show), "%s_", buf);
            text(ren, px + 16, 242, 3, 0xe8, 0xf4, 0xff, show);
        }
        for (int r = 0; r < 4; r++) {
            int n = (int)strlen(rows[r]);
            for (int i = 0; i < n; i++) {
                int kx = px + i * (kw + gap), ky = py + r * (kh + gap);
                int on = (mx >= kx && mx < kx + kw && my >= ky && my < ky + kh);
                fill(ren, kx, ky, kw, kh, on ? 0x2e : 0x18, on ? 0x48 : 0x26, on ? 0x70 : 0x3c, 255);
                char c = rows[r][i];
                if (caps && c >= 'a' && c <= 'z') c -= 32;
                char s2[2] = {c, 0};
                text(ren, kx + kw/2 - 8, ky + kh/2 - 8, 2, 0xe8, 0xf0, 0xff, s2);
            }
        }
        int sy = py + 4 * (kh + gap), sx = px;
        for (int s = 0; s < nsp; s++) {
            int on = (mx >= sx && mx < sx + sp[s].w && my >= sy && my < sy + kh);
            int danger = (!strcmp(sp[s].lbl, "CANCEL"));
            int go = (!strcmp(sp[s].lbl, "OK"));
            fill(ren, sx, sy, sp[s].w, kh,
                 danger ? 0xff : (go ? 0x2b : (on ? 0x2e : 0x18)),
                 danger ? 0x8a : (go ? 0xd4 : (on ? 0x48 : 0x26)),
                 danger ? 0x8a : (go ? 0x7e : (on ? 0x70 : 0x3c)), 255);
            text(ren, sx + 14, sy + kh/2 - 8, 2,
                 (danger || go) ? 0x10 : 0xe8, (danger || go) ? 0x14 : 0xf0,
                 (danger || go) ? 0x18 : 0xff, sp[s].lbl);
            sx += sp[s].w + gap;
        }
        draw_cursor(ren, mx, my);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }
    SDL_StopTextInput();
    if (ok) snprintf(out, outsz, "%s", buf);
    return ok;
}

#define DESKTOP_DIR "/data/nami/desktop"
#define DK_MAX 64

/* ---- original icon art (drawn from primitives; no existing icon set) ---- */
static void
icon_folder(SDL_Renderer *r, int x, int y, int s, Uint8 cr, Uint8 cg, Uint8 cb) {
    fill(r, x, y + s/6, s*2/5, s/8, cr, cg, cb, 255);            /* tab */
    fill(r, x, y + s/4, s, s*3/5, cr, cg, cb, 255);             /* body */
    fill(r, x + 3, y + s/4 + 3, s - 6, 2, 0xff, 0xff, 0xff, 50);/* sheen */
}
static void
icon_drive(SDL_Renderer *r, int x, int y, int s, Uint8 cr, Uint8 cg, Uint8 cb) {
    fill(r, x, y + s/5, s, s*3/5, cr, cg, cb, 255);
    fill(r, x + s/6, y + s/5 + 6, s*2/3, 4, 0x10, 0x16, 0x22, 255);
    fill(r, x + s - s/4, y + s/5 + s*3/5 - 12, 8, 6, 0x2b, 0xd4, 0x7e, 255);
}
static void
icon_trash(SDL_Renderer *r, int x, int y, int s, Uint8 cr, Uint8 cg, Uint8 cb) {
    fill(r, x + s/8, y + s/5, s*3/4, s*3/4, cr, cg, cb, 255);
    fill(r, x, y + s/8, s, s/8, cr, cg, cb, 255);
    fill(r, x + s*3/8, y + s/20, s/4, s/14, cr, cg, cb, 255);
    for (int i = 1; i < 4; i++)
        fill(r, x + s/8 + i*(s*3/4)/4, y + s/5 + 8, 3, s*3/4 - 16, 0x10, 0x16, 0x22, 120);
}
/* Original Nami wave mark for the Start button. */
static void
nami_mark(SDL_Renderer *r, int x, int y, int w, int h) {
    for (int i = 0; i < w; i++) {
        float p = (float)i / w * 6.2832f;
        int y1 = y + h/2 + (int)(sinf(p) * h/3);
        int y2 = y + h/2 + (int)(sinf(p + 1.6f) * h/3);
        fill(r, x + i, y1, 2, 4, 0x7f, 0xd0, 0xff, 255);
        fill(r, x + i, y2, 2, 4, 0xbd, 0xa6, 0xff, 200);
    }
}

static void
ensure_desktop_dir(void) {
    mkdir("/data/nami", 0777);
    mkdir(DESKTOP_DIR, 0777);
}

/* icon kinds for desktop places */
enum { IC_DRIVE, IC_FOLDER, IC_TRASH };

typedef struct { char label[48]; char path[1024]; int kind; } Place;

static void
desktop(SDL_Renderer *ren, SDL_GameController *pad) {
    ensure_trash();
    ensure_desktop_dir();

    Win wins[MAX_WIN];
    memset(wins, 0, sizeof(wins));
    int zmax = 0, n_opened = 0;

    Place places[] = {
        {"PS5 Root", "/", IC_DRIVE},
        {"data",     "/data", IC_DRIVE},
        {"user",     "/user", IC_DRIVE},
        {"USB0",     "/mnt/usb0", IC_DRIVE},
        {"Trash",    TRASH_DIR, IC_TRASH},
    };
    int n_place = 5;

    /* desktop-folder icons live under DESKTOP_DIR */
    FEntry desk[DK_MAX];
    int n_desk = load_dir(DESKTOP_DIR, desk, DK_MAX);

    float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;
    float touch_x = 0.5f;
    int drag = -1, drag_dx = 0, drag_dy = 0;       /* window move drag */
    int rsz = -1;                                  /* window resize drag */
    int start_open = 0, running = 1;
    int confirm = 0;                               /* empty-trash */
    int ctx = 0, ctx_x = 0, ctx_y = 0, ctx_win = -1, ctx_row = -1; /* -1 win = desktop */
    char clip[1024] = ""; int clip_op = 0;
    char notice[192] = "";
    int desk_sel = -1;

    char ip[64];
    local_ipv4(ip, sizeof(ip));

    /* context-menu item sets */
    static const char *ITEM_MENU[] = {"Open", "Copy", "Cut", "Paste", "Rename", "Delete"};
    static const char *DESK_MENU[] = {"New Folder", "Paste", "Refresh", "Show Desktop", "Empty Trash"};

    while (running) {
        int click = 0, rclick = 0, release = 0;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_MOUSEMOTION) { cx = e.motion.x; cy = e.motion.y; }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) click = 1;
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) rclick = 1;
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) release = 1;
            else if (e.type == SDL_CONTROLLERTOUCHPADDOWN || e.type == SDL_CONTROLLERTOUCHPADMOTION) {
                cx = e.ctouchpad.x * SCREEN_W; cy = e.ctouchpad.y * SCREEN_H;
                touch_x = e.ctouchpad.x;
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
            else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                int b = e.cbutton.button;
                if (b == SDL_CONTROLLER_BUTTON_A) click = 1;
                else if (b == SDL_CONTROLLER_BUTTON_RIGHTSTICK) rclick = 1;
                else if (b == SDL_CONTROLLER_BUTTON_TOUCHPAD) { if (touch_x > 0.6f) rclick = 1; else click = 1; }
                else if (b == SDL_CONTROLLER_BUTTON_START) running = 0;
            }
            else if (e.type == SDL_CONTROLLERBUTTONUP) {
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_A) release = 1;
            }
        }

        /* right stick -> cursor; left stick -> move focused window */
        if (pad) {
            int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
            int ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
            if (ax > 8000 || ax < -8000) cx += (ax / 32768.0f) * 16.0f;
            if (ay > 8000 || ay < -8000) cy += (ay / 32768.0f) * 16.0f;
            if (cx < 0) cx = 0; if (cx > SCREEN_W - 2) cx = SCREEN_W - 2;
            if (cy < 0) cy = 0; if (cy > SCREEN_H - 2) cy = SCREEN_H - 2;
            Win *fw = NULL;
            for (int i = 0; i < MAX_WIN; i++)
                if (wins[i].open && !wins[i].minimized && (!fw || wins[i].z > fw->z)) fw = &wins[i];
            if (fw && !fw->maximized) {
                int lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
                int ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
                if (lx > 9000 || lx < -9000) fw->r.x += (int)((lx / 32768.0f) * 14.0f);
                if (ly > 9000 || ly < -9000) fw->r.y += (int)((ly / 32768.0f) * 14.0f);
            }
        }
        int mx = (int)cx, my = (int)cy;

        if (release) { drag = -1; rsz = -1; }
        if (drag >= 0 && wins[drag].open) { wins[drag].r.x = mx - drag_dx; wins[drag].r.y = my - drag_dy; }
        if (rsz >= 0 && wins[rsz].open) {
            int nw = mx - wins[rsz].r.x, nh = my - wins[rsz].r.y;
            if (nw < 360) nw = 360; if (nh < 240) nh = 240;
            wins[rsz].r.w = nw; wins[rsz].r.h = nh;
        }

        /* topmost window under cursor (ignores minimized) */
        Win *hot = NULL;
        for (int i = 0; i < MAX_WIN; i++)
            if (wins[i].open && !wins[i].minimized && pt_in(wins[i].r, mx, my) &&
                (!hot || wins[i].z > hot->z)) hot = &wins[i];

        /* ---------- right click: open a context menu ---------- */
        if (rclick) {
            ctx = 1; ctx_x = mx; ctx_y = my; ctx_win = -1; ctx_row = -1;
            if (hot) {
                ctx_win = (int)(hot - wins);
                hot->z = ++zmax;
                int bodyy = hot->r.y + TBAR_H + TOOLBAR_H + 8;
                int vis = (hot->r.h - TBAR_H - TOOLBAR_H - 16) / 34;
                int row = (my - bodyy) / 34;
                if (row >= 0 && row < vis && hot->top + row < hot->count) {
                    ctx_row = hot->top + row;
                    hot->sel = ctx_row;
                }
            }
        }

        /* ---------- empty-trash confirm modal ---------- */
        if (click && confirm) {
            int bw2 = 760, bh2 = 220, bx2 = (SCREEN_W-bw2)/2, by2 = (SCREEN_H-bh2)/2;
            SDL_Rect yes = {bx2+40, by2+150, 200, 50}, no = {bx2+260, by2+150, 200, 50};
            if (pt_in(yes, mx, my)) {
                int rc = empty_trash();
                snprintf(notice, sizeof(notice), "Trash emptied (%s)", rc==0?"ok":"part");
                for (int i = 0; i < MAX_WIN; i++)
                    if (wins[i].open && strncmp(wins[i].path, TRASH_DIR, strlen(TRASH_DIR))==0) win_load(&wins[i]);
                confirm = 0;
            } else if (pt_in(no, mx, my)) confirm = 0;
            click = 0;
        }

        /* ---------- context menu click ---------- */
        if (click && ctx) {
            int is_item = (ctx_win >= 0 && ctx_row >= 0);
            const char **items = is_item ? ITEM_MENU : DESK_MENU;
            int nmenu = is_item ? 6 : 5;
            int cw2 = 230, ch2 = nmenu * 40 + 10, rx = ctx_x, ry = ctx_y;
            if (rx + cw2 > SCREEN_W) rx = SCREEN_W - cw2;
            if (ry + ch2 > SCREEN_H - TASK_H) ry = SCREEN_H - TASK_H - ch2;
            SDL_Rect cr = {rx, ry, cw2, ch2};
            if (pt_in(cr, mx, my)) {
                int idx = (my - ry - 6) / 40;
                if (is_item) {
                    Win *w = &wins[ctx_win];
                    FEntry *f = (ctx_row < w->count) ? &w->ents[ctx_row] : NULL;
                    int is_up = f && !strcmp(f->name, "..");
                    char self[1024] = "";
                    if (f) snprintf(self, sizeof(self), "%s%s%s", w->path, strcmp(w->path,"/")==0?"":"/", f->name);
                    if (idx == 0 && f) {                       /* Open */
                        if (is_up) { path_up(w->path); win_load(w); }
                        else if (f->is_dir) { snprintf(w->path, sizeof(w->path), "%s", self); win_load(w); }
                    } else if (idx == 1 && f && !is_up) { snprintf(clip, sizeof(clip), "%s", self); clip_op = 1; snprintf(notice, sizeof(notice), "Copied %s", f->name); }
                    else if (idx == 2 && f && !is_up) { snprintf(clip, sizeof(clip), "%s", self); clip_op = 2; snprintf(notice, sizeof(notice), "Cut %s", f->name); }
                    else if (idx == 3 && clip[0]) {            /* Paste */
                        const char *base = strrchr(clip, '/'); base = base?base+1:clip;
                        char dst[1024]; snprintf(dst, sizeof(dst), "%s%s%s", w->path, strcmp(w->path,"/")==0?"":"/", base);
                        int rc = (clip_op==2)?move_path(clip,dst):copy_path(clip,dst);
                        snprintf(notice, sizeof(notice), "%s %s (%s)", clip_op==2?"Move":"Copy", base, rc==0?"ok":"FAILED");
                        if (clip_op==2 && rc==0) clip[0]=0; win_load(w);
                    } else if (idx == 4 && f && !is_up) {      /* Rename */
                        char nm[256];
                        if (text_input(ren, pad, "Rename", f->name, nm, sizeof(nm)) && nm[0]) {
                            char dst[1024]; snprintf(dst, sizeof(dst), "%s%s%s", w->path, strcmp(w->path,"/")==0?"":"/", nm);
                            int rc = rename(self, dst);
                            snprintf(notice, sizeof(notice), "Rename -> %s (%s)", nm, rc==0?"ok":"FAILED");
                            win_load(w);
                        }
                    } else if (idx == 5 && f && !is_up) {      /* Delete -> Trash */
                        ensure_trash(); char dst[1024]; snprintf(dst, sizeof(dst), "%s/%s", TRASH_DIR, f->name);
                        int rc = move_path(self, dst);
                        snprintf(notice, sizeof(notice), "%s -> Trash (%s)", f->name, rc==0?"ok":"FAILED");
                        win_load(w);
                    }
                } else {   /* desktop menu */
                    if (idx == 0) {                            /* New Folder on desktop */
                        char nm[256];
                        if (text_input(ren, pad, "New folder name", "", nm, sizeof(nm)) && nm[0]) {
                            char np[1024]; snprintf(np, sizeof(np), "%s/%s", DESKTOP_DIR, nm);
                            int rc = mkdir(np, 0777);
                            if (rc == 0) snprintf(notice, sizeof(notice), "New folder %s ok", nm);
                            else snprintf(notice, sizeof(notice), "New folder FAILED errno %d", errno);
                            n_desk = load_dir(DESKTOP_DIR, desk, DK_MAX);
                        }
                    } else if (idx == 1 && clip[0]) {          /* Paste to desktop */
                        const char *base = strrchr(clip, '/'); base = base?base+1:clip;
                        char dst[1024]; snprintf(dst, sizeof(dst), "%s/%s", DESKTOP_DIR, base);
                        int rc = (clip_op==2)?move_path(clip,dst):copy_path(clip,dst);
                        snprintf(notice, sizeof(notice), "%s %s -> Desktop (%s)", clip_op==2?"Move":"Copy", base, rc==0?"ok":"FAILED");
                        if (clip_op==2 && rc==0) clip[0]=0;
                        n_desk = load_dir(DESKTOP_DIR, desk, DK_MAX);
                    } else if (idx == 2) { for (int i=0;i<MAX_WIN;i++) if (wins[i].open) win_load(&wins[i]); n_desk = load_dir(DESKTOP_DIR, desk, DK_MAX); }
                    else if (idx == 3) { for (int i=0;i<MAX_WIN;i++) wins[i].minimized = 1; }
                    else if (idx == 4) confirm = 1;
                }
            }
            ctx = 0; click = 0;
        }

        /* ---------- left click ---------- */
        if (click) {
            int handled = 0;
            /* taskbar: Start button */
            SDL_Rect startbtn = {12, SCREEN_H - TASK_H + 8, 160, TASK_H - 16};
            if (pt_in(startbtn, mx, my)) { start_open = !start_open; handled = 1; }

            /* taskbar: window buttons */
            if (!handled) {
                int tx = 186;
                for (int i = 0; i < MAX_WIN && !handled; i++) {
                    if (!wins[i].open) continue;
                    SDL_Rect wb = {tx, SCREEN_H - TASK_H + 8, 190, TASK_H - 16};
                    if (pt_in(wb, mx, my)) {
                        wins[i].minimized = 0; wins[i].z = ++zmax; handled = 1;
                    }
                    tx += 198;
                }
            }

            /* start menu */
            if (!handled && start_open) {
                const char *sm[] = {"Open  /", "Open  /data", "Open  /user",
                                    "Open  /mnt/usb0", "Open Trash", "Empty Trash", "Close Desktop"};
                const char *sp[] = {"/", "/data", "/user", "/mnt/usb0", TRASH_DIR, "", ""};
                int ns = 7, mw = 340, mh = ns*46+16, mxp = 12, myp = SCREEN_H - TASK_H - mh - 6;
                SDL_Rect mr = {mxp, myp, mw, mh};
                if (pt_in(mr, mx, my)) {
                    int idx = (my - myp - 8) / 46;
                    if (idx >= 0 && idx < ns) {
                        if (idx == ns-1) running = 0;
                        else if (idx == 5) confirm = 1;
                        else if (sp[idx][0]) { if (strcmp(sp[idx],TRASH_DIR)==0) ensure_trash(); win_open(wins,&zmax,sp[idx],n_opened++); }
                    }
                    start_open = 0; handled = 1;
                } else start_open = 0;
                (void)sm;
            }

            /* desktop places + folder icons */
            if (!handled) {
                int slot = 0;
                for (int i = 0; i < n_place && !handled; i++, slot++) {
                    SDL_Rect ir = {60, 60 + slot*120, 120, 112};
                    if (pt_in(ir, mx, my)) {
                        if (desk_sel == 1000+i) { if (strcmp(places[i].path,TRASH_DIR)==0) ensure_trash(); win_open(wins,&zmax,places[i].path,n_opened++); desk_sel=-1; }
                        else desk_sel = 1000+i;
                        handled = 1;
                    }
                }
                for (int i = 0; i < n_desk && !handled; i++, slot++) {
                    int col = slot / 8, row = slot % 8;
                    SDL_Rect ir = {60 + col*140, 60 + row*120, 120, 112};
                    if (pt_in(ir, mx, my)) {
                        if (desk_sel == i) {
                            char np[1024]; snprintf(np,sizeof np,"%s/%s",DESKTOP_DIR,desk[i].name);
                            if (desk[i].is_dir) win_open(wins,&zmax,np,n_opened++);
                            desk_sel=-1;
                        } else desk_sel = i;
                        handled = 1;
                    }
                }
            }

            /* windows */
            if (!handled && hot) {
                Win *w = hot;
                w->z = ++zmax;
                SDL_Rect closeb = {w->r.x + w->r.w - 40, w->r.y + 6, 34, 32};
                SDL_Rect maxb   = {w->r.x + w->r.w - 80, w->r.y + 6, 34, 32};
                SDL_Rect minb   = {w->r.x + w->r.w - 120, w->r.y + 6, 34, 32};
                SDL_Rect tbar   = {w->r.x, w->r.y, w->r.w - 128, TBAR_H};
                SDL_Rect rhandle= {w->r.x + w->r.w - 20, w->r.y + w->r.h - 20, 20, 20};
                if (pt_in(closeb, mx, my)) { w->open = 0; }
                else if (pt_in(minb, mx, my)) { w->minimized = 1; }
                else if (pt_in(maxb, mx, my)) {
                    if (w->maximized) { w->r = w->saved; w->maximized = 0; }
                    else { w->saved = w->r; w->maximized = 1;
                           w->r.x = 20; w->r.y = 20; w->r.w = SCREEN_W - 40; w->r.h = SCREEN_H - TASK_H - 40; }
                }
                else if (pt_in(rhandle, mx, my)) { rsz = (int)(w - wins); }
                else if (pt_in(tbar, mx, my)) { drag = (int)(w - wins); drag_dx = mx - w->r.x; drag_dy = my - w->r.y; }
                else if (my < w->r.y + TBAR_H + TOOLBAR_H) {
                    int bx0 = w->r.x + 8, bw = 104, gap = 6, off = mx - bx0;
                    int b = off/(bw+gap), within = off>=0 && (off%(bw+gap))<bw && b>=0 && b<=5;
                    int have = w->sel>=0 && w->sel<w->count;
                    FEntry *f = have ? &w->ents[w->sel] : NULL;
                    int is_up = f && !strcmp(f->name,"..");
                    char self[1024]=""; if (f) snprintf(self,sizeof self,"%s%s%s",w->path,strcmp(w->path,"/")==0?"":"/",f->name);
                    if (within && b==0) { char nm[256]; if (text_input(ren,pad,"New folder name","",nm,sizeof nm)&&nm[0]) { char np[1024]; snprintf(np,sizeof np,"%s%s%s",w->path,strcmp(w->path,"/")==0?"":"/",nm); int rc=mkdir(np,0777); if(rc==0)snprintf(notice,sizeof notice,"New folder %s ok",nm); else snprintf(notice,sizeof notice,"New folder %s FAILED errno %d",nm,errno); win_load(w);} }
                    else if (within && b==1 && f && !is_up) { char nm[256]; if (text_input(ren,pad,"Rename",f->name,nm,sizeof nm)&&nm[0]) { char dst[1024]; snprintf(dst,sizeof dst,"%s%s%s",w->path,strcmp(w->path,"/")==0?"":"/",nm); int rc=rename(self,dst); snprintf(notice,sizeof notice,"Rename -> %s (%s)",nm,rc==0?"ok":"FAILED"); win_load(w);} }
                    else if (within && (b==2||b==3) && f && !is_up) { snprintf(clip,sizeof clip,"%s",self); clip_op=(b==2)?1:2; snprintf(notice,sizeof notice,"%s %s",b==2?"Copied":"Cut",f->name); }
                    else if (within && b==4 && clip[0]) { const char* base=strrchr(clip,'/'); base=base?base+1:clip; char dst[1024]; snprintf(dst,sizeof dst,"%s%s%s",w->path,strcmp(w->path,"/")==0?"":"/",base); int rc=(clip_op==2)?move_path(clip,dst):copy_path(clip,dst); snprintf(notice,sizeof notice,"%s %s (%s)",clip_op==2?"Move":"Copy",base,rc==0?"ok":"FAILED"); if(clip_op==2&&rc==0)clip[0]=0; win_load(w); }
                    else if (within && b==5 && f && !is_up) { ensure_trash(); char dst[1024]; snprintf(dst,sizeof dst,"%s/%s",TRASH_DIR,f->name); int rc=move_path(self,dst); snprintf(notice,sizeof notice,"%s -> Trash (%s)",f->name,rc==0?"ok":"FAILED"); win_load(w); }
                } else {
                    /* body row: click selects, click again opens */
                    int bodyy = w->r.y + TBAR_H + TOOLBAR_H + 8;
                    int vis = (w->r.h - TBAR_H - TOOLBAR_H - 16) / 34;
                    int row = (my - bodyy) / 34;
                    if (row >= 0 && row < vis) {
                        int j = w->top + row;
                        if (j >= 0 && j < w->count) {
                            FEntry *f = &w->ents[j];
                            if (w->sel == j) {   /* second click = open */
                                if (!strcmp(f->name,"..")) { path_up(w->path); win_load(w); }
                                else if (f->is_dir) { char np[1024]; snprintf(np,sizeof np,"%s%s%s",w->path,strcmp(w->path,"/")==0?"":"/",f->name); snprintf(w->path,sizeof w->path,"%s",np); win_load(w); }
                            } else w->sel = j;    /* first click = select */
                        }
                    }
                }
                handled = 1;
            }

            if (!handled) desk_sel = -1;   /* clicked empty desktop */
        }

        /* ================= render ================= */
        draw_nami_bg(ren);

        /* desktop icons: places then desktop folders */
        {
            int slot = 0;
            for (int i = 0; i < n_place; i++, slot++) {
                int ix = 60, iy = 60 + slot*120;
                int sel = (desk_sel == 1000+i);
                if (sel) fill(ren, ix-6, iy-6, 132, 118, 0x7f, 0xd0, 0xff, 50);
                if (places[i].kind == IC_TRASH) icon_trash(ren, ix+24, iy, 72, 0xc8, 0xcf, 0xda);
                else icon_drive(ren, ix+24, iy, 72, 0x7f, 0xc4, 0xff);
                int lw = (int)strlen(places[i].label) * 8;
                text(ren, ix + 60 - lw, iy + 82, 1, 0xd8, 0xe4, 0xff, places[i].label);
            }
            for (int i = 0; i < n_desk; i++, slot++) {
                int col = slot / 8, row = slot % 8;
                int ix = 60 + col*140, iy = 60 + row*120;
                int sel = (desk_sel == i);
                if (sel) fill(ren, ix-6, iy-6, 132, 118, 0x7f, 0xd0, 0xff, 50);
                icon_folder(ren, ix+24, iy, 72, 0xe6, 0xc9, 0x7a);
                char nm[20]; snprintf(nm, sizeof nm, "%.14s", desk[i].name);
                int lw = (int)strlen(nm) * 8;
                text(ren, ix + 60 - lw, iy + 82, 1, 0xd8, 0xe4, 0xff, nm);
            }
        }

        /* windows (z order) */
        for (int pass = 1; pass <= zmax; pass++) {
            for (int i = 0; i < MAX_WIN; i++) {
                Win *w = &wins[i];
                if (!w->open || w->minimized || w->z != pass) continue;
                fill(ren, w->r.x, w->r.y, w->r.w, w->r.h, 0x0c, 0x16, 0x26, 245);
                fill(ren, w->r.x, w->r.y, w->r.w, TBAR_H, 0x1c, 0x30, 0x50, 255);
                fill(ren, w->r.x, w->r.y, w->r.w, 3, 0x7f, 0xd0, 0xff, 255);
                text(ren, w->r.x + 14, w->r.y + 14, 2, 0xdf, 0xec, 0xff, w->path);
                fill(ren, w->r.x + w->r.w - 120, w->r.y + 6, 34, 32, 0x2a, 0x3e, 0x5c, 220);
                text(ren, w->r.x + w->r.w - 108, w->r.y + 20, 2, 0xe0, 0xf0, 0xff, "_");
                fill(ren, w->r.x + w->r.w - 80, w->r.y + 6, 34, 32, 0x2a, 0x3e, 0x5c, 220);
                text(ren, w->r.x + w->r.w - 70, w->r.y + 14, 2, 0xe0, 0xf0, 0xff, "[]");
                fill(ren, w->r.x + w->r.w - 40, w->r.y + 6, 34, 32, 0xff, 0x6a, 0x6a, 220);
                text(ren, w->r.x + w->r.w - 30, w->r.y + 14, 2, 0x20, 0x08, 0x08, "x");

                fill(ren, w->r.x, w->r.y + TBAR_H, w->r.w, TOOLBAR_H, 0x12, 0x20, 0x34, 255);
                static const char *tb[] = {"New","Rename","Copy","Cut","Paste","Del"};
                for (int b = 0; b < 6; b++) {
                    int bxx = w->r.x + 8 + b*110;
                    Uint8 br=(b==5)?0xff:0x2a, bg=(b==5)?0x8a:0x3e, bb=(b==5)?0x8a:0x5c;
                    fill(ren, bxx, w->r.y+TBAR_H+3, 104, TOOLBAR_H-6, br, bg, bb, 160);
                    text(ren, bxx+8, w->r.y+TBAR_H+10, 2, 0xe8, 0xf0, 0xff, tb[b]);
                }

                int bodyy = w->r.y + TBAR_H + TOOLBAR_H + 8;
                int vis = (w->r.h - TBAR_H - TOOLBAR_H - 16) / 34;
                if (w->sel >= 0) { if (w->sel < w->top) w->top = w->sel; if (w->sel >= w->top + vis) w->top = w->sel - vis + 1; }
                int shown = w->count - w->top; if (shown > vis) shown = vis;
                for (int k = 0; k < shown; k++) {
                    int j = w->top + k, ry = bodyy + k*34;
                    FEntry *f = &w->ents[j];
                    if (j == w->sel) { fill(ren, w->r.x+6, ry-2, w->r.w-12, 30, 0x7f, 0xd0, 0xff, 55); fill(ren, w->r.x+6, ry-2, 4, 30, 0x7f, 0xd0, 0xff, 255); }
                    char line[280]; snprintf(line, sizeof line, "%s%s", f->is_dir?"[ ] ":"    ", f->name);
                    text(ren, w->r.x+16, ry, 2, f->is_dir?0x9f:0xc0, f->is_dir?0xe0:0xc0, f->is_dir?0xff:0xd0, line);
                    if (!f->is_dir) { char sz[32]; fmt_size(f->size, sz, sizeof sz); int sw=(int)strlen(sz)*8*2; text(ren, w->r.x+w->r.w-16-sw, ry, 2, 0x72, 0x80, 0x98, sz); }
                }
                /* resize handle */
                fill(ren, w->r.x + w->r.w - 18, w->r.y + w->r.h - 18, 14, 14, 0x7f, 0xd0, 0xff, 120);
            }
        }

        /* taskbar */
        fill(ren, 0, SCREEN_H - TASK_H, SCREEN_W, TASK_H, 0x08, 0x10, 0x1e, 255);
        fill(ren, 0, SCREEN_H - TASK_H, SCREEN_W, 2, 0x7f, 0xd0, 0xff, 120);
        fill(ren, 12, SCREEN_H - TASK_H + 8, 160, TASK_H - 16, 0x14, 0x24, 0x3c, 255);
        nami_mark(ren, 20, SCREEN_H - TASK_H + 16, 40, TASK_H - 28);
        text(ren, 70, SCREEN_H - TASK_H + 18, 2, 0xe0, 0xf0, 0xff, "NAMI");
        {
            int tx = 186;
            for (int i = 0; i < MAX_WIN; i++) {
                if (!wins[i].open) continue;
                int on = !wins[i].minimized;
                fill(ren, tx, SCREEN_H - TASK_H + 8, 190, TASK_H - 16, on?0x1c:0x12, on?0x30:0x1c, on?0x50:0x2c, 255);
                const char *b = strrchr(wins[i].path, '/'); b = (b && b[1]) ? b+1 : wins[i].path;
                char lb[16]; snprintf(lb, sizeof lb, "%.11s", b);
                text(ren, tx + 12, SCREEN_H - TASK_H + 18, 2, 0xcf, 0xdc, 0xf0, lb);
                tx += 198;
            }
        }
        {
            time_t now = time(NULL); struct tm *tmv = localtime(&now);
            char dt[64]=""; if (tmv) strftime(dt, sizeof dt, "%a %d/%m  %H:%M", tmv);
            char tb[128]; snprintf(tb, sizeof tb, "%s   IP %s", dt, ip);
            int lw = (int)strlen(tb)*8*2;
            text(ren, SCREEN_W - lw - 20, SCREEN_H - TASK_H + 18, 2, 0x9a, 0xac, 0xd0, tb);
        }

        /* start menu */
        if (start_open) {
            const char *sm[] = {"Open  /", "Open  /data", "Open  /user",
                                "Open  /mnt/usb0", "Open Trash", "Empty Trash", "Close Desktop"};
            int ns = 7, mw = 340, mh = ns*46+16, mxp = 12, myp = SCREEN_H - TASK_H - mh - 6;
            fill(ren, mxp, myp, mw, mh, 0x10, 0x1c, 0x30, 250);
            fill(ren, mxp, myp, mw, 3, 0x7f, 0xd0, 0xff, 255);
            for (int i = 0; i < ns; i++) {
                int iy = myp + 8 + i*46;
                if (mx>=mxp && mx<mxp+mw && my>=iy && my<iy+46) fill(ren, mxp+4, iy, mw-8, 44, 0x7f, 0xd0, 0xff, 40);
                text(ren, mxp+20, iy+14, 2, (i==ns-1)?0xff:0xcf, (i==ns-1)?0x9a:0xde, (i==ns-1)?0x9a:0xff, sm[i]);
            }
        }

        /* notice + clipboard */
        if (notice[0]) text(ren, 70, SCREEN_H - TASK_H - 28, 2, 0x9f, 0xf0, 0xc2, notice);
        if (clip[0]) { const char *b=strrchr(clip,'/'); b=b?b+1:clip; char cb[140]; snprintf(cb,sizeof cb,"CLIP[%s] %s",clip_op==2?"cut":"copy",b); text(ren, SCREEN_W/2-120, SCREEN_H - TASK_H - 28, 2, 0xd4, 0xd0, 0x92, cb); }

        /* context menu */
        if (ctx) {
            int is_item = (ctx_win >= 0 && ctx_row >= 0);
            const char **items = is_item ? ITEM_MENU : DESK_MENU;
            int nmenu = is_item ? 6 : 5, cw2 = 230, ch2 = nmenu*40+10, rx = ctx_x, ry = ctx_y;
            if (rx + cw2 > SCREEN_W) rx = SCREEN_W - cw2;
            if (ry + ch2 > SCREEN_H - TASK_H) ry = SCREEN_H - TASK_H - ch2;
            fill(ren, rx, ry, cw2, ch2, 0x12, 0x1c, 0x30, 252);
            fill(ren, rx, ry, cw2, 3, 0x7f, 0xd0, 0xff, 255);
            for (int i = 0; i < nmenu; i++) {
                int iy = ry + 6 + i*40;
                if (mx>=rx && mx<rx+cw2 && my>=iy && my<iy+40) fill(ren, rx+3, iy, cw2-6, 38, 0x7f, 0xd0, 0xff, 40);
                text(ren, rx+16, iy+11, 2, 0xd8, 0xe4, 0xff, items[i]);
            }
        }

        /* empty-trash confirm */
        if (confirm) {
            int bw2=760, bh2=220, bx2=(SCREEN_W-bw2)/2, by2=(SCREEN_H-bh2)/2;
            fill(ren, 0,0,SCREEN_W,SCREEN_H, 0,0,0,150);
            fill(ren, bx2,by2,bw2,bh2, 0x16,0x1e,0x2e,250);
            fill(ren, bx2,by2,bw2,3, 0xff,0x8a,0x8a,255);
            text(ren, bx2+40, by2+46, 3, 0xff,0xd0,0xd0, "Empty Trash ?");
            text(ren, bx2+40, by2+108, 2, 0x9a,0xa6,0xc0, "Permanently deletes everything in the Trash.");
            fill(ren, bx2+40, by2+150, 200, 50, 0xff,0x6a,0x6a,210);
            text(ren, bx2+84, by2+166, 2, 0x20,0x08,0x08, "EMPTY");
            fill(ren, bx2+260, by2+150, 200, 50, 0x2a,0x3e,0x5c,210);
            text(ren, bx2+338, by2+166, 2, 0xe0,0xf0,0xff, "NO");
        }

        draw_cursor(ren, mx, my);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }
}

int
main(void) {
    build_content();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) return 1;
    fonts_load();
    SDL_Window *win = SDL_CreateWindow(
        "Nami", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_FULLSCREEN);
    if (!win) { SDL_Quit(); return 2; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { SDL_DestroyWindow(win); SDL_Quit(); return 3; }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    IMG_Init(IMG_INIT_PNG);

    SDL_GameController *pad = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            pad = SDL_GameControllerOpen(i);
            if (pad) break;
        }
    }

    char ip[64];
    local_ipv4(ip, sizeof(ip));
    char hud[128];
    snprintf(hud, sizeof(hud), "IP %s   PORT %d", ip, WEB_PORT);

    int sel_cat = 0, sel_item = 0, top = 0, layout = 0;
    char status[160];
    snprintf(status, sizeof(status),
             "D-pad move  -  Cross launch/select  -  Options exit");
    float cat_scroll = 0.0f;
    int running = 1;

    /* Lazily-loaded cover for the selected game. */
    SDL_Texture *cover = NULL;
    int cover_cat = -1, cover_item = -1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            int left = 0, right = 0, up = 0, down = 0, select = 0;
            if (e.type == SDL_QUIT) {
                running = 0;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = 0; break;
                    case SDLK_LEFT:  left = 1; break;
                    case SDLK_RIGHT: right = 1; break;
                    case SDLK_UP:    up = 1; break;
                    case SDLK_DOWN:  down = 1; break;
                    case SDLK_RETURN: select = 1; break;
                    default: break;
                }
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  left = 1; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: right = 1; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:    up = 1; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  down = 1; break;
                    case SDL_CONTROLLER_BUTTON_A:          select = 1; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  layout = (layout + 2) % 3; break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: layout = (layout + 1) % 3; break;
                    case SDL_CONTROLLER_BUTTON_START:      running = 0; break;
                    default: break;
                }
            }

            if (left && sel_cat > 0)           { sel_cat--; sel_item = 0; top = 0; }
            if (right && sel_cat < N_CATS - 1) { sel_cat++; sel_item = 0; top = 0; }
            if (up && sel_item > 0)            { sel_item--; }
            if (down && sel_item < g_cats[sel_cat].count - 1) { sel_item++; }
            if (sel_item < top)              top = sel_item;
            if (sel_item >= top + VIS_ITEMS) top = sel_item - VIS_ITEMS + 1;

            if (select) {
                Category *c = &g_cats[sel_cat];
                Item *it = &c->items[sel_item];
                if (sel_cat == CAT_POWER) {
                    running = 0;
                } else if (it->kind == KIND_PAYLOAD && it->path[0]) {
                    snprintf(status, sizeof(status), "Launching %s ...", it->label);
                    request_launch(it->path);
                    running = 0;   /* bootstrap launches it after we exit */
                } else if (it->kind == KIND_TITLE && it->path[0]) {
                    int rc = launch_title(it->path);
                    if (rc == 0) {
                        snprintf(status, sizeof(status), "Launching %s ...", it->label);
                        running = 0;   /* release our slot so the title foregrounds */
                    } else {
                        snprintf(status, sizeof(status),
                                 "%s (%s): launch rc=0x%x", it->label, it->path, rc);
                    }
                } else if (it->kind == KIND_DIR && it->path[0]) {
                    browse(ren, pad, it->path);
                } else if (it->kind == KIND_DESKTOP) {
                    desktop(ren, pad);
                } else {
                    snprintf(status, sizeof(status), "%s > %s   (coming later)",
                             c->name, it->label);
                }
            }
        }

        cat_scroll += ((float)sel_cat - cat_scroll) * 0.25f;

        /* Load the selected game's cover0 when the selection changes. */
        if (sel_cat == CAT_GAMES) {
            if (cover_cat != sel_cat || cover_item != sel_item) {
                if (cover) { SDL_DestroyTexture(cover); cover = NULL; }
                Item *gi = &g_cats[CAT_GAMES].items[sel_item];
                if (gi->icon[0]) {
                    SDL_Surface *s = IMG_Load(gi->icon);
                    if (s) {
                        cover = SDL_CreateTextureFromSurface(ren, s);
                        SDL_FreeSurface(s);
                    }
                }
                cover_cat = sel_cat;
                cover_item = sel_item;
            }
        } else if (cover) {
            SDL_DestroyTexture(cover);
            cover = NULL;
            cover_cat = cover_item = -1;
        }

        for (int band = 0; band < SCREEN_H; band += 6) {
            float t = (float)band / SCREEN_H;
            Uint8 rr = (Uint8)(0x0a + (0x17 - 0x0a) * (1.0f - t));
            Uint8 gg = (Uint8)(0x13 + (0x36 - 0x13) * (1.0f - t));
            Uint8 bb = (Uint8)(0x22 + (0x5c - 0x22) * (1.0f - t));
            fill(ren, 0, band, SCREEN_W, 6, rr, gg, bb, 255);
        }
        /* L1/R1 cycle three XMB layouts (strip at different heights). */
        int bar_y   = (layout == 1) ? 210 : (layout == 2) ? 150 : 330;
        int item_y0 = bar_y + 100;
        int focus_x = FOCUS_X;

        fill(ren, 0, bar_y - 2, SCREEN_W, 2, 0xff, 0xff, 0xff, 22);

        text(ren, 60, 60, 4, 0x9f, 0xd0, 0xff, "NAMI");
        text(ren, 64, 120, 2, 0x5f, 0x76, 0xa0, "PS5 SHELL");
        {
            char li[32];
            snprintf(li, sizeof(li), "LAYOUT %d/3  L1 R1", layout + 1);
            int lw = (int)strlen(li) * 8 * 2;
            text(ren, SCREEN_W - lw - 40, 64, 2, 0x5f, 0x76, 0xa0, li);
        }

        for (int i = 0; i < N_CATS; i++) {
            int cx = focus_x + (int)lroundf(((float)i - cat_scroll) * CAT_GAP);
            if (cx < -120 || cx > SCREEN_W + 120) continue;
            int active = (i == sel_cat);
            Category *c = &g_cats[i];
            draw_glyph(ren, cx, bar_y, active ? 34 : 24, active, c->r, c->g, c->b);
            int lw = (int)strlen(c->name) * 8 * 2;
            text(ren, cx - lw / 2, bar_y + 48, 2,
                 active ? 0xff : 0x66, active ? 0xff : 0x72,
                 active ? 0xff : 0x88, c->name);
        }

        Category *ac = &g_cats[sel_cat];
        int shown = ac->count - top;
        if (shown > VIS_ITEMS) shown = VIS_ITEMS;
        for (int k = 0; k < shown; k++) {
            int j = top + k;
            int iy = item_y0 + k * ITEM_DY;
            int on = (j == sel_item);
            Item *it = &ac->items[j];
            int launchable = (it->kind == KIND_PAYLOAD || it->kind == KIND_TITLE);
            if (on) {
                fill(ren, focus_x - 40, iy - 6, 900, ITEM_DY - 10,
                     ac->r, ac->g, ac->b, 36);
                fill(ren, focus_x - 40, iy - 6, 5, ITEM_DY - 10,
                     ac->r, ac->g, ac->b, 255);
            }
            Uint8 tr = on ? 0xf0 : (launchable ? 0x9a : 0x80);
            Uint8 tg = on ? 0xf6 : (launchable ? 0xa6 : 0x88);
            Uint8 tb = on ? 0xff : (launchable ? 0xc8 : 0x98);
            text(ren, focus_x - 12, iy, on ? 3 : 2, tr, tg, tb, it->label);
        }
        if (ac->count > VIS_ITEMS) {
            char cnt[48];
            snprintf(cnt, sizeof(cnt), "%d / %d", sel_item + 1, ac->count);
            text(ren, focus_x - 12, item_y0 + VIS_ITEMS * ITEM_DY + 6, 2,
                 0x66, 0x72, 0x88, cnt);
        }

        /* Selected game's own cover art (its icon0.png), right of the list.
         * Falls back to a plain accent tile when a title has no icon. */
        if (sel_cat == CAT_GAMES) {
            int cw = 340, cx = SCREEN_W - cw - 140, cy = 440;
            fill(ren, cx - 4, cy - 4, cw + 8, cw + 8, 0xff, 0xff, 0xff, 24);
            if (cover) {
                SDL_Rect dst = {cx, cy, cw, cw};
                SDL_RenderCopy(ren, cover, NULL, &dst);
            } else {
                Category *gc = &g_cats[CAT_GAMES];
                fill(ren, cx, cy, cw, cw, gc->r, gc->g, gc->b, 40);
                text(ren, cx + cw / 2 - 16, cy + cw / 2 - 16, 4,
                     gc->r, gc->g, gc->b, "?");
            }
        }

        text(ren, 60, SCREEN_H - 130, 2, 0x7a, 0x8a, 0xb0, status);

        int bw = 460, bh = 70;
        int bx = SCREEN_W - bw - 48, by = SCREEN_H - bh - 48;
        fill(ren, bx, by, bw, bh, 0x0e, 0x2a, 0x1d, 255);
        fill(ren, bx, by, bw, 3, 0x2b, 0xd4, 0x7e, 255);
        fill(ren, bx + 20, by + 28, 12, 12, 0x2b, 0xd4, 0x7e, 255);
        text(ren, bx + 48, by + 27, 2, 0x9f, 0xf0, 0xc2, hud);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    if (cover) SDL_DestroyTexture(cover);
    if (pad) SDL_GameControllerClose(pad);
    IMG_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
