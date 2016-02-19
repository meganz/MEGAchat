# This script edits a gclient DEPS file and removed the specified dependencies and hooks.

import re
import os
import sys

if len(sys.argv) < 2:
    print("Too few argments")
    print("Usage: <DEPS file> <list-of-things-to-remove>")
    exit(1)
 
deplistfile=open(sys.argv[2])
deplist=deplistfile.readlines();

depfile = open(sys.argv[1], "r+b");
data=depfile.read();

for dep in deplist:
    dep = dep.strip();
    if not dep:
        continue
    type = dep[0]
    if type == '#':
        continue
    if dep[1] != ' ':
        raise Exception("Second char on a non-comment line must be space")

    dep=dep[1:].strip()
    prevlen = len(data)
    if type == 'd':
        rxdep=dep.replace('/','\/')
        rx="^\s*'src\/"+rxdep+"':[^,]+,.*\n"
        data=re.sub(rx, "", data, 0, re.M)
        print("Remove dependency '%s': %s" % (dep, ("success" if len(data) != prevlen else "FAIL")))
    elif type == 'h':
        data = re.sub("{(\s*#.*$)*\n\s*'name':\s*'"+dep+"',\s*\n[^}]+},", "", data, 0, re.M)
        print("Remove hook '%s': %s" % (dep, ("success" if len(data) != prevlen else "FAIL")))
    elif type == 'm': #hook without a name, identify plain string match
        data = re.sub(dep, "", data, 0, re.M);
        print("Remove hook by match '%s': %s" % (dep, ("success" if len(data) != prevlen else "FAIL")))
    else:
        print("Uknown entry type '%s'" % (type))

depfile.seek(0)
depfile.write(data)
depfile.truncate()
depfile.close()

