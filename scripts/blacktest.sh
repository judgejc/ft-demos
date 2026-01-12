#!/bin/bash
# Run multiple instances of the black demo to test clearing layers

# Spawn a new WSL terminal and launch the Flaschen-Taschen server in hd terminal mode
FLASCHEN_TASCHEN_SERVER_DIR="~/projects/led-matrix/flaschen-taschen/server"
cmd.exe /c start wsl bash -c "cd $(FLASCHEN_TASCHEN_SERVER_DIR); ./ft-server --hd-terminal"

echo "Running Black Demo Test Sequence.."

../black -l1 -cFF0000 -t60 -I2 -O5 debug &
sleep 10
../black -l2 -c00FF00 -t15 -I0.5 -O1.5 debug &
sleep 10
../black -l3 -c0000FF -t15 -I1 -O3 debug &
sleep 10
../black -l4 -c00FFFF -t15 -I1.5 -O2 debug