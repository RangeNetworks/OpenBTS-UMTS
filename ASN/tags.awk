#!/bin/sh

awk '
BEGIN {
	print "!_TAG_FILE_FORMAT	2"
	print "!_TAG_FILE_SORTED	0	/0=unsorted, 1=sorted, 2=foldcase/"
	OFS="\t";
}
/::=/ { print $1,FILENAME,"/^" $0 "/;\"","f","" }
' rrc.asn1 > tags
