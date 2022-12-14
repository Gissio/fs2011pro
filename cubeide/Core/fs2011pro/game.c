/*
 * FS2011 Pro
 * Nuclear chess
 *
 * (C) 2022 Gissio
 *
 * License: MIT
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "format.h"
#include "keyboard.h"
#include "menus.h"
#include "settings.h"
#include "ui.h"

#include "mcu-max/mcu-max.h"

#define GAME_HISTORY_SIZE 128
#define GAME_VALID_MOVES_NUM_MAX 128

enum GameState
{
    GAME_SELECT_FIRST_MOVE,
    GAME_SELECT_SECOND_MOVE,
    GAME_PLAY_MOVE,
    GAME_UNDO_MOVE,
};

struct Game
{
    bool isPlayerBlack;

    enum GameState state;
    char board[8][9];
    int time[2];
    mcumax_move move;

    int moveIndex;

    mcumax_move history[GAME_HISTORY_SIZE];

    int validMovesNum;
    int validMovesIndex;
    mcumax_move validMoves[GAME_VALID_MOVES_NUM_MAX];

    bool isButtonSelected;
} game;

const char *const gamePieceMap = "@AACFBDEHIIKNJLM";

const int gameSkillToNodesCount[] = {
    250,
    500,
    1000,
    2000,
    4000,
    8000,
    16000,
    32000,
};

bool isGameStart()
{
    return ((game.moveIndex == 0) ||
            ((game.state == GAME_SELECT_FIRST_MOVE) &&
             (game.validMovesNum == 0)));
}

bool isTimerRunning()
{
    return (getView() == VIEW_GAME) &&
           !((game.state == GAME_SELECT_FIRST_MOVE) &&
             (game.validMovesNum == 0));
}

int getGameCurrentSide()
{
    return (game.moveIndex & 0x1);
}

bool isGamePlayerMove()
{
    return (getGameCurrentSide() == game.isPlayerBlack);
}

bool isGameUndoPossible()
{
    return isGamePlayerMove() &&
           (game.moveIndex > 1) &&
           (game.moveIndex < GAME_HISTORY_SIZE);
}

void recordGameMove(mcumax_move move)
{
    if (game.moveIndex >= GAME_VALID_MOVES_NUM_MAX)
        return;

    game.history[game.moveIndex++] = move;
}

void undoGameMove()
{
    game.moveIndex -= 2;
    if (game.moveIndex < 0)
        game.moveIndex = 0;

    mcumax_reset();

    for (int i = 0; i < game.moveIndex; i++)
        mcumax_play_move(game.history[i]);
}

void updateGameBoard()
{
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            mcumax_square square = ((0x10 * y) + x);

            int alt = (((x + y) & 0x1) << 4);
            if ((square == game.move.from) || (square == game.move.to))
                alt = (1 << 5);

            int piece = mcumax_get_piece(square) & 0xf;

            if (game.isPlayerBlack)
                game.board[7 - y][7 - x] = alt + gamePieceMap[piece];
            else
                game.board[y][x] = alt + gamePieceMap[piece];
        }

        game.board[y][8] = '\0';
    }
}

void onGameCallback(void *userdata)
{
    updateUI();
}

void updateValidMoves()
{
    game.validMovesNum = mcumax_get_valid_moves(game.validMoves, GAME_VALID_MOVES_NUM_MAX);
    game.validMovesIndex = game.isPlayerBlack ? (game.validMovesNum - 1) : 0;

    if (game.moveIndex == 0)
        game.move = (mcumax_move){game.validMoves[game.validMovesIndex].from, MCUMAX_INVALID};

    game.isButtonSelected = (game.validMovesNum == 0);

    game.state = GAME_SELECT_FIRST_MOVE;
}

void resetGame(int isPlayerBlack)
{
    mcumax_reset();
    mcumax_set_callback(onGameCallback, NULL);

    game.isPlayerBlack = isPlayerBlack;
    game.moveIndex = 0;
    game.validMovesIndex = 0;

    game.time[0] = 0;
    game.time[1] = 0;
    game.move = (mcumax_move){MCUMAX_INVALID, MCUMAX_INVALID};
    game.isButtonSelected = isPlayerBlack;

    if (!isPlayerBlack)
    {
        game.state = GAME_SELECT_FIRST_MOVE;
        updateValidMoves();
    }
    else
        game.state = GAME_PLAY_MOVE;

    updateGameBoard();
}

void updateGame()
{
    switch (game.state)
    {
    case GAME_PLAY_MOVE:
        if (game.move.from != MCUMAX_INVALID)
        {
            mcumax_play_move(game.move);

            recordGameMove(game.move);

            game.move = (mcumax_move){MCUMAX_INVALID, MCUMAX_INVALID};

            updateGameBoard();
            updateView();
        }

        if (!isGamePlayerMove())
        {
            if (!mcumax_play_best_move(gameSkillToNodesCount[settings.gameSkillLevel], &game.move))
            {
                game.state = GAME_SELECT_FIRST_MOVE;

                return;
            }

            recordGameMove(game.move);
        }

        updateValidMoves();
        updateGameBoard();
        updateView();

        break;

    case GAME_UNDO_MOVE:
        undoGameMove();

        updateValidMoves();
        updateGameBoard();
        updateView();

        break;

    default:
        break;
    }
}

void updateGameTimer()
{
    if (!isTimerRunning())
        return;

    int side = getGameCurrentSide();

    int time = game.time[side] + 1;
    if (time < 3600)
        game.time[side] = time;
}

void formatGameMove(mcumax_move move, char *buffer)
{
    sprintf(buffer, "%c%c-%c%c",
            'a' + (move.from & 0x7),
            '1' + 7 - ((move.from & 0x70) >> 4),
            'a' + (move.to & 0x7),
            '1' + 7 - ((move.to & 0x70) >> 4));
}

void drawGameView()
{
    // Time
    char time[2][6];
    formatTime(game.time[!game.isPlayerBlack], time[0]);
    formatTime(game.time[game.isPlayerBlack], time[1]);

    // Moves
    int start = ((game.moveIndex + 1) & ~0x1) - 2 * GAME_MOVES_LINE_NUM;
    if (start < 0)
        start = 0;

    char moves[GAME_MOVES_LINE_NUM][2][6];

    for (int y = 0; y < GAME_MOVES_LINE_NUM; y++)
    {
        for (int x = 0; x < 2; x++)
        {
            int index = start + 2 * y + x;

            if ((index < game.moveIndex) &&
                (index < GAME_HISTORY_SIZE) &&
                game.history[index].from != MCUMAX_INVALID)
                formatGameMove(game.history[index], moves[y][x]);
            else
                strcpy(moves[y][x], "");
        }
    }

    char *buttonText;
    if (isGameUndoPossible() ||
        (game.state == GAME_SELECT_SECOND_MOVE))
        buttonText = "Undo";
    else
        buttonText = NULL;

    bool buttonSelected = game.isButtonSelected;

    drawGameBoard(game.board, time, moves, buttonText, buttonSelected);
}

void traverseGameMoveTo(int direction)
{
    mcumax_square from = game.validMoves[game.validMovesIndex].from;

    while (true)
    {
        game.validMovesIndex += direction;

        if ((game.validMovesIndex < 0) || (game.validMovesIndex >= game.validMovesNum))
            break;

        if (game.validMoves[game.validMovesIndex].from != from)
            break;
    }

    game.validMovesIndex -= direction;
}

void selectGameMoveFrom(int direction)
{
    if (!game.validMovesNum)
        return;

    if (game.isPlayerBlack)
        direction = -direction;

    if (game.isButtonSelected)
    {
        game.isButtonSelected = false;

        if (direction == 1)
            game.validMovesIndex = 0;
        else
            game.validMovesIndex = game.validMovesNum - 1;
    }
    else
    {
        traverseGameMoveTo(direction);

        game.validMovesIndex += direction;

        if (game.validMovesIndex >= game.validMovesNum)
        {
            game.validMovesIndex = 0;

            if (isGameUndoPossible())
                game.isButtonSelected = true;
        }
        else if (game.validMovesIndex < 0)
        {
            game.validMovesIndex = game.validMovesNum - 1;

            if (isGameUndoPossible())
                game.isButtonSelected = true;
        }
    }

    traverseGameMoveTo(game.isPlayerBlack ? 1 : -1);

    if (game.isButtonSelected)
        game.move.from = MCUMAX_INVALID;
    else
        game.move.from = game.validMoves[game.validMovesIndex].from;
}

void selectGameMoveTo(int direction)
{
    if (game.isPlayerBlack)
        direction = -direction;

    if (game.isButtonSelected)
    {
        game.isButtonSelected = false;

        traverseGameMoveTo(-direction);
    }
    else
    {
        mcumax_square moveFrom = game.validMoves[game.validMovesIndex].from;

        int index = game.validMovesIndex + direction;
        if ((index < 0) ||
            (index >= game.validMovesNum) ||
            (game.validMoves[index].from != moveFrom))
        {
            traverseGameMoveTo(-direction);

            game.isButtonSelected = true;
        }
        else
            game.validMovesIndex = index;
    }

    game.move.from = game.validMoves[game.validMovesIndex].from;
    if (game.isButtonSelected)
        game.move.to = MCUMAX_INVALID;
    else
        game.move.to = game.validMoves[game.validMovesIndex].to;
}

void setGameMoveFrom()
{
    if (game.validMovesNum)
    {
        if (game.isPlayerBlack)
            game.move.from = game.validMoves[game.validMovesNum - 1].from;
        else
            game.move.from = game.validMoves[0].from;
    }
    else
        game.move.from = MCUMAX_INVALID;
    game.move.to = MCUMAX_INVALID;
}

void onGameViewKey(int key)
{
    switch (key)
    {
    case KEY_UP:
        if (game.state == GAME_SELECT_FIRST_MOVE)
        {
            if (game.move.to != MCUMAX_INVALID)
                setGameMoveFrom();
            else
                selectGameMoveFrom(-1);

            updateGameBoard();
            updateView();
        }
        else if (game.state == GAME_SELECT_SECOND_MOVE)
        {
            selectGameMoveTo(-1);

            updateGameBoard();
            updateView();
        }

        break;

    case KEY_DOWN:
        if (game.state == GAME_SELECT_FIRST_MOVE)
        {
            if (game.move.to != MCUMAX_INVALID)
                setGameMoveFrom();
            else
                selectGameMoveFrom(1);

            updateGameBoard();
            updateView();
        }
        else if (game.state == GAME_SELECT_SECOND_MOVE)
        {
            selectGameMoveTo(1);

            updateGameBoard();
            updateView();
        }

        break;

    case KEY_SELECT:
        if (isGamePlayerMove())
        {
            if (game.isButtonSelected)
            {
                if (game.state == GAME_SELECT_FIRST_MOVE)
                    game.state = GAME_UNDO_MOVE;
                else if (game.state == GAME_SELECT_SECOND_MOVE)
                {
                    game.state = GAME_SELECT_FIRST_MOVE;
                    game.move.to = MCUMAX_INVALID;
                    game.isButtonSelected = false;
                }
            }
            else if (game.state == GAME_SELECT_FIRST_MOVE)
            {
                if (game.move.to != MCUMAX_INVALID)
                    setGameMoveFrom();
                else
                {
                    game.state = GAME_SELECT_SECOND_MOVE;
                    game.move.to = game.validMoves[game.validMovesIndex].to;
                }

                updateGameBoard();
                updateView();
            }
            else if (game.state == GAME_SELECT_SECOND_MOVE)
            {
                game.state = GAME_PLAY_MOVE;
                game.isButtonSelected = true;
            }
        }
        break;

    case KEY_BACK:
        mcumax_stop_search();

        openGameMenu();

        break;
    }
}
