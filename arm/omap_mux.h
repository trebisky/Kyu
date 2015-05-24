/* omap_mux.h
 *  Kyu project  5-21-2015  Tom Trebisky
 *
 * These are offsets into the conf array in the
 * Control Module that are used to control the pinmux
 *
 * Note that there are some gaps near the end that
 *  reflect gaps in the TRM.
 *
 * Names are chosen to match "pin names" from the tables in
 *  pages 19-47 of the datasheet.
 * In some cases (such as NNMI vs EXTINTN) this is different
 *  from the "signal name", and I always use the pin name.
 *
 * The comments give the "ball number" for the ZCZ package
 *  from this same table and serve mostly to clarify which
 *  pins are which on the schematic where signal names are
 *  hard to read.
 *
 * Curiously, there are registers to control some pins that
 *  can only possibly have one signal.
 */

#define	MUX_GPMC_AD0		0	/* U7 */
#define	MUX_GPMC_AD1		1	/* V7 */
#define	MUX_GPMC_AD2		2	/* R8 */
#define	MUX_GPMC_AD3		3	/* T8 */
#define	MUX_GPMC_AD4		4	/* U8 */
#define	MUX_GPMC_AD5		5	/* V8 */
#define	MUX_GPMC_AD6		6	/* R9 */
#define	MUX_GPMC_AD7		7	/* T9 */
#define	MUX_GPMC_AD8		8	/* U10 */
#define	MUX_GPMC_AD9		9	/* T10 */
#define	MUX_GPMC_AD10		10	/* T11 */
#define	MUX_GPMC_AD11		11	/* U12 */
#define	MUX_GPMC_AD12		12	/* T12 */
#define	MUX_GPMC_AD13		13	/* R12 */
#define	MUX_GPMC_AD14		14	/* V13 */
#define	MUX_GPMC_AD15		15	/* U13 */
#define	MUX_GPMC_A0		16	/* R13 */
#define	MUX_GPMC_A1		17	/* V14 */
#define	MUX_GPMC_A2		18	/* U14 */
#define	MUX_GPMC_A3		19	/* T14 */
#define	MUX_GPMC_A4		20	/* R14 */
#define	MUX_GPMC_A5		21	/* V15 */
#define	MUX_GPMC_A6		22	/* U15 */
#define	MUX_GPMC_A7		23	/* T15 */
#define	MUX_GPMC_A8		24	/* V16 */
#define	MUX_GPMC_A9		25	/* U16 */
#define	MUX_GPMC_A10		26	/* T16 */
#define	MUX_GPMC_A11		27	/* V17 */
#define	MUX_GPMC_WAIT0		28	/* T17 */
#define	MUX_GPMC_WPN		29	/* U17 */
#define	MUX_GPMC_BEN1		30	/* U18 */
#define	MUX_GPMC_CSN0		31	/* V6 */
#define	MUX_GPMC_CSN1		32	/* U9 */
#define	MUX_GPMC_CSN2		33	/* V9 */
#define	MUX_GPMC_CSN3		34	/* T13 */
#define	MUX_GPMC_CLK		35	/* V12 */
#define	MUX_GPMC_ADVN_ALE	36	/* R7 */
#define	MUX_GPMC_OEN_REN	37	/* T7 */
#define	MUX_GPMC_WEN		38	/* U6 */
#define	MUX_GPMC_BEN0_CLE	39	/* T6 */
#define	MUX_LCD_DATA0		40	/* R1 */
#define	MUX_LCD_DATA1		41	/* R2 */
#define	MUX_LCD_DATA2		42	/* R3 */
#define	MUX_LCD_DATA3		43	/* R4 */
#define	MUX_LCD_DATA4		44	/* T1 */
#define	MUX_LCD_DATA5		45	/* T2 */
#define	MUX_LCD_DATA6		46	/* T3 */
#define	MUX_LCD_DATA7		47	/* T4 */
#define	MUX_LCD_DATA8		48	/* U1 */
#define	MUX_LCD_DATA9		49	/* U2 */
#define	MUX_LCD_DATA10		50	/* U3 */
#define	MUX_LCD_DATA11		51	/* U4 */
#define	MUX_LCD_DATA12		52	/* V2 */
#define	MUX_LCD_DATA13		53	/* V3 */
#define	MUX_LCD_DATA14		54	/* V4 */
#define	MUX_LCD_DATA15		55	/* T5 */
#define	MUX_LCD_VSYNC		56	/* U5 */
#define	MUX_LCD_HSYNC		57	/* R5 */
#define	MUX_LCD_PCLK		58	/* V5 */
#define	MUX_LCD_AC_BIAS_EN	59	/* R6 */
#define	MUX_MMC0_DAT3		60	/* F17 */
#define	MUX_MMC0_DAT2		61	/* F18 */
#define	MUX_MMC0_DAT1		62	/* G15 */
#define	MUX_MMC0_DAT0		63	/* G16 */
#define	MUX_MMC0_CLK		64	/* G17 */
#define	MUX_MMC0_CMD		65	/* G18 */
#define	MUX_MII1_COL		66	/* H16 */
#define	MUX_MII1_CRS		67	/* H17 */
#define	MUX_MII1_RX_ER		68	/* J15 */
#define	MUX_MII1_TX_EN		69	/* J16 */
#define	MUX_MII1_RX_DV		70	/* J17 */
#define	MUX_MII1_TXD3		71	/* J18 */
#define	MUX_MII1_TXD2		72	/* K15 */
#define	MUX_MII1_TXD1		73	/* K16 */
#define	MUX_MII1_TXD0		74	/* K17 */
#define	MUX_MII1_TX_CLK		75	/* K18 */
#define	MUX_MII1_RX_CLK		76	/* L18 */
#define	MUX_MII1_RXD3		77	/* L17 */
#define	MUX_MII1_RXD2		78	/* L16 */
#define	MUX_MII1_RXD1		79	/* L15 */
#define	MUX_MII1_RXD0		80	/* M16 */
#define	MUX_RMII1_REFCLK	81	/* H18 */
#define	MUX_MDIO		82	/* M17 */
#define	MUX_MDC			83	/* M18 */
#define	MUX_SPI0_SCLK		84	/* A17 */
#define	MUX_SPI0_D0		85	/* B17 */
#define	MUX_SPI0_D1		86	/* B16 */
#define	MUX_SPI0_CS0		87	/* A16 */
#define	MUX_SPI0_CS1		88	/* C15 */
#define	MUX_ECAP0_IN_PWM0_OUT	89	/* C18 */
#define	MUX_UART0_CTSN		90	/* E18 */
#define	MUX_UART0_RTSN		91	/* E17 */
#define	MUX_UART0_RXD		92	/* E15 */
#define	MUX_UART0_TXD		93	/* E16 */
#define	MUX_UART1_CTSN		94	/* D18 */
#define	MUX_UART1_RTSN		95	/* D17 */
#define	MUX_UART1_RXD		96	/* D16 */
#define	MUX_UART1_TXD		97	/* D15 */
#define	MUX_I2C0_SDA		98	/* C17 */
#define	MUX_I2C0_SCL		99	/* C16 */
#define	MUX_MCASP0_ACLKX	100	/* A13 */
#define	MUX_MCASP0_FSX		101	/* B13 */
#define	MUX_MCASP0_AXR0		102	/* D12 */
#define	MUX_MCASP0_AHCLKR	103	/* C12 */
#define	MUX_MCASP0_ACLKR	104	/* B12 */
#define	MUX_MCASP0_FSR		105	/* C13 */
#define	MUX_MCASP0_AXR1		106	/* D13 */
#define	MUX_MCASP0_AHCLKX	107	/* A14 */
#define	MUX_XDMA_EVENT_INTR0	108	/* A15 */
#define	MUX_XDMA_EVENT_INTR1	109	/* D14 */
#define	MUX_WARMRSTN		110	/* A10 */
#define	MUX_EXTINTN		112	/* B18 */
#define	MUX_TMS			116	/* C11 */
#define	MUX_TDI			117	/* B11 */
#define	MUX_TDO			118	/* A11 */
#define	MUX_TCK			119	/* A12 */
#define	MUX_TRSTN		120	/* B10 */
#define	MUX_EMU0		121	/* C14 */
#define	MUX_EMU1		122	/* B14 */
#define	MUX_RTC_PWRONRSTN	126	/* B5 */
#define	MUX_PMIC_POWER_EN	127	/* C6 */
#define	MUX_EXT_WAKEUP		128	/* C5 */
#define	MUX_RTC_KALDO_ENN	129	/* B4 */
#define	MUX_USB0_DRVVBUS	135	/* F16 */
#define	MUX_USB1_DRVVBUS	141	/* F15 */

/* THE END */
