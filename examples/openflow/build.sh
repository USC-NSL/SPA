PROCNUM=`grep -i processor /proc/cpuinfo | wc -l`
./configure-llvm
echo
echo
echo
make clean
make -skj${PROCNUM}
echo
echo
echo
rm instrumentation/*.o
make -f llvm.mk clean
make -kj${PROCNUM} -f llvm.mk
