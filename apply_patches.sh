#!/bin/bash

for p in `ls -1d patches/*`
do
   patch -N -p1 < "$p"
done
