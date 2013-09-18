#!/usr/bin/python

import csv
from itertools import *
from array import *
from numpy import *

from pprint import pprint
import math
bufsize=1024

pipe1 = [[0 for i in range(bufsize)] for j in range(6)]
pipe2 = [[0 for i in range(bufsize)] for j in range(6)]


def filterDiff(i,stage,filterLen,curVal,filterFactor):
  diff1 = -pipe1[stage][(i+bufsize-filterLen)%bufsize] + pipe1[stage][(i)%bufsize]
  diff2 = -pipe2[stage][(i+bufsize-filterLen)%bufsize] + pipe2[stage][(i)%bufsize]
  raw_diff = diff2 - diff1
  return raw_diff*filterFactor + curVal*(1-filterFactor)
   

csvfile1 = open('orig_cp.txt', 'rb')
csvfile2 = open('ooo-base.txt', 'rb')

reader1 = csv.reader(csvfile1, delimiter=' ')
reader2 = csv.reader(csvfile2, delimiter=' ')

filter_val=[  0,    0,    0]
filter_len=[ 50,  200, 1000]
filter_fac=[0.1, 0.05, 0.01]

sdiff=zeros(6)
idiff=zeros(5)

inst=0
filter_diff=0
for row1,row2 in izip(reader1,reader2):
  inst=inst+1;
  rinst=inst%bufsize
  #bookkeeping
  for stage in range(6):
    pipe1[stage][rinst]=int(row1[stage+1])
    pipe2[stage][rinst]=int(row2[stage+1])
    sdiff[stage]=filterDiff(inst,stage,1,0,1)
    if stage != 0:
      idiff[stage-1]=(pipe2[stage][rinst]-pipe2[stage-1][rinst]) - \
                     (pipe1[stage][rinst]-pipe1[stage-1][rinst])

  for index,(val,len,fac) in enumerate(izip(filter_val,filter_len,filter_fac)):
    filter_val[index] = filterDiff(inst,5,len,val,fac)
 
#  print '{0:5.0f} {1:5.0f} {2:5.0f} {3:5.0f} {4:5.0f} {5:5.0f} | {6:5.0f} {7:5.0f} | {8:5.0f} {9:5.0f} {10:5.0f}'.format(
#         sdiff[0],sdiff[1],sdiff[2],sdiff[3],sdiff[4],sdiff[5], 
#         pipe1[5][rinst] - pipe1[0][rinst], pipe2[5][rinst] - pipe2[0][rinst],
#         filter_val[0],filter_val[1],filter_val[2])

  print '{0:5.0f} {1:5.0f} {2:5.0f} {3:5.0f} {4:5.0f} {5:5.0f} | {6:5.0f} {7:5.0f} {8:5.0f} {9:5.0f} {10:5.0f} | {11:5.0f} {12:5.0f} {13:5.0f} '.format(
         sdiff[0],sdiff[1],sdiff[2],sdiff[3],sdiff[4],sdiff[5], 
#         pipe1[5][rinst] - pipe1[0][rinst], pipe2[5][rinst] - pipe2[0][rinst],
         idiff[0],idiff[1],idiff[2],idiff[3],idiff[4],
         filter_val[0],filter_val[1],filter_val[2])
