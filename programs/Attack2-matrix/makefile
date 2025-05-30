compile:main.c
	../../riscv-toolchain/bin/riscv64-unknown-linux-gnu-g++ -g -pedantic -Wall -o matrix main.c
	@[ -f matrix ] && echo "Compilation successful!" || echo "Compilation failed"

clean:
	@rm -f linear_reg
	@echo "Clean successful!"
