#ifndef _ConnectalProjectConfig_h
#define _ConnectalProjectConfig_h

#define DataBusWidth 64
#define NumReadClients 9
#define NumWriteClients 9
#define IMPORT_HOST_CLOCKS ""
#define ConnectalVersion "16.05.2"
#define NumberOfMasters 4
#define PinType "Top_Pins"
#define PinTypeInclude "Top_Pins"
#define NumberOfUserTiles 1
#define SlaveDataBusWidth 32
#define SlaveControlAddrWidth 5
#define BurstLenSize 10
#define project_dir "$(DTOP)"
#define MainClockPeriod 6
#define DerivedClockPeriod 5.000000
#define XILINX 1
#define ZYNQ ""
#define ZynqHostInterface ""
#define PhysAddrWidth 32
#define NUMBER_OF_LEDS 4
#define PcieLanes 4
#define CONNECTAL_BITS_DEPENDENCES "hw/mkTop.bit"
#define CONNECTAL_RUN_SCRIPT "$(CONNECTALDIR)/scripts/run.ubuntu"
#define CONNECTAL_EXENAME "ubuntu.exe"
#define CONNECTAL_EXENAME2 "ubuntu.exe2"
#define BOARD_zc706_ubuntu ""

#endif // _ConnectalProjectConfig_h
