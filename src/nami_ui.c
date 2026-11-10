/* Nami - PS5 homebrew shell UI (M2: XMB navigation)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Launched as a bigapp by nami_bootstrap.elf (video-out + pad).
 * M2: cross-media-bar style navigation - a horizontal strip of categories,
 * a vertical list of items under the active one, D-pad to move, cross to
 * select. All artwork is original simple geometry. The bottom-right IP/port
 * HUD box stays. Real actions (launch/dump/ftp...) land in later milestones.
 */
#include <SDL.h>
#include <SDL_test_font.h>

#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define SCREEN_W 1920
#define SCREEN_H 1080
#define WEB_PORT 8080

#define FOCUS_X   260   /* screen x where the active category sits */
#define CAT_GAP   340   /* horizontal spacing between categories   */
#define BAR_Y     330   /* vertical line of the category strip     */
#define ITEM_Y0   430   /* first item row                          */
#define ITEM_DY   66    /* item row spacing                        */
#define MAX_ITEMS 10

typedef struct {
    const char *label;
    const char *items[MAX_ITEMS];
    int item_count;
    Uint8 r, g, b;              /* category accent */
} Category;

static Category g_cats[] = {
    {"GAMES",    {"(no titles scanned yet)"}, 1, 0x7f, 0xd0, 0xff},
    {"PAYLOADS", {"nami_probe", "ps5_pup_decrypt", "(scan /data)"}, 3, 0xbd, 0xa6, 0xff},
    {"FILES",    {"Desktop", "/data", "/mnt/usb0", "/mnt/usb1"}, 4, 0x8a, 0xe6, 0xb4},
    {"NETWORK",  {"FTP server", "Web UI :8080", "IP info"}, 3, 0x6f, 0xd0, 0xe8},
    {"DISC",     {"Drive status", "Activate (research)"}, 2, 0xff, 0xcf, 0x7a},
    {"SETTINGS", {"Video", "Audio", "System", "About Nami"}, 4, 0xc0, 0xc8, 0xd8},
    {"POWER",    {"Exit Nami"}, 1, 0xff, 0x8a, 0x8a},
};
#define N_CATS ((int)(sizeof(g_cats) / sizeof(g_cats[0])))

static void
local_ipv4(char *out, size_t n) {
    struct ifaddrs *ifs = NULL, *p;
    snprintf(out, n, "offline");
    if (getifaddrs(&ifs) != 0) {
        return;
    }
    for (p = ifs; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) {
            continue;
        }
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

/* Original category glyph: a rounded-ish diamond made of two triangles,
 * tinted with the category accent. Not based on any existing icon set. */
static void
draw_glyph(SDL_Renderer *r, int cx, int cy, int size, int active,
           Uint8 cr, Uint8 cg, Uint8 cb) {
    int s = active ? size : size - 10;
    Uint8 a = active ? 255 : 150;
    /* outer frame */
    fill(r, cx - s, cy - s, s * 2, s * 2, cr, cg, cb, (Uint8)(a / 4));
    /* inner square rotated-feel: just a smaller solid block + border rules */
    fill(r, cx - s + 6, cy - s + 6, s * 2 - 12, s * 2 - 12, cr, cg, cb, a);
    if (active) {
        fill(r, cx - s, cy - s, s * 2, 3, 0xff, 0xff, 0xff, 200);
    }
}

int
main(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        return 1;
    }
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

    int sel_cat = 0;
    int sel_item = 0;
    char status[128];
    snprintf(status, sizeof(status), "D-pad to move  -  Cross to select  -  Options to exit");
    float cat_scroll = 0.0f;     /* animated toward sel_cat */

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
                    case SDLK_LEFT:  left = 1;  break;
                    case SDLK_RIGHT: right = 1; break;
                    case SDLK_UP:    up = 1;    break;
                    case SDLK_DOWN:  down = 1;  break;
                    case SDLK_RETURN: select = 1; break;
                    default: break;
                }
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  left = 1;  break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: right = 1; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:    up = 1;    break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  down = 1;  break;
                    case SDL_CONTROLLER_BUTTON_A:          select = 1; break;
                    case SDL_CONTROLLER_BUTTON_START:      running = 0; break;
                    default: break;
                }
            }

            if (left && sel_cat > 0)            { sel_cat--; sel_item = 0; }
            if (right && sel_cat < N_CATS - 1)  { sel_cat++; sel_item = 0; }
            if (up && sel_item > 0)             { sel_item--; }
            if (down && sel_item < g_cats[sel_cat].item_count - 1) { sel_item++; }
            if (select) {
                snprintf(status, sizeof(status), "%s > %s   (action in a later milestone)",
                         g_cats[sel_cat].label, g_cats[sel_cat].items[sel_item]);
            }
        }

        /* Ease the horizontal strip toward the selected category. */
        cat_scroll += ((float)sel_cat - cat_scroll) * 0.25f;

        /* ---- background gradient ---- */
        for (int band = 0; band < SCREEN_H; band += 6) {
            float t = (float)band / SCREEN_H;
            Uint8 rr = (Uint8)(0x0a + (0x17 - 0x0a) * (1.0f - t));
            Uint8 gg = (Uint8)(0x13 + (0x36 - 0x13) * (1.0f - t));
            Uint8 bb = (Uint8)(0x22 + (0x5c - 0x22) * (1.0f - t));
            fill(ren, 0, band, SCREEN_W, 6, rr, gg, bb, 255);
        }

        /* subtle focus line the active column rides on */
        fill(ren, 0, BAR_Y - 2, SCREEN_W, 2, 0xff, 0xff, 0xff, 22);

        text(ren, 60, 60, 4, 0x9f, 0xd0, 0xff, "NAMI");
        text(ren, 64, 120, 2, 0x5f, 0x76, 0xa0, "PS5 SHELL");

        /* ---- category strip ---- */
        for (int i = 0; i < N_CATS; i++) {
            int cx = FOCUS_X + (int)lroundf(((float)i - cat_scroll) * CAT_GAP);
            if (cx < -120 || cx > SCREEN_W + 120) {
                continue;
            }
            int active = (i == sel_cat);
            Category *c = &g_cats[i];
            draw_glyph(ren, cx, BAR_Y, active ? 34 : 24, active, c->r, c->g, c->b);
            int lw = (int)strlen(c->label) * 8 * 2;
            text(ren, cx - lw / 2, BAR_Y + 48, 2,
                 active ? 0xff : 0x66, active ? 0xff : 0x72,
                 active ? 0xff : 0x88, c->label);
        }

        /* ---- items of the active category ---- */
        Category *ac = &g_cats[sel_cat];
        for (int j = 0; j < ac->item_count; j++) {
            int iy = ITEM_Y0 + j * ITEM_DY;
            int on = (j == sel_item);
            if (on) {
                fill(ren, FOCUS_X - 40, iy - 8, 760, ITEM_DY - 12,
                     ac->r, ac->g, ac->b, 36);
                fill(ren, FOCUS_X - 40, iy - 8, 5, ITEM_DY - 12,
                     ac->r, ac->g, ac->b, 255);
            }
            text(ren, FOCUS_X - 12, iy, on ? 3 : 2,
                 on ? 0xf0 : 0x90, on ? 0xf6 : 0x9c, on ? 0xff : 0xb8,
                 ac->items[j]);
        }

        /* ---- status line ---- */
        text(ren, 60, SCREEN_H - 130, 2, 0x7a, 0x8a, 0xb0, status);

        /* ---- bottom-right IP/port HUD (PS3-JB style) ---- */
        int bw = 460, bh = 70;
        int bx = SCREEN_W - bw - 48, by = SCREEN_H - bh - 48;
        fill(ren, bx, by, bw, bh, 0x0e, 0x2a, 0x1d, 255);
        fill(ren, bx, by, bw, 3, 0x2b, 0xd4, 0x7e, 255);
        fill(ren, bx + 20, by + 28, 12, 12, 0x2b, 0xd4, 0x7e, 255);
        text(ren, bx + 48, by + 27, 2, 0x9f, 0xf0, 0xc2, hud);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    if (pad) {
        SDL_GameControllerClose(pad);
    }
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
