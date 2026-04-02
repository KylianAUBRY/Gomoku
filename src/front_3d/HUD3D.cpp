// ─────────────────────────────────────────────────────────────────────────────
// HUD3D.cpp — HUD overlay 2D (Pass 3 du pipeline de rendu)
// ─────────────────────────────────────────────────────────────────────────────

// ⚠ ORDRE OBLIGATOIRE : engine avant raylib (conflit macros BLACK/WHITE)
// HUD3D.cpp ne compare pas de Cell directement, pas besoin de sauvegardes kBlack/kWhite.
#include "../engine/GameState.hpp"
#include "HUD3D.hpp"
#include "front3d_compat.hpp" // isBlackPlayer, makePlayerBlack, makePlayerWhite

#include "../engine/Rules.hpp"
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// draw_crosshair — réticule en croix au centre de l'écran
//
// Vert (has_hover=true) : le réticule pointe sur une case valide du goban.
// Blanc (has_hover=false) : hors du plateau.
// ─────────────────────────────────────────────────────────────────────────────
void draw_crosshair(bool has_hover) {
    int cx    = GetScreenWidth()  / 2;
    int cy    = GetScreenHeight() / 2;
    int size  = 12;
    int thick = 2;
    Color color = has_hover ? GREEN : WHITE;
    DrawRectangle(cx - size,  cy - thick / 2, size * 2, thick, color); // Horizontal
    DrawRectangle(cx - thick / 2, cy - size,  thick, size * 2, color); // Vertical
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_hud — barre de statut en bas de l'écran
//
// Informations affichées :
//   - Mode + commandes (gauche)
//   - Timer IA en cyan (exigence sujet)
//   - Tour courant (centré)
//   - Compteurs de captures (centré)
//   - Coordonnée visée "Aim: L-N" (haut droite, si has_hover)
// ─────────────────────────────────────────────────────────────────────────────
void draw_hud(const GameState& state, UI3DState current_state,
              bool has_hover, int hovered_row, int hovered_col) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // Fond semi-transparent
    DrawRectangle(0, sh - 80, sw, 80, {40, 40, 40, 200});

    // Tour courant (centré)
    const char* turn_label = isBlackPlayer(state.current_player) ? "NOIR" : "BLANC";
    char turn_buf[64];
    snprintf(turn_buf, sizeof(turn_buf), "Tour %d : %s", state.current_turn, turn_label);
    DrawText(turn_buf, sw / 2 - MeasureText(turn_buf, 24) / 2, sh - 70, 24, WHITE);

    // Mode à gauche
    const char* mode = (current_state == UI3DState::PLAYING_SOLO)
                           ? "Solo (vs IA) - ESC: Menu"
                           : "Multi (Local 1v1) - ESC: Menu";
    DrawText(mode, 20, sh - 70, 18, LIGHTGRAY);

    // Timer IA en cyan (obligatoire sujet)
    if (current_state == UI3DState::PLAYING_SOLO) {
        char timer_buf[64];
        snprintf(timer_buf, sizeof(timer_buf), "AI Time: %.2f ms", state.last_ai_move_time_ms);
        DrawText(timer_buf, 20, sh - 40, 18, {0, 200, 255, 255});
    }

    // Compteurs de captures (centré)
    char cap_buf[64];
    snprintf(cap_buf, sizeof(cap_buf), "Captures  N: %d/10  B: %d/10",
             state.board.blackCaptures, state.board.whiteCaptures);
    DrawText(cap_buf, sw / 2 - MeasureText(cap_buf, 18) / 2, sh - 35, 18, WHITE);

    // Coordonnée visée (haut droite, vert si valide)
    if (has_hover) {
        char aim_buf[32];
        char col_char = 'A' + hovered_col;
        snprintf(aim_buf, sizeof(aim_buf), "Aim: %c-%d", col_char, hovered_row + 1);
        DrawText(aim_buf, sw - 160, 20, 20, GREEN);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_game_over_overlay — écran de fin en 3D
//
// Overlay noir semi-transparent (alpha=150) + message vainqueur + dernier
// temps IA (Solo) + instruction [R] pour rejouer.
// ─────────────────────────────────────────────────────────────────────────────
void draw_game_over_overlay(const GameState& state, UI3DState current_state) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 150});

    bool black_wins = Rules::check_win_condition(state, makePlayerBlack()) ||
                      Rules::check_win_by_capture(state, makePlayerBlack());
    bool white_wins = Rules::check_win_condition(state, makePlayerWhite()) ||
                      Rules::check_win_by_capture(state, makePlayerWhite());

    const char* msg = "GAME OVER";
    if (black_wins)      msg = "NOIR GAGNE!";
    else if (white_wins) msg = "BLANC GAGNE!";

    DrawText(msg, sw / 2 - MeasureText(msg, 60) / 2, sh / 2 - 60, 60, RED);

    if (current_state == UI3DState::PLAYING_SOLO) {
        char timer_buf[64];
        snprintf(timer_buf, sizeof(timer_buf), "Dernier temps IA: %.2f ms",
                 state.last_ai_move_time_ms);
        DrawText(timer_buf, sw / 2 - MeasureText(timer_buf, 22) / 2,
                 sh / 2 + 10, 22, {0, 200, 255, 255});
    }

    const char* restart = "Appuyer [R] pour Rejouer";
    DrawText(restart, sw / 2 - MeasureText(restart, 24) / 2, sh / 2 + 50, 24, WHITE);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_capture_anims_3d — croix X en coordonnées écran aux positions capturées
//
// GetWorldToScreen(pos_3d, camera) : projette un point 3D en 2D écran.
// DrawLineEx : ligne 2D avec épaisseur (DrawLine n'en a pas).
// Alpha décroît proportionnellement à timer (timer * 255).
// ─────────────────────────────────────────────────────────────────────────────
void draw_capture_anims_3d(const std::vector<CaptureAnim3D>& anims,
                           const Camera3D& camera) {
    for (const auto& a : anims) {
        // boardToWorld n'est pas disponible ici sans inclure front3d_compat.hpp
        // on réécrit la conversion inline (col → X, 18-row → Y)
        Vector3  world_pos  = {(float)a.col, 18.0f - (float)a.row, 0.6f};
        Vector2  screen_pos = GetWorldToScreen(world_pos, camera);
        float    arm        = 16.0f;
        Color    red        = {220, 50, 50, (unsigned char)(a.timer * 255)};

        Vector2 tl = {screen_pos.x - arm, screen_pos.y - arm};
        Vector2 br = {screen_pos.x + arm, screen_pos.y + arm};
        Vector2 tr = {screen_pos.x + arm, screen_pos.y - arm};
        Vector2 bl = {screen_pos.x - arm, screen_pos.y + arm};
        DrawLineEx(tl, br, 3.0f, red);
        DrawLineEx(tr, bl, 3.0f, red);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_history — panneau historique des coups (gauche de l'écran)
//
// Fond semi-transparent (alpha=180). max_lines calculé dynamiquement selon
// la hauteur disponible. Format : "N. C L-R" (N=numéro, C=N/B, L=lettre col,
// R=numéro ligne).
// ─────────────────────────────────────────────────────────────────────────────
void draw_history(const GameState& state) {
    int sh      = GetScreenHeight();
    int panel_w = 200;
    int panel_h = sh - 80; // S'arrête avant la barre HUD

    DrawRectangle(0, 0, panel_w, panel_h, {50, 50, 50, 180});
    DrawText("Historique", 10, 10, 22, WHITE);
    DrawLine(10, 35, panel_w - 10, 35, GRAY);

    int max_lines = (panel_h - 50) / 25;
    int start_idx = 0;
    if ((int)state.move_history.size() > max_lines)
        start_idx = (int)state.move_history.size() - max_lines;

    int y = 45;
    for (int i = start_idx; i < (int)state.move_history.size(); i++) {
        const GameMove& mv       = state.move_history[i];
        char            col_char = 'A' + mv.x;
        char            line[64];
        snprintf(line, sizeof(line), "%d. %c %c-%d", mv.turn,
                 isBlackPlayer(mv.player) ? 'N' : 'B', col_char, mv.y + 1);
        // Gris clair pour Noir, blanc pour Blanc
        Color text_color = isBlackPlayer(mv.player) ? LIGHTGRAY : WHITE;
        DrawText(line, 10, y, 16, text_color);
        y += 25;
    }
}
