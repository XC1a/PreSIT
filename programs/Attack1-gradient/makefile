compile:main.c
	../../riscv-toolchain/bin/riscv64-unknown-linux-gnu-g++ -g -pedantic -Wall -o linear_reg main.c
	@[ -f linear_reg ] && echo "Compilation successful!" || echo "Compilation failed"

clean:
	@rm -f linear_reg
	@echo "Clean successful!"
