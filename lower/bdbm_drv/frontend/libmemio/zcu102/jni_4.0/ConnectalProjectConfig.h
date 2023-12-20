#ifndef _ConnectalProjectConfig_h
#define _ConnectalProjectConfig_h

#define DataBusWidth 128
#define NumReadClients 4
#define NumWriteClients 8
#define NumReFlash 2
#define NumWeFlash 4
#define IMPORT_HOST_CLOCKS ""
#define ZCU_AXI_SLAVE_START 2
#define DEFAULT_NOPROGRAM 1
#define MMU_INDEX_WIDTH 13
#define ConnectalVersion "18.01.4"
#define NumberOfMasters 2
#define PinType "Top_Pins"
#define PinTypeInclude "Top_Pins"
#define NumberOfUserTiles 1
#define SlaveDataBusWidth 32
#define SlaveControlAddrWidth 5
#define BurstLenSize 10
#define project_dir "$(DTOP)"
#define MainClockPeriod 5.714000
#define DerivedClockPeriod 9.090000
#define XILINX 1
#define ZYNQ ""
#define ZynqUltrascale ""
#define ZynqHostInterface ""
#define PhysAddrWidth 40
#define CONNECTAL_BITS_DEPENDENCES "hw/mkTop.bit"
#define CONNECTAL_RUN_SCRIPT "$(CONNECTALDIR)/scripts/run.ubuntu"
#define CONNECTAL_EXENAME "ubuntu.exe"
#define CONNECTAL_EXENAME2 "ubuntu.exe2"
#define BOARD_zcu102 ""

#endif // _ConnectalProjectConfig_h
