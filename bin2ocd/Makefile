bin2ocd: bin2ocd.c
	gcc -Wall -o $@ $^

clean:
	rm -f bin2ocd

install: bin2ocd
	install -D bin2ocd /usr/local/bin
