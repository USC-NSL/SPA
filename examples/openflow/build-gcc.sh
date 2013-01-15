PROCNUM=`grep -i processor /proc/cpuinfo | wc -l`
./configure-gcc
echo
echo
echo
make clean
make -skj${PROCNUM}
echo
echo
echo
make -f gcc.mk clean
make -kj${PROCNUM} -f gcc.mk

