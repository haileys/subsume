msdos.img: msdos-base.img subsume.com
	cp msdos-base.img msdos.img
	MTOOLSRC=mtoolsrc mcopy subsume.com c:/

subsume.com: subsume.asm subsumek.bin
	nasm -f bin -o $@ $<

subsumek.bin: subsumek.asm
	nasm -f bin -o $@ $<

.PHONY: clean
clean:
	rm -f msdos.img *.com *.bin
