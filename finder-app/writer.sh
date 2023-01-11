#!/bin/sh

FILE=
STR=

if [ $# -ne 2 ]
then
  echo "Error: Wrong number of arguments"
  exit 1
else
	FILE=$1
	STR=$2
fi

mkdir -p "$(dirname "$FILE")"

if [ $? -ne 0 ]
then
  echo "Error: Error creating parent folder"
  exit 1
fi

echo "$STR" > "$FILE"

if [ $? -ne 0 ]
then
  echo "Error: Error creating file"
  exit 1
fi

