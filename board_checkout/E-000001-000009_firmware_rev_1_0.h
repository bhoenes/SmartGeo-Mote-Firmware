#include <avr/io.h>
#include <avr/pgmspace.h>
#define F_CPU 32000000UL
#include <util/delay.h>
#include "adc_driver.h"
#include "clksys_driver.h"

// Hardware Defines
// General SPI Prescaler
#define SPI_PRESCALER SPI_PRESCALER_DIV16_gc
#define SPI_PRESCALER_TIGHT SPI_PRESCALER_DIV4_gc


// Op codes for Ramtron FM25V05-G
#define FR_WREN 0x06  // Set Write Enable Latch
#define FR_WRDI 0x04  // Write Disable
#define FR_RDSR 0x05  // Read Status Register
#define FR_WRSR 0x01  // Write Status Register
#define FR_READ 0x03  // Read Memory Data
#define FR_FSTRD 0x0B // Fast Read Memory Data
#define FR_WRITE 0x02  // Write Memory Data
#define FR_SLEEP 0xB9  // Enter Sleep Mode
#define FR_RDID 0x9F  // Read Device ID
#define FR_BASEADD 0x0000
#define FR_CAPACITY 65536 // 64KB
#define FR_TOTAL_NUM_SAMPLES 7281 // closest multiple of 9 bytes to capacity
#define FR_TOTAL_NUM_SE_SAMPLES 21843 // closest multiple of 3 bytes per channel to capacity
#define FR_READ_BUFFER_SIZE 7281  // (bytes) fits within memory of microprocessor evenly divisible by 9
#define FR_READ_BUFFER_SAMPLES 809  // 7281 / 9 bytes per sample
#define FR_NUM_READ_BUFFERS 9 // 65536 / 7281
#define FR_SPI_CONFIG_gc 0xD0

// Codes for MCP23S17 Port Selector
#define PS_WRITE 0x40
#define PS_READ 0x41
#define PS_GPIOA 0x12
#define PS_GPIOB 0x13
#define PS_OLATA 0x14
#define PS_OLATB 0x15
#define PS_IODIRA 0x00
#define PS_IODIRB 0x01
#define PS_SPI_MODE SPI_MODE_0_gc
#define PS_BANKA 0x01
#define PS_BANKB 0x00
#define PS_HIGH 0x01
#define PS_LOW 0x00


// AD7767 
#define ADC_VREF 2500000  // 2.5volts in microvolts
#define ADC_MAX 0x7FFFFF  // 24-bit ADC.  2^23 because signed
#define ADC_DRIVER_GAIN_NUMERATOR 3  // gain of ADC stage
#define ADC_DRIVER_GAIN_DENOMINATOR 2


// Hardware filter config
#define FILTER_CH_1AND5_bm 0x01
#define FILTER_CH_2AND6_bm 0x02
#define FILTER_CH_3AND7_bm 0x04
#define FILTER_CH_4AND8_bm 0x08
#define FILTER_HP_0_bm 0x80
#define FILTER_HP_2_bm 0x00
#define FILTER_LP_INF_gc 0x00
#define FILTER_LP_32K_gc 0x10
#define FILTER_LP_6K_gc 0x20
#define FILTER_LP_600_gc 0x40

// Software Defines
#define NUM_SAMPLES 1024
#define TRUE 1
#define FALSE 0
#define ADC_DISCARD 128

// Sample frequency (samples per second)
#define SPS_32_gc 0x05
#define SPS_64_gc 0x06
#define SPS_128_gc 0x07
#define SPS_256_gc 0x08
#define SPS_512_gc 0x09
#define SPS_1K_gc 0x0A
#define SPS_2K_gc 0x0B
#define SPS_4K_gc 0x0C
#define SPS_MAX_gc SPS_4K_gc

// Sample frequency (subsamples per second)
#define SSPS_SE_32_gc 0x0B
#define SSPS_SE_64_gc 0x0A
#define SSPS_SE_128_gc 0x09
#define SSPS_SE_256_gc 0x08
#define SSPS_SE_512_gc 0x07
#define SSPS_SE_1K_gc 0x06
#define SSPS_SE_2K_gc 0x05
#define SSPS_SE_4K_gc 0x04
#define SSPS_SE_8K_gc 0x03
#define SSPS_SE_16K_gc 0x02
#define SSPS_SE_32K_gc 0x01
#define SSPS_SE_64K_gc 0x00
#define SSPS_SE_MAX_gc SSPS_SE_32_gc

// ADC channels
#define ADC_CH_1_gc 0x00
#define ADC_CH_2_gc 0x01
#define ADC_CH_3_gc 0x02
#define ADC_CH_4_gc 0x03
#define ADC_CH_5_gc 0x04
#define ADC_CH_6_gc 0x05
#define ADC_CH_7_gc 0x06
#define ADC_CH_8_gc 0x07
#define ADC_SPI_CONFIG_gc 0x54

// Gain settings
#define GAIN_1_gc 0x00
#define GAIN_2_gc 0x01
#define GAIN_4_gc 0x02
#define GAIN_8_gc 0x03
#define GAIN_16_gc 0x04
#define GAIN_32_gc 0x05
#define GAIN_64_gc 0x06
#define GAIN_128_gc 0x07

// Bit masks
#define BIT0_bm 0x01
#define BIT1_bm 0x02
#define BIT2_bm 0x04
#define BIT3_bm 0x08
#define BIT4_bm 0x10
#define BIT5_bm 0x20
#define BIT6_bm 0x40
#define BIT7_bm 0x80

#define SDHC_SECTOR_SIZE 512 
#define SDHC_CMD_RESET 0x00
#define SDHC_CMD_START_INITIALIZATION 0x01
#define SDHC_CMD_SEND_CSD 0x09
#define SDHC_CMD_SEND_CID 0x0A
#define SDHC_CMD_STOP_TRANSMISSION 0x0C     12
#define SDHC_CMD_SEND_STATUS 0x0           13
#define SDHC_CMD_SET_BLOCK_SIZE        16
#define SDHC_CMD_READ_SINGLE_BLOCK     17
#define SDHC_CMD_READ_MULTIPLE_BLOCKS  18
#define SDHC_CMD_WRITE_SINGLE_BLOCK    24
#define SDHC_CMD_WRITE_MULTIPLE_BLOCKS 25
#define SDHC_CMD_PROGRAM_CSD           27
#define SDHC_CMD_SET_WRITE_PROT        28
#define SDHC_CMD_CLR_WRITE_PROT        29
#define SDHC_CMD_SEND_WRITE_PROT       30
#define SDHC_CMD_TAG_SECTOR_START      32
#define SDHC_CMD_TAG_SECTOR_END        33 
#define SDHC_CMD_UNTAG_SECTOR          34
#define SDHC_CMD_TAG_ERASE_GROUP_START 35
#define SDHC_CMD_TAG_ERASE_GROUP_END   36
#define SDHC_CMD_UNTAG_ERASE_GROUP     37
#define SDHC_CMD_ERASE                 38
#define SDHC_CMD_LOCK_UNLOCK           42
#define SDHC_CMD_READ_OCR              58 
#define SDHC_CMD_CRC_ON_OFF            59 
#define SDHC_DATA_TOKEN               0xFE
#define SDHC_RESPONSE_OK              0x05
#define SDHC_RESPONSE_CRC_ERROR       0x0B
#define SDHC_RESPONSE_WRITE_ERROR     0x0D
#define SDHC_RESPONSE_MASK            0x1F

// Function Prototypes
// breakpoint check functions
void CO_collectTemp(uint16_t *avgV, uint16_t *minV, uint16_t *maxV);
void CO_collectBatt(uint16_t *avgV, uint16_t *minV, uint16_t *maxV);
void CO_collectSP(uint8_t channel, int32_t *averageV, int32_t *minV,
			int32_t *maxV, uint8_t gainExponent);
void CO_collectADC(uint8_t channel, uint8_t filterConfig, int32_t *avgV, int32_t *minV,
			int32_t *maxV, uint8_t gainExponent, uint8_t spsExponent);
void CO_collectSeismic3Channel(uint8_t filterConfig, uint8_t gain[], uint8_t subsamplesPerSecond,
	uint8_t subsamplesPerChannel, uint8_t DCPassEnable, uint16_t averagingPtA, uint16_t averagingPtB,
	uint16_t averagingPtC, uint16_t averagingPtD);
void CO_collectSeismic1Channel(uint8_t channel, uint8_t filterConfig, uint8_t gain,
	uint8_t subsamplesPerSecond, uint8_t subsamplesPerSample, uint8_t DCPassEnable,
	uint16_t averagingPtA, uint16_t averagingPtB, uint16_t averagingPtC,
	uint16_t averagingPtD);
void FRAMTest3Channel();
void FRAMTest1Channel();
void FRAMWriteKnownsCheck();


// Utility functions
void set_16MHz();
void set_32MHz();
void setXOSC_32MHz();
void PortEx_DIRSET(uint8_t portMask, uint8_t bank);
void PortEx_DIRCLR(uint8_t portMask, uint8_t bank);
void PortEx_OUTSET(uint8_t portMask, uint8_t bank);
void PortEx_OUTCLR(uint8_t portMask, uint8_t bank);
//void setPortEx(uint8_t portMask, uint8_t bank);  // replaced with functions above for greater flexibility
void portExCS(uint8_t write);
void ADCPower(uint8_t on);
void Ext1Power(uint8_t on);
void Ext2Power(uint8_t on);
void HVPower(uint8_t on);
void set_ampGain(uint8_t channel, uint8_t gainExponent);
void lowerMuxCS(uint8_t write);
void upperMuxCS(uint8_t write);
void setADCInput(uint8_t channel);
uint8_t readPortEx(uint8_t readRegister);
void SPIInit(uint8_t mode);
void SPICS(uint8_t enable);
void SPIDisable();
void enableADCMUX(uint8_t on);
void set_filter(uint8_t filterConfig);
void ACC_DCPassEnable(uint8_t enable);
void writeSE2FRAM();
void readFRAM (uint16_t numBytes);
void calcChecksumFRAM(void);
void sampleCurrentChannel();
void SDHC_init(void);
void SDHC_send_command(uint8_t command, uint32_t arg);
void SDHC_read_sector(void);
void SDHC_write_sector(void);
uint8_t SDHC_get_response(void);
uint16_t SDHC_CRC16(uint8_t *data, uint16_t bytes);
void SDHC_read_block(uint8_t *buffer, uint16_t address, uint16_t numBlocks);
void SDHC_write_block(uint8_t *buffer, uint16_t address, uint16_t numBlocks);
void SDHC_read_register(uint8_t *buffer, uint8_t cmd);
void SDHC_CS(uint8_t enable);
void FRAMWriteKnowns();
