#!/bin/bash


VERSION=$(cat appinfo.json | grep "versionLabel" | cut -d ":" -f 2 | sed 's/[ ",]//g')

REMOTEPATH=$(cat .remotepath)

scp build/metronebble.pbw $REMOTEPATH/metronebble-$VERSION.pbw
