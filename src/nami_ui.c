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
#include <sqlite3.h>

#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
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

/* Provided by libSceSystemService (symbols present in the stub). */
int sceSystemServiceLaunchApp(const char *titleId, const char *const *argv,
                              void *param);
int sceLncUtilLaunchApp(const char *titleId, const char *const *argv,
                        void *param);

enum { KIND_INFO, KIND_PAYLOAD, KIND_TITLE };

typedef struct {
    char label[128];
    char path[512];   /* ELF path (payload) or titleId (title) */
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
typedef struct { char tid[16]; char name[96]; } TitleName;
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
add_name(const char *tid, const char *name) {
    if (!tid || !tid[0] || !name || g_name_count >= MAX_NAMES) {
        return;
    }
    for (int i = 0; i < g_name_count; i++) {
        if (strcmp(g_names[i].tid, tid) == 0) return;   /* first wins */
    }
    snprintf(g_names[g_name_count].tid, 16, "%s", tid);
    sanitize_ascii(g_names[g_name_count].name, 96, name);
    g_name_count++;
}

static const char *
name_for(const char *tid) {
    for (int i = 0; i < g_name_count; i++) {
        if (strcmp(g_names[i].tid, tid) == 0) return g_names[i].name;
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
                add_name(tid, nm);
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
        if (nm) {
            snprintf(label, sizeof(label), "%s", nm);
        } else {
            snprintf(label, sizeof(label), "%s", e->d_name);
        }
        add_item(c, label, e->d_name, KIND_TITLE);
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

    add_item(&g_cats[CAT_FILES], "Desktop (file browser) - M6", "", KIND_INFO);
    add_item(&g_cats[CAT_FILES], "/data", "", KIND_INFO);
    add_item(&g_cats[CAT_FILES], "/mnt/usb0", "", KIND_INFO);

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

static void
text(SDL_Renderer *r, int x, int y, int scale,
     Uint8 cr, Uint8 cg, Uint8 cb, const char *s) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderSetScale(r, (float)scale, (float)scale);
    SDLTest_DrawString(r, (float)x / scale, (float)y / scale, s);
    SDL_RenderSetScale(r, 1.0f, 1.0f);
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

int
main(void) {
    build_content();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) return 1;
    SDL_Window *win = SDL_CreateWindow(
        "Nami", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_FULLSCREEN);
    if (!win) { SDL_Quit(); return 2; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { SDL_DestroyWindow(win); SDL_Quit(); return 3; }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

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

    int sel_cat = 0, sel_item = 0, top = 0;
    char status[160];
    snprintf(status, sizeof(status),
             "D-pad move  -  Cross launch/select  -  Options exit");
    float cat_scroll = 0.0f;
    int running = 1;

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
                    int rc = sceSystemServiceLaunchApp(it->path, NULL, NULL);
                    int rc2 = (rc != 0) ? sceLncUtilLaunchApp(it->path, NULL, NULL)
                                        : 0;
                    if (rc == 0 || rc2 == 0) {
                        snprintf(status, sizeof(status), "Launching %s ...", it->label);
                        running = 0;   /* release our slot so the title foregrounds */
                    } else {
                        snprintf(status, sizeof(status),
                                 "%s (%s): svc=0x%x lnc=0x%x",
                                 it->label, it->path, rc, rc2);
                    }
                } else {
                    snprintf(status, sizeof(status), "%s > %s   (coming later)",
                             c->name, it->label);
                }
            }
        }

        cat_scroll += ((float)sel_cat - cat_scroll) * 0.25f;

        for (int band = 0; band < SCREEN_H; band += 6) {
            float t = (float)band / SCREEN_H;
            Uint8 rr = (Uint8)(0x0a + (0x17 - 0x0a) * (1.0f - t));
            Uint8 gg = (Uint8)(0x13 + (0x36 - 0x13) * (1.0f - t));
            Uint8 bb = (Uint8)(0x22 + (0x5c - 0x22) * (1.0f - t));
            fill(ren, 0, band, SCREEN_W, 6, rr, gg, bb, 255);
        }
        fill(ren, 0, BAR_Y - 2, SCREEN_W, 2, 0xff, 0xff, 0xff, 22);

        text(ren, 60, 60, 4, 0x9f, 0xd0, 0xff, "NAMI");
        text(ren, 64, 120, 2, 0x5f, 0x76, 0xa0, "PS5 SHELL");

        for (int i = 0; i < N_CATS; i++) {
            int cx = FOCUS_X + (int)lroundf(((float)i - cat_scroll) * CAT_GAP);
            if (cx < -120 || cx > SCREEN_W + 120) continue;
            int active = (i == sel_cat);
            Category *c = &g_cats[i];
            draw_glyph(ren, cx, BAR_Y, active ? 34 : 24, active, c->r, c->g, c->b);
            int lw = (int)strlen(c->name) * 8 * 2;
            text(ren, cx - lw / 2, BAR_Y + 48, 2,
                 active ? 0xff : 0x66, active ? 0xff : 0x72,
                 active ? 0xff : 0x88, c->name);
        }

        Category *ac = &g_cats[sel_cat];
        int shown = ac->count - top;
        if (shown > VIS_ITEMS) shown = VIS_ITEMS;
        for (int k = 0; k < shown; k++) {
            int j = top + k;
            int iy = ITEM_Y0 + k * ITEM_DY;
            int on = (j == sel_item);
            Item *it = &ac->items[j];
            int launchable = (it->kind == KIND_PAYLOAD || it->kind == KIND_TITLE);
            if (on) {
                fill(ren, FOCUS_X - 40, iy - 6, 900, ITEM_DY - 10,
                     ac->r, ac->g, ac->b, 36);
                fill(ren, FOCUS_X - 40, iy - 6, 5, ITEM_DY - 10,
                     ac->r, ac->g, ac->b, 255);
            }
            Uint8 tr = on ? 0xf0 : (launchable ? 0x9a : 0x80);
            Uint8 tg = on ? 0xf6 : (launchable ? 0xa6 : 0x88);
            Uint8 tb = on ? 0xff : (launchable ? 0xc8 : 0x98);
            text(ren, FOCUS_X - 12, iy, on ? 3 : 2, tr, tg, tb, it->label);
        }
        if (ac->count > VIS_ITEMS) {
            char cnt[48];
            snprintf(cnt, sizeof(cnt), "%d / %d", sel_item + 1, ac->count);
            text(ren, FOCUS_X - 12, ITEM_Y0 + VIS_ITEMS * ITEM_DY + 6, 2,
                 0x66, 0x72, 0x88, cnt);
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

    if (pad) SDL_GameControllerClose(pad);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
