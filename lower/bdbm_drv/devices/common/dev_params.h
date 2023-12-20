/*
The MIT License (MIT)

Copyright (c) 2014-2015 CSAIL, MIT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef _BLUEDBM_DM_PARAMS_H
#define _BLUEDBM_DM_PARAMS_H

extern int _param_nr_channels;
extern int _param_nr_chips_per_channel;
extern int _param_nr_blocks_per_chip;
extern int _param_nr_pages_per_block;
extern int _param_page_main_size; 
extern int _param_page_oob_size;
extern int _param_device_type; 
extern int _param_host_bus_trans_time_us;
extern int _param_chip_bus_trans_time_us;
extern int _param_page_prog_time_us;
extern int _param_page_read_time_us;
extern int _param_block_erase_time_us;
extern int _param_ramdrv_timing_mode;

bdbm_device_params_t get_default_device_params (void);
void display_device_params (bdbm_device_params_t* p);

#endif
