#!/usr/bin/env bash

mkdir -p sender_dir
mkdir -p receiver_dir

gcc sender.c -o send
gcc receiver.c -o receive

mv send sender_dir/
cp img_test.png sender_dir/
mv receive receiver_dir/

if ! command -v tmux &> /dev/null; then
    echo "Tmux is not installed. Please intall TMUX."
    exit 1
fi

SESSION_NAME="udp_rdt"
tmux new-session -d -s $SESSION_NAME

tmux send-keys -t $SESSION_NAME "cd receiver_dir && ./receive 1234 0.1" C-m

sleep 1

tmux split-window -h -t $SESSION_NAME
tmux send-keys -t $SESSION_NAME:0.1 "cd sender_dir && ./send 4321 127.0.0.1 1234 1 img_test.png 0.1" C-m

tmux attach -t $SESSION_NAME
