#!/usr/bin/env bash

bspc rule -a \* -o state=floating && 
st -g=60x10 /home/clados/dev/launcher/launcher.out
