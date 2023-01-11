#!/bin/sh

FILESDIR=
STR=

if [ $# -ne 2 ]
then
  echo "Error: Wrong number of arguments"
  exit 1
else
	FILESDIR=$1
	STR=$2
fi

if [ ! -d "$FILESDIR" ]
then
  echo "Error: ${FILESDIR} is not a directory."
  exit 1
fi

COUNT=$(grep -l -r "$STR" "$FILESDIR" | wc -l)
FILES=$(grep -r "$STR" "$FILESDIR" | wc -l)

echo "The number of files are $FILES and the number of matching lines are $COUNT"