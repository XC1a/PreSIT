CC=g++
FLAG=-g -std=c++11
target=vault
src=test_vault.cc vault.hh vault.cc encrypt_lib.cc encrypt_lib.hh

$(target):$(src)
	$(CC) $(FLAG) -o $(target) $(src) 

clean:
	rm -rf $(target)