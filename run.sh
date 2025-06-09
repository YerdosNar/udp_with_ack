#!/usr/bin/env bash

# Compile both programs
gcc sender.c -o send
gcc receiver.c -o receive

mkdir sender_dir
mkdir receiver_dir

mv send sender_dir
cp test_files/* sender_dir/
mv receive receiver_dir/

if ! command -v tmux &> /dev/null; then
    echo "tmux is not installed. Please install tmux."
    exit 1
fi

SESSION_NAME="udp_transfer"
tmux new-session -d -s $SESSION_NAME

# execurte receive executable
tmux send-keys -t $SESSION_NAME "cd receiver_dir && ./receive" C-m

# Sleep to give receiver time to start
sleep 1

# Split the window and run sender
tmux split-window -h -t $SESSION_NAME
tmux send-keys -t $SESSION_NAME:0.1 "cd sender_dir && ./send" C-m

# Attach to the tmux session so user can see both
tmux attach -t $SESSION_NAME


