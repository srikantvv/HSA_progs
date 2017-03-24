all:
	g++ -static -I../../cl-runtime/ gpu-hello.cpp -L../../cl-runtime/64b/ -lOpenCL -o hello-prog
kernel:
	clc2 --enable-hsail-extensions --cl-std=CL2.0 gpu-hello-kernel.cl -o my_kernel.fe.bc
	llvm-link my_kernel.fe.bc -l /opt/rocm/hlc3.2/lib/builtins-hsail.bc -o my_kernel.linked.bc
	opt -O3 -gpu -whole -verify my_kernel.linked.bc -o my_kernel.opt.bc
