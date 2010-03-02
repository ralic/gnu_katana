#!/usr/bin/env python
#File: run_tests.py
#Author: James Oakley
#Date: January, 2010
#Description: runs unit tests and reports their success or failure

import subprocess,sys,string,os,os.path

passTests=0
totalTests=0

if os.path.exists("katana_log"):
  os.remove("katana_log")
if os.path.exists("katana_err_log"):
  os.remove("katana_err_log")
if os.path.exists("validator_log"):
  os.remove("validator_log")

def runTestInDir(dir):
  msg="running test "+os.path.basename(dir)
  dots=string.join(['.' for x in range(0,35-len(msg))],'')
  sys.stdout.write(msg+dots+'|')
  sys.stdout.flush()
  if 0==subprocess.call(["python","validator.py",dir]):
    sys.stdout.write("PASSED\n")
    return True
  else:
    sys.stdout.write("FAILED\n")
    return False

if len(sys.argv)>1:
  print "Taking tests to run from the command line"
  for i in range(1,len(sys.argv)):
    if True==runTestInDir(sys.argv[1]):
      passTests+=1
    totalTests+=1
else:
  print "Running all tests"
  for dir in sorted(os.listdir("tests")):
    if os.path.isdir("tests/"+dir) and dir.startswith("t"):
      if True==runTestInDir("tests/"+dir):
        passTests+=1
      totalTests+=1

print "Passed "+str(passTests)+" out of "+str(totalTests)+" tests"
if passTests!=totalTests:
  print "See validator_log for more information"
  sys.exit(1)

