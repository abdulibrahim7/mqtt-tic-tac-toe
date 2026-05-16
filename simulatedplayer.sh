#!/bin/bash
 
BROKER="localhost"
PORT=1883
PLAYER="O"
TOPIC_MOVE="tictactoe/move"
TOPIC_BOARD="tictactoe/board"
TOPIC_STATUS="tictactoe/status"
 
BOARD_FILE="/tmp/tictactoe_board"
rm -f "$BOARD_FILE"  # clear any leftover from a previous game
 
echo "  TicTacToe AI Player"
echo "  Playing as: $PLAYER"
echo "  Broker: $BROKER:$PORT"
echo ""
echo "  Waiting for game to start..."
echo ""
 
# format: "X O 4 5 6 7 8 9"
pick_move() {
    local board_msg="$1"
    local available=()
 
    for slot in $board_msg; do
        if [[ "$slot" =~ ^[1-9]$ ]]; then
            available+=("$slot")
        fi
    done
 
    if [ ${#available[@]} -eq 0 ]; then
        echo "No available moves found."
        return
    fi
 
    local rand_index=$(( RANDOM % ${#available[@]} ))
    local chosen="${available[$rand_index]}"
 
    echo "Available spots: ${available[*]}"
    echo "AI chose: $chosen"
 
    mosquitto_pub -h "$BROKER" -p "$PORT" \
        -t "$TOPIC_MOVE" -m "${PLAYER}:${chosen}"
 
    echo "Published move: ${PLAYER}:${chosen}"
}
 
mosquitto_sub \
    -h "$BROKER" \
    -p "$PORT" \
    -t "$TOPIC_BOARD" \
    -t "$TOPIC_STATUS" \
    -F "%t %p" | while read -r MESSAGE; do
 
    # Split into topic and payload
    TOPIC=$(echo "$MESSAGE" | cut -d' ' -f1)
    PAYLOAD=$(echo "$MESSAGE" | cut -d' ' -f2-)
 
    echo "  Received [$TOPIC]: $PAYLOAD"
 
    if [ "$TOPIC" = "$TOPIC_BOARD" ]; then
        echo "$PAYLOAD" > "$BOARD_FILE"
        echo "  Board saved."
    fi
 
    if [ "$TOPIC" = "$TOPIC_STATUS" ]; then
        case "$PAYLOAD" in
            "X_TURN")
                echo "Waiting for Player X to move..."
                ;;
            "O_TURN")
                echo "It's my turn!"
                # Read board from temp file
                if [ -f "$BOARD_FILE" ]; then
                    LAST_BOARD=$(cat "$BOARD_FILE")
                    echo  "Current board: $LAST_BOARD"
                    echo "Thinking..."
                    sleep 3
                    pick_move "$LAST_BOARD"
                else
                    echo "No board saved yet, skipping move."
                fi
                ;;
            "X_WINS")
                echo ""
                echo "  Player X wins!"
                rm -f "$BOARD_FILE"
                break
                ;;
            "O_WINS")
                echo ""
                echo "  Player O wins!"
                rm -f "$BOARD_FILE"
                break
                ;;
            "DRAW")
                echo ""
                echo "  Draw!"
                rm -f "$BOARD_FILE"
                break
                ;;
        esac
    fi
 
done
 
echo ""
echo "  Game Over."

