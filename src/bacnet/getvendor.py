#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import os
import re
import urllib2
import HTMLParser

_pat = re.compile("<TR>[ \t\r\n]*<TD>([0-9]+)</TD>[ \t\r\n]*<TD[^\r\n>]*>(?:<[^>]*>)?([^\r\n<]+)(?:<[^>]*>)?</TD>")

def main():
    def prtUsage(msg=None):
        if msg is None:
            print>>sys.stderr, "Usage: python getvendor.py"
            print>>sys.stderr, "EX: python getvendor.py > ./vendor_def.h"
        else:
            print >>sys.stderr, msg

    if len(sys.argv) != 1:
        prtUsage()
        return 1
    
    response = urllib2.urlopen("http://www.bacnet.org/VendorID/BACnet%20Vendor%20IDs.htm")
    page = response.read()
    h = HTMLParser.HTMLParser()
    page = h.unescape(page)

    result = []
    pos = 0
    while True:
        match = _pat.search(page, pos)
        if match is None:
            break

        pos = match.end(0)
        result.append((int(match.group(1)), match.group(2)))

    result = sorted(result, key=lambda item:item[0])
    
    print "static const char * const vendor_name[" + str(result[-1][0] + 1) + "] = {"

    idx = 0
    for vendorid, vendorname in result:
        if idx != vendorid:
            for x in xrange(idx, vendorid):
                idx += 1
                print "NULL,"

        idx += 1
        print "\"" + vendorname.encode('utf8') + "\","

    print "};"

    return 0;


if __name__ == '__main__':
    sys.exit(main())    
