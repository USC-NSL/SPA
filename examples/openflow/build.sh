./configure-llvm
echo
echo
echo
`cat llvm-vars` make -skj8
echo
echo
echo
make -skj8 -f Makefile.llvm

