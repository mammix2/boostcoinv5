#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/boostcoin.ico

convert ../../src/qt/res/icons/boostcoin-16.png ../../src/qt/res/icons/boostcoin-32.png ../../src/qt/res/icons/boostcoin-48.png ${ICON_DST}
