all:
	g++ -static -I../../cl-runtime/ gpu-hello.cpp -L../../cl-runtime/64b/ -lOpenCL -o hello-prog
kernel:
	../../HSAIL-HLC-Stable/bin/clc2 --enable-hsail-extensions --cl-std=CL2.0 gpu-hello-kernel.cl -o my_kernel.fe.bc
	../../HSAIL-HLC-Stable/bin/llvm-link my_kernel.fe.bc -l=../../HSAIL-HLC-Stable/bin/builtins-hsail.bc -o=my_kernel.linked.bc
	../../HSAIL-HLC-Stable/bin/opt -O3 -gpu -whole -verify my_kernel.linked.bc -o my_kernel.opt.bc
