#!/bin/bash

if [ ! -d 'repo' ]; then
  echo "Creating test repository"
  mkdir repo
  svnadmin create repo > /dev/null 2>&1
  test -f repo.svnadmin.bz2 && bunzip2 repo.svnadmin.bz2
  svnadmin load repo < repo.svnadmin > /dev/null 2>&1
fi

if [ ! -d 'wc' ]; then
  echo "Checking out test working copy"
  svn co file:///`pwd`/repo wc
fi

phpunit tests
