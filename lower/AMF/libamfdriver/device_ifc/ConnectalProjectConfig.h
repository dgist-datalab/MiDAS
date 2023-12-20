#ifndef _ConnectalProjectConfig_h
#define _ConnectalProjectConfig_h

#define DataBusWidth 128
#define IMPORT_HOST_CLOCKS ""
#define FLASH_FMC2 ""
#define NumReadClients 2
#define NumWriteClients 2
#define DEFAULT_NOPROGRAM 1
#define ConnectalVersion "18.12.1"
#define NumberOfMasters 1
#define PinType "Top_Pins"
#define PinTypeInclude "Top_Pins"
#define NumberOfUserTiles 1
#define SlaveDataBusWidth 32
#define SlaveControlAddrWidth 5
#define BurstLenSize 10
#define project_dir "$(DTOP)"
#define MainClockPeriod 5.000000
#define DerivedClockPeriod 9.090000
#define PcieClockPeriod 4
#define XILINX 1
#define VirtexUltrascale ""
#define XilinxUltrascale ""
#define PCIE ""
#define PCIE3 ""
#define PcieHostInterface ""
#define PhysAddrWidth 40
#define NUMBER_OF_LEDS 2
#define PcieLanes 8
#define CONNECTAL_BITS_DEPENDENCES "hw/mkTop.bit"
#define CONNECTAL_RUN_SCRIPT "$(CONNECTALDIR)/scripts/run.pcietest"
#define BOARD_vcu108 ""

#endif // _ConnectalProjectConfig_h
