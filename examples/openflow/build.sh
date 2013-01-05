PROCNUM=`grep -i processor /proc/cpuinfo | wc -l`
./configure-llvm
echo
echo
echo
make -skj${PROCNUM}
echo
echo
echo
make -kj${PROCNUM} -f Makefile.llvm

