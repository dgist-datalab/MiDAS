#if { $argc != 1 } {
#    puts "Need a Digilent ID as an argument"
#    exit 1 
#}

open_hw
connect_hw_server

open_hw_target 

set artixfpga_0 [lindex [get_hw_devices xc7a200t_1] 0]
set artixfpga_1 [lindex [get_hw_devices xc7a200t_2] 0] 
set vcu108fpga [lindex [get_hw_devices xcvu095_0] 0]

set file1 ./clearA7.bit
set file2 ./clearVCU108.bit
set file3 ./mkTopArtix.bit
set file4 ./mkTopAmf.bit

set_property PROGRAM.FILE $file1 $artixfpga_0
puts "fpga is $artixfpga_0, bit file size is [exec ls -sh $file1], CLEAR BEGIN"
program_hw_devices $artixfpga_0

set_property PROGRAM.FILE $file1 $artixfpga_1
puts "fpga is $artixfpga_1, bit file size is [exec ls -sh $file1], CLEAR BEGIN"
program_hw_devices $artixfpga_1

set_property PROGRAM.FILE $file2 $vcu108fpga
puts "fpga is $vcu108fpga, bit file size is [exec ls -sh $file2], CLEAR BEGIN"
program_hw_devices $vcu108fpga

set_property PROGRAM.FILE $file3 $artixfpga_0
puts "fpga is $artixfpga_0, bit file size is [exec ls -sh $file3], PROGRAM BEGIN"
program_hw_devices $artixfpga_0

set_property PROGRAM.FILE $file3 $artixfpga_1
puts "fpga is $artixfpga_1, bit file size is [exec ls -sh $file3], PROGRAM BEGIN"
program_hw_devices $artixfpga_1

set_property PROGRAM.FILE $file4 $vcu108fpga
puts "fpga is $vcu108fpga, bit file size is [exec ls -sh $file4], PROGRAM BEGIN"
program_hw_devices $vcu108fpga

refresh_hw_device $vcu108fpga
refresh_hw_device $artixfpga_0
refresh_hw_device $artixfpga_1

close_hw_target

disconnect_hw_server
