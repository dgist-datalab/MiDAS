VIVADO_PATH=/opt/Xilinx/Vivado/2018.1/bin
MY_PATH=$(dirname $0)
cd $MY_PATH

# If multiple boards, use -tcl args flag:
#  -tclargs 210203861260
${VIVADO_PATH}/vivado -nolog -nojournal -mode batch -source vcu108-ready.tcl 
pciescanportal
