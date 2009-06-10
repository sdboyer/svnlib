#!/bin/bash
# ensure we're invoking from the right place, or else it burps
LSOF=$(lsof -p $$ | grep -E "/"$(basename $0)"$")
MY_PATH=$(echo $LSOF | sed -r s/'^([^\/]+)\/'/'\/'/1 2>/dev/null)
if [ $? -ne 0 ]; then
## OSX
  MY_PATH=$(echo $LSOF | sed -E s/'^([^\/]+)\/'/'\/'/1 2>/dev/null)
fi
root=$(dirname $MY_PATH)
cd $root


if [ ! -d "$root/testdata/repo" ]; then
  echo "Creating test repository"
  mkdir -p $root/testdata/repo > /dev/null
  svnadmin create $root/testdata/repo > /dev/null 2>&1
  test -f $root/repo.svnadmin.bz2 && bunzip2 $root/repo.svnadmin.bz2
  svnadmin load $root/testdata/repo < $root/repo.svnadmin > /dev/null 2>&1
fi

if [ ! -d "$root/testdata/wc" ]; then
  echo "Checking out test working copy"
  svn co file://$root/repo wc
fi

php -d safe_mode=Off phpunit.php -- $root/tests
