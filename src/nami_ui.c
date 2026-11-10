/* Nami - PS5 homebrew shell UI (M1: first frame)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Launched as a bigapp by nami_bootstrap.elf so it has video-out + pad.
 * M1 goal: open full-screen, clear to the Nami background, draw the title
 * and the bottom-right IP/port HUD box. XMB navigation lands in M2.
 */
#include <SDL.h>
#include <SDL_test_font.h>

#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>

#define SCREEN_W 1920
#define SCREEN_H 1080
#define WEB_PORT 8080

/* Find the first non-loopback IPv4 address for the HUD. */
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

/* Filled rectangle helper. */
static void
fill(SDL_Renderer *r, int x, int y, int w, int h,
     Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_RenderFillRect(r, &rect);
}

/* SDL_test 8x8 font, drawn at an integer scale via a temporary scale. */
static void
text(SDL_Renderer *r, int x, int y, int scale,
     Uint8 cr, Uint8 cg, Uint8 cb, const char *s) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderSetScale(r, (float)scale, (float)scale);
    SDLTest_DrawString(r, (float)(x / scale), (float)(y / scale), s);
    SDL_RenderSetScale(r, 1.0f, 1.0f);
}

int
main(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "Nami", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_FULLSCREEN);
    if (!win) {
        SDL_Quit();
        return 2;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 3;
    }

    /* Grab the first controller so we can read the quit button. */
    SDL_GameController *pad = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            pad = SDL_GameControllerOpen(i);
            if (pad) {
                break;
            }
        }
    }

    char ip[64];
    local_ipv4(ip, sizeof(ip));
    char hud[128];
    snprintf(hud, sizeof(hud), "IP %s   PORT %d", ip, WEB_PORT);

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else if (e.type == SDL_KEYDOWN &&
                       e.key.keysym.sym == SDLK_ESCAPE) {
                running = 0;
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                /* Options/Start quits for now (M2 replaces this). */
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    running = 0;
                }
            }
        }

        /* Background: deep blue top, near-black bottom (banded fills). */
        for (int band = 0; band < SCREEN_H; band += 8) {
            float t = (float)band / SCREEN_H;
            Uint8 rr = (Uint8)(0x0a + (0x17 - 0x0a) * (1.0f - t));
            Uint8 gg = (Uint8)(0x13 + (0x36 - 0x13) * (1.0f - t));
            Uint8 bb = (Uint8)(0x22 + (0x5c - 0x22) * (1.0f - t));
            fill(ren, 0, band, SCREEN_W, 8, rr, gg, bb, 255);
        }

        /* Title block. */
        text(ren, 120, 110, 8, 0x9f, 0xd0, 0xff, "NAMI");
        text(ren, 124, 190, 3, 0x6f, 0x86, 0xb0, "PS5 SHELL  -  M1");

        /* Center placeholder (XMB arrives in M2). */
        text(ren, 120, 520, 3, 0x8a, 0x9a, 0xc0,
             "first frame on the TV. XMB navigation next.");

        /* Bottom-right HUD box (PS3-JB style): IP + port. */
        int bw = 460, bh = 70;
        int bx = SCREEN_W - bw - 48, by = SCREEN_H - bh - 48;
        fill(ren, bx, by, bw, bh, 0x0e, 0x2a, 0x1d, 255);       /* panel */
        fill(ren, bx, by, bw, 3, 0x2b, 0xd4, 0x7e, 255);        /* top rule */
        fill(ren, bx + 20, by + 28, 12, 12, 0x2b, 0xd4, 0x7e, 255); /* dot */
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
