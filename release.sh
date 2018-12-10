#!/bin/sh 
VERSION=$(awk '{if(/QCoreApplication::setApplicationVersion/) print $0}' src/main.cpp)
VERSION=${VERSION/QCoreApplication::setApplicationVersion(\"}
VERSION=${VERSION/\");}
VERSION=$(echo $VERSION | tr -d '\n')

sed -i "/    <release date=/c\    <release date=\"$(date +%Y-%m-%d)\" version=\"$VERSION\"/>" com.github.Bleuzen.FFaudioConverter.appdata.xml

mkdir -p release

tar -cJf release/FFaudioConverter-$VERSION.tar.xz \
    src/ \
    FFaudioConverter.pro \
    build.sh \
    LICENSE \
    com.github.Bleuzen.FFaudioConverter.appdata.xml \
    com.github.Bleuzen.FFaudioConverter.desktop \
    com.github.Bleuzen.FFaudioConverter.png \
    com.github.Bleuzen.FFaudioConverter.ico \
    ffaudioconverter.qrc

cd release/
sha256sum FFaudioConverter-$VERSION.tar.xz
