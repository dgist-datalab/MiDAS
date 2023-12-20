SRCS +=\
	   io.c\
	   level.c\
	   lftl_slab.c\
	   lsmtree.c\
	   page_manager.c\
	   run.c\
	   write_buffer.c\
	   compaction.c\
	   compaction_assistant.c\
	   function_test.c\
	   version.c\
	   write_buffer.c\
	   sst_page_file_stream.c\
	   sst_block_file_stream.c\
	   sst_file.c\
	   read_helper.c\
	   segment_level_manager.c\
	   compaction_merge.c\
	   compaction_sst_convert.c\
	   compaction_data_move.c\
	   gc_helper.c\
	   lsmtree_testing.c\
	   lsmtree_setting.c\
	   oob_manager.c\
#	   gc.c\
#	   helper_algorithm/bf_set.c\
#	   helper_algorithm/compressed_bloomfilter.c
#	   helper_algorithm/bloomfilter.c\
	   compaction_early_invalidation.c\


DIRS := $(shell find ./caching/ -type d)

OBJS :=\
	$(patsubst %.c,%.o,$(SRCS))\

CACHE_OBJS:=\
	$(patsubst %.c,%.o,$(CACHE_SRCS))\

TARGETOBJ:=\
	$(addprefix ../../object/,$(OBJS))\


LEAK: all
all: $(TARGETOBJ) HelperAlgorithm

libnom.a : $(TARGETOBJ)
	$(AR) r $(@) $(TARGETOBJ)

%.o :%.c
	$(CC) $(CFLAGS_ALGO) -c $< -o $@

../../object/%.o:%.c
	$(CC) $(CFLAGS_ALGO) -c $< -o $@

HelperAlgorithm:
	cd helper_algorithm && make 
	cp helper_algorithm/*.o ../../object/
	cd design_knob && make 
	mv design_knob/*.o ../../object/

clean : 
	@$(RM) ./helper_algorithm/*.o
	@$(RM) libnom.a
	@$(RM) *.o
