#!/bin/bash

usage() {
  echo "This program should be run once after initially cloning the ButtonDAQ repository. It guides you to download the CAEN libraries and prepares them for the compilation.
Usage: $0 [options...]
Allowed options:
  -a <archives>: path to the directory where the archives with the CAEN libraries are/will be downloaded. Default is <destination>.
  -d <destination>: path to the directory the CAEN libraries will be extracted to. Default is $destination"
}

destination=Dependencies/caen
archives=

argv=(`getopt -o 'a:d:h' -- "$@"`) || exit $?
eval "set -- ${argv[*]}"
while [[ $1 != -- ]]; do
  case $1 in
    -h) usage; exit;;
    -a) archives=$2;    shift 2;;
    -d) destination=$2; shift 2;;
  esac
done

[[ -z "$archives" ]] && archives="$destination"

set -e

for dir in lib include; do
  [[ -d "$destination/$lib" ]] || mkdir -vp "$destination/$lib"
done

for lib in CAEN{Comm,VME,Digitizer}; do
  link="https://www.caen.it/download?filter=$lib%20library"
  glob="${lib}*.tgz"
  while :; do
    archive=`compgen -G "$archives/$glob" | head -n 1`
    [[ -z "$archive" ]] || break
    archive="${archives:-.}/$glob"
    read -p "$archive not found. Please go to $link, select \"Software\", pick the \"TGZ\" file, accept the license agreement, download the file into ${archive%/*}/ and press Enter. "
  done

  tar -xvf "$archive" \
      -C "$destination" \
      --strip-components=1 \
      --transform='s,/x64/,/,' \
      --transform='s,\.so\..*,.so,' \
      --wildcards \
      --no-wildcards-match-slash \
      '*/include/*.h' \
      '*/lib/x64/*.so*'
done
