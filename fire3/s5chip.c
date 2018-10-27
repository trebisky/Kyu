/*
from: Fire3-bl1/prototype/module/nx_clkpwr.h
*/
struct  NX_CLKPWR_RegisterSet
{
                volatile U32 CLKMODEREG0;                               ///< 0x000 : Clock Mode Register 0
                volatile U32 __Reserved0;                               ///< 0x004
                volatile U32 PLLSETREG[4];                              ///< 0x008 ~ 0x014 : PLL Setting Register
                volatile U32 __Reserved1[2];                    	///< 0x018 ~ 0x01C
                volatile U32 DVOREG[9];                                 ///< 0x020 ~ 0x040 : Divider Setting Register
                volatile U32 __Reserved2;                               ///< 0x044
                volatile U32 PLLSETREG_SSCG[6];                 	///< 0x048 ~ 0x05C
                volatile U32 __reserved3[8];                    	///< 0x060 ~ 0x07C
                volatile U8 __Reserved4[0x200-0x80];    		// padding (0x80 ~ 0x1FF)
                volatile U32 GPIOWAKEUPRISEENB;                 	///< 0x200 : GPIO Rising Edge Detect Enable Register
                volatile U32 GPIOWAKEUPFALLENB;                 	///< 0x204 : GPIO Falling Edge Detect Enable Register
                volatile U32 GPIORSTENB;                                ///< 0x208 : GPIO Reset Enable Register
                volatile U32 GPIOWAKEUPENB;                             ///< 0x20C : GPIO Wakeup Source Enable
                volatile U32 GPIOINTENB;                                ///< 0x210 : Interrupt Enable Register
                volatile U32 GPIOINTPEND;                               ///< 0x214 : Interrupt Pend Register
                volatile U32 RESETSTATUS;                               ///< 0x218 : Reset Status Register
                volatile U32 INTENABLE;                                 ///< 0x21C : Interrupt Enable Register
                volatile U32 INTPEND;                                   ///< 0x220 : Interrupt Pend Register
                volatile U32 PWRCONT;                                   ///< 0x224 : Power Control Register
                volatile U32 PWRMODE;                                   ///< 0x228 : Power Mode Register
                volatile U32 __Reserved5;                               ///< 0x22C : Reserved Region
                volatile U32 SCRATCH[3];                                ///< 0x230 ~ 0x238      : Scratch Register
                volatile U32 SYSRSTCONFIG;                              ///< 0x23C : System Reset Configuration Register.
                volatile U8  __Reserved6[0x2A0-0x240];  		// padding (0x240 ~ 0x29F)
                volatile U32 CPUPOWERDOWNREQ;                   	///< 0x2A0 : CPU Power Down Request Register
                volatile U32 CPUPOWERONREQ;                             ///< 0x2A4 : CPU Power On Request Register
                volatile U32 CPURESETMODE;                              ///< 0x2A8 : CPU Reset Mode Register
                volatile U32 CPUWARMRESETREQ;                   	///< 0x2AC : CPU Warm Reset Request Register
                volatile U32 __Reserved7;                               ///< 0x2B0
                volatile U32 CPUSTATUS;                                 ///< 0x2B4 : CPU Status Register
                volatile U8  __Reserved8[0x400-0x2B8];			// padding (0x2B8 ~ 0x33F)
};

#define PHY_BASEADDR_CLKPWR_MODULE                          0xC0010000

/*
from: Fire3-bl1/prototype/module/nx_cci400.h
*/

struct NX_CCISlaveIF_RegisterSet
{
        volatile U32 SCR;                       // 0x0000 Snoop Control
        volatile U32 SOR;                       // 0x0004 Shareable Override for slave interface
        volatile U32 _Reserved0[0x3E];          // 0x0008~0x00FC
        volatile U32 RCQoSVOR;                  // 0x0100 Read Channel QoS Value Override
        volatile U32 WCQoSVOR;                  // 0x0104 Write Channel QoS Value Override
        volatile U32 _Reserved1;                // 0x0108
        volatile U32 QoSCR;                     // 0x010C QoS Control
        volatile U32 MOTR;                      // 0x0110 Max OT
        volatile U32 _Reserved2[0x7];           // 0x0114~0x012C
        volatile U32 RTR;                       // 0x0130 Regulator Target
        volatile U32 QoSRSFR;                   // 0x0134 QoS Regulator Scale Factor
        volatile U32 QoSRR;                     // 0x0138 QoS Range
        volatile U32 _Reserved3[0x3B1];         // 0x013C~0x0FFC
};

struct NX_CCIPerCnt_RegisterSet
{
        volatile U32 ESR;                       // 0x0000 Event Select
        volatile U32 ECR;                       // 0x0004 Event Count
        volatile U32 CCTRLR;                    // 0x0008 Counter Control
        volatile U32 OFFSR;                     // 0x000C Overflow Flag Status
        volatile U32 _Reserved[0x3FC];          // 0x0010~0x0FFC
};

struct  NX_CCI400_RegisterSet
{
        volatile U32 COR;                       // 0x0000 Control Override
        volatile U32 SCR;                       // 0x0004 Speculation Control
        volatile U32 SAR;                       // 0x0008 Secure Access
        volatile U32 STSR;                      // 0x000C Status
        volatile U32 IER;                       // 0x0010 Imprecise Error
        volatile U32 _Reserved0[0x3B];          // 0x0014~0x00FC
        volatile U32 PMCR;                      // 0x0100 Performance Monitor Control
        volatile U32 _Reserved1[0x3B3];         // 0x0104~0x0FCC
        volatile U32 CPIDR[0xC];                // 0x0FD0~0x0FFC
        struct NX_CCISlaveIF_RegisterSet CSI[5];// 0x1000~0x5FFC
        volatile U32 _Reserved2[0xC01];         // 0x6000~0x9000
        volatile U32 CCR;                       // 0x9004 Cycle counter
        volatile U32 CCTRLR;                    // 0x9008 Count Control
        volatile U32 OFFSR;                     // 0x900C Overflow Flag Status
        volatile U32 _Reserved3[0x3FC];         // 0x9010~0x9FFC
        struct NX_CCIPerCnt_RegisterSet CPC[4]; // 0xA000~DFFC
        volatile U32 _Reserved4[0x800];         // 0xE000~0xFFFC
};

#define PHY_BASEADDR_CCI400_MODULE                          0xE0090000
/* datasheet says F0090000 */

/* THE END */
