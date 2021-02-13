#!/bin/bash
#
# Google virtual Ethernet (gve) driver
#
# Copyright (C) 2015-2018 Google, Inc.
#
# This software is available to you under a choice of one of two licenses. You
# may choose to be licensed under the terms of the GNU General Public License
# version 2, as published by the Free Software Foundation, and may be copied,
# distributed, and modified under those terms. See the GNU General Public
# License for more details. Otherwise you may choose to be licensed under the
# terms of the MIT license below.
#
# --------------------------------------------------------------------------
#
# MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS

USAGE=$(cat <<-END
build_src.sh [-t|--target=T] [-c|--compress=CF] [-d|--deb] [-v|--version=V | -r|--release]
    -t=T or --target=T: target for build (oot, upstream, cos).
    -c=CF or --compress=CF: compression format (gz).
    -d or --deb: Package into a .deb as well.
    --rpm: Package into a .rpm .
    -v=V or --version=V: Specify a version rather then creating it.
    -r or --release: This is a build for release.
END
)
MAINDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PATCHDIR="$MAINDIR"/patches
SRCDIR="$MAINDIR"/google/gve

for var in "$@"
do
case $var in
 -t=*|--target=*)
 TARGET="${var#*=}"
 shift
 ;;
 -c=*|--compress=*)
 COMPRESS="${var#*=}"
 shift
 ;;
 -d|--deb)
 DEB="yes"
 RELEASE="yes"
 shift
 ;;
 --rpm)
 RPM="yes"
 RELEASE="yes"
 shift
 ;;
 -v=*|--version=*)
 GIVEN_VERSION="${var#*=}"
 shift
 ;;
 -r|--release)
 RELEASE="yes"
 shift
 ;;
 *)
 echo "Unknown option"
 echo -e "$USAGE"
 exit 1
 ;;
esac
done

set -e
set -x

# Check release and version weren't both set
if [ "$RELEASE" == "yes" ] && [ -n "$GIVEN_VERSION" ]; then
 echo "Cannot set release and version options at the same time."
 exit 1
fi

# Save the version of the driver
if [ -z "${GIVEN_VERSION}" ]; then
 VERSION=`sed -n 's/#define GVE_VERSION\s*"\(.*\)"$/\1/p' "$SRCDIR"/gve_main.c`
else
 VERSION="${GIVEN_VERSION}"
fi
if [ -z "$RELEASE" ] && [ -z "${GIVEN_VERSION}" ]; then
 pushd $MAINDIR > /dev/null
 if [ `git rev-parse --git-dir` ]; then
  BRANCH=`git branch | sed -n '/\* /s///p'`
  ISREMOTE=`git ls-remote origin "$BRANCH"`
  if [ -z "$ISREMOTE" ]; then
    BRANCH="master"
  fi
  CALC_VERSION=`git log --oneline origin/"$BRANCH"..HEAD | wc -l`"-"`git log --oneline -1 origin/"$BRANCH" --pretty=%h`"-"`git log --oneline -1 --pretty=%h`
 else
  CALC_VERSION=`date +%F`
 fi
 VERSION="${VERSION}-${CALC_VERSION}"
 popd > /dev/null
fi

# Save and check value of target option
if [ -z "$TARGET" ]; then
 TARGET="oot";
fi
if [ "$TARGET" == "oot" ]; then
 MAKEFILE="Makefile.oot"
 if [ -z "$RELEASE" ]; then
  VERSION="${VERSION}-oot"
 fi
elif [ "$TARGET" == "upstream" ]; then
 MAKEFILE="google/gve/Makefile"
 if [ -z "$RELEASE" ]; then
  VERSION="${VERSION}-k"
 fi
elif [ "$TARGET" == "cos" ]; then
 MAKEFILE="google/gve/Makefile"
 if [ -z "$RELEASE" ]; then
  VERSION="${VERSION}-cos"
 fi
else
 echo ERROR: Invalid target "$TARGET"
 echo -e "$USAGE"
 exit 1
fi

# Save and check value of compress option
if [ "$COMPRESS" != "gz" ] && [ -e "$COMPRESS" ]; then
 echo ERROR: Invalid compress option "$COMPRESS"
 echo -e "$USAGE"
 exit 1
fi

if [ "$COMPRESS" == "gz" ] || [ "$DEB" == "yes" ] || [ "$RPM" == "yes" ]; then
 DESTDIR="$MAINDIR"/gve-"$VERSION"
 DEBDIR="$MAINDIR"/deb-"$VERSION"
else
 DESTDIR="$MAINDIR"/build
fi

if [ -z "$SPATCH" ]; then
 SPATCH="spatch";
fi

if [ ! -d "$DESTDIR" ]; then
 mkdir "$DESTDIR";
fi

if [ "$DEB" == "yes" ] && [ ! -d "$DEBDIR" ]; then
 mkdir "$DEBDIR";
fi

cp "$SRCDIR"/*.c "$DESTDIR"/;
cp "$SRCDIR"/*.h "$DESTDIR"/;

if [ "$TARGET" == "oot" ] || [ "$TARGET" == "cos" ]; then
 cp "$PATCHDIR"/header/gve_linux_version.h "$DESTDIR"
fi

cp "$MAINDIR"/"$MAKEFILE" "$DESTDIR"/Makefile;

if [ "$TARGET" == "oot" ] || [ "$TARGET" == "cos" ]; then
 for f in $(ls "$PATCHDIR"/*.cocci); do
  $SPATCH "$f" "$DESTDIR"/*.c --in-place --no-includes;
  $SPATCH "$f" "$DESTDIR"/*.h --in-place --no-includes;
 done
 cp "$MAINDIR"/LICENSE "$DESTDIR"/LICENSE
fi

if [ "$TARGET" == "oot" ]; then
 cp "$MAINDIR"/LICENSE "$DESTDIR"/LICENSE
 cp "$MAINDIR"/README.md "$DESTDIR"/README
fi

if [ -z "$RELEASE" ]; then
 sed -i "s/\(#define GVE_VERSION\s*\)\".*\"$/\1 \""${VERSION}"\"/" "$DESTDIR/gve_main.c"
fi

if [ "$COMPRESS"  == "gz" ]; then
 # Using tar -C doesn't result in a flat tarball on some distros -- it still
 # creates a directory inside the tarball. Because of this, we switch to
 # $DESTDIR before running tar, then switch back after creating the tarball.
 pushd "$DESTDIR"
 tar -cvzf "$MAINDIR"/gve-"$VERSION".tar.gz *
 popd
fi

if [ "$DEB" == "yes" ] || [ "$RPM" == "yes" ]; then
 if [ "$EUID" != 0 ]; then
  SUDO='sudo'
 fi
 cp "$MAINDIR"/dkms.conf "$DESTDIR"/dkms.conf
 sed -i "/PACKAGE_VERSION=/s/$/\"${VERSION}\"/" "$DESTDIR/dkms.conf"
 $SUDO cp -R "$DESTDIR" /usr/src/
 $SUDO dkms add -m gve -v "$VERSION"
fi

if [ "$DEB" == "yes" ]; then
 $SUDO dkms mkdsc -m gve -v "$VERSION" --source-only
 # The use of debuild package requires special directory structure where the
 # .tar.gz file generated by dkms needs to be in the parent directory of the
 # directory from which we run debuild.
 $SUDO cp /var/lib/dkms/gve/"$VERSION"/dsc/gve-dkms_"$VERSION".tar.gz "$DEBDIR"
 pushd "$DEBDIR"
 tar xvzf gve-dkms_"$VERSION".tar.gz
 pushd gve-dkms-"$VERSION"
 debuild -uc -us
 popd
 cp gve-dkms_"$VERSION"_all.deb "$MAINDIR"
 popd
fi

if [ "$RPM" == "yes" ]; then
  $SUDO dkms mkrpm -m gve -v "$VERSION" --source-only
  $SUDO cp /var/lib/dkms/gve/"$VERSION"/rpm/gve-"$VERSION"-1dkms.noarch.rpm "$MAINDIR"
fi
