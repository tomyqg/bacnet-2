#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import os
import glob
import re
import subprocess
import struct

"""
Please install python and gettext package
scan TLT string in all c/cpp file
and search in translate.cpp
"""

_alldict = ["zhdict"]

_msgidpat = re.compile("^msgid +\"(.*)\"", re.M)

def searchText(filename, key):
    result = []

    output = subprocess.check_output(['xgettext', '-o' '/dev/stdout', \
        '-k' + key, '-s', filename])

    soff = 0
    while 1:
        match = _msgidpat.search(output, soff)
        if match:
            soff = match.end()
        else:
            break

        if match.group(1):
            result.append(match.group(1))

    return result


def searchDict(txtmap, tranfile):
    alldict = {}
    for di in _alldict:
        dipat = re.compile(di + "\\[\"(.*)\"\\]( |\t)*=", re.M)
        soff = 0
        while 1:
            match = dipat.search(tranfile, soff)
            if (match):
                alldict[match.group(1)] = match.group(1)
                soff = match.end()
            else:
                break

        for key in txtmap.iterkeys():
            if alldict.pop(key, None) is None:
                print "\"" + key + "\"", "not found in", di 

        for key in alldict.iterkeys():
            print "\"" + key + "\"", "not used in", di 


def main():
    def prtUsage(msg=None):
        if msg is None:
            print>>sys.stderr, "Usage: python scantlt.py key dst_dir"
            print>>sys.stderr, "EX: python scantlt.py TLT ./"
        else:
            print >>sys.stderr, msg

    if len(sys.argv) != 3:
        prtUsage()
        return 1
    
    key = sys.argv[1]
    path = sys.argv[2]
    
    txtmap = {}
    os.chdir(path)
    for fname in glob.glob("*.c"):
        result = searchText(fname, key)
        for txt in result:
            txtmap[txt] = txt

    for fname in glob.glob("*.cpp"):
        result = searchText(fname, key)
        for txt in result:
            txtmap[txt] = txt

    translated = open("translate.cpp").read()

    searchDict(txtmap, translated)
    return 0;


if __name__ == '__main__':
    sys.exit(main())    
