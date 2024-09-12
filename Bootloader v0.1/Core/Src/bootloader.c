/*
 *
 * bootloader.c
 *
 *  Created on: Sep 9, 2024
 *      Author: batuhan.odcikin
 */

#include "bootloader.h"
#include <stdio.h>
#include <string.h>
#include "stm32h7xx_hal.h"



HAL_StatusTypeDef status;

uint8_t response_wait_for_upload[4] = {0xAA, RESPONSE, 0x01, 0xEE};
uint8_t response_ready_to_upload[4] = {0xAA, RESPONSE, 0x02, 0xEE};
uint8_t response_frame_receive_ok[4] = {0xAA, RESPONSE, 0x03, 0xEE};
uint8_t response_frame_receive_full[4] = {0xAA, RESPONSE, 0x04, 0xEE};
uint8_t response_frame_err[4] = {0xAA, RESPONSE, 0x05, 0xEE};

uint8_t rxData[10000];

uint8_t binaryFile[128][CHUNK_SIZE];
uint8_t ret;


void runApplication(uint32_t addr){

	void (*app_reset_handler)(void) = (void*)(*((volatile uint32_t*) (addr + 4)));
	// __set_MSP(*(__IO uint32_t*) addr);

	app_reset_handler(); // run the app
}

uint8_t ready_to_upload(){
	HAL_StatusTypeDef stat;
	uint8_t app_no;
	uint16_t chunk_cnt;
	uint32_t addr;

	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
	ret = recieve_bin_file(binaryFile, &app_no, &chunk_cnt);
	if(ret == 1){
		addr = 	(app_no == 1) ? APP1 :
				(app_no == 2) ? APP2 :
				(app_no == 3) ? APP3 : APP4;

		stat = upload_to_flash(binaryFile,addr,chunk_cnt);
	}
	else{
		// an error occured while file transfering
		return 0;
	}
	return 1;
}

uint8_t recieve_bin_file(uint8_t data[][CHUNK_SIZE],uint8_t *app_no,uint16_t *chunk_cnt){
	DataFrame data_frame;
	uint8_t rx_byte[2*CHUNK_SIZE] = {0};
	uint8_t listen_flag = 2;
	uint16_t index = 0;
	uint32_t tick_ctr = HAL_GetTick();
    uint16_t chunk_nbr = 0;
    uint16_t chunk_receive_ctr = 0;

	// SOF-PackNo-Len-DATA-CRC32-EOF
	while(listen_flag != 0){
		data_frame.sof = 0;
		data_frame.eof = 0;
		HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 1, 100); // Recieve SOF byte
		if (rx_byte[index] == SOF_BYTE){ // if SOF Recieved start receiving rest of the frame
			data_frame.sof = rx_byte[index];
			index++;

			HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 1, 100); // Recieve SOF byte
			data_frame.type = rx_byte[index];
			index++;
//			HAL_Delay(100);

			switch(data_frame.type){
				case UPLOAD_INFO: 	// SOF-TYPE-APP-FRAME_NBR-EOF
					HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 1, 100); // Recieve SOF byte
					*app_no = rx_byte[index];
					index++;

					HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 2, 100);
					chunk_nbr = (rx_byte[index] << 8) | (rx_byte[index+1]);
					*chunk_cnt = chunk_nbr;
					index += 2;

					HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 1, 100);
					data_frame.eof = rx_byte[index];
					index++;
					break;
				case DATA: 			// SOF-TYPE-PKGNO-LEN-DATA-CRC-EOF
					// Receive pkg no
					HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 2, 100);
					data_frame.pkg_no = (rx_byte[index] << 8) | (rx_byte[index+1]);
					index += 2;

					// Receive data len
					HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 2, 100);
					data_frame.len = (rx_byte[index] << 8) | (rx_byte[index+1]);
					index += 2;

					// Receive data
					for(int i = 0; i < data_frame.len;i++){
						HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 1, 100);
						data_frame.data[i] = rx_byte[index];
						index++;
					}

					HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 5, 100);
					data_frame.crc = (rx_byte[index] << 24) | (rx_byte[index+1] << 16) | (rx_byte[index+2]) << 8 | (rx_byte[index+3]);
					index += 4;
					//TODO: Add CRC verification

					HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 1, 100);
					data_frame.eof = rx_byte[index];
					index++;
					break;
				case END:
					HAL_UART_Receive(&UART_HANDLER, &rx_byte[index], 1, 100);
					data_frame.eof = rx_byte[index];
					index++;
					break;
			}
			if(data_frame.eof == EOF_BYTE && data_frame.type == UPLOAD_INFO){
				// Frame ends
				HAL_UART_Transmit(&UART_HANDLER, &response_ready_to_upload,sizeof(response_ready_to_upload ), 100);
				listen_flag = 1;
			}
			else if(data_frame.eof == EOF_BYTE && data_frame.type == DATA){
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,GPIO_PIN_SET);
				HAL_UART_Transmit(&UART_HANDLER, &response_frame_receive_ok, sizeof(response_frame_receive_ok) , 100);
				for(int i=0; i<data_frame.len; i++){
					data[chunk_receive_ctr][i] = data_frame.data[i];
				}
				chunk_receive_ctr++;
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,GPIO_PIN_RESET);
			}
			else if(data_frame.eof == EOF_BYTE && data_frame.type == END){

				if(chunk_receive_ctr-1 == chunk_nbr){
					HAL_UART_Transmit(&UART_HANDLER, &response_frame_receive_full, sizeof(response_frame_receive_full) , 100);
					listen_flag = 0;
					HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,GPIO_PIN_SET);
				}
				else{
					HAL_UART_Transmit(&UART_HANDLER, &response_frame_err, sizeof(response_frame_err) , 100);
					return 0;
				}
			}
			else {
				// data pack err
				HAL_UART_Transmit(&UART_HANDLER, &response_frame_err, sizeof(response_frame_err) , 100);
				return 0;
			}
			index = 0;
		}
	}
	return 1;
}

void uart_mirror(){
//	memset(rxData,'\0',sizeof(rxData));
	HAL_UART_Receive(&UART_HANDLER, rxData, sizeof(rxData), 100);
	HAL_Delay(10);
	if(rxData[0] != '\0'){
		HAL_UART_Transmit(&UART_HANDLER, rxData, sizeof(rxData), 100);
		memset(rxData,'\0',sizeof(rxData));
	}

}

HAL_StatusTypeDef upload_to_flash(uint8_t data[][CHUNK_SIZE], uint32_t app_addr,uint16_t chunk_cnt){
	static FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t SECTORError = 0;
	status = HAL_FLASH_Unlock();
	HAL_Delay(100);

//	EraseInitStruct.TypeErase     =  FLASH_TYPEERASE_MASSERASE;// FLASH_TYPEERASE_SECTORS;
	EraseInitStruct.TypeErase     =  FLASH_TYPEERASE_SECTORS;

	EraseInitStruct.VoltageRange  = 4;
	EraseInitStruct.Banks     	  = FLASH_BANK_1;
	EraseInitStruct.NbSectors 	  = 20;
    EraseInitStruct.Sector        = (app_addr == APP1) ? FLASH_SECTOR_30 :
                                    (app_addr == APP2) ? FLASH_SECTOR_40 :
                                    (app_addr == APP3) ? FLASH_SECTOR_50 :
                                    FLASH_SECTOR_60;

	status = HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError);


	for(int i=0; i <= chunk_cnt; i++){
//		memset(data_word,'\0',sizeof(data_word));
		for(int j=0; j < CHUNK_SIZE; j+=16){
			status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,(app_addr + i*CHUNK_SIZE + j),(uint64_t*)&data[i][j]);
			if(status != HAL_OK){
				return HAL_ERROR;
			}
		}
	}


//	data_word[0] = 0;
//	data_word[1] = 1000;
//	data_word[2] = 123;
//	data_word[3] = 34232;
//	status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,(app_addr),(data_word[0]));
//	status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,(app_addr + 16),(data_word[1]));
//	status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,(app_addr + 32),(data_word[2]));
//	status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,(app_addr + 48),(data_word[3]));

	HAL_FLASH_Lock();
	return HAL_OK;
}

void send_response(uint8_t type){
	uint8_t *response;
	response = 	(type == WAIT_FOR_UPLOAD) ? response_wait_for_upload:
				(type == READY_TO_UPLOAD) ? response_ready_to_upload :
				(type == RECEIVE_OK) ? response_frame_receive_ok :
				(type == RECEIVE_FULL) ? response_frame_receive_full :
				response_frame_err;

	HAL_UART_Transmit(&UART_HANDLER, response, 4 , 100);
}
