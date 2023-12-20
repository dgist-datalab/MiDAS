.PHONY: all kernel user hybrid
all: kernel user hybrid

kernel:
	cd kernel; $(MAKE) 

user:
	cd user; $(MAKE)

hybrid:
	cd hybrid; $(MAKE)

.PHONY: clean clean-kernel clean-user clean-hybrid
clean: clean-kernel clean-user clean-hybrid

clean-kernel:
	@cd kernel; $(MAKE) clean

clean-user:
	@cd user; $(MAKE) clean

clean-hybrid:
	@cd hybrid; $(MAKE) clean
