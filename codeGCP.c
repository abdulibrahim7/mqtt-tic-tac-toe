#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <unistd.h>

#define BROKER_HOST "localhost"
#define BROKER_PORT 1883
#define TOPIC_MOVE "tictactoe/move"
#define TOPIC_BOARD "tictactoe/board"
#define TOPIC_STATUS "tictactoe/status"
#define TOPIC_MODE "tictactoe/mode" 

char board[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
char currentPlayer = 'X';
int gameOver = 0;
int moveReceived = 0;
int pendingMove = -1;
int gameReady = 0;
int gameMode = 0;

struct mosquitto *mosq = NULL;

void printBoard() {
    system("clear");
    printf("TIC-TAC-TOE!\n");
    printf("Mode: %s\n", gameMode == 1 ? "1 Player (vs AI)" : "2 Players");
    printf("\n-------------\n");
    printf("| %c | %c | %c |\n", board[0], board[1], board[2]);
    printf("-------------\n");
    printf("| %c | %c | %c |\n", board[3], board[4], board[5]);
    printf("-------------\n");
    printf("| %c | %c | %c |\n", board[6], board[7], board[8]);
    printf("-------------\n\n");

    printf("Waiting for Player %c's move...\n", currentPlayer);
}

void publishBoard() {
    char message[32] = "";
    for (int i = 0; i < 9; i++) {
        char slot[3];
        if (i < 8) {
            snprintf(slot, sizeof(slot), "%c ", board[i]);
        } else {
            snprintf(slot, sizeof(slot), "%c", board[i]);
        }
        strcat(message, slot);
    }
    mosquitto_publish(mosq, NULL, TOPIC_BOARD, strlen(message), message, 0, false);
    printf("Published board: %s\n", message);
}

void publishStatus(const char *status) {
    mosquitto_publish(mosq, NULL, TOPIC_STATUS, strlen(status), status, 0, false);
    printf("Published status: %s\n", status);
}

int checkWinner(char player) {
    int wins[8][3] = {
        {0, 1, 2}, //top row
        {3, 4, 5}, //middle row
        {6, 7, 8}, //bottom row
        {0, 3, 6}, //left column
        {1, 4, 7}, //middle column
        {2, 5, 8}, //right column
        {0, 4, 8}, //diag. top left to bottom right
        {2, 4, 6} //diag. top right to bottom left
    };

    for (int i = 0; i < 8; i++) {
        if (board[wins[i][0]] == player &&
            board[wins[i][1]] == player &&
            board[wins[i][2]] == player) {
                return 1;
            }
    }
    return 0;
}

int checkDraw() {
    for (int i = 0; i < 9; i++) {
        if (board[i] != 'X' && board[i] != 'O') {
            return 0;
        }
    }
    return 1;
}

void placeMove(int position, char player) {
    board[position - 1] = player;
}

char switchPlayer(char current) {
    if (current == 'X') {
        return 'O';
    }
    return 'X';
}

void onMessage(struct mosquitto *m, void *userdata, const struct mosquitto_message *msg) {
    char payload[256];
    int len = msg->payloadlen < 255 ? msg->payloadlen : 255;
    strncpy(payload, (char *)msg->payload, len);
    payload[len] = '\0';
 
    printf("  Received [%s]: %s\n", msg->topic, payload);
 
    if (strcmp(msg->topic, TOPIC_MODE) == 0) {
        gameMode  = payload[0] - '0';
        gameReady = 1;
 
        if (gameMode == 1) {
            printf("\n  1 Player mode. Launching AI script...\n");
            system("bash ~/AIPlayer.sh &");
            sleep(3);
            printf("AI ready!\n\n");
        } else {
            printf("\n  2 Player mode.\n\n");
        }
        return;
    }
 
    if (strcmp(msg->topic, TOPIC_MOVE) == 0) {
        if (gameOver || !gameReady) return;
 
        // parse format  (ex: "X:5")
        if (strlen(payload) < 3 || payload[1] != ':') {
            printf("  Bad message format. Expected X:5\n");
            return;
        }
 
        char player  = payload[0];
        int position = payload[2] - '0';
 
        if (player != currentPlayer) {
            printf("Not %c's turn. Ignoring.\n", player);
            return;
        }
 
        if (position < 1 || position > 9) {
            printf("Position out of range.\n");
            return;
        }
 
        if (board[position - 1] == 'X' || board[position - 1] == 'O') {
            printf("Spot %d is already taken.\n", position);
            return;
        }
        pendingMove  = position;
        moveReceived = 1;
    }
}

void connectMQTT() {
    mosquitto_lib_init();

    mosq = mosquitto_new("GCP_TicTacToe", true, NULL);
    if (!mosq) {
        fprintf(stderr, "Error: Could not create instance.\n");
        exit(1);
    }

    mosquitto_message_callback_set(mosq, onMessage);

    int rc = mosquitto_connect(mosq, BROKER_HOST, BROKER_PORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Error: Could not connect to broker. rc=%d\n", rc);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        exit(1);
    }

    mosquitto_subscribe(mosq, NULL, TOPIC_MODE, 0);
    mosquitto_subscribe(mosq, NULL, TOPIC_MOVE, 0);
    printf("Connected to broker! Subscribed to %s\n", TOPIC_MOVE);
}

int main() {
    connectMQTT();
 
    printf("\n       TIC-TAC-TOE via MQTT    \n\n");
    printf("Waiting for GUI to select game mode...\n\n");
 
    while (!gameReady) {
        mosquitto_loop(mosq, 100, 1);
   }
    printf("Game starting!\n\n");
    publishBoard();
    publishStatus("X_TURN");
    printBoard();
 
    while (!gameOver) {
        mosquitto_loop(mosq, 100, 1);
 
        if (moveReceived) {
            moveReceived = 0;
            placeMove(pendingMove, currentPlayer);
            printBoard();
            publishBoard();
 
            if (checkWinner(currentPlayer)) {
                if (currentPlayer == 'X') {
                    publishStatus("X_WINS");
                    printf("Player X wins!\n\n");
                } else {
                    publishStatus("O_WINS");
                    printf("Player O wins!\n\n");
                }
                gameOver = 1;
                break;
            }
 
            if (checkDraw()) {
                publishStatus("DRAW");
                printf("It's a draw!\n\n");
                gameOver = 1;
                break;
            }

            currentPlayer = switchPlayer(currentPlayer);
            if (currentPlayer == 'X') {
                publishStatus("X_TURN");
            } else {
                publishStatus("O_TURN");
            }
 
            printBoard();
        }
    }
 
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}