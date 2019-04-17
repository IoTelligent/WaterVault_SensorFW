#!/bin/sh -x

ARDUINO=arduino
SKETCH_NAME="sdu_boot_source.ino"
SKETCH="$PWD/$SKETCH_NAME"
#SKETCH=${SKETCH// /\\ } 
BUILD_PATH="$PWD/build"
#BUILD_PATH=${SKETCH// /\\ } 




if [[ "$OSTYPE" == "darwin"* ]]; then
	ARDUINO="/Applications/Arduino.app/Contents/MacOS/Arduino"
fi

buildSDUBootSketch() {
	BOARD=$1
	DESTINATION=$2

	$ARDUINO --verify --board $BOARD --preserve-temp-files --pref build.path="\"$BUILD_PATH\"" "\"$SKETCH\""
	cat "\"$BUILD_PATH/$SKETCH_NAME.bin\"" | xxd -i > "\"$DESTINATION\""
	#rm -rf "$BUILD_PATH"
}

mkdir -p "$OUTPUT_PATH"

buildSDUBootSketch "arduino:samd:arduino_zero_edbg" "/Users/jeffseese/uC/Client Projects/Refrigeration Inovation/TS3/firmware/simple_firmware/sdu_boot_source/sdu_boot_source.h"
