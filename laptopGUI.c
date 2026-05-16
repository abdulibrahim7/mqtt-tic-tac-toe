#include <stdio.h>
#include <string.h>
#include <mosquitto.h>
#include <raylib.h>
#include <stdlib.h>

#define BROKER_HOST "diazcs2600.duckdns.org"
#define BROKER_PORT 8883
#define TOPIC_MOVE "tictactoe/move"
#define TOPIC_BOARD "tictactoe/board"
#define TOPIC_STATUS "tictactoe/status"
#define TOPIC_MODE "tictactoe/mode"

#define SCREEN_HEIGHT 540
#define SCREEN_WIDTH 660
#define CELL_SIZE 160

#define GRID_Y 90
#define GRID_PX 480

typedef enum {
    SCREEN_MODE_SELECT,
    SCREEN_WAITING,
    SCREEN_GAME,
    SCREEN_GAME_OVER
} Screen;

struct mosquitto *mosq = NULL;
char board[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
char statusMsg[64] = "Connecting...";
char myPlayer = 'O';
int myTurn = 0;
int gameOver = 0;
int gameMode = 0;
Screen currentScreen = SCREEN_MODE_SELECT;

void onMessage(struct mosquitto *m, void *userdata, const struct mosquitto_message *msg);

void parseBoard(const char *msg) {
    char temp[128];
    strncpy(temp, msg, 127);
    temp[127] = '\0';

    int idx = 0;
    char *token = strtok(temp, " ");
    while (token && idx < 9) {
        board[idx++] = token[0];
        token = strtok(NULL, " ");
    }
}

void connectMQTT() {
    mosquitto_lib_init();

    mosq = mosquitto_new("Laptop_Player", true, NULL);
    if (!mosq) {
        fprintf(stderr, "Error creating mosquitto instance\n");
        exit(1);
    }

    mosquitto_message_callback_set(mosq, onMessage);

    int rc = mosquitto_connect(mosq, BROKER_HOST, BROKER_PORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Could not connect to broker. rc=%d\n", rc);
        exit(1);
    }

    mosquitto_subscribe(mosq, NULL, TOPIC_BOARD, 0);
    mosquitto_subscribe(mosq, NULL, TOPIC_STATUS, 0);

    mosquitto_loop_start(mosq);

    printf("Connected to broker.\n");
}

void publishMove(int position) {
    char msg[8];
    char player;
    if (gameMode == 1) {
        player = 'X';
    } else {
        player = 'O';
    }

    snprintf(msg, sizeof(msg), "%c:%d", player, position);
    mosquitto_publish(mosq, NULL, TOPIC_MOVE, strlen(msg), msg, 0, false);
    printf("Published move: %s\n", msg);
}

void publishMode(int mode) {
    char msg[2] = { '0' + mode, '\0'};
    mosquitto_publish(mosq, NULL, TOPIC_MODE, strlen(msg), msg, 0, false);
    printf("Published mode: %s\n", msg);
}

void onMessage(struct mosquitto *m, void *userdata, const struct mosquitto_message *msg) {
    
    char payload[256];
    int len = msg->payloadlen < 255 ? msg->payloadlen : 255;

    strncpy(payload, (char *)msg->payload, len);
    payload[len] = '\0';

    if (strcmp(msg->topic, TOPIC_BOARD) == 0) {
        parseBoard(payload);
        if (currentScreen == SCREEN_WAITING) {
            currentScreen = SCREEN_GAME;
        }
    } else if (strcmp(msg->topic, TOPIC_STATUS) == 0) {
        if (strcmp(payload, "X_TURN") == 0) {
            if (gameMode == 1) {
                strcpy(statusMsg, "Your turn! Select a square (X)...");
                myTurn = 1;
            } else {
                strcpy(statusMsg, "Waiting for ESP32 (X)...");
                myTurn = 0;
            }
        } else if (strcmp(payload, "O_TURN") == 0) {
            if (gameMode == 1) {
                strcpy(statusMsg, "AI (O) is thinking...");
                myTurn = 0;
            } else {
            strcpy(statusMsg, "Your turn! Click a square (O)");
            myTurn = 1;
            }
        } else if (strcmp(payload, "X_WINS") == 0) {
            if (gameMode == 1) {
                strcpy(statusMsg, "You win!");
            } else {
            strcpy(statusMsg, "ESP32 Wins!");
            }
            gameOver = 1;
            myTurn = 0;
            currentScreen = SCREEN_GAME_OVER;
        } else if (strcmp(payload, "O_WINS") == 0) {
            if (gameMode == 1) {
                strcpy(statusMsg, "You lose!");
            } else {
            strcpy(statusMsg, "You win!");
            }
            gameOver = 1;
            myTurn = 0;
            currentScreen = SCREEN_GAME_OVER;
        } else if (strcmp(payload, "DRAW") == 0) {
            strcpy(statusMsg, "It's a Draw!");
            gameOver = 1;
            myTurn = 0;
            currentScreen = SCREEN_GAME_OVER;
        }
    }
}

int getCellFromMouse(Vector2 mouse) {
    int gridX = GetScreenWidth() / 2 - GRID_PX / 2;

    int col = (int)(mouse.x - gridX) / CELL_SIZE;
    int row = (int)(mouse.y - GRID_Y) / CELL_SIZE;

    if (mouse.x < gridX || mouse.y < GRID_Y) {
        return -1;
    }

    if (col < 0 || col > 2 || row < 0 || row > 2) {
        return -1;
    }

    return row * 3 + col + 1;
}
 
void drawBoard(Color gridColor, Color xColor, Color oColor, Color hoverColor, Color numberColor, Color statusColor) {

    int gridX = GetScreenWidth() / 2 - GRID_PX / 2;
    Vector2 mouse = GetMousePosition();
    int hoverCell = getCellFromMouse(mouse);
 
    // status msg
    DrawText(statusMsg, SCREEN_WIDTH / 2 - MeasureText(statusMsg, 18) / 2, 58, 18, statusColor);
 
    // grid lines
    for (int i = 1; i < 3; i++) {
        DrawRectangle(gridX + i * CELL_SIZE - 2, GRID_Y, 4, GRID_PX, gridColor);
        DrawRectangle(gridX, GRID_Y + i * CELL_SIZE - 2, GRID_PX, 4, gridColor);
    }
 
    // cells
    for (int i = 0; i < 9; i++) {
        int row = i / 3;
        int col = i % 3;
        int cx  = gridX + col * CELL_SIZE + CELL_SIZE / 2;
        int cy  = GRID_Y + row * CELL_SIZE + CELL_SIZE / 2;
        int x   = gridX + col * CELL_SIZE;
        int y   = GRID_Y + row * CELL_SIZE;
 
        // Hover highlight
        if (!gameOver && myTurn && (i + 1) == hoverCell && board[i] != 'X' && board[i] != 'O') {
            DrawRectangle(x + 3, y + 3, CELL_SIZE - 6, CELL_SIZE - 6, hoverColor);
        }
 
        if (board[i] == 'X') {
            int s = 52;
            DrawLineEx((Vector2){cx - s, cy - s}, (Vector2){cx + s, cy + s}, 9, xColor);
            DrawLineEx((Vector2){cx + s, cy - s}, (Vector2){cx - s, cy + s}, 9, xColor);
        } else if (board[i] == 'O') {
            DrawCircleLines(cx, cy, 52, oColor);
            DrawCircleLines(cx, cy, 50, oColor);
            DrawCircleLines(cx, cy, 48, oColor);
        } else {
            char num[2] = { board[i], '\0' };
            DrawText(num, cx - MeasureText(num, 22) / 2, cy - 11, 22, numberColor);
        }
    }
 
    char playerLabel[16];
    snprintf(playerLabel, sizeof(playerLabel), "You are: %c", myPlayer);
    Color playerColor;
    if (myPlayer == 'X') {
        playerColor = xColor;
    } else {
        playerColor = oColor;
    }
    
 
    // Mode indicator
    const char *modeStr;
    if (gameMode == 1) {
        modeStr = "Mode: 1 Player (AI)";
    } else {
        modeStr = "Mode: 2 Players";
    }
    DrawText(modeStr, SCREEN_WIDTH - MeasureText(modeStr, 14) - 10, SCREEN_HEIGHT- 28, 14, (Color){ 80, 80, 100, 255 });
}

int main() {
    connectMQTT();
 
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "TicTacToe - Laptop Player");
    SetTargetFPS(60);
 
    Color bgColor     = {  15,  15,  25, 255 };
    Color gridColor   = {  60,  60,  90, 255 };
    Color xColor      = { 255,  80,  80, 255 };
    Color oColor      = {  80, 180, 255, 255 };
    Color hoverColor  = {  80,  80, 120,  80 };
    Color numberColor = {  55,  55,  75, 255 };
    Color statusColor = { 200, 200, 220, 255 };
    Color dimColor    = { 120, 120, 140, 255 };
    Color highlightColor = { 255, 210,  80, 255 };
 
    while (!WindowShouldClose()) {
 
        BeginDrawing();
        ClearBackground(bgColor);
 
        const char *title = "TIC-TAC-TOE";
        DrawText(title, SCREEN_WIDTH / 2 - MeasureText(title, 32) / 2, 18, 32, WHITE);
 
        if (currentScreen == SCREEN_MODE_SELECT) {
 
            DrawText("Select Game Mode", SCREEN_WIDTH / 2 - MeasureText("Select Game Mode", 24) / 2, 140, 24, statusColor);
 
            // Option 1 
            bool hover1 = CheckCollisionPointRec(GetMousePosition(), (Rectangle){ 110, 220, 320, 80 });
            DrawRectangleRounded((Rectangle){ SCREEN_WIDTH / 2 - 160, 220, 320, 80 }, 0.2f, 8, hover1 ? highlightColor : (Color){ 40, 40, 70, 255 });
            DrawText("1 Player", SCREEN_WIDTH / 2 - MeasureText("1 Player", 28) / 2, 238, 28, hover1 ? bgColor : WHITE);
            DrawText("(You vs AI)", SCREEN_WIDTH / 2 - MeasureText("(You vs AI)", 16) / 2, 272, 16, hover1 ? bgColor : dimColor);
 
            // Option 2
            bool hover2 = CheckCollisionPointRec(GetMousePosition(), (Rectangle){ 110, 340, 320, 80 });
            DrawRectangleRounded((Rectangle){ SCREEN_WIDTH / 2 - 160, 340, 320, 80 }, 0.2f, 8, hover2 ? highlightColor : (Color){ 40, 40, 70, 255 });
            DrawText("2 Player", SCREEN_WIDTH / 2 - MeasureText("2 Player", 28) / 2, 358, 28, hover2 ? bgColor : WHITE);
            DrawText("(You vs ESP32 Player)", SCREEN_WIDTH / 2 - MeasureText("(You vs ESP32 Player)", 16) / 2, 392, 16, hover2 ? bgColor : dimColor);
 
            DrawText("You will play as X if solo or O if 2 player", SCREEN_WIDTH / 2 - MeasureText("You will play as X if solo or O if 2 player", 16) / 2, 460, 16, (Color){ 80, 180, 255, 200 });
 
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (hover1) {
                    gameMode = 1;
                    myPlayer = 'X';
                    currentScreen = SCREEN_WAITING;
                    publishMode(1);
                } else if (hover2) {
                    gameMode = 2;
                    myPlayer = 'O';
                    currentScreen = SCREEN_WAITING;
                    publishMode(2);
                }
            }
 
            if (IsKeyPressed(KEY_ONE)) {
                gameMode = 1;
                myPlayer = 'X';
                currentScreen = SCREEN_WAITING;
                strcpy(statusMsg, "Starting 1 Player game...");
                publishMode(1);
            }
            if (IsKeyPressed(KEY_TWO)) {
                gameMode = 2;
                myPlayer = 'O';
                currentScreen = SCREEN_WAITING;
                strcpy(statusMsg, "Starting 2 Player game...");
                publishMode(2);
            }
        }
 
        // waiting for gcp
        else if (currentScreen == SCREEN_WAITING) {
            DrawText("Waiting for game to start...", SCREEN_WIDTH / 2 - MeasureText("Waiting for game to start...", 20) / 2, SCREEN_HEIGHT / 2 - 10, 20, statusColor);
 
            const char *modeStr;
            if (gameMode == 1) {
                modeStr = "Mode: 1 Player (vs AI)";
            } else {
                modeStr = "Mode: 2 Players";
            }
            DrawText(modeStr, SCREEN_WIDTH / 2 - MeasureText(modeStr, 16) / 2, SCREEN_HEIGHT / 2 + 30, 16, dimColor);
        }
 
        // game in prog
        else if (currentScreen == SCREEN_GAME) {
 
            if (!gameOver && myTurn && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                int cell = getCellFromMouse(mouse);
                if (cell >= 1 && cell <= 9 && board[cell - 1] != 'X' && board[cell - 1] != 'O') {
                    publishMove(cell);
                    myTurn = 0;
                    strcpy(statusMsg, "Move sent! Waiting...");
                }
            }
 
            drawBoard(gridColor, xColor, oColor, hoverColor, numberColor, statusColor);
        }
 
        // game over
        else if (currentScreen == SCREEN_GAME_OVER) {
            drawBoard(gridColor, xColor, oColor, hoverColor, numberColor, statusColor);
 
            DrawRectangle(0, SCREEN_HEIGHT / 2 - 60, SCREEN_WIDTH, 120,(Color){ 0, 0, 0, 210 });
            DrawText(statusMsg, SCREEN_WIDTH / 2 - MeasureText(statusMsg, 38) / 2, SCREEN_HEIGHT / 2 - 19, 38, WHITE);
            DrawText("Close window to exit", SCREEN_WIDTH / 2 - MeasureText("Close window to exit", 16) / 2,SCREEN_HEIGHT / 2 + 30, 16, dimColor);
        }
 
        EndDrawing();
    }
 
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    CloseWindow();
    return 0;
}
