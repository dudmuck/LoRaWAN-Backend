#!/bin/bash
cd build
#sudo chmod 0755 /var/run/screen/
screen -d -m -S loraSession
# window 0 created by default
# screen -r to attach to this session
screen -S loraSession -t network_server -p 0 -X stuff ./network_server^M
screen -S loraSession -X screen 1
screen -S loraSession -t join_server -p 1 -X stuff ./join_server^M
screen -S loraSession -X screen 2
screen -S loraSession -t app_server -p 2 -X stuff ./app_server^M
